#ifndef DF_STORE_HH
#define DF_STORE_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/bighashmap.hh>

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

    struct group {
	IPAddress addr;
	IPAddress prefix;
	String group_name;

	group(IPAddress addr_, IPAddress prefix_, String group_name_) :
	    addr(addr_), prefix(prefix_), group_name(group_name_) {};
    };

    typedef Vector<group *> GroupTable;

    struct ClientKey {
	int type;
	IPAddress addr;

	inline ClientKey() { type = 0; };
	ClientKey(int type_, IPAddress addr_) :
	    type(type_), addr(addr_) {};

        inline bool
	operator==(DF_Store::ClientKey other)
	{
	    return addr == other.addr;
	};

    public:
	inline uint32_t hashcode() const;
    };

    struct ClientRule {
	String src;
	String dst;
	int out;

	ClientRule(String src_, String dst_, int out_) :
	    src(src_), dst(dst_), out(out_) {};
    };

    typedef Vector<ClientRule *> ClientRuleTable;

    struct ClientValue {
	String group;

	inline ClientValue() {};
	ClientValue(String group_) :
	    group(group_) {};
    };

    typedef HashMap<ClientKey, ClientValue> ClientTable;

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

    private:
	// data structure decoder functions
	int ei_decode_ipaddress(IPAddress &addr);
	int ei_decode_string(String &str);
	int ei_decode_binary_string(String &str);

	int ei_decode_group(GroupTable &groups);
	int ei_decode_groups(GroupTable &groups);

	int ei_decode_nat();
	int ei_decode_client_rules(ClientRuleTable &rules);

	int ei_decode_client_key(ClientKey &key);
	int ei_decode_client_value(ClientValue &value);

	int ei_decode_client(ClientTable &clients);
	int ei_decode_clients(ClientTable &clients);

	// Erlang call handler
	void erl_bind(int arity);
	void erl_init(int arity);

	// Erlang generic call handler
	void handle_gen_call_click(const char *fn, int arity);
	void handle_gen_call(const char *to);
	void handle_gen_cast(const char *to);
	void handle_msg(const char *to);
    };
    Vector<connection *> _conns;

    int initialize_socket_error(ErrorHandler *, const char *);
    int initialize_socket_error(ErrorHandler *, const char *, int);
    void initialize_connection(int fd, ErlConnect *conp);
};

uint32_t DF_Store::ClientKey::hashcode() const
{
    //FIXME: define hash function
    return 0;
}

CLICK_ENDDECLS
#endif
