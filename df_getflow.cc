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
    FlowData fd = FlowData(p);
    Flow *f = _store->lookup_flow(fd);

    click_chatter("%s: flow %s : %p\n", declaration().c_str(), fd.unparse().c_str(), f);

    if (!f) { // unknown flow
	SET_ACTION_ANNO(p, DF_RULE_UNKNOWN);
	return p;
    }

    SET_GROUP_SRC_ANNO(p, f->srcGroup);
    SET_GROUP_DST_ANNO(p, f->dstGroup);
    SET_CLIENT_ANNO(p, f->client_id());
    SET_FLOW_ANNO(p, f->id());
    SET_ACTION_ANNO(p, f->action);

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_GetFlow)
