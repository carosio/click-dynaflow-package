#ifndef DF_FLOW_HH
#define DF_FLOW_HH
#include <click/packet.hh>
#include <click/bighashmap.hh>

CLICK_DECLS

enum Rule_action {
    DF_RULE_UNKNOWN, // default for new Flows
    DF_RULE_NO_ACTION,
    DF_RULE_ACCEPT,
    DF_RULE_DENY,
    DF_RULE_DROP,
    _DF_RULE_SIZE,
    DF_RULE_ELSE = 255 // catch all
};

#define DF_RULE_SIZE (_DF_RULE_SIZE + 1)

struct FlowData {
    uint32_t src_ipv4;
    uint32_t dst_ipv4;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t proto;

    FlowData(uint32_t src_ipv4_, uint32_t dst_ipv4_, uint16_t src_port_, uint16_t dst_port_, uint8_t proto_) :
        src_ipv4(src_ipv4_), dst_ipv4(dst_ipv4_), src_port(src_port_), dst_port(dst_port_), proto(proto_) {};

    inline FlowData reverse() { return FlowData(dst_ipv4, src_ipv4, dst_port, src_port, proto); };
};

struct Flow {
    FlowData data;
    Flow * srcFlow;

    Flow(Packet *);

    inline bool operator==(Flow other) { return _id == other._id; }
    inline uint32_t src_id() { return srcFlow->_id; };

  private:
    uint32_t _id;
};

typedef HashMap<uint32_t, Flow *> FlowTable;
typedef HashMap<uint32_t, uint8_t> FlowActionTable;

CLICK_ENDDECLS
#endif
