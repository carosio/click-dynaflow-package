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

const Vector<String> NATTranslation::Translations({
    "SymetricAddressKeyed",
    "AddressKeyed",
    "PortKeyed",
    "Random",
    "RandomPersistent",
    "Masquerade" });

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

CLICK_ENDDECLS
