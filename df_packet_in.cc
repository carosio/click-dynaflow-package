#include <click/config.h>
#include <click/args.hh>
#include "df_packet_in.hh"
#include "df_store.hh"
#include "df_flow.hh"
#include "df.hh"

#define DEBUG

CLICK_DECLS

DF_PacketIn::DF_PacketIn() : _reason("unknown") {}

DF_PacketIn::~DF_PacketIn() {}

int
DF_PacketIn::configure(Vector<String> &conf, ErrorHandler *errh)
{
    DF_Store *new_store;

    if (Args(conf, this, errh)
	.read_mp("STORE", ElementCastArg("DF_Store"), new_store)
	.read_p("REASON", _reason)
	.complete() < 0)
        return -1;

    _store = new_store;
    return 0;
}

void
DF_PacketIn::push(int, Packet *p)
{
    _store->notify_packet_in(_reason, p);
    p->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_PacketIn)
