#include <click/config.h>
#include "df_clients.hh"
#include "df_store.hh"
#include "df_grouptable.hh"
#include "df_grouptable_ip.hh"
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

DF_GetGroupIP::DF_GetGroupIP() {}

DF_GetGroupIP::~DF_GetGroupIP() {}

int
DF_GetGroupIP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    DF_Store *new_store;
    int new_offset = -1;
    String ip_word;
    String anno;
    bool is_client;

    if (Args(conf, this, errh)
	.read_mp("STORE", ElementCastArg("DF_Store"), new_store)
	.read_p("OFFSET", new_offset)
        .read("IP", ip_word)
	.read_mp("ANNO", anno)
	.read_p("CLIENT", is_client)
       .complete() < 0)
        return -1;

    if ((new_offset >= 0 && ip_word) || (new_offset < 0 && !ip_word))
        return errh->error("set one of OFFSET, IP");
    else if (ip_word == "src")
        new_offset = offset_ip_src;
    else if (ip_word == "dst")
        new_offset = offset_ip_dst;
    else if (ip_word)
        return errh->error("bad IP");

    uint32_t new_anno = 0;
    if (anno.lower() == "src")
        new_anno = GROUP_SRC_ANNO_OFFSET;
    else if (anno.lower() == "dst")
        new_anno = GROUP_DST_ANNO_OFFSET;
    else
        return errh->error("type mismatch: bad ANNO");

    _store = new_store;
    _offset = new_offset;
    _anno = new_anno;
    _is_client = is_client;
    return 0;
}

Packet *
DF_GetGroupIP::simple_action(Packet *p)
{
    uint32_t ip = 0;

    if (_offset >= 0)
        ip = IPAddress(p->data() + _offset).addr();
    else if (_offset == offset_ip_src)
        ip = p->ip_header()->ip_src.s_addr;
    else if (_offset == offset_ip_dst)
        ip = p->ip_header()->ip_dst.s_addr;

    // lookup group and set anno or whatever....

    DF_GroupEntryIP *group = _store->lookup_group_ip(ip);
    if (!group) {
	click_chatter("%s: IP %s -> unknown\n", declaration().c_str(), IPAddress(ip).unparse().c_str());
        checked_output_push(1, p);
        return 0;
    }

    click_chatter("%s: IP %s -> %s\n", declaration().c_str(), IPAddress(ip).unparse().c_str(), group->unparse().c_str());

    ClientValue *client = group->client();
    click_chatter("%s: Client: %p, %d\n", declaration().c_str(), client, _is_client);
    if (client && _is_client) {
	click_chatter("%s: Client-Id: %08x\n", declaration().c_str(), client->id());
	SET_CLIENT_ANNO(p, client->id());
    }

    click_chatter("%s: Group-Id: %08x\n", declaration().c_str(), group->id());
    SET_GROUP_ANNO(p, _anno, group->id());

    return p;
}

StringAccum& DF_GroupEntryIP::unparse(StringAccum& sa) const
{
    sa << _addr.unparse_with_mask(_prefix) << " -> " << id();
    return sa;
}

String DF_GroupEntryIP::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}
CLICK_ENDDECLS
EXPORT_ELEMENT(DF_GetGroupIP)
