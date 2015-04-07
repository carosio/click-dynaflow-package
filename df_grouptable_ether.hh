#ifndef DF_GROUPTABLE_Ether_HH
#define DF_GROUPTABLE_Ether_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <click/straccum.hh>
#include "ei.hh"
#include "df_grouptable.hh"

/*
 * =c
 *
 * DF_GetGroupEther(STORE, OFFSET)
 *
 * =s ip
 *
 * try to find a DF_GetGroup for Ether address
 *
 * =d
 *
 * Attempt to find a GroupEther element.
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

class DF_GroupEntryEther: public DF_GroupEntry {
public:
    DF_GroupEntryEther(ClientValue *client) :
	DF_GroupEntry(client), _addr(client->ether()) {};
  DF_GroupEntryEther(GroupId id, EtherAddress addr_) :
	DF_GroupEntry(id), _addr(addr_) {};
    ~DF_GroupEntryEther() {};

    StringAccum& unparse(StringAccum& sa) const;
    String unparse() const;

private:
    EtherAddress _addr;

    friend ei_x &operator<<(ei_x &x, const DF_GroupEntryEther &e);
};

typedef HashTable<EtherAddress, DF_GroupEntryEther *> GroupTableEther;

class DF_GetGroupEther : public Element {
public:
    DF_GetGroupEther() CLICK_COLD;
    ~DF_GetGroupEther() CLICK_COLD;

    const char *class_name() const	{ return "DF_GetGroupEther"; }
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
operator<<(StringAccum& sa, const DF_GroupEntryEther& entry)
{
    return entry.unparse(sa);
}

ei_x &operator<<(ei_x &x, const DF_GroupEntryEther &e);

CLICK_ENDDECLS
#endif
