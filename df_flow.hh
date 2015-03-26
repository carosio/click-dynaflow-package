#ifndef DF_FLOW_HH
#define DF_FLOW_HH
#include <click/packet.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>
#include "df.hh"
#include "df_clients.hh"
#include "uniqueid.hh"
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
    inline bool operator==(FlowData other) { return (src_ipv4 == other.src_ipv4)
                                             && (dst_ipv4 == other.dst_ipv4)
                                             && (src_port == other.src_port)
                                             && (dst_port == other.dst_port)
                                             && (proto == other.proto); };
};

struct Flow {
    FlowData  * data;
    Flow * srcFlow;

    Flow(Packet *, ClientValue *, uint32_t, uint32_t, DF_RuleAction);
    Flow(Packet *);
    Flow(FlowData *, ClientValue *, uint32_t, uint32_t, DF_RuleAction);
    ~Flow();

    inline bool operator==(Flow other) { return data == other.data; };
    inline uint32_t src_id() { return srcFlow->_id; };
    inline uint32_t hash() { return jhash(&data, sizeof(FlowData), 0); };
    inline Flow * reverse() { return new Flow(data->reverse(),
                                           client, srcGroup, dstGroup, action); };

    ClientValue *client;
    uint32_t srcGroup;
    uint32_t dstGroup;
    DF_RuleAction action;
    uint32_t _id;
    uint32_t _count;
    bool _count_valid;
};

typedef Vector<Flow *> FlowVector;

class FlowHashEntry {
  public:

    FlowHashEntry() {};
    ~FlowHashEntry() {};

    Flow * get(Flow *f);
    Flow * get(uint32_t count);
    void add(Flow *f, bool check=true);
    Flow * remove(Flow *f);

  private:

    FlowVector fv;
    IdManager ids;
};

typedef HashMap<uint32_t, FlowHashEntry *> FlowTable;

CLICK_ENDDECLS
#endif
