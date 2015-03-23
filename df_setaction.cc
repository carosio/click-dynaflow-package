#include <click/config.h>
#include "df_clients.hh"
#include "df_store.hh"

#define DEBUG

CLICK_DECLS

DF_SetAction::DF_SetAction() {}

DF_SetAction::~DF_SetAction() {}

int
DF_SetAction::configure(Vector<String> &conf, ErrorHandler *errh)
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
DF_SetAction::simple_action(Packet *p)
{

    uint32_t c_id = CLIENT_ANNO(p);
    uint32_t gs_id = GROUP_SRC_ANNO(p);
    uint32_t gd_id = GROUP_DST_ANNO(p);
    DF_Client *client = _store->lookup_client(c_id);

    ClientRuleTable *cr = &(client->rules);
    uint8_t action = cr->find(struct ClientRuleGroups(gs_id, gd_is), DF_ACTION_UNKOWN);

    SET_ACTION_ANNO(p, action);

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_SetAction)
