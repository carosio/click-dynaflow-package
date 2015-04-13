#include <click/config.h>
#include <click/args.hh>
#include "df_packet_out.hh"
#include "df_store.hh"
#include "df_flow.hh"
#include "df.hh"

#define DEBUG

CLICK_DECLS

DF_PacketOut::DF_PacketOut() {}

DF_PacketOut::~DF_PacketOut() {}

int
DF_PacketOut::configure(Vector<String> &conf, ErrorHandler *errh)
{
    DF_Store *new_store;

    if (Args(conf, this, errh)
	.read_mp("STORE", ElementCastArg("DF_Store"), new_store)
	.complete() < 0)
        return -1;

    _store = new_store;
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_PacketOut)
