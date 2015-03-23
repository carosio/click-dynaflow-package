#include <click/config.h>
#include <click/args.hh>
#include "df_store.hh"
#include "df_saveanno.hh"

#define DEBUG

CLICK_DECLS

DF_SaveAnno::DF_SaveAnno() {}

DF_SaveAnno::~DF_SaveAnno() {}

int
DF_SaveAnno::configure(Vector<String> &conf, ErrorHandler *errh)
{
    DF_Store *new_store;

    if (Args(conf, this, errh)
	.read_mp("STORE", ElementCastArg("DF_Store"), new_store)
    .complete() < 0)
        return -1;

    _store = new_store;
    return 0;
}

Packet *
DF_SaveAnno::simple_action(Packet *p)
{
    uint32_t f_id = FLOW_ANNO(p);
    uint8_t action = ACTION_ANNO(p);

    _store->set_flow_action(f_id, action); // too less info.?

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_SaveAnno)
