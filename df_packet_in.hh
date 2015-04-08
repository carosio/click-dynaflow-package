#ifndef DF_PACKET_IN_IP_HH
#define DF_PACKET_IN_IP_HH
#include <click/element.hh>
#include "df_store.hh"

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

class DF_PacketIn : public Element {
public:
    DF_PacketIn() CLICK_COLD;
    ~DF_PacketIn() CLICK_COLD;

    const char *class_name() const	{ return "DF_PacketIn"; }
    const char *port_count() const	{ return PORTS_1_0; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const   { return true; }

    void push(int, Packet *);

private:
    DF_Store *_store;
    String _reason;
};

CLICK_ENDDECLS
#endif
