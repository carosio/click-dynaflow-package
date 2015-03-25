#ifndef DF_FLOW_HH
#define DF_FLOW_HH
#include <click/packet.hh>
#include <click/bighashmap.hh>
#include "df.hh"
#include "jhash.hh"

CLICK_DECLS

struct FlowData {
    uint32_t src_ipv4;
    uint32_t dst_ipv4;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t proto;

    FlowData(uint32_t src_ipv4_, uint32_t dst_ipv4_, uint16_t src_port_, uint16_t dst_port_, uint8_t proto_) :
        src_ipv4(src_ipv4_), dst_ipv4(dst_ipv4_), src_port(src_port_), dst_port(dst_port_), proto(proto_) {};

    inline FlowData * reverse() { return new FlowData(dst_ipv4, src_ipv4, dst_port, src_port, proto); };
};

struct Flow {
    FlowData  * data;
    Flow * srcFlow;

    Flow(Packet *);
    Flow(FlowData *);
    ~Flow();

    inline bool operator==(Flow other) { return _id == other._id; }
    inline uint32_t src_id() { return srcFlow->_id; };
    inline uint32_t hash() { return jhash(&data, sizeof(FlowData), 0); };
    inline Flow * reverse() { return new Flow(data->reverse()); };

    uint32_t _id;
};

typedef HashMap<uint32_t, Flow *> FlowTable;
typedef HashMap<uint32_t, DF_RuleAction> FlowActionTable;

CLICK_ENDDECLS
#endif
