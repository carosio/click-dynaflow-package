#include <click/config.h>
#include "df_clients.hh"
#include "df.hh"
#include <click/hashallocator.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>

CLICK_DECLS

IdManager client_ids;

const String NATTranslation::Translations[] = {
    "SymetricAddressKeyed",
    "AddressKeyed",
    "PortKeyed",
    "Random",
    "RandomPersistent",
    "Masquerade" };

StringAccum& NATTranslation::unparse(StringAccum& sa) const
{
    switch (type) {
    case SymetricAddressKeyed:
    case AddressKeyed:
	sa << Translations[type] << ": " << nat_addr;
	break;

    case PortKeyed:
	sa << Translations[type] << ": " << nat_addr << '[' << min_port << '-' << max_port << ']';
	break;

    case Random:
    case RandomPersistent:
    case Masquerade:
	sa << Translations[type];
	break;

    default:
	sa << "invalid";
	break;
    }

    return sa;
}

String NATTranslation::unparse() const
{
    StringAccum sa;
    sa << *this;
    return sa.take_string();
}

const String ClientRule::ActionType[] = {
	[DF_RULE_UNKNOWN]   = "unknown",
	[DF_RULE_NO_ACTION] = "no_action",
	[DF_RULE_ACCEPT]    = "accept",
	[DF_RULE_DENY]      = "deny",
	[DF_RULE_DROP]      = "drop" };

ClientValue::ClientValue()
{
    _id = client_ids.AllocateId();
    click_chatter("%p{element}: AllocateId: %d\n", this, _id);
}

ClientValue::~ClientValue()
{
    click_chatter("%p{element}: FreeId: %d\n", this, _id);
    client_ids.FreeId(_id);
}

ei_x &operator<<(ei_x &x, const NATTranslation &n)
{
    // {Key, Type, Translation}
    x << tuple(3) << n.key << n.type;

    switch (n.type) {
    case NATTranslation::SymetricAddressKeyed:
    case NATTranslation::AddressKeyed:
	x << n.nat_addr;
	break;

    case NATTranslation::PortKeyed:
	x << tuple(3) << n.nat_addr << n.min_port << n.max_port;
	break;

    case NATTranslation::Random:
    case NATTranslation::RandomPersistent:
    case NATTranslation::Masquerade:
	x << atom("undefined");
	break;

    default:
	x << (long)n.type;
	break;
    }

    return x;
}

ei_x &operator<<(ei_x &x, const NATTable &t)
{
    for (NATTable::const_iterator it = t.begin(); it != t.end(); ++it) {
	x << list(1) << it.value();
    }
    x << list;
    return x;
}

ei_x &operator<<(ei_x &x, const ClientKey &ck)
{
    // {Type, Addr} - FIXME: only 'inet' for now
    x << tuple(2) << atom("inet") << binary(ck.addr);
    return x;
}

ei_x &operator<<(ei_x &x, const ClientRule &cr)
{
    x << tuple(3) << cr.src << cr.dst << cr.out;
    return x;
}

ei_x &operator<<(ei_x &x, const ClientRuleTable &t)
{
    for (ClientRuleTable::const_iterator it = t.begin(); it != t.end(); ++it) {
	x << list(1) << **it;
    }
    x << list;
    return x;
}

ei_x &operator<<(ei_x &x, const ClientValue &cv)
{
    // {Key, {Group, NATTable, ClientRuleTable}}
    x << tuple(2) << cv.key
		  << tuple(3) << cv.group << cv.nat_rules << cv.rules;
    return x;
}

ei_x &operator<<(ei_x &x, const ClientTable &t)
{
    for (ClientTable::const_iterator it = t.begin(); it != t.end(); ++it) {
	x << list(1) << *it.value();
    }
    x.encode_empty_list();
    return x;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(NATTranslation)
