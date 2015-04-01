#include <click/config.h>
#include <click/args.hh>
#include "df_store.hh"
#include "df_saveanno.hh"
#include "df.hh"
#include "df_flow.hh"
#include "df_clients.hh"

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
    uint32_t srcGroup = GROUP_SRC_ANNO(p);
    uint32_t dstGroup = GROUP_DST_ANNO(p);
    uint32_t client_id = CLIENT_ANNO(p);

    DF_RuleAction action = static_cast<DF_RuleAction>(ACTION_ANNO(p));
    ClientValue *client = _store->lookup_client(client_id);

    Flow *f = new Flow(p, client, srcGroup, dstGroup, action);
    click_chatter("%s: Client: (%p)%p\n", declaration().c_str(), &client, client);
    click_chatter("%s: Flow: %s\n", declaration().c_str(), f->unparse().c_str());

    _store->set_flow(f);

    SET_FLOW_ANNO(p, f->id());

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_SaveAnno)
