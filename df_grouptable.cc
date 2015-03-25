#include <click/config.h>
#include "df_store.hh"
#include "df_grouptable.hh"
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

DF_GroupEntry::~DF_GroupEntry()
{
}

StringAccum& DF_GroupEntry::unparse(StringAccum& sa) const
{
    sa << " -> " << id();
    return sa;
}

String DF_GroupEntry::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}

DF_SetGroup::DF_SetGroup() {}

DF_SetGroup::~DF_SetGroup() {}

int
DF_SetGroup::configure(Vector<String> &conf, ErrorHandler *errh)
{
    DF_Store *new_store;
    String anno;
    DF_GroupEntry::GroupId group;

    if (Args(conf, this, errh)
	.read_mp("STORE", ElementCastArg("DF_Store"), new_store)
	.read_mp("GROUP", group)
	.read_mp("ANNO", anno)
       .complete() < 0)
        return -1;

    uint32_t new_anno = 0;
    if (anno.lower() == "src")
        new_anno = GROUP_SRC_ANNO_OFFSET;
    else if (anno.lower() == "dst")
        new_anno = GROUP_DST_ANNO_OFFSET;
    else
        return errh->error("type mismatch: bad ANNO");

    _store = new_store;
    _group = group;
    _anno = new_anno;
    return 0;
}

Packet *
DF_SetGroup::simple_action(Packet *p)
{
    SET_GROUP_ANNO(p, _anno, _group);

    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_SetGroup)
