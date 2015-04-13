#ifndef DF_PACKET_OUT_HH
#define DF_PACKET_OUT_HH
#include <click/element.hh>
#include "df_store.hh"

/*
 */

class DF_Store;

class DF_PacketOut : public Element {
public:
    DF_PacketOut() CLICK_COLD;
    ~DF_PacketOut() CLICK_COLD;

    const char *class_name() const	{ return "DF_PacketOut"; }
    const char *port_count() const	{ return PORTS_0_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const   { return true; }

private:
    DF_Store *_store;
};

CLICK_ENDDECLS
#endif
