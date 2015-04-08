#ifndef DF_STORE_HH
#define DF_STORE_HH
#include <click/timer.hh>
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include "df_clients.hh"
#include "df_grouptable.hh"
#include "df_grouptable_ip.hh"
#include "df_grouptable_ether.hh"
#include "df_flow.hh"
#include "df.hh"
#include "uniqueid.hh"
#include "ei.hh"

CLICK_DECLS

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

    DF_GroupEntryEther *lookup_group_ether(EtherAddress &addr) const;
    DF_GroupEntryIP *lookup_group_ip(uint32_t addr) const;
    Flow *lookup_flow(const FlowData &fd);
    void set_flow(Flow *f);
    void delete_flow(Flow *f);
    ClientValue *lookup_client(uint32_t id_) const;

    void expire_flows();

    void notify_packet_in(const String &reason, const Packet *p);

private:
    int _listen_fd;
    ei_cnode ec;

    String cookie;
    String node_name;
    IPAddress local_ip;

    GroupTableIP ip_groups;
    GroupTableEther ether_groups;

    FlowTable flows;

    uint32_t _gc_interval_sec;
    Timer _gc_timer;

    static void gc_timer_hook(Timer *t, void *user_data);

public:
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

	DF_Store *store;

	ClientTable &clients;
	GroupTableEther &ether_groups;
	GroupTableIP &ip_groups;

	FlowTable &flows;

        connection(int fd_, ErlConnect *conp_,
		   DF_Store *store_,
		   ClientTable &clients_,
		   GroupTableEther &ether_groups_,
		   GroupTableIP &ip_groups_,
		   FlowTable &flows_,
		   bool debug = false, bool trace = false);
        void read();

    private:
	// data structure en/decoder functions
	void decode_group();
	void decode_groups();

	// NAT
	NATTranslation decode_nat_translation(const String type_atom);
	void decode_nat(NATTable &nat_rules);
	NATTable decode_nat_list();

	void encode_nat_list(const NATTable &rules);

	// Client Rules
	void decode_client_rule(ClientRuleTable &rules);
	ClientRuleTable decode_client_rules_list();

	void encode_client_rules_list(const ClientRuleTable &rules);

	// Clients
	ClientKey * decode_client_key();
	ClientValue * decode_client_value(ClientKey *key);

	void decode_client_entry();
	void decode_clients();

	// Dump Handler
	void dump_groups();

	const Handler * parse_handler(Element **es);

	// Erlang call handler
	void erl_bind(int arity);
	void erl_init(int arity);
	void erl_insert(int arity);
	void erl_dump(int arity);
	void erl_read(int arity);
	void erl_write(int arity);

	// Erlang generic call handler
	void handle_gen_call_click(const String fn, int arity);
	void handle_gen_call(const String to);
	void handle_gen_cast(const String to);
	void handle_msg(const String to);

    public:
	// Notifies
	void notify_packet_in(const String &reason, const Packet *);
    };
    Vector<connection *> _conns;

    int initialize_socket_error(ErrorHandler *, const char *);
    int initialize_socket_error(ErrorHandler *, const char *, int);
    void initialize_connection(int fd, ErlConnect *conp);
};

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
