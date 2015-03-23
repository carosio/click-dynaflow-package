#ifndef DF_PEFSWITCH_IP_HH
#define DF_PEFSWITCH_IP_HH
#include <click/element.hh>
#include "df_flow.hh"

/*
 * =c
 *
 * DF_PEFSwitch(action1,...,actionN)
 *
 * =s classification
 *
 * classifies packets by the ACTION_ANNO annotation
 *
 * =d
 * Classifies packets. The DF_PEFSwitch has N outputs, each associated with the
 * corresponding action from the configuration string.
 *
 * The action is stored in the ACTION_ANNO of the packet.
 *
 * =e
 * For example,
 *
 *  DF_PEFSwitch(unknown, else);
 *
 *  creates an element with two outputs. Each packet where the ACTION_ANNO is
 *  set to "unknown" are sent to output 0, all others to output 1.
 */

class DF_PEFSwitch : public Element {
public:
    DF_PEFSwitch() CLICK_COLD;
    ~DF_PEFSwitch() CLICK_COLD;

    const char *class_name() const	{ return "DF_PEFSwitch"; }
    const char *port_count() const	{ return "1/-"; }
    const char *processing() const	{ return PUSH; }
    const char *flags() const		{ return "A"; }
    bool can_live_reconfigure() const   { return true; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    void push(int, Packet *);

private:

    uint8_t mapping[DF_RULE_SIZE] = { DF_RULE_ELSE };
};

CLICK_ENDDECLS
#endif
