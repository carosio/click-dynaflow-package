#include <click/config.h>
#include <click/args.hh>
#include "df_getflow.hh"
#include "df_store.hh"
#include "df_flow.hh"
#include "df.hh"

#define DEBUG

CLICK_DECLS

DF_GetFlow::DF_GetFlow() {}

DF_GetFlow::~DF_GetFlow() {}

int
DF_GetFlow::configure(Vector<String> &conf, ErrorHandler *errh)
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
DF_GetFlow::simple_action(Packet *p)
{
    Flow *f = new Flow(p);
    DF_RuleAction action = _store->lookup_flow_action(f->_id);

    SET_FLOW_ANNO(p, f->_id);
    SET_ACTION_ANNO(p, action);

    delete f;

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_GetFlow)
