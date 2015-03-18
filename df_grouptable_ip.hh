#ifndef DF_GROUPTABLE_IP_HH
#define DF_GROUPTABLE_IP_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
#include "df_grouptable.hh"

/*
 * =c
 *
 * DF_GetGroupIP(STORE, OFFSET [, keyword IP])
 *
 * =s ip
 *
 * try to find a DF_GetGroup for IP address
 *
 * =d
 *
 * Attempt to find a GroupIP element to the specified IP address.
 * It packets with group on output 0. Packets without a group are
 * pushed out on output 1, unless output 1 was unused; if so, drops them.
 *
 * IP address is read from the packet, starting at OFFSET.
 * OFFSET is usually 16, to fetch the destination address from an IP packet.
 *
 * You may also give an IP keyword argument instead of an OFFSET.  IP must
 * equal 'src' or 'dst'.  The input packet must have its IP header annotation
 * set.  The named destination IP address annotation is set to that IP
 * header's source or destination address, respectively.
 *
 * STORE points to named DF_Store instance to read the group information
 * from.
 */

class DF_Store;

class DF_GroupEntryIP: public DF_GroupEntry {
public:
    DF_GroupEntryIP(IPAddress addr_, IPAddress prefix_, String group_name_) :
	DF_GroupEntry(group_name_), _addr(addr_), _prefix(prefix_) {};
    ~DF_GroupEntryIP() {};

    inline uint32_t addr() const { return _addr; };
    inline uint32_t mask() const { return _prefix; };

    StringAccum& unparse(StringAccum& sa) const;
    String unparse() const;

private:
    IPAddress _addr;
    IPAddress _prefix;
};

typedef Vector<DF_GroupEntryIP *> GroupTableIP;

class DF_GetGroupIP : public Element {
public:
    DF_GetGroupIP() CLICK_COLD;
    ~DF_GetGroupIP() CLICK_COLD;

    const char *class_name() const	{ return "DF_GetGroupIP"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PROCESSING_A_AH; }
    const char *flags() const		{ return "A"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const   { return true; }

    Packet *simple_action(Packet *);

private:
    enum {
        offset_ip_src = -1,
        offset_ip_dst = -2
    };

    DF_Store *_store;
    int _offset;
    int _anno;
};

inline StringAccum&
operator<<(StringAccum& sa, const DF_GroupEntryIP& entry)
{
    return entry.unparse(sa);
}

CLICK_ENDDECLS
#endif
