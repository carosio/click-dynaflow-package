#ifndef DF_GROUPTABLE_HH
#define DF_GROUPTABLE_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>

class DF_GetGroupEntry {
private:
    String _group_name;

public:
    DF_GetGroupEntry() {};
    DF_GetGroupEntry(String group_name_) : _group_name(group_name_) {};
    ~DF_GetGroupEntry() {};

    virtual StringAccum& unparse(StringAccum& sa) const;
    virtual String unparse() const;

    inline String group_name() const { return _group_name; };

    inline bool operator==(DF_GetGroupEntry b) { return _group_name == b._group_name; }

    // TODO: add HashContainer support for finding groups by name
};

CLICK_ENDDECLS
#endif
