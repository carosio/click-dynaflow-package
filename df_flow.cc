#include <click/config.h>
#include <click/packet.hh>
#include "df_flow.hh"
#include "jhash.hh"

CLICK_DECLS

IdManager flows_ids;

Flow::Flow(Packet *p, int offset)
{
    data = FlowData(p->ip_header()->ip_src.s_addr, p->ip_header()->ip_dst.s_addr, ); // ports , proto
    _id = hash();
    srcFlow = this;
}

Flow::~Flow()
{
}

uint32_t
Flow::hash() {
    return jhash(data);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(Flow)
