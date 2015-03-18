#ifndef DF_GROUPTABLE_HH
#define DF_GROUPTABLE_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>

class DF_GroupEntry {
private:
    int _id;
    String _group_name;

public:
    DF_GroupEntry();
    DF_GroupEntry(String group_name_) : _group_name(group_name_) {};
    ~DF_GroupEntry();

    virtual StringAccum& unparse(StringAccum& sa) const;
    virtual String unparse() const;

    inline int id() const { return _id; };
    inline String group_name() const { return _group_name; };

    inline bool operator==(DF_GroupEntry b) { return _group_name == b._group_name; }

    // TODO: add HashContainer support for finding groups by name
};

typedef Vector<DF_GroupEntry *> GroupTable;

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
    String _group;
    int _anno;
};

CLICK_ENDDECLS
#endif
