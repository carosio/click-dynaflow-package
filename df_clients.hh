#ifndef DF_CLIENTS_HH
#define DF_CLIENTS_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
#include "uniqueid.hh"
#include "df.hh"

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
    unsigned int type;
    IPAddress nat_addr;
    int min_port;
    int max_port;

    inline NATTranslation() { type = min_port = max_port = 0; };
    NATTranslation(unsigned int type_, IPAddress nat_addr_, int min_port_, int max_port_) :
	type(type_), nat_addr(nat_addr_), min_port(min_port_), max_port(max_port_) {};
    StringAccum& unparse(StringAccum& sa) const;
    String unparse() const;
};

typedef HashMap<IPAddress, NATTranslation> NATTable;

struct ClientKey {
private:
    uint32_t _id;

public:
    int type;
    IPAddress addr;

    inline ClientKey() { type = 0; };
    ClientKey(int type_, IPAddress addr_) :
	type(type_), addr(addr_) { _id = addr.addr(); };

    inline bool
    operator==(ClientKey other)
    {
	return addr == other.addr;
    };

    inline uint32_t hashcode() const;
};

struct ClientRule {
    int src;
    int dst;
    DF_RuleAction out;

    ClientRule(int src_, int dst_, DF_RuleAction out_) :
	src(src_), dst(dst_), out(out_) {};
};

typedef Vector<ClientRule *> ClientRuleTable;

struct ClientValue {
private:
    int _id; // ?

public:
    ClientKey key;

    String group;
    NATTable nat_rules;
    ClientRuleTable rules;

    ClientValue();
    ClientValue(ClientKey key_, String group_,
		NATTable nat_rules_, ClientRuleTable rules_) :
	key(key_), group(group_), nat_rules(nat_rules_), rules(rules_) {};
    ~ClientValue();

    inline int id() const { return _id; };
};

typedef HashMap<uint32_t, ClientValue *> ClientTable;

uint32_t ClientKey::hashcode() const
{
    //FIXME: define hash function
    //return 0;
    return _id;
}

inline StringAccum&
operator<<(StringAccum& sa, const NATTranslation& translation)
{
    return translation.unparse(sa);
}

CLICK_ENDDECLS
#endif
