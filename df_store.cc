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

    static int ei_x_new_size(ei_x_buff* x, long size)
    {
#define BLK_MAX 127

	size = (size + BLK_MAX) & ~BLK_MAX;

#undef BLK_MAX

	x->buff = (char *)ei_malloc(size);
	x->buffsz = size;
	x->index = 0;
	return x->buff != NULL ? 0 : -1;
    }

}

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

StringAccum& DF_Store::group::unparse(StringAccum& sa) const
{
    sa << addr.unparse_with_mask(prefix) << " -> " << group_name;
    return sa;
}

String DF_Store::group::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
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

DF_Store::connection::connection(int fd_, ErlConnect *conp_)
    : fd(fd_), in_closed(false), out_closed(false),
      conp(*conp_) {

    /* prealloc enough space to hold a full CAPWAP PDU plus Erlang encoded meta data */
    ei_x_new_size(&x_in, 2048);
    ei_x_new_size(&x_out, 2048);
}

DF_Store::connection::~connection()
{
    ei_x_free(&x_in);
    ei_x_free(&x_out);
}

// data structure decoder functions

int
DF_Store::connection::ei_decode_ipaddress(IPAddress &addr)
{
    int arity;
    uint32_t a = 0;

    if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) != 0
	|| arity != 4)
	return -1;

    for (int i = 0; i < 4; i++) {
	unsigned long p;
	if (ei_decode_ulong(x_in.buff, &x_in.index, &p) != 0)
	    return -1;
	a = a << 8;
	a |= p;
    }

    addr = IPAddress(htonl(a));
    return 0;
}

int
DF_Store::connection::ei_decode_string(String &str)
{
    int type;
    int size;

    if (ei_get_type(x_in.buff, &x_in.index, &type, &size) != 0)
	return -1;

    str = String::make_uninitialized(size);
    // NB: mutable_c_str() creates space for the terminating null character
    if (char *x = str.mutable_c_str()) {
	if (::ei_decode_string(x_in.buff, &x_in.index, x) != 0)
	    return -1;
    } else
	return -1;

    return 0;
}

int
DF_Store::connection::ei_decode_binary_string(String &str)
{
    int type;
    int size;
    long bin_size;

    if (ei_get_type(x_in.buff, &x_in.index, &type, &size) != 0
	|| type != ERL_BINARY_EXT)
	return -1;

    str = String::make_uninitialized(size);
    // NB: mutable_c_str() creates space for the terminating null character
    if (char *x = str.mutable_c_str()) {
	if (::ei_decode_binary(x_in.buff, &x_in.index, x, &bin_size) != 0)
	    return -1;
    } else
	return -1;

    return 0;
}

// rule sample: {{{10, 0, 1, 0}, 24}, "Class 1"}
int
DF_Store::connection::ei_decode_group(GroupTable &groups)
{
    int arity;
    unsigned long prefix_len;
    IPAddress addr;
    IPAddress prefix;
    String group_name;

    if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) != 0
	|| arity != 2
	|| ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) != 0
	|| arity != 2
	|| ei_decode_ipaddress(addr) != 0
	|| ei_decode_ulong(x_in.buff, &x_in.index, &prefix_len) != 0
	|| ei_decode_string(group_name) != 0)
	return -1;

    click_chatter("ei_decode_group: %s/%d -> %s\n", addr.unparse().c_str(), prefix_len, group_name.c_str());
    groups.push_back(new group(addr, IPAddress::make_prefix(prefix_len), group_name));
    return 0;
}

// rules sample: [{{{10, 0, 1, 0}, 24}, "Class 1"}, {{{10, 0, 2, 0}, 24}, "Class 2"}],
int
DF_Store::connection::ei_decode_groups(GroupTable &groups)
{
    int arity;

#if defined(DEBUG)
    click_chatter("ei_decode_groups: %s\n", unparse(x_in).c_str());
#endif

    if (ei_decode_list_header(x_in.buff, &x_in.index, &arity) != 0)
	return -1;

#if defined(DEBUG)
    click_chatter("ei_decode_groups: %d\n", arity);
#endif

    if (arity == 0)
	return 0;

    do {
	int type;
	int size;

	if (ei_get_type(x_in.buff, &x_in.index, &type, &size) != 0)
	    return -1;

	click_chatter("ei_decode_nat_list: %c, %d\n", type, size);
	if (type == ERL_NIL_EXT) {
	    if (ei_skip_term(x_in.buff, &x_in.index) != 0)
		return -1;
	    break;
	}

	if (ei_decode_group(groups) != 0)
	    return -1;
    } while (arity-- > 0);

    return 0;
}

// Key: {inet,<<172,20,48,19>>}
int
DF_Store::connection::ei_decode_client_key(ClientKey &key)
{
    int arity;
    int type;
    int size;
    long bin_size;
    char addr_type[MAXATOMLEN+1] = {0};
    struct in_addr addr;

#if defined(DEBUG)
    click_chatter("ei_decode_client_key: %s\n", unparse(x_in).c_str());
#endif

    if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) != 0
	|| arity != 2
	|| ei_decode_atom(x_in.buff, &x_in.index, addr_type) < 0)
	return -1;

    if (strncmp(addr_type, "inet", 4) == 0) {
	if (ei_get_type(x_in.buff, &x_in.index, &type, &size) != 0
	    || type != ERL_BINARY_EXT
	    || size != 4
	    || ::ei_decode_binary(x_in.buff, &x_in.index, (void *)&addr, &bin_size) != 0
	    || bin_size != 4)
	    return -1;
	key = ClientKey(AF_INET, IPAddress(addr));
    }
    else
	return -1;

    return 0;
}

int
DF_Store::connection::ei_decode_nat_translation(const char *type_atom, NATTranslation &translation)
{
    int arity;
    unsigned int type;
    IPAddress nat_addr;
    long unsigned int min_port = 0;
    long unsigned int max_port = 0;

#if defined(DEBUG)
    click_chatter("ei_decode_nat_translation: %s\n", unparse(x_in).c_str());
#endif

    for (type = 0; type < sizeof(NATTranslation::Translations) / sizeof(char *); type++)
	if (strcmp(NATTranslation::Translations[type], type_atom) == 0)
	    break;

    switch (type) {
    case NATTranslation::SymetricAddressKeyed:
    case NATTranslation::AddressKeyed:
	if (ei_decode_ipaddress(nat_addr) != 0)
	    return -1;
	break;

    case NATTranslation::PortKeyed:
    if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) != 0
	|| arity != 3
	|| ei_decode_ipaddress(nat_addr) != 0
	|| ei_decode_ulong(x_in.buff, &x_in.index, &min_port) != 0
	|| ei_decode_ulong(x_in.buff, &x_in.index, &max_port) != 0)
	break;

    case NATTranslation::Random:
    case NATTranslation::RandomPersistent:
    case NATTranslation::Masquerade:
	if (ei_skip_term(x_in.buff, &x_in.index) != 0)
	    return -1;
	break;

    default:
	return -1;
    }

    translation = NATTranslation(type, nat_addr, min_port, max_port);
    return 0;
}

int
DF_Store::connection::ei_decode_nat(NATTable &nat_rules)
{
    int arity;
    IPAddress addr;
    char translation_atom[MAXATOMLEN+1] = {0};
    NATTranslation translation;

#if defined(DEBUG)
    click_chatter("ei_decode_nat: %s\n", unparse(x_in).c_str());
#endif

    if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) != 0
	|| arity != 3
	|| ei_decode_ipaddress(addr) != 0
	|| ei_decode_atom(x_in.buff, &x_in.index, translation_atom) < 0
	|| ei_decode_nat_translation(translation_atom, translation) != 0)
	return -1;

    click_chatter("ei_decode_nat: %s -> %s\n", addr.unparse().c_str(), translation.unparse().c_str());
    nat_rules.insert(addr, translation);
    return 0;
}

int
DF_Store::connection::ei_decode_nat_list(NATTable &nat_rules)
{
    int arity;

#if defined(DEBUG)
    click_chatter("ei_decode_nat_list: %s\n", unparse(x_in).c_str());
#endif

    if (ei_decode_list_header(x_in.buff, &x_in.index, &arity) != 0)
	return -1;

    click_chatter("ei_decode_nat_list: %d\n", arity);

    if (arity == 0)
	return 0;

    do {
	int type;
	int size;

	if (ei_get_type(x_in.buff, &x_in.index, &type, &size) != 0)
	    return -1;

	click_chatter("ei_decode_nat_list: %c, %d\n", type, size);
	if (type == ERL_NIL_EXT) {
	    if (ei_skip_term(x_in.buff, &x_in.index) != 0)
		return -1;
	    break;
	}

	if (ei_decode_nat(nat_rules) != 0)
	    return -1;
    } while (arity-- > 0);

    return 0;
}

int
DF_Store::connection::ei_decode_client_rule(ClientRuleTable &rules)
{
    int arity;
    String src;
    String dst;
    char out_atom[MAXATOMLEN+1] = {0};
    int out = 0;

#if defined(DEBUG)
    click_chatter("ei_decode_client_rules: %s\n", unparse(x_in).c_str());
#endif

    if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) != 0
	|| arity != 3
	|| ei_decode_binary_string(src) != 0
	|| ei_decode_binary_string(dst) != 0
	|| ei_decode_atom(x_in.buff, &x_in.index, out_atom) < 0)
	return -1;

#if defined(DEBUG)
    click_chatter("ei_decode_client_rules: atom: %s\n", out_atom);
#endif

    if (strcmp(out_atom, "deny") == 0)
	out = 0;
    else if (strcmp(out_atom, "drop") == 0)
	out = 1;
    else if (strcmp(out_atom, "accept") == 0)
	out = 2;

    rules.push_back(new ClientRule(src, dst, out));
    return 0;
}

int
DF_Store::connection::ei_decode_client_rules_list(ClientRuleTable &rules)
{
    int arity;

#if defined(DEBUG)
    click_chatter("ei_decode_client_rules_list: %s\n", unparse(x_in).c_str());
#endif

    if (ei_decode_list_header(x_in.buff, &x_in.index, &arity) != 0)
	return -1;

    click_chatter("ei_decode_client_rules_list: %d\n", arity);

    if (arity == 0)
	return 0;

    do {
	int type;
	int size;

	if (ei_get_type(x_in.buff, &x_in.index, &type, &size) != 0)
	    return -1;

	click_chatter("ei_decode_client_rules_list: %c, %d\n", type, size);
	if (type == ERL_NIL_EXT) {
	    if (ei_skip_term(x_in.buff, &x_in.index) != 0)
		return -1;
	    break;
	}

	if (ei_decode_client_rule(rules) != 0)
	    return -1;
    } while (arity-- > 0);

    return 0;
}

// Value: {<<"DEFAULT">>,[],[{<<"Class 1">>,<<"Class 2">>,accept},{<<"Class 2">>,<<"Class 1">>,drop}]}
int
DF_Store::connection::ei_decode_client_value(ClientValue &value)
{
    int arity;
    String group;
    NATTable nat_rules;
    ClientRuleTable rules;

#if defined(DEBUG)
    click_chatter("ei_decode_client_value: %s\n", unparse(x_in).c_str());
#endif

    if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) != 0
	|| arity != 3
	|| ei_decode_binary_string(group) != 0
	|| ei_decode_nat_list(nat_rules) != 0
	|| ei_decode_client_rules_list(rules) != 0)
	return -1;

    value = ClientValue(group, nat_rules, rules);
    return 0;
}

// client sample {{inet,<<172,20,48,19>>}, {<<"DEFAULT">>,[],[{<<"Class 1">>,<<"Class 2">>,accept},{<<"Class 2">>,<<"Class 1">>,drop}]}}
int
DF_Store::connection::ei_decode_client(ClientTable &clients)
{
    int arity;
    ClientKey key;
    ClientValue value;

#if defined(DEBUG)
    click_chatter("ei_decode_client: %s\n", unparse(x_in).c_str());
#endif

    if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) != 0
	|| arity != 2
	|| ei_decode_client_key(key) != 0
	|| ei_decode_client_value(value) != 0)
	return -1;

    clients.insert(key, value);
    return 0;
}

int
DF_Store::connection::ei_decode_clients(ClientTable &clients)
{
    int arity;

#if defined(DEBUG)
    click_chatter("ei_decode_clients: %s\n", unparse(x_in).c_str());
#endif

    if (ei_decode_list_header(x_in.buff, &x_in.index, &arity) != 0)
	return -1;

    click_chatter("ei_decode_clients: %d\n", arity);

    if (arity == 0)
	return 0;

    do {
	int type;
	int size;

	if (ei_get_type(x_in.buff, &x_in.index, &type, &size) != 0)
	    return -1;

	click_chatter("ei_decode_nat_list: %c, %d\n", type, size);
	if (type == ERL_NIL_EXT) {
	    if (ei_skip_term(x_in.buff, &x_in.index) != 0)
		return -1;
	    break;
	}

	if (ei_decode_client(clients) != 0)
	    return -1;
    } while (arity-- > 0);

    return 0;
}

// Erlang call handlers

void
DF_Store::connection::erl_bind(int arity)
{
    if (arity != 2
	|| ei_decode_pid(x_in.buff, &x_in.index, &bind_pid) != 0)
	ei_x_encode_atom(&x_out, "badarg");
    else
	ei_x_encode_atom(&x_out, "ok");
}

void
DF_Store::connection::erl_init(int arity)
{
    GroupTable groups;
    ClientTable clients;

    click_chatter("erl_init: %d\n", arity);
    if (arity != 3) {
	ei_x_encode_atom(&x_out, "badarg");
	return;
    }

    if (ei_decode_groups(groups) != 0
	|| ei_decode_clients(clients) != 0) {
	ei_x_encode_atom(&x_out, "badarg");
	return;
    }

    ei_x_encode_atom(&x_out, "ok");
}

// Erlang generic call handlers

void
DF_Store::connection::handle_gen_call_click(const char *fn, int arity)
{
    if (strncmp(fn, "bind", 4) == 0) {
	erl_bind(arity);
    }
    if (strncmp(fn, "init", 4) == 0) {
	erl_init(arity);
    }
    else
	ei_x_encode_atom(&x_out, "error");
}

void
DF_Store::connection::handle_gen_call(const char *to)
{
	int arity;
	erlang_pid pid;
	erlang_ref ref;
	char fn[MAXATOMLEN+1] = {0};
	int r __attribute__((unused));

	/* decode From tupple */
	if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) < 0
	    || arity != 2
	    || ei_decode_pid(x_in.buff, &x_in.index, &pid) < 0
	    || ei_decode_ref(x_in.buff, &x_in.index, &ref) < 0) {
		click_chatter("Ignoring malformed message.");
		return;
	}

	/* decode call args */
	if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) < 0
	    || ei_decode_atom(x_in.buff, &x_in.index, fn) < 0) {
		click_chatter("Ignoring malformed message.");
		return;
	}

	ei_x_encode_version(&x_out);
	ei_x_encode_tuple_header(&x_out, 2);
	ei_x_encode_ref(&x_out, &ref);

	if (strncmp(to, "net_kernel", 10) == 0 &&
	    strncmp(fn, "is_auth", 7) == 0) {
		ei_x_encode_atom(&x_out, "yes");
	}
	else if (strncmp(to, "click", 6) == 0) {
		handle_gen_call_click(fn, arity);
	}
	else
		ei_x_encode_atom(&x_out, "error");

#if defined(DEBUG)
	{
		int index = 0;
		char *s = NULL;
		int version;

		ei_decode_version(x_out.buff, &index, &version);
		ei_s_print_term(&s, x_out.buff, &index);
		click_chatter("Msg-Out: %d, %s", index, s);
		free(s);
	}
#endif

	r = ei_send(fd, &pid, x_out.buff, x_out.index);
	click_chatter("send ret: %d", r);
}

void
DF_Store::connection::handle_gen_cast(const char *to __attribute__((unused)))
{
}

void
DF_Store::connection::handle_msg(const char *to)
{
    int version;
    int arity;
    char type[MAXATOMLEN+1] = {0};

    x_in.index = 0;

    if (ei_decode_version(x_in.buff, &x_in.index, &version) < 0) {
	click_chatter("Ignoring malformed message (bad version: %d).\n", version);
	return;
    }

#if defined(DEBUG)
    click_chatter("Msg-In: %s\n", unparse(x_in).c_str());
#endif

    if (ei_decode_tuple_header(x_in.buff, &x_in.index, &arity) < 0
	|| ei_decode_atom(x_in.buff, &x_in.index, type) < 0) {
	click_chatter("Ignoring malformed message.");
	return;
    }

    click_chatter("Type: %s\n", type);

    if (arity == 3 && strncmp(type, "$gen_call", 9) == 0) {
	handle_gen_call(to);
    }
    else if (arity == 2 && strncmp(type, "$gen_cast", 9) == 0) {
	handle_gen_cast(to);
    }
    else
	click_chatter("Ignoring Msg %s.", type);
}

void
DF_Store::connection::read()
{
    erlang_msg msg;
    int r;

    x_out.index = x_in.index = 0;

    r = ei_xreceive_msg(fd, &msg, &x_in);
    click_chatter("conn:read: %d\n", fd);

    if (r == ERL_TICK) {
	click_chatter("DEBUG: TICK");
	/* ignore */
    } else if (r == ERL_ERROR) {
	click_chatter("DEBUG: ERL_ERROR");
	in_closed = true;
    } else {
	switch (msg.msgtype) {
	case ERL_SEND:
	case ERL_REG_SEND:
	    handle_msg(msg.toname);
	    break;

	default:
	    click_chatter("msg.msgtype: %ld\n", msg.msgtype);
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
    _conns[fd] = new connection(fd, conp);
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
