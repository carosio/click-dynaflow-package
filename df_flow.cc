#include <click/config.h>
#include <click/packet.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include "df_flow.hh"
#include "jhash.hh"

CLICK_DECLS

Flow::Flow(Packet *p)
{
    const click_ip *iph = p->ip_header();
    const click_udp *udph = p->udp_header();
    assert(p->has_network_header() && p->has_transport_header() && IP_FIRSTFRAG(iph));
    data = new FlowData(iph->ip_src.s_addr, iph->ip_dst.s_addr, udph->uh_sport, udph->uh_dport, iph->ip_p);
    _id = hash();
    srcFlow = this;
}

Flow::Flow(FlowData *fd)
{
    data = fd;
    _id = hash();
    srcFlow = this;
}

Flow::~Flow()
{
    delete data;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(Flow)
