#ifndef DF_GROUPTABLE_MAC_HH
#define DF_GROUPTABLE_MAC_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
#include "df_grouptable.hh"

/*
 * =c
 *
 * DF_GetGroupMAC(STORE, OFFSET)
 *
 * =s ip
 *
 * try to find a DF_GetGroup for MAC address
 *
 * =d
 *
 * Attempt to find a GroupMAC element.
 *
 * Reads an Ethernet address ADDR from the packet at offset OFFSET.  If OFFSET
 * is out of range, the input packet is dropped or emitted on optional output 1.
 *
 * The OFFSET argument may be 'src' or 'dst'.  These strings are equivalent to
 * offsets 6 and 0, respectively, which are the offsets into an Ethernet header
 * of the source and destination Ethernet addresses.
 *
 * STORE points to named DF_Store instance to read the group information
 * from.
 */

class DF_Store;

class DF_GroupEntryMAC: public DF_GroupEntry {
public:
  DF_GroupEntryMAC(int id, IPAddress addr_, IPAddress prefix_) :
	DF_GroupEntry(id), addr(addr_), prefix(prefix_) {};
    ~DF_GroupEntryMAC() {};

    StringAccum& unparse(StringAccum& sa) const;
    String unparse() const;

private:
    IPAddress addr;
    IPAddress prefix;
};

typedef Vector<DF_GroupEntryMAC *> GroupTableMAC;

class DF_GetGroupMAC : public Element {
public:
    DF_GetGroupMAC() CLICK_COLD;
    ~DF_GetGroupMAC() CLICK_COLD;

    const char *class_name() const	{ return "DF_GetGroupMAC"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PROCESSING_A_AH; }
    const char *flags() const		{ return "A"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const   { return true; }

    Packet *simple_action(Packet *);

private:
    DF_Store *_store;
    int _offset;
    int _anno;
};

inline StringAccum&
operator<<(StringAccum& sa, const DF_GroupEntryMAC& entry)
{
    return entry.unparse(sa);
}

CLICK_ENDDECLS
#endif
