#include <click/config.h>
#include <click/packet.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include "df_flow.hh"
#include "df_clients.hh"
#include "df.hh"
#include "jhash.hh"

CLICK_DECLS

Flow::Flow(Packet *p, ClientValue *client_, uint32_t srcGroup_, uint32_t dstGroup_, DF_RuleAction action_)
{
    const click_ip *iph = p->ip_header();
    const click_udp *udph = p->udp_header();
    assert(p->has_network_header() && p->has_transport_header() && IP_FIRSTFRAG(iph));
    data = new FlowData(iph->ip_src.s_addr, iph->ip_dst.s_addr, udph->uh_sport, udph->uh_dport, iph->ip_p);
    _id = hash();
    srcFlow = this;
    client = client_;
    srcGroup = srcGroup_;
    dstGroup = dstGroup_;
    action = action_;
    _count = 0;
    _count_valid = false;
}

Flow::Flow(Packet *p)
{
    const click_ip *iph = p->ip_header();
    const click_udp *udph = p->udp_header();
    assert(p->has_network_header() && p->has_transport_header() && IP_FIRSTFRAG(iph));
    data = new FlowData(iph->ip_src.s_addr, iph->ip_dst.s_addr, udph->uh_sport, udph->uh_dport, iph->ip_p);
    _id = hash();
    srcFlow = this;
    client = NULL;
    srcGroup = 0;
    dstGroup = 0;
    action = DF_RULE_UNKNOWN;
    _count = 0;
    _count_valid = false;
}

Flow::Flow(FlowData *fd, ClientValue *client_, uint32_t srcGroup_, uint32_t dstGroup_, DF_RuleAction action_)
{
    data = fd;
    _id = hash();
    srcFlow = this;
    client = client_;
    srcGroup = srcGroup_;
    dstGroup = dstGroup_;
    action = action_;
    _count = 0;
    _count_valid = false;
}

Flow::~Flow()
{
    delete data;
}

Flow * FlowHashEntry::get(Flow *f) {
    for (FlowVector::const_iterator it = fv.begin(); it != fv.end(); ++it) {
        if ((*it) == f)
            return (*it);
    }
    return NULL;
}

Flow * FlowHashEntry::get(uint32_t count) {
    for (FlowVector::const_iterator it = fv.begin(); it != fv.end(); ++it) {
        if ((*it)->_count == count)
            return (*it);
    }
    return NULL;
}

void FlowHashEntry::add(Flow *f) {
    fv.push_back(f);
    f->_count = ids.AllocateId();
    f->_count_valid = true;

    return;
}

Flow * FlowHashEntry::remove(Flow *f) {
    Flow * tmpf;
    for (FlowVector::iterator it = fv.begin(); it != fv.end(); ++it) {
        if ((*it) == f) {
            tmpf = *(fv.erase(it));
            ids.FreeId(tmpf->_count);
            tmpf->_count_valid = false;
        }
    }

    return NULL;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(Flow)
ELEMENT_PROVIDES(FlowHashEntry)
