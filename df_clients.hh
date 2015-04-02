#ifndef DF_CLIENTS_HH
#define DF_CLIENTS_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/hashtable.hh>
#include <click/straccum.hh>
#include "uniqueid.hh"
#include "ei.hh"
#include "df.hh"
#include "df_grouptable.hh"

CLICK_DECLS

// global Id Manager for Clients
extern IdManager client_ids;

struct NATTranslation {
    static const String Translations[];
    enum Type { SymetricAddressKeyed,
		AddressKeyed,
		PortKeyed,
		Random,
		RandomPersistent,
		Masquerade,
		LastEntry = Masquerade};
    IPAddress key;
    Type type;
    IPAddress nat_addr;
    int min_port;
    int max_port;

    inline NATTranslation() : type(Random), min_port(0), max_port(0) {};
    NATTranslation(Type type_, IPAddress nat_addr_, int min_port_, int max_port_) :
	type(type_), nat_addr(nat_addr_), min_port(min_port_), max_port(max_port_) {};
    StringAccum& unparse(StringAccum& sa) const;
    String unparse() const;
};

inline StringAccum&
operator<<(StringAccum& sa, const NATTranslation& translation)
{
    return translation.unparse(sa);
}

ei_x &operator<<(ei_x &x, const NATTranslation &n);

inline ei_x &operator<<(ei_x &x, const NATTranslation::Type t)
{
    x << atom(NATTranslation::Translations[t]);
    return x;
}

typedef HashMap<IPAddress, NATTranslation> NATTable;

ei_x &operator<<(ei_x &x, const NATTable &t);

struct ClientKey {
public:
    int type;
    IPAddress addr;

    inline ClientKey() { type = 0; };
    ClientKey(int type_, IPAddress addr_) :
	type(type_), addr(addr_) { };

    inline bool operator==(ClientKey other) const { return addr == other.addr; };
};

struct ClientRule {
    static const String ActionType[];

    DF_GroupEntry::GroupId src;
    DF_GroupEntry::GroupId dst;
    DF_RuleAction out;

    ClientRule(DF_GroupEntry::GroupId src_, DF_GroupEntry::GroupId dst_, DF_RuleAction out_) :
	src(src_), dst(dst_), out(out_) {};
};

typedef Vector<ClientRule *> ClientRuleTable;

struct ClientValue {
private:
    uint32_t _id;

public:
    ClientKey key;

    DF_GroupEntry::GroupId group;
    NATTable nat_rules;
    ClientRuleTable rules;

    ClientValue();
    ClientValue(ClientKey key_, DF_GroupEntry::GroupId group_,
		NATTable nat_rules_, ClientRuleTable rules_) :
	key(key_), group(group_), nat_rules(nat_rules_), rules(rules_) {};
    ~ClientValue();

    inline uint32_t id() const { return _id; };
};


typedef HashTable<uint32_t, ClientValue *> ClientTable;

ei_x &operator<<(ei_x &x, const ClientKey &ck);
inline ei_x &operator<<(ei_x &x, const DF_RuleAction action)
{
    x << atom(ClientRule::ActionType[action]);
    return x;
}
ei_x &operator<<(ei_x &x, const ClientRule &cr);
ei_x &operator<<(ei_x &x, const ClientRuleTable &t);
ei_x &operator<<(ei_x &x, const ClientValue &cv);
ei_x &operator<<(ei_x &x, const ClientTable &t);

CLICK_ENDDECLS
#endif
