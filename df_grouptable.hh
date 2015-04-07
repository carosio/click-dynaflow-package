#ifndef DF_GROUPTABLE_HH
#define DF_GROUPTABLE_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>
#include "df_clients.hh"

struct ClientValue;

class DF_GroupEntry {
public:
    typedef uint32_t GroupId;
private:
    GroupId _id;
    ClientValue *_client;

public:
    DF_GroupEntry(GroupId id_);
    DF_GroupEntry(ClientValue *client_);
    virtual ~DF_GroupEntry();

    virtual StringAccum& unparse(StringAccum& sa) const;
    virtual String unparse() const;

    inline GroupId id() const { return _id; };
    inline ClientValue *client() const { return _client; };

    inline bool operator==(DF_GroupEntry b) { return _id == b._id; }
};

inline StringAccum&
operator<<(StringAccum& sa, const DF_GroupEntry& entry)
{
    return entry.unparse(sa);
}

/*
 * =c
 *
 * DF_SetGroup(STORE, GROUP, ANNO)
 *
 * =s ip
 *
 * set the DF_Group
 *
 * =d
 *
 * STORE points to named DF_Store instance to read the group information
 * from.
 */

class DF_Store;

class DF_SetGroup : public Element {
public:
    DF_SetGroup() CLICK_COLD;
    ~DF_SetGroup() CLICK_COLD;

    const char *class_name() const	{ return "DF_SetGroup"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const   { return true; }

    Packet *simple_action(Packet *);

private:
    DF_Store *_store;
    DF_GroupEntry::GroupId _group;
    int _anno;
};

CLICK_ENDDECLS
#endif
