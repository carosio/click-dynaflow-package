#include <click/config.h>
#include <click/packet.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include "df_flow.hh"
#include "df_clients.hh"
#include "df.hh"
#include "jhash.hh"

CLICK_DECLS

#define TIMEOUT 30
#define SLOTS_PER_FLOW_HASH 4              // should be power of 2

IdManager flow_ids;

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

    click_chatter("FlowData(%p - %d): %s\n", this, sizeof(*this), unparse().c_str());
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

void
Flow::make_id()
{
    click_chatter("Flow make_id: %ld\n", (UINT32_MAX / SLOTS_PER_FLOW_HASH));

    _id = data.hashcode() % (UINT32_MAX / SLOTS_PER_FLOW_HASH);
    while (!flow_ids.MarkAsUsed(_id))
	_id++;
}

Flow::Flow(const FlowData &fd, ClientValue *client_, uint32_t srcGroup_, uint32_t dstGroup_, DF_RuleAction action_)
    : data(fd), client(client_),
      srcGroup(srcGroup_), dstGroup(dstGroup_),
      action(action_)
{
    click_chatter("Flow: data: %p, client: (%p): %p, client (%p) %p\n", &data, &client, client, &client_, client_);
    if (client)
	click_chatter("Client: %d\n", *(char *)client);

    make_id();
    click_chatter("Flow: %s hashed to %08x\n", unparse().c_str(), _id);

    reset_timeout();
}

bool Flow::is_expired() const
{
    return click_jiffies() > _expiry;
}

void Flow::reset_timeout()
{
    _expiry = click_jiffies() + (TIMEOUT * CLICK_HZ);
}

StringAccum& Flow::unparse(StringAccum& sa) const
{
    sa << _id << ": " << data << " : " << "(" << (void *)client << ")" << srcGroup << ":" << dstGroup << " -> " << action;
    return sa;
}

String Flow::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(Flow)
ELEMENT_PROVIDES(FlowData)
