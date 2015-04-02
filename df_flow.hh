#ifndef DF_FLOW_HH
#define DF_FLOW_HH
#include <clicknet/icmp.h>

#include <click/packet.hh>
#include <click/hashtable.hh>
#include <click/vector.hh>
#include "ei.hh"
#include "df.hh"
#include "df_clients.hh"
#include "uniqueid.hh"
#include "jhash.hh"

CLICK_DECLS

extern IdManager flow_ids;

struct FlowData {
private:
    static constexpr uint8_t invmap[ICMP_MASKREQREPLY + 1] = {
	ICMP_ECHO + 1,                          //  0 = ICMP_ECHOREPLY
	0, 0, 0, 0, 0, 0, 0,
	ICMP_ECHOREPLY + 1,                     //  8 = ICMP_ECHO
	0, 0, 0, 0,
	ICMP_TSTAMPREPLY + 1,                   // 13 = ICMP_TIMESTAMP
	ICMP_TSTAMP + 1,                        // 14 = ICMP_TIMESTAMPREPLY
	ICMP_IREQREPLY + 1,                     // 15 = ICMP_INFO_REQUEST
	ICMP_IREQ + 1,                          // 16 = ICMP_INFO_REPLY
	ICMP_MASKREQREPLY + 1,                  // 17 = ICMP_ADDRESS
	ICMP_MASKREQ + 1                        // 18 = ICMP_ADDRESSREPLY
    };

protected:
    struct flow_endp_tuple {
	union {
	    in_addr_t   v4;
	    in6_addr    v6;
	} a;
	union {
	    uint16_t all;

	    struct {
		uint16_t port;
	    } tcp;
	    struct {
		uint16_t port;
	    } udp;
	    struct {
		uint8_t type, code;
	    } icmp;
	    struct {
		uint16_t port;
	    } dccp;
	    struct {
		uint16_t port;
	    } sctp;
	    struct {
		uint16_t key;
	    } gre;
	} u;
    };

    struct flow_tuple {
	uint8_t protonum;
	struct flow_endp_tuple src;
	struct flow_endp_tuple dst;
    } data;

private:
    FlowData(uint8_t protonum, const struct flow_endp_tuple &, const struct flow_endp_tuple &);

public:
    FlowData(const Packet *p);

    bool is_reversible() const;
    FlowData reverse() const;

    inline bool operator==(const FlowData other) const {
	return memcmp(&data, &other.data, sizeof(data) == 0);
    };

    StringAccum& unparse(StringAccum& sa) const;
    String unparse() const;

    inline hashcode_t hashcode() const {
	return jhash(&data, sizeof(data), 0);
    };

    friend ei_x &operator<<(ei_x &x, const FlowData &fd);
};

inline StringAccum&
operator<<(StringAccum& sa, const FlowData& entry)
{
    return entry.unparse(sa);
}

class Flow {
private:
    FlowData data;
    ClientValue *client;

public:
    Flow(const FlowData &, ClientValue *, uint32_t, uint32_t, DF_RuleAction);
    Flow(const Packet *p, ClientValue *client_, uint32_t srcGroup_, uint32_t dstGroup_, DF_RuleAction action_)
	: Flow(FlowData(p), client_, srcGroup_, dstGroup_, action_) {};
    Flow(const Packet *p) : Flow(p, NULL, 0, 0, DF_RULE_UNKNOWN) {};
    Flow(const FlowData &fd) : Flow(fd, NULL, 0, 0, DF_RULE_UNKNOWN) {};
    ~Flow() {};

    inline bool operator==(Flow other) const { return data == other.data; };

    inline uint32_t id() const { return _id; };
    inline uint32_t client_id() const { return client ? client->id() : 0; };

    inline const FlowData origin()  const { return data; };
    inline const FlowData reverse() const { return data.reverse(); };
    inline const bool is_reversible() const { return data.is_reversible(); };

    StringAccum& unparse(StringAccum& sa) const;
    String unparse() const;

    void reset_timeout();
    bool is_expired() const;

    uint32_t srcGroup;
    uint32_t dstGroup;
    DF_RuleAction action;
    uint32_t _id;

    click_jiffies_t _expiry;

    bool deleted = false;
private:
    void make_id();
};

inline StringAccum&
operator<<(StringAccum& sa, const Flow& f)
{
    return f.unparse(sa);
}

typedef HashTable<FlowData, Flow *> FlowTable;

ei_x &operator<<(ei_x &x, const FlowData &fd);
ei_x &operator<<(ei_x &x, const Flow &f);
ei_x &operator<<(ei_x &x, const FlowTable &f);

CLICK_ENDDECLS
#endif
