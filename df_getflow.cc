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
    Flow *real_flow = _store->lookup_flow(f);
    if(!real_flow) { // unknown flow
        SET_FLOW_ANNO(p, f->_id);
        SET_ACTION_ANNO(p, DF_RULE_UNKNOWN);

        delete f;

        return p;
    }

    assert(f->_id == real_flow->_id);

    delete f;

    SET_GROUP_SRC_ANNO(p, real_flow->srcGroup);
    SET_GROUP_DST_ANNO(p, real_flow->dstGroup);
    SET_CLIENT_ANNO(p, real_flow->client->id());
    SET_FLOW_ANNO(p, real_flow->_id);
    SET_FLOW_COUNT_ANNO(p, real_flow->_count);
    SET_ACTION_ANNO(p, real_flow->action);

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_GetFlow)
