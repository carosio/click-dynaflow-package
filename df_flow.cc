#include <click/config.h>
#include <click/packet.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include "df_flow.hh"
#include "df_clients.hh"
#include "df.hh"
#include "jhash.hh"

CLICK_DECLS

FlowData::FlowData(const Packet *p)
{
    const click_ip *iph = p->ip_header();
    const click_udp *udph = p->udp_header();

    assert(p->has_network_header() && p->has_transport_header() && IP_FIRSTFRAG(iph));

    proto = iph->ip_p;
    src_ipv4 = iph->ip_src.s_addr;
    dst_ipv4 = iph->ip_dst.s_addr;
    src_port = udph->uh_sport;
    dst_port = udph->uh_dport;

    click_chatter("FlowData: %s\n", unparse().c_str());
}

StringAccum& FlowData::unparse(StringAccum& sa) const
{
    sa << "(" << (int)proto << "):" << IPAddress(src_ipv4) << ":" << ntohs(src_port) << " -> " << IPAddress(dst_ipv4) << ":" << ntohs(dst_port);
    return sa;
}

String FlowData::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}

Flow::Flow(const Packet *p, ClientValue *client_, uint32_t srcGroup_, uint32_t dstGroup_, DF_RuleAction action_)
 : data(FlowData(p))
{
    _id = hash();
    click_chatter("FlowData: %s hashed to %08x\n", data.unparse().c_str(), _id);

    srcFlow = this;
    client = client_;
    srcGroup = srcGroup_;
    dstGroup = dstGroup_;
    action = action_;
    _count = 0;
    _count_valid = false;
}

Flow::Flow(const Packet *p)
  : data(FlowData(p))
{
    _id = hash();
    click_chatter("FlowData: %s hashed to %08x\n", data.unparse().c_str(), _id);

    srcFlow = this;
    client = NULL;
    srcGroup = 0;
    dstGroup = 0;
    action = DF_RULE_UNKNOWN;
    _count = 0;
    _count_valid = false;
}

Flow::Flow(const FlowData &fd, ClientValue *client_, uint32_t srcGroup_, uint32_t dstGroup_, DF_RuleAction action_)
  : data(fd)
{
    _id = hash();
    click_chatter("FlowData: %s hashed to %08x\n", data.unparse().c_str(), _id);

    srcFlow = this;
    client = client_;
    srcGroup = srcGroup_;
    dstGroup = dstGroup_;
    action = action_;
    _count = 0;
    _count_valid = false;
}

Flow * FlowHashEntry::get(Flow *f) {
    for (FlowVector::const_iterator it = fv.begin(); it != fv.end(); ++it) {
        if (*(*it) == *f)
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

void FlowHashEntry::add(Flow *f, bool check) {
    if(check) {
        if(get(f))
            return;
    }

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
ELEMENT_PROVIDES(FlowData)
ELEMENT_PROVIDES(FlowHashEntry)
