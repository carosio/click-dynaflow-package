#include <click/config.h>
#include "df_store.hh"
#include "df.hh"
#include <click/hashallocator.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
#include <fcntl.h>
#include "ei.h"

#define DEBUG

CLICK_DECLS

extern "C" {

void* ei_malloc (long size);

}

ei_x::ei_x(long size)
{
#define BLK_MAX 127

    size = (size + BLK_MAX) & ~BLK_MAX;

#undef BLK_MAX

    x.buff = (char *)ei_malloc(size);
    x.buffsz = size;
    x.index = 0;

    if (!x.buff)
	throw std::bad_alloc();
}

ei_x::~ei_x()
{
    ei_x_free(&x);
}

// EI_X decoder

String ei_x::decode_string()
{
    int type;
    int size;

    get_type(&type, &size);

    String str = String::make_uninitialized(size);
    // NB: mutable_c_str() creates space for the terminating null character
    if (char *s = str.mutable_c_str()) {
	if (::ei_decode_string(x.buff, &x.index, s) != 0)
	    throw ei_badarg();
    } else
	    throw std::bad_alloc();

    return str;
}

String ei_x::decode_binary_string()
{
    int type;
    int size;
    long bin_size;

    get_type(&type, &size);

    if (type != ERL_BINARY_EXT)
	throw ei_badarg();

    String str = String::make_uninitialized(size);
    // NB: mutable_c_str() creates space for the terminating null character
    if (char *s = str.mutable_c_str()) {
	if (::ei_decode_binary(x.buff, &x.index, s, &bin_size) != 0)
	    throw ei_badarg();
    } else
	throw std::bad_alloc();

    return str;
}
int ei_x::decode_tuple_header()
{
    int arity;

    if (::ei_decode_tuple_header(x.buff, &x.index, &arity) != 0)
	throw ei_badarg();

    return arity;
}

int ei_x::decode_list_header()
{
    int arity;

    if (::ei_decode_list_header(x.buff, &x.index, &arity) != 0)
	throw ei_badarg();

    return arity;
}

int ei_x::decode_version()
{
    int version;

    if (::ei_decode_version(x.buff, &x.index, &version) != 0)
	throw ei_badarg();

    return version;
}

erlang_ref ei_x::decode_ref()
{
    erlang_ref p;

    if (::ei_decode_ref(x.buff, &x.index, &p) != 0)
	throw ei_badarg();

    return p;
}

erlang_pid ei_x::decode_pid()
{
    erlang_pid p;

    if (::ei_decode_pid(x.buff, &x.index, &p) != 0)
	throw ei_badarg();

    return p;
}

unsigned long ei_x::decode_ulong()
{
    unsigned long p;

    if (::ei_decode_ulong(x.buff, &x.index, &p) != 0)
	throw ei_badarg();

    return p;
}

String ei_x::decode_atom()
{
    char atom[MAXATOMLEN+1] = {0};

    if (::ei_decode_atom(x.buff, &x.index, atom) != 0)
	throw ei_badarg();

    return atom;
}

void ei_x::decode_binary(void *b, int len)
{
    int type;
    int size;
    long bin_size;

    get_type(&type, &size);
    if (type != ERL_BINARY_EXT
	|| size != len
	|| ::ei_decode_binary(x.buff, &x.index, b, &bin_size) != 0
	|| bin_size != len)
	throw ei_badarg();
}

void ei_x::get_type(int *type, int *size)
{
    if (::ei_get_type(x.buff, &x.index, type, size) != 0)
	throw ei_badarg();
}

void ei_x::skip_term()
{
    if (::ei_skip_term(x.buff, &x.index) != 0)
	throw ei_badarg();
}

// EI_X encoder

void ei_x::encode_version()
{
    ei_x_encode_version(&x);
}

void ei_x::encode_tuple_header(int arity)
{
    ei_x_encode_tuple_header(&x, arity);
}

void ei_x::encode_ref(const erlang_ref &ref)
{
    ei_x_encode_ref(&x, &ref);
}

void ei_x::encode_atom(const char *s)
{
    ei_x_encode_atom(&x, s);
}

void ei_x::encode_atom(const String &s)
{
    ei_x_encode_atom(&x, s.c_str());
}

IdManager group_ids;

DF_Store::DF_Store()
    : _listen_fd(-1), cookie("secret"), node_name("click-dp")
{
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

int
DF_Store::lookup_group(const String name) const
{
    // FIXME: stupid and slow linear lookup, has to visit every entry in the vector => O(n) complexity!
    for (DF_GetGroupIP::GroupTable::const_iterator it = ip_groups.begin(); it != ip_groups.end(); ++it) {
	if (((*it)->group_name() == name))
	    return it - ip_groups.begin();
    }

    // FIXME: we have multiple group vectors, using the vector index as group id is misleading
    return 0;
}

int
DF_Store::lookup_group_ip(uint32_t addr) const
{
    DF_GetGroupIP::entry *e = NULL;
    int pos = 0;

    // FIXME: stupid and slow linear lookup, has to visit every entry in the vector => O(n) complexity!
    for (DF_GetGroupIP::GroupTable::const_iterator it = ip_groups.begin(); it != ip_groups.end(); ++it) {
	if (((*it)->addr() & (*it)->mask()) == (addr & (*it)->mask())) {
	    if (!e || ntohl(e->mask()) < ntohl((*it)->mask())) {
		e = *it;
		pos = it - ip_groups.begin();
	    }
	}
    }

    // FIXME: we have multiple group vectors, using the vector index as group id is misleading
    return pos;
}

const char *DF_Store::NATTranslation::Translations[] =
    { "SymetricAddressKeyed",
      "AddressKeyed",
      "PortKeyed",
      "Random",
      "RandomPersistent",
      "Masquerade" };

StringAccum& DF_Store::NATTranslation::unparse(StringAccum& sa) const
{
    switch (type) {
    case SymetricAddressKeyed:
    case AddressKeyed:
	sa << Translations[type] << ": " << nat_addr;
	break;

    case PortKeyed:
	sa << Translations[type] << ": " << nat_addr << '[' << min_port << '-' << max_port << ']';
	break;

    case Random:
    case RandomPersistent:
    case Masquerade:
	sa << Translations[type];
	break;

    default:
	sa << "invalid";
	break;
    }

    return sa;
}

String DF_Store::NATTranslation::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}

DF_Store::connection::connection(int fd_, ErlConnect *conp_,
				 ClientTable &clients_,
				 DF_GetGroupMAC::GroupTable &mac_groups_,
				 DF_GetGroupIP::GroupTable &ip_groups_,
				 bool debug_, bool trace_)
    : debug(debug_), trace(trace_), fd(fd_),
      in_closed(false), out_closed(false),
      conp(*conp_), x_in(2048), x_out(2048),
      clients(clients_), mac_groups(mac_groups_), ip_groups(ip_groups_)
{
}

// data structure decoder functions

IPAddress
DF_Store::connection::decode_ipaddress()
{
    uint32_t a = 0;

    if (x_in.decode_tuple_header() != 4)
	throw ei_badarg();

    for (int i = 0; i < 4; i++) {
	a = a << 8;
	a |= x_in.decode_ulong();
    }

    return IPAddress(htonl(a));
}

// rule sample: {{{10, 0, 1, 0}, 24}, "Class 1"}
void
DF_Store::connection::decode_group()
{
    unsigned long prefix_len;
    IPAddress addr;
    IPAddress prefix;
    String group_name;

    if (x_in.decode_tuple_header() != 2 || x_in.decode_tuple_header() != 2)
	throw ei_badarg();

    addr = decode_ipaddress();
    prefix_len = x_in.decode_ulong();
    group_name = x_in.decode_string();

    DF_GetGroupIP::entry *grp = new DF_GetGroupIP::entry(addr, IPAddress::make_prefix(prefix_len), group_name);
    if (trace)
	click_chatter("decode_group: %s\n", grp->unparse().c_str());
    ip_groups.push_back(grp);
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
DF_Store::ClientKey
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

DF_Store::NATTranslation
DF_Store::connection::decode_nat_translation(const String type_atom)
{
    unsigned int type;
    IPAddress nat_addr;
    long unsigned int min_port = 0;
    long unsigned int max_port = 0;

    if (trace)
	click_chatter("decode_nat_translation: %s\n", x_in.unparse().c_str());

    for (type = 0; type < sizeof(NATTranslation::Translations) / sizeof(char *); type++)
	if (NATTranslation::Translations[type] ==  type_atom)
	    break;

    switch (type) {
    case NATTranslation::SymetricAddressKeyed:
    case NATTranslation::AddressKeyed:
	nat_addr = decode_ipaddress();
	break;

    case NATTranslation::PortKeyed:
	if (x_in.decode_tuple_header() != 3)
	    throw ei_badarg();

	nat_addr = decode_ipaddress();
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

    IPAddress addr = decode_ipaddress();
    String translation_type = x_in.decode_atom();
    NATTranslation translation = decode_nat_translation(translation_type);

    if (trace)
	click_chatter("decode_nat: %s -> %s\n", addr.unparse().c_str(), translation.unparse().c_str());
    nat_rules.insert(addr, translation);
}

DF_Store::NATTable
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
DF_Store::connection::decode_client_rule(ClientRuleTable &rules)
{
    int out = 0;

    if (trace)
	click_chatter("decode_client_rules: %s\n", x_in.unparse().c_str());

    if (x_in.decode_tuple_header() != 3)
	throw ei_badarg();

    String src = x_in.decode_binary_string();
    String dst = x_in.decode_binary_string();
    String out_atom = x_in.decode_atom();

    if (out_atom == "deny")
	out = 0;
    else if (out_atom ==  "drop")
	out = 1;
    else if (out_atom == "accept")
	out = 2;

    rules.push_back(new ClientRule(src, dst, out));
}

DF_Store::ClientRuleTable
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

// Value: {<<"DEFAULT">>,[],[{<<"Class 1">>,<<"Class 2">>,accept},{<<"Class 2">>,<<"Class 1">>,drop}]}
DF_Store::ClientValue
DF_Store::connection::decode_client_value()
{
    if (trace)
	click_chatter("decode_client_value: %s\n", x_in.unparse().c_str());

    if (x_in.decode_tuple_header() != 3)
	throw ei_badarg();

    String group = x_in.decode_binary_string();
    NATTable nat_rules = decode_nat_list();
    ClientRuleTable rules = decode_client_rules_list();

    return ClientValue(group, nat_rules, rules);
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
    ClientValue value = decode_client_value();

    clients.insert(key, value);
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
    ClientValue value = decode_client_value();

    clients.insert(key, value);

    x_out.encode_atom("ok");
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

	x_out.encode_version();
	x_out.encode_tuple_header(2);
	x_out.encode_ref(ref);

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
    _conns[fd] = new connection(fd, conp, clients, mac_groups, ip_groups, true);
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
