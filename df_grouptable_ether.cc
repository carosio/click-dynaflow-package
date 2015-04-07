#include <click/config.h>
#include "df_store.hh"
#include "df_grouptable.hh"
#include "df_grouptable_ether.hh"
#include "df.hh"
#include <click/hashallocator.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <fcntl.h>
#include "ei.h"

#define DEBUG

CLICK_DECLS

DF_GetGroupEther::DF_GetGroupEther() {}

DF_GetGroupEther::~DF_GetGroupEther() {}

int
DF_GetGroupEther::configure(Vector<String> &conf, ErrorHandler *errh)
{
    DF_Store *new_store;
    String off;
    String anno;

    if (Args(conf, this, errh)
	.read_mp("STORE", ElementCastArg("DF_Store"), new_store)
	.read_mp("OFFSET", off)
	.read_mp("ANNO", anno)
        .complete() < 0)
        return -1;

    uint32_t offset = 0;
    if (off.lower() == "src")
        offset = 6;
    else if (off.lower() == "dst")
        offset = 0;
    else if (!IntArg().parse(off, offset) || offset + 6 < 6)
        return errh->error("type mismatch: bad OFFSET");

    uint32_t new_anno = 0;
    if (anno.lower() == "src")
        new_anno = GROUP_SRC_ANNO_OFFSET;
    else if (anno.lower() == "dst")
        new_anno = GROUP_DST_ANNO_OFFSET;
    else
        return errh->error("type mismatch: bad ANNO");

    _store = new_store;
    _offset = offset;
    _anno = new_anno;
    return 0;
}

Packet *
DF_GetGroupEther::simple_action(Packet *p)
{
    EtherAddress addr = EtherAddress(p->data() + _offset);

    DF_GroupEntryEther *group = _store->lookup_group_ether(addr);
    if (!group) {
	click_chatter("%s: Ether %s -> unknown\n", declaration().c_str(), addr.unparse().c_str());
        checked_output_push(1, p);
        return 0;
    }

    click_chatter("%s: Ether %s -> %s\n", declaration().c_str(), addr.unparse().c_str(), group->unparse().c_str());

    ClientValue *client = group->client();
    SET_CLIENT_ANNO(p, client->id());

    click_chatter("%s: Group-Id: %08x\n", declaration().c_str(), group->id());
    SET_GROUP_ANNO(p, _anno, group->id());

    return p;
}

StringAccum& DF_GroupEntryEther::unparse(StringAccum& sa) const
{
    sa << _addr << " -> " << id();
    return sa;
}

String DF_GroupEntryEther::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}

ei_x &operator<<(ei_x &x, const DF_GroupEntryEther &e)
{
    x << tuple(2) << binary(e._addr) << e.id();
    return x;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_GetGroupEther)
