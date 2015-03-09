#include <click/config.h>
#include "df_store.hh"
#include "df.hh"
#include <click/hashallocator.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
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
    {
	int index = x_in.index;
	char *s = NULL;

	ei_s_print_term(&s, x_in.buff, &index);
	click_chatter("Msg-In: %d, %s\n", index, s);
	free(s);
    }
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
