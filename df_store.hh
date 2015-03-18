#ifndef DF_STORE_HH
#define DF_STORE_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
#include "df_grouptable.hh"
#include "df_grouptable_ip.hh"
#include "df_grouptable_mac.hh"
#include "uniqueid.hh"

#include "ei.h"

CLICK_DECLS

// bytes 16-23

#define GROUP_SRC_ANNO_OFFSET           16
#define GROUP_SRC_ANNO_SIZE             4
#define GROUP_SRC_ANNO(p)               ((p)->anno_u32(GROUP_SRC_ANNO_OFFSET))
#define SET_GROUP_SRC_ANNO(p, v)        ((p)->set_anno_u32(GROUP_SRC_ANNO_OFFSET, (v)))

#define GROUP_DST_ANNO_OFFSET           20
#define GROUP_DST_ANNO_SIZE             4
#define GROUP_DST_ANNO(p)               ((p)->anno_u32(GROUP_DST_ANNO_OFFSET))
#define SET_GROUP_DST_ANNO(p, v)        ((p)->set_anno_u32(GROUP_DST_ANNO_OFFSET, (v)))

#define GROUP_ANNO_SIZE                 4
#define GROUP_ANNO(p, offset)           ((p)->anno_u32((offset)))
#define SET_GROUP_ANNO(p, offset, v)    ((p)->set_anno_u32((offset), (v)))

struct ei_badarg : public std::exception
{
    const char * what () const throw () { return "Erlang Bad Argument"; }
};

class ei_x {
private:
    ei_x_buff x;

public:
    ei_x(long size);
    ~ei_x();

    inline void reset() { x.index = 0; };
    inline ei_x_buff *buffer() { return &x; };

    inline String unparse() { return unparse(x.index); }
    inline String unparse(int index)
    {
	char *s = NULL;
	String r;

	if (index == 0) {
	    int version;
	    ei_decode_version(x.buff, &index, &version);
	}
	ei_s_print_term(&s, x.buff, &index);
	r = s;
	free(s);

	return r;
    }

    // EI_X decoder
    String decode_string();
    String decode_binary_string();
    int decode_tuple_header();
    int decode_list_header();
    int decode_version();
    erlang_ref decode_ref();
    erlang_pid decode_pid();
    unsigned long decode_ulong();
    String decode_atom();
    void decode_binary(void *b, int len);
    void get_type(int *type, int *size);
    void skip_term();

    // EI_X encoder
    void encode_version();
    void encode_tuple_header(int arity);
    void encode_ref(const erlang_ref &ref);
    void encode_atom(const char *s);
    void encode_atom(const String &s);
};

// global Id Manager for Groups
extern IdManager group_ids;

class DF_Store : public Element {
public:
    DF_Store() CLICK_COLD;
    ~DF_Store() CLICK_COLD;

    const char *class_name() const		{ return "DF_Store"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *errh) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;

    void selected(int fd, int mask);

    int lookup_group(const String name) const;
    int lookup_group_ip(uint32_t addr) const;

private:
    int _listen_fd;
    ei_cnode ec;

    String cookie;
    String node_name;
    IPAddress local_ip;

    GroupTableIP ip_groups;
    GroupTableMAC mac_groups;

public:
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
	bool debug;
	bool trace;

        int fd;
        bool in_closed;
        bool out_closed;

	ErlConnect conp;
	erlang_pid bind_pid;
	ei_x x_in;
	ei_x x_out;

	ClientTable &clients;
	GroupTableMAC &mac_groups;
	GroupTableIP &ip_groups;

        connection(int fd_, ErlConnect *conp_,
		   ClientTable &clients_,
		   GroupTableMAC &mac_groups_,
		   GroupTableIP &ip_groups_,
		   bool debug = false, bool trace = false);
        void read();

    private:
	// data structure decoder functions
	IPAddress decode_ipaddress();

	void decode_group();
	void decode_groups();

	NATTranslation decode_nat_translation(const String type_atom);
	void decode_nat(NATTable &nat_rules);
	NATTable decode_nat_list();

	void decode_client_rule(ClientRuleTable &rules);
	ClientRuleTable decode_client_rules_list();

	ClientKey decode_client_key();
	ClientValue decode_client_value();

	void decode_client();
	void decode_clients();

	// Erlang call handler
	void erl_bind(int arity);
	void erl_init(int arity);
	void erl_insert(int arity);

	// Erlang generic call handler
	void handle_gen_call_click(const String fn, int arity);
	void handle_gen_call(const String to);
	void handle_gen_cast(const String to);
	void handle_msg(const String to);
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
operator<<(StringAccum& sa, const DF_Store::NATTranslation& translation)
{
    return translation.unparse(sa);
}

inline StringAccum&
operator<<(StringAccum& sa, const ei_x_buff & buf)
{
    int index = buf.index;
    char *s = NULL;

    if (index == 0) {
	int version;
	ei_decode_version(buf.buff, &index, &version);
    }
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

    if (index == 0) {
	int version;
	ei_decode_version(buf.buff, &index, &version);
    }
    ei_s_print_term(&s, buf.buff, &index);
    r = s;
    free(s);

    return r;
}

CLICK_ENDDECLS
#endif
