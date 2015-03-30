#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include "df_pefswitch.hh"
#include "df_store.hh"
#include "df_clients.hh"

CLICK_DECLS

DF_PEFSwitch::DF_PEFSwitch() {
    for(int i=0; i < DF_RULE_SIZE; i++)
        mapping[i] = DF_RULE_ELSE;
}

DF_PEFSwitch::~DF_PEFSwitch() {}

int
DF_PEFSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int else_port = -1; // not set

    if (conf.size() != noutputs())
        return errh->error("need %d arguments, one per output port", noutputs());

    for (int slot = 0; slot < conf.size(); slot++) {
        String s = conf[slot];

	click_chatter("%s: slot %d -> %s\n", declaration().c_str(), slot, s.c_str());

        if(s.lower() == "accept") {
            mapping[DF_RULE_ACCEPT] = slot;
        } else if(s.lower() == "drop") {
            mapping[DF_RULE_DROP] = slot;
        } else if(s.lower() == "deny") {
            mapping[DF_RULE_DENY] = slot;
        } else if(s.lower() == "unknown") {
            mapping[DF_RULE_UNKNOWN] = slot;
        } else if(s.lower() == "no_action") {
            mapping[DF_RULE_NO_ACTION] = slot;
        } else if(s.lower() == "else") {
            else_port = slot;
        } else {
            return errh->error("unknown action: %s", s.c_str());
        }
    }

    if(else_port != -1) {
        for(int i = 0; i < DF_RULE_SIZE; i++) {
            if(mapping[i] == DF_RULE_ELSE)
                mapping[i] = else_port;
        }
    }

    for(int i = 0; i < DF_RULE_SIZE; i++)
	click_chatter("%s: config %d -> %d\n", declaration().c_str(), i, mapping[i]);

    return 0;
}

void
DF_PEFSwitch::push(int, Packet *p)
{
    click_chatter("%s: push %s -> %d\n", declaration().c_str(), ClientRule::ActionType[ACTION_ANNO(p)].c_str(), mapping[ACTION_ANNO(p)]);
    checked_output_push(mapping[ACTION_ANNO(p)], p);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_PEFSwitch)
