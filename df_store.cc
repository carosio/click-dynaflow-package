#include <click/config.h>
#include "ei.hh"
#include "df_clients.hh"
#include "df_store.hh"
#include "df.hh"
#include "df_flow.hh"
#include <click/hashallocator.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
#include <fcntl.h>

IdManager group_ids;

DF_Store::DF_Store()
    : _listen_fd(-1), cookie("secret"), node_name("click-dp"),
      _gc_timer(gc_timer_hook, this)
{
    _gc_interval_sec = 30;
}

DF_Store::~DF_Store()
{
}

int
DF_Store::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(this, errh).bind(conf)
	.read("NODE", StringArg(), node_name)
	.read("COOKIE", StringArg(), cookie)
	.read_p("LOCAL_ADDR", local_ip)
        .consume() < 0)
        return -1;

    return 0;
}

int
DF_Store::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
    int e = errno;                // preserve errno

    if (_listen_fd >= 0) {
	remove_select(_listen_fd, SELECT_READ | SELECT_WRITE);
	close(_listen_fd);
	_listen_fd = -1;
    }

    return errh->error("%s: %s", syscall, strerror(e));
}

int
DF_Store::initialize_socket_error(ErrorHandler *errh, const char *syscall, int e)
{
    if (_listen_fd >= 0) {
	remove_select(_listen_fd, SELECT_READ | SELECT_WRITE);
	close(_listen_fd);
	_listen_fd = -1;
    }

    return errh->error("%s: %s", syscall, strerror(e));
}

int
DF_Store::initialize(ErrorHandler *errh)
{
    int on = 1;
    struct sockaddr_in addr;
    socklen_t slen;
    char *p;
    char thishostname[EI_MAXHOSTNAMELEN];
    char thisalivename[EI_MAXHOSTNAMELEN];
    char thisnodename[EI_MAXHOSTNAMELEN];
    struct hostent *hp;

    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr = local_ip.in_addr();

    if ((_listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) < 0)
	return initialize_socket_error(errh, "socket");

    setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if (bind(_listen_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
	return initialize_socket_error(errh, "bind");

    slen = sizeof(addr);
    if (getsockname(_listen_fd, (struct sockaddr *)&addr, &slen) < 0)
	return initialize_socket_error(errh, "getsockname");

    // derive Erlang node name
    strncpy(thisalivename, node_name.c_str(), sizeof(thishostname));
    p = strchr(thisalivename, '@');
    if (p) {
	*p = '\0';
	strncpy(thishostname, p + 1, sizeof(thishostname));
	strncpy(thisnodename, node_name.c_str(), sizeof(thishostname));
    } else {
	if (gethostname(thishostname, sizeof(thishostname)) < 0)
	    return initialize_socket_error(errh, "gethostname");

	if ((hp = gethostbyname(thishostname)) == 0) {
	    /* Looking up IP given hostname fails. We must be on a standalone
	       host so let's use loopback for communication instead. */
	    if ((hp = gethostbyname("localhost")) == 0)
		return initialize_socket_error(errh, "gethostname");
	}

	click_chatter("%p{element}: thishostname: %s, hp->h_name: '%s'\n", this, thishostname, hp->h_name);
#if 0
	if (!node_name_long) {
	    char* ct;
	    if (strncmp(hp->h_name, "localhost", 9) == 0) {
		/* We use a short node name */
		if ((ct = strchr(thishostname, '.')) != NULL) *ct = '\0';
		snprintf(thisnodename, sizeof(thisnodename), "%s@%s", thisalivename, thishostname);
	    } else {
		/* We use a short node name */
		if ((ct = strchr(hp->h_name, '.')) != NULL) *ct = '\0';
		strcpy(thishostname, hp->h_name);
		snprintf(thisnodename, sizeof(thisnodename), "%s@%s", thisalivename, hp->h_name);
	    }
	} else
#endif
	    snprintf(thisnodename, sizeof(thisnodename), "%s@%s", thisalivename, hp->h_name);
    }

    click_chatter("%p{element}: thishostname: '%s'\n", this, thishostname);
    click_chatter("%p{element}: thisalivename: '%s'\n", this, thisalivename);
    click_chatter("%p{element}: thisnodename: '%s'\n", this, thisnodename);

    if (ei_connect_xinit(&ec, thishostname, thisalivename, thisnodename,
			 &addr.sin_addr, cookie.c_str(), 0) < 0)
	return initialize_socket_error(errh, "ei_connect_xinit", erl_errno);

    if (ei_publish(&ec, ntohs(addr.sin_port)) < 0)
	return initialize_socket_error(errh, "ei_publish", erl_errno);

    // start listening
    if (listen(_listen_fd, 2) < 0)
	return initialize_socket_error(errh, "listen");

    add_select(_listen_fd, SELECT_READ);

    _gc_timer.initialize(this);
    if (_gc_interval_sec)
        _gc_timer.schedule_after_sec(_gc_interval_sec);

    return 0;
}

void
DF_Store::cleanup(CleanupStage)
{
    if (_listen_fd >= 0) {
	// shut down the listening socket in case we forked
#ifdef SHUT_RDWR
	shutdown(_listen_fd, SHUT_RDWR);
#else
	shutdown(_listen_fd, 2);
#endif
	close(_listen_fd);
	_listen_fd = -1;
    }

    for (connection **it = _conns.begin(); it != _conns.end(); ++it)
        if (*it) {
            close((*it)->fd);
            delete *it;
        }
}

void
DF_Store::expire_flows()
{
    Vector<Flow *> expired;

    click_chatter("%s: expire_flows: %ld\n", declaration().c_str(), click_jiffies());

    for (FlowTable::iterator it = flows.begin(); it != flows.end(); ++it) {
	if (it.value()->is_expired() && !it.value()->deleted) {
	    it.value()->deleted = true;
	    expired.push_back(it.value());
	}
    }

    for (Vector<Flow *>::const_iterator it = expired.begin(); it != expired.end(); ++it) {
	delete_flow(*it);
    }
}

void
DF_Store::gc_timer_hook(Timer *t, void *user_data)
{
    DF_Store *store = static_cast<DF_Store *>(user_data);
    store->expire_flows();
    if (store->_gc_interval_sec)
        t->reschedule_after_sec(store->_gc_interval_sec);
}

DF_GroupEntryIP *
DF_Store::lookup_group_ip(uint32_t addr) const
{
    DF_GroupEntryIP *e = NULL;

    // FIXME: stupid and slow linear lookup, has to visit every entry in the vector => O(n) complexity!
    for (GroupTableIP::const_iterator it = ip_groups.begin(); it != ip_groups.end(); ++it) {
	if (((*it)->addr() & (*it)->mask()) == (addr & (*it)->mask())) {
	    if (!e || ntohl(e->mask()) < ntohl((*it)->mask()))
		e = *it;
	}
    }

    return e;
}

ClientValue *
DF_Store::lookup_client(uint32_t id_) const
{
    ClientValue *cv = clients.get(id_);
    click_chatter("%s: lookup_client %08x -> %p\n", declaration().c_str(), id_, cv);
    return cv;
}

Flow *
DF_Store::lookup_flow(const FlowData &fd)
{
    Flow *f = flows.get(fd);
    click_chatter("%s: lookup_flow f: %p\n", declaration().c_str(), f);

    if (f && f->is_expired()) {
	delete_flow(f);
	return NULL;
    }

    return f;
}

void
DF_Store::set_flow(Flow *f)
{
    click_chatter("%s: set_flow %08x -> %p\n", declaration().c_str(), f->id(), f);

    flows[f->origin()] = f;
    flows[f->reverse()] = f;
}

void
DF_Store::delete_flow(Flow *f)
{
    click_chatter("%s: delete_flow %08x -> %p\n", declaration().c_str(), f->id(), f);

    flows.erase(f->origin());
    flows.erase(f->reverse());
    delete f;
}

DF_Store::connection::connection(int fd_, ErlConnect *conp_,
				 DF_Store *store_,
				 ClientTable &clients_,
				 GroupTableMAC &mac_groups_,
				 GroupTableIP &ip_groups_,
				 FlowTable &flows_,
				 bool debug_, bool trace_)
    : debug(debug_), trace(trace_), fd(fd_),
      in_closed(false), out_closed(false),
      conp(*conp_), x_in(2048), x_out(2048),
      store(store_), clients(clients_),
      mac_groups(mac_groups_), ip_groups(ip_groups_),
      flows(flows_)
{
}

// data structure decoder functions

// rule sample: {{{10, 0, 1, 0}, 24}, "Class 1"}
void
DF_Store::connection::decode_group()
{
    unsigned long prefix_len;
    IPAddress addr;
    IPAddress prefix;
    DF_GroupEntry::GroupId group;

    if (x_in.decode_tuple_header() != 2 || x_in.decode_tuple_header() != 2)
	throw ei_badarg();

    addr = x_in.decode_ipaddress();
    prefix_len = x_in.decode_ulong();
    group = x_in.decode_ulong();

    DF_GroupEntryIP *ip_grp = new DF_GroupEntryIP(group, addr, IPAddress::make_prefix(prefix_len));
    if (trace)
	click_chatter("decode_group: %s\n", ip_grp->unparse().c_str());
    ip_groups.push_back(ip_grp);
}

// rules sample: [{{{10, 0, 1, 0}, 24}, "Class 1"}, {{{10, 0, 2, 0}, 24}, "Class 2"}],
void
DF_Store::connection::decode_groups()
{
    int arity;

    if (trace)
	click_chatter("decode_groups: %s\n", x_in.unparse().c_str());

    if ((arity = x_in.decode_list_header()) == 0)
	// empty list
	return;

    do {
	int type;
	int size;

	x_in.get_type(&type, &size);
	if (type == ERL_NIL_EXT) {
	    x_in.skip_term();
	    break;
	}
	decode_group();
    } while (arity-- > 0);
}

// Key: {inet,<<172,20,48,19>>}
ClientKey
DF_Store::connection::decode_client_key()
{
    int type;
    int size;
    struct in_addr addr;

    if (trace)
	click_chatter("decode_client_key: %s\n", x_in.unparse().c_str());

    if (x_in.decode_tuple_header() != 2)
	throw ei_badarg();

    if (x_in.decode_atom() == "inet") {
	x_in.get_type(&type, &size);
	if (type != ERL_BINARY_EXT || size != 4)
	    throw ei_badarg();
	x_in.decode_binary((void *)&addr, sizeof(addr));
	return ClientKey(AF_INET, IPAddress(addr));
    }
    else
	throw ei_badarg();
}

NATTranslation
DF_Store::connection::decode_nat_translation(const String type_atom)
{
    int type;
    IPAddress nat_addr;
    long unsigned int min_port = 0;
    long unsigned int max_port = 0;

    if (trace)
	click_chatter("decode_nat_translation: %s\n", x_in.unparse().c_str());

    for (type = 0; type < NATTranslation::LastEntry + 1; type++)
	if (NATTranslation::Translations[type] ==  type_atom)
	    break;

    switch (type) {
    case NATTranslation::SymetricAddressKeyed:
    case NATTranslation::AddressKeyed:
	nat_addr = x_in.decode_ipaddress();
	break;

    case NATTranslation::PortKeyed:
	if (x_in.decode_tuple_header() != 3)
	    throw ei_badarg();

	nat_addr = x_in.decode_ipaddress();
	min_port = x_in.decode_ulong();
	max_port = x_in.decode_ulong();
	break;

    case NATTranslation::Random:
    case NATTranslation::RandomPersistent:
    case NATTranslation::Masquerade:
	x_in.skip_term();
	break;

    default:
	throw ei_badarg();
    }

    return NATTranslation(type, nat_addr, min_port, max_port);
}

void
DF_Store::connection::decode_nat(NATTable &nat_rules)
{
    if (trace)
	click_chatter("decode_nat: %s\n", x_in.unparse().c_str());

    if (x_in.decode_tuple_header() != 3)
	throw ei_badarg();

    IPAddress addr = x_in.decode_ipaddress();
    String translation_type = x_in.decode_atom();
    NATTranslation translation = decode_nat_translation(translation_type);

    if (trace)
	click_chatter("decode_nat: %s -> %s\n", addr.unparse().c_str(), translation.unparse().c_str());
    nat_rules.insert(addr, translation);
}

NATTable
DF_Store::connection::decode_nat_list()
{
    NATTable nat_rules;
    int arity;

    if (trace)
	click_chatter("decode_nat_list: %s\n", x_in.unparse().c_str());

    if ((arity = x_in.decode_list_header()) == 0)
	// empty list
	return nat_rules;

    do {
	int type;
	int size;

	x_in.get_type(&type, &size);
	if (type == ERL_NIL_EXT) {
	    x_in.skip_term();
	    break;
	}

	decode_nat(nat_rules);
    } while (arity-- > 0);

    return nat_rules;
}

void
DF_Store::connection::encode_nat_list(const NATTable &rules)
{
     for (NATTable::const_iterator it = rules.begin(); it != rules.end(); ++it) {
	 x_out.encode_list_header(1)
	     .encode_tuple_header(3)
	     .encode_ipaddress(it.key())
	     .encode_atom(NATTranslation::Translations[it.value().type]);

	 switch (it.value().type) {
	 case NATTranslation::SymetricAddressKeyed:
	 case NATTranslation::AddressKeyed:
	     x_out.encode_ipaddress(it.value().nat_addr);
	     break;

	 case NATTranslation::PortKeyed:
	     x_out.encode_tuple_header(3)
		 .encode_ipaddress(it.value().nat_addr)
		 .encode_long(it.value().min_port)
		 .encode_long(it.value().max_port);
	     break;

	 case NATTranslation::Random:
	 case NATTranslation::RandomPersistent:
	 case NATTranslation::Masquerade:
	     x_out.encode_atom("undefined");
	     break;

	 default:
	     x_out.encode_long(it.value().type);
	     break;
	 }
     }
     x_out.encode_empty_list();
}

void
DF_Store::connection::decode_client_rule(ClientRuleTable &rules)
{
    DF_RuleAction out = DF_RULE_UNKNOWN;

    if (trace)
	click_chatter("decode_client_rules: %s\n", x_in.unparse().c_str());

    if (x_in.decode_tuple_header() != 3)
	throw ei_badarg();

    DF_GroupEntry::GroupId gs_id = x_in.decode_ulong();
    DF_GroupEntry::GroupId gd_id = x_in.decode_ulong();
    String out_atom = x_in.decode_atom();

    for (int i = 0; i < DF_RULE_SIZE; i++)
	if (ClientRule::ActionType[i] == out_atom) {
	    out = (DF_RuleAction)i;
	    break;
	}

    rules.push_back(new ClientRule(gs_id, gd_id, out));
}

ClientRuleTable
DF_Store::connection::decode_client_rules_list()
{
    ClientRuleTable rules;
    int arity;

    if (trace)
	click_chatter("decode_client_rules_list: %s\n", x_in.unparse().c_str());

    if ((arity = x_in.decode_list_header()) == 0)
	// empty list
	return rules;

    do {
	int type;
	int size;

	x_in.get_type(&type, &size);
	if (type == ERL_NIL_EXT) {
	    x_in.skip_term();
	    break;
	}

	decode_client_rule(rules);
    } while (arity-- > 0);

    return rules;
}

void
DF_Store::connection::encode_client_rules_list(const ClientRuleTable &rules)
{
    for (ClientRuleTable::const_iterator it = rules.begin(); it != rules.end(); ++it) {
	x_out.encode_list_header(1)
	    .encode_tuple_header(3)
	    .encode_long((*it)->src)
	    .encode_long((*it)->dst)
	    .encode_atom(ClientRule::ActionType[(*it)->out]);
    }
    x_out.encode_empty_list();
}

// Value: {<<"DEFAULT">>,[],[{<<"Class 1">>,<<"Class 2">>,accept},{<<"Class 2">>,<<"Class 1">>,drop}]}
ClientValue *
DF_Store::connection::decode_client_value(ClientKey key)
{
    if (trace)
	click_chatter("decode_client_value: %s\n", x_in.unparse().c_str());

    if (x_in.decode_tuple_header() != 3)
	throw ei_badarg();

    DF_GroupEntry::GroupId group = x_in.decode_ulong();
    NATTable nat_rules = decode_nat_list();
    ClientRuleTable rules = decode_client_rules_list();

    return new ClientValue(key, group, nat_rules, rules);
}

// client sample {{inet,<<172,20,48,19>>}, {<<"DEFAULT">>,[],[{<<"Class 1">>,<<"Class 2">>,accept},{<<"Class 2">>,<<"Class 1">>,drop}]}}
void
DF_Store::connection::decode_client()
{
    if (trace)
	click_chatter("decode_client: %s\n", x_in.unparse().c_str());

    if (x_in.decode_tuple_header() != 2)
	throw ei_badarg();

    ClientKey key = decode_client_key();
    ClientValue *value = decode_client_value(key);

    click_chatter("insert client: %08x, %p\n", value->id(), value);
    clients[value->id()] = value;

    DF_GroupEntryIP *ip_grp = new DF_GroupEntryIP(value);
    if (trace)
	click_chatter("decode_group: %s\n", ip_grp->unparse().c_str());
    ip_groups.push_back(ip_grp);
}

void
DF_Store::connection::decode_clients()
{
    int arity;

    if (trace)
	click_chatter("decode_clients: %s\n", x_in.unparse().c_str());

    if ((arity = x_in.decode_list_header()) == 0)
	// empty list
	return;

    do {
	int type;
	int size;

	x_in.get_type(&type, &size);
	if (type == ERL_NIL_EXT) {
	    x_in.skip_term();
	    break;
	}

	decode_client();
    } while (arity-- > 0);
}

// rule sample: {{{10, 0, 1, 0}, 24}, "Class 1"}
void
DF_Store::connection::dump_groups()
{
    for (GroupTableMAC::const_iterator it = mac_groups.begin(); it != mac_groups.end(); ++it) {
	x_out.encode_list_header(1);
	x_out.encode_atom("ok");
    }
    for (GroupTableIP::const_iterator it = ip_groups.begin(); it != ip_groups.end(); ++it) {
	x_out.encode_list_header(1)
	    .encode_tuple_header(2);

	x_out.encode_tuple_header(2)
	    .encode_ipaddress((*it)->addr())
	    .encode_long((*it)->prefix_len());

	x_out.encode_long((*it)->id());
    }
    x_out.encode_empty_list();
}


void
DF_Store::connection::dump_clients()
{
    for (ClientTable::const_iterator it = clients.begin(); it != clients.end(); ++it) {
	x_out.encode_list_header(1);

	// {Key, Value}
	x_out.encode_tuple_header(2);

	// Key = {Type, Addr} - FIXME: only 'inet' for now
	x_out.encode_tuple_header(2)
	    .encode_atom("inet")
	    .encode_binary(it.value()->key.addr.data(), 4);

	// Value = {Group, NATTable, ClientRuleTable}
	x_out.encode_tuple_header(3);
	x_out.encode_long(it.value()->group);
	encode_nat_list(it.value()->nat_rules);
	encode_client_rules_list(it.value()->rules);
    }
    x_out.encode_empty_list();
}

#define jiffies_diff(a, b) ({			\
	    __typeof__(a) _a = (a);		\
	    __typeof__(b) _b = (b);		\
	    _a > _b ? (click_jiffies_difference_t)(_a - _b) : -(click_jiffies_difference_t)(_b - _a); \
	})

void
DF_Store::connection::dump_flows()
{
    click_jiffies_t now = click_jiffies();

    for (FlowTable::const_iterator it = flows.begin(); it != flows.end(); ++it) {
	const FlowData &k = it.key();
	const Flow *f = it.value();

	if (f) {
	    x_out.encode_list_header(1);

	    x_out.encode_tuple_header(4)
		.encode_long(f->id())
		// {Proto, {SrcIPv4, SrcPort}, {DstIPv4, DstPort}}
		.encode_tuple_header(3)
		    .encode_long(k.proto)
		    .encode_tuple_header(2)
			.encode_ipaddress(k.src_ipv4)
			.encode_long(ntohs(k.src_port))
		    .encode_tuple_header(2)
			.encode_ipaddress(k.dst_ipv4)
			.encode_long(ntohs(k.dst_port))
		// {SourceGroup, DestinationGroup, Action}
		.encode_tuple_header(3)
		    .encode_long(f->srcGroup)
		    .encode_long(f->dstGroup)
		    .encode_long(f->action)
		.encode_long(jiffies_diff(f->_expiry, now) * (1000 / CLICK_HZ));
	}
    }

    x_out.encode_empty_list();
}

// Erlang call handlers

void
DF_Store::connection::erl_bind(int arity)
{
    if (arity != 2)
	throw ei_badarg();

    bind_pid = x_in.decode_pid();

    x_out.encode_atom("ok");
}

void
DF_Store::connection::erl_init(int arity)
{
    if (trace)
	click_chatter("erl_init: %d\n", arity);

    if (arity != 3)
	throw ei_badarg();

    decode_groups();
    decode_clients();

    x_out.encode_atom("ok");
}

void
DF_Store::connection::erl_insert(int arity)
{
    if (trace)
	click_chatter("erl_insert: %d\n", arity);

    if (arity != 3)
	throw ei_badarg();

    ClientKey key = decode_client_key();
    ClientValue *value = decode_client_value(key);

    clients[value->id()] = value;
    click_chatter("insert client: %08x, %p\n", value->id(), value);

    DF_GroupEntryIP *ip_grp = new DF_GroupEntryIP(value);
    if (trace)
	click_chatter("decode_group: %s\n", ip_grp->unparse().c_str());
    ip_groups.push_back(ip_grp);

    x_out.encode_atom("ok");
}

void
DF_Store::connection::erl_dump(int arity)
{
    if (trace)
	click_chatter("erl_init: %d\n", arity);

    if (arity == 1) {
	x_out.encode_tuple_header(2);
	dump_groups();
	dump_clients();
	dump_flows();
    }
    else if (arity == 2) {
	String what = x_in.decode_atom();
	if (what == "groups")
	    dump_groups();
	else if (what == "clients")
	    dump_clients();
	else if (what == "flows")
	    dump_flows();
	else
	    throw ei_badarg();
    }
    else
	throw ei_badarg();
}

// Erlang generic call handlers

void
DF_Store::connection::handle_gen_call_click(const String fn, int arity)
{
    if (fn == "bind") {
	erl_bind(arity);
    }
    if (fn == "init") {
	erl_init(arity);
    }
    if (fn ==  "insert") {
	erl_insert(arity);
    }
    if (fn ==  "dump") {
	erl_dump(arity);
    }
    else
	x_out.encode_atom("error");
}

void
DF_Store::connection::handle_gen_call(const String to)
{
	erlang_pid pid;
	erlang_ref ref;
	int arity;
	String fn;

	int r __attribute__((unused));

	/* decode From tupple */
	if (x_in.decode_tuple_header() != 2)
	    throw ei_badarg();

	pid = x_in.decode_pid();
	ref = x_in.decode_ref();

	/* decode call args */
	arity = x_in.decode_tuple_header();
	fn = x_in.decode_atom();

	x_out.encode_version()
	    .encode_tuple_header(2)
	    .encode_ref(ref);

	if (to == "net_kernel" && fn ==  "is_auth") {
	    x_out.encode_atom("yes");
	}
	else if (to == "click") {
	    try {
		handle_gen_call_click(fn, arity);
	    }
	    catch (ei_badarg &e) {
		x_out.encode_atom("badarg");
	    }
	}
	else
	    x_out.encode_atom("error");

	if (debug)
	    click_chatter("Msg-Out: %s\n", x_out.unparse(0).c_str());

	r = ei_send(fd, &pid, x_out.buffer()->buff, x_out.buffer()->index);

	if (debug)
	    click_chatter("send ret: %d", r);
}

void
DF_Store::connection::handle_gen_cast(const String to __attribute__((unused)))
{
}

void
DF_Store::connection::handle_msg(const String to)
{
    if (debug)
	click_chatter("Msg-In: %s\n", x_in.unparse(0).c_str());

    x_in.reset();
    x_in.decode_version();

    try {
	int arity = x_in.decode_tuple_header();
	String type = x_in.decode_atom();

	if (arity == 3 && type == "$gen_call") {
	    handle_gen_call(to);
	}
	else if (arity == 2 && type == "$gen_cast") {
	    handle_gen_cast(to);
	}
	else
	    click_chatter("Ignoring Msg %s.\n", type.c_str());
    }
    catch (std::exception &e) {
	click_chatter("Exception %s.\n", e.what());
    }
}

void
DF_Store::connection::read()
{
    erlang_msg msg;
    int r;

    x_out.reset();
    x_in.reset();

    r = ei_xreceive_msg(fd, &msg, x_in.buffer());
    if (debug)
	click_chatter("conn:read: %d\n", fd);

    if (r == ERL_TICK) {
	if (debug) click_chatter("DEBUG: TICK");
	/* ignore */
    } else if (r == ERL_ERROR) {
	if (debug) click_chatter("DEBUG: ERL_ERROR");
	in_closed = true;
    } else {
	switch (msg.msgtype) {
	case ERL_SEND:
	case ERL_REG_SEND:
	    handle_msg(msg.toname);
	    break;

	default:
	    if (debug) click_chatter("Invalid msg.msgtype: %ld\n", msg.msgtype);
	    break;
	}
    }
}

void
DF_Store::initialize_connection(int fd, ErlConnect *conp)
{
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    add_select(fd, SELECT_READ);
    if (_conns.size() <= fd)
        _conns.resize(fd + 1);
    _conns[fd] = new connection(fd, conp, this, clients, mac_groups, ip_groups, flows, true);
}

void
DF_Store::selected(int fd, int)
{
    if (fd == _listen_fd) {
	ErlConnect conp;

	int new_fd = ei_accept_tmo(&ec, _listen_fd, &conp, 100);
	if (new_fd == ERL_ERROR) {
	    click_chatter("%s: accept: %d, %d", declaration().c_str(), new_fd, erl_errno);
            return;
        }

        initialize_connection(new_fd, &conp);
        fd = new_fd;
    }

    // find file descriptor
    if (fd >= _conns.size() || !_conns[fd]) {
	click_chatter("%s: fd not found: %d\n", declaration().c_str(), fd);
        return;
    }
    connection *conn = _conns[fd];

    // read commands from socket (but only a bit on each select)
    if (!conn->in_closed)
	conn->read();

    // maybe close out
    if (conn->in_closed || conn->out_closed) {
        remove_select(conn->fd, SELECT_READ | SELECT_WRITE);
        close(conn->fd);
	click_chatter("%s: closed connection %d", declaration().c_str(), fd);
        _conns[conn->fd] = 0;
        delete conn;
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_Store)
