#ifndef DF_FLOW_HH
#define DF_FLOW_HH
#include <click/packet.hh>
#include <click/hashtable.hh>
#include <click/vector.hh>
#include "df.hh"
#include "df_clients.hh"
#include "uniqueid.hh"
#include "jhash.hh"

CLICK_DECLS

extern IdManager flow_ids;

struct __attribute__ ((__packed__)) FlowData {
    uint32_t src_ipv4;
    uint32_t dst_ipv4;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t proto;

    FlowData(const Packet *p);
    FlowData(uint32_t src_ipv4_, uint32_t dst_ipv4_, uint16_t src_port_, uint16_t dst_port_, uint8_t proto_):
        src_ipv4(src_ipv4_), dst_ipv4(dst_ipv4_), src_port(src_port_), dst_port(dst_port_), proto(proto_) {};

    inline const FlowData reverse() const { return FlowData(dst_ipv4, src_ipv4, dst_port, src_port, proto); };
    inline bool operator==(FlowData other) const { return (src_ipv4 == other.src_ipv4)
                                             && (dst_ipv4 == other.dst_ipv4)
                                             && (src_port == other.src_port)
                                             && (dst_port == other.dst_port)
                                             && (proto == other.proto); };

    StringAccum& unparse(StringAccum& sa) const;
    String unparse() const;

    inline hashcode_t hashcode() const {
	return jhash(this, sizeof(FlowData), 0);
    };
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

    StringAccum& unparse(StringAccum& sa) const;
    String unparse() const;

    uint32_t srcGroup;
    uint32_t dstGroup;
    DF_RuleAction action;
    uint32_t _id;


private:
    void make_id();
};

inline StringAccum&
operator<<(StringAccum& sa, const Flow& f)
{
    return f.unparse(sa);
}

typedef HashTable<FlowData, Flow *> FlowTable;

CLICK_ENDDECLS
#endif
