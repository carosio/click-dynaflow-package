#include <click/config.h>
#include "df_store.hh"
#include "df_grouptable.hh"
#include "df_grouptable_mac.hh"
#include "df.hh"
#include <click/hashallocator.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
#include <fcntl.h>
#include "ei.h"

#define DEBUG

CLICK_DECLS

DF_GetGroupMAC::DF_GetGroupMAC() {}

DF_GetGroupMAC::~DF_GetGroupMAC() {}

int
DF_GetGroupMAC::configure(Vector<String> &conf, ErrorHandler *errh)
{
    DF_Store *new_store;
    int new_offset = -1;
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
DF_GetGroupMAC::simple_action(Packet *p)
{
    // TODO: implement
    //
    // lookup group and set anno or whatever....

    DF_GetGroupMAC *group = NULL;

    // group = _store.lookup_group_mac(p->data() + _offset);

    if (!group) {
        checked_output_push(1, p);
        return 0;
    }

    // implement annotation set ...

    return p;
}

StringAccum& DF_GroupEntryMAC::unparse(StringAccum& sa) const
{
    sa << addr.unparse_with_mask(prefix) << " -> " << id();
    return sa;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_GetGroupMAC)
