#ifndef DF_STORE_HH
#define DF_STORE_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>

#include "ei.h"

CLICK_DECLS

class DF_Store : public Element {
public:
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

public:
    struct group {
	IPAddress addr;
	IPAddress prefix;
	String group_name;

	group(IPAddress addr_, IPAddress prefix_, String group_name_) :
	    addr(addr_), prefix(prefix_), group_name(group_name_) {};

	StringAccum& unparse(StringAccum& sa) const;
	String unparse() const;
    };

    typedef Vector<group *> GroupTable;
    GroupTable groups;

    struct NATTranslation {
	static const char *Translations[];
	enum Type { SymetricAddressKeyed,
		    AddressKeyed,
		    PortKeyed,
		    Random,
		    RandomPersistent,
		    Masquerade };
	unsigned int type;
	IPAddress nat_addr;
	int min_port;
	int max_port;

	inline NATTranslation() { type = min_port = max_port = 0; };
	NATTranslation(unsigned int type_, IPAddress nat_addr_, int min_port_, int max_port_) :
	    type(type_), nat_addr(nat_addr_), min_port(min_port_), max_port(max_port_) {};
	StringAccum& unparse(StringAccum& sa) const;
	String unparse() const;
    };

    typedef HashMap<IPAddress, NATTranslation> NATTable;

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
	NATTable nat_rules;
	ClientRuleTable rules;

	inline ClientValue() {};
	ClientValue(String group_, NATTable nat_rules_, ClientRuleTable rules_) :
	    group(group_), nat_rules(nat_rules_), rules(rules_) {};
    };

    typedef HashMap<ClientKey, ClientValue> ClientTable;
    ClientTable clients;

private:
    struct connection {
        int fd;
        bool in_closed;
        bool out_closed;

	ErlConnect conp;
	erlang_pid bind_pid;
	ei_x_buff x_in;
	ei_x_buff x_out;

	ClientTable &clients;
	GroupTable &groups;

        connection(int fd_, ErlConnect *conp_, ClientTable &clients_, GroupTable &groups_);
        ~connection();
        void read();

    private:
	// data structure decoder functions
	int ei_decode_ipaddress(IPAddress &addr);
	int ei_decode_string(String &str);
	int ei_decode_binary_string(String &str);

	int ei_decode_group();
	int ei_decode_groups();

	int ei_decode_nat_translation(const char *type_atom, NATTranslation &translation);
	int ei_decode_nat(NATTable &nat_rules);
	int ei_decode_nat_list(NATTable &nat_rules);

	int ei_decode_client_rule(ClientRuleTable &rules);
	int ei_decode_client_rules_list(ClientRuleTable &rules);

	int ei_decode_client_key(ClientKey &key);
	int ei_decode_client_value(ClientValue &value);

	int ei_decode_client();
	int ei_decode_clients();

	// Erlang call handler
	void erl_bind(int arity);
	void erl_init(int arity);
	void erl_insert(int arity);

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

inline StringAccum&
operator<<(StringAccum& sa, const DF_Store::group& group)
{
    return group.unparse(sa);
}

inline StringAccum&
operator<<(StringAccum& sa, const DF_Store::NATTranslation& translation)
{
    return translation.unparse(sa);
}

inline StringAccum&
operator<<(StringAccum& sa, const ei_x_buff & buf)
{
    int index = 0;
    char *s = NULL;

    ei_s_print_term(&s, buf.buff, &index);
    sa.append(s);
    free(s);

    return sa;
}

inline
String unparse(const ei_x_buff & buf)
{
    int index = buf.index;
    char *s = NULL;
    String r;

    ei_s_print_term(&s, buf.buff, &index);
    r = s;
    free(s);

    return r;
}


CLICK_ENDDECLS
#endif
