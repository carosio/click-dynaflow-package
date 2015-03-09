#ifndef DF_STORE_HH
#define DF_STORE_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>

#include "ei.h"

CLICK_DECLS

class DF_Store : public Element { public:

    DF_Store() CLICK_COLD;
    ~DF_Store() CLICK_COLD;

    const char *class_name() const		{ return "DF_Store"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

    void selected(int fd, int mask);

  private:
    int _listen_fd;
    ei_cnode ec;

    String cookie;
    String node_name;
    IPAddress local_ip;

    struct connection {
        int fd;
        bool in_closed;
        bool out_closed;

	ErlConnect conp;
	erlang_pid bind_pid;
	ei_x_buff x_in;
	ei_x_buff x_out;

        connection(int fd_, ErlConnect *conp_);
        ~connection();
        void read();
	void handle_gen_call(const char *to);
	void handle_gen_cast(const char *to);
	void handle_msg(const char *to);
    };
    Vector<connection *> _conns;

    int initialize_socket_error(ErrorHandler *, const char *);
    int initialize_socket_error(ErrorHandler *, const char *, int);
    void initialize_connection(int fd, ErlConnect *conp);
};

CLICK_ENDDECLS
#endif
