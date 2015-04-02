#include <click/config.h>
#include <click/packet.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
#include <clicknet/icmp.h>
#include "df_flow.hh"
#include "df_clients.hh"
#include "df.hh"
#include "jhash.hh"

CLICK_DECLS

#define TIMEOUT 30
#define SLOTS_PER_FLOW_HASH 4              // should be power of 2

IdManager flow_ids;

constexpr uint8_t FlowData::invmap[ICMP_MASKREQREPLY + 1];

FlowData::FlowData(uint8_t protonum, const struct flow_endp_tuple &src, const struct flow_endp_tuple &dst)
{
    memset(&data, 0, sizeof(data));
    data.protonum = protonum;
    data.src = src;
    data.dst = dst;
}

FlowData::FlowData(const Packet *p)
{
    const click_ip *iph = p->ip_header();
    const click_tcp *tcph = p->tcp_header();
    const click_udp *udph = p->udp_header();
    const click_icmp *icmph = p->icmp_header();

    assert(p->has_network_header() && p->has_transport_header() && IP_FIRSTFRAG(iph));

    memset(&data, 0, sizeof(data));

    data.protonum = iph->ip_p;
    data.src.a.v4 = iph->ip_src.s_addr;
    data.dst.a.v4 = iph->ip_dst.s_addr;

    switch (iph->ip_p) {
    case IPPROTO_TCP:
	data.src.u.tcp.port = tcph->th_sport;
	data.dst.u.tcp.port = tcph->th_dport;
	break;

    case IPPROTO_UDP:
	data.src.u.udp.port = udph->uh_sport;
	data.dst.u.udp.port = udph->uh_dport;
	break;

    case IPPROTO_ICMP:
	data.src.u.icmp.type = icmph->icmp_type;
	data.src.u.icmp.code = icmph->icmp_code;
	break;
    }


    click_chatter("FlowData(%p - %d): %s\n", this, sizeof(*this), unparse().c_str());
}

bool
FlowData::is_reversible() const
{
    switch (data.protonum) {
    case IPPROTO_ICMP:
	return invmap[data.src.u.icmp.type] != 0;

    default:
	return true;
    }
}

FlowData
FlowData::reverse() const
{
    switch (data.protonum) {
    case IPPROTO_ICMP: {
	FlowData r = FlowData(data.protonum, data.dst, data.src);
	if (invmap[data.src.u.icmp.type] != 0) {
	    r.data.src.u.icmp.type = invmap[data.src.u.icmp.type] - 1;
	    r.data.src.u.icmp.code = data.src.u.icmp.code;
	}

	r.data.dst.u.icmp.type = r.data.dst.u.icmp.code = 0;
	return r;
    }

    default:
	return FlowData(data.protonum, data.dst, data.src);
    }
}

StringAccum& FlowData::unparse(StringAccum& sa) const
{
    switch (data.protonum) {
    case IPPROTO_TCP:
	sa << "TCP: "
	   << IPAddress(data.src.a.v4) << ":" << ntohs(data.src.u.tcp.port)
	   << " -> "
	   << IPAddress(data.dst.a.v4) << ":" << ntohs(data.dst.u.tcp.port);
	break;

    case IPPROTO_UDP:
	sa << "UDP: "
	   << IPAddress(data.src.a.v4) << ":" << ntohs(data.src.u.udp.port)
	   << " -> "
	   << IPAddress(data.dst.a.v4) << ":" << ntohs(data.dst.u.udp.port);
	break;

    case IPPROTO_ICMP:
	sa << "ICMP "
	   << (int)data.src.u.icmp.type << ":" << (int)data.src.u.icmp.code << " : "
	   << IPAddress(data.src.a.v4)
	   << " -> "
	   << IPAddress(data.dst.a.v4);
	break;

    default:
	sa << "Proto "
	   << (int)data.protonum << " : "
	   << IPAddress(data.src.a.v4) << ":" << ntohs(data.src.u.all)
	   << " -> "
	   << IPAddress(data.dst.a.v4) << ":" << ntohs(data.src.u.all);
	break;
   }

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

ei_x &operator<<(ei_x &x, const FlowData &fd)
{
    // {Protocol, Source, Destination}
    switch (fd.data.protonum) {
    case IPPROTO_TCP:
	// {'tcp', {SrcIPv4, SrcPort}, {DstIPv4, DstPort}}
	x << tuple(3) << atom("tcp")
		      << tuple(2) << binary(&fd.data.src.a.v4, 4) << ntohs(fd.data.src.u.tcp.port)
		      << tuple(2) << binary(&fd.data.dst.a.v4, 4) << ntohs(fd.data.dst.u.tcp.port);
	break;

    case IPPROTO_UDP:
	// {'udp', {SrcIPv4, SrcPort}, {DstIPv4, DstPort}}
	x << tuple(3) << atom("udp")
		      << tuple(2) << binary(&fd.data.src.a.v4, 4) << ntohs(fd.data.src.u.udp.port)
		      << tuple(2) << binary(&fd.data.dst.a.v4, 4) << ntohs(fd.data.dst.u.udp.port);
	break;

    case IPPROTO_ICMP:
	// {{'icmp', type, code}, SrcIPv4, DstIPv4}
	x << tuple(3) << tuple(3) << atom("icmp") << fd.data.src.u.icmp.type << fd.data.src.u.icmp.code
		      << binary(&fd.data.src.a.v4, 4)
		      << binary(&fd.data.dst.a.v4, 4);
	break;

    default:
	x << tuple(3) << fd.data.protonum
		      << tuple(2) << binary(&fd.data.src.a.v4, 4) << ntohs(fd.data.src.u.all)
		      << tuple(2) << binary(&fd.data.dst.a.v4, 4) << ntohs(fd.data.dst.u.all);
	break;
    }
    return x;
}

ei_x &operator<<(ei_x &x, const Flow &f)
{
    // {SourceGroup, DestinationGroup, Action}
    x << tuple(3) << f.srcGroup << f.dstGroup << (long)f.action;
    return x;
}

#define jiffies_diff(a, b) ({			\
	    __typeof__(a) _a = (a);		\
	    __typeof__(b) _b = (b);		\
	    _a > _b ? (click_jiffies_difference_t)(_a - _b) : -(click_jiffies_difference_t)(_b - _a); \
	})

ei_x &operator<<(ei_x &x, const FlowTable &t)
{
    click_jiffies_t now = click_jiffies();

    for (FlowTable::const_iterator it = t.begin(); it != t.end(); ++it) {
	const FlowData &k = it.key();
	const Flow *f = it.value();

	if (f) {
	    x << list(1)
	      << tuple(4) << f->id() << k << *f << jiffies_diff(f->_expiry, now) * (1000 / CLICK_HZ);
	}
    }
    x << list;
    return x;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(Flow)
ELEMENT_PROVIDES(FlowData)
