#ifndef DF_SAVEANNO_IP_HH
#define DF_SAVEANNO_IP_HH
#include <click/element.hh>
#include "df_store.hh"

/*
 * =c
 *
 * DF_SaveAnno(STORE)
 *
 */

class DF_Store;

class DF_SaveAnno : public Element {
public:
    DF_SaveAnno() CLICK_COLD;
    ~DF_SaveAnno() CLICK_COLD;

    const char *class_name() const	{ return "DF_SaveAnno"; }
    const char *port_count() const	{ return PORTS_1_1; }
    const char *processing() const	{ return PROCESSING_A_AH; }
    const char *flags() const		{ return "A"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const   { return true; }

    Packet *simple_action(Packet *);

private:

    DF_Store *_store;
};

CLICK_ENDDECLS
#endif
