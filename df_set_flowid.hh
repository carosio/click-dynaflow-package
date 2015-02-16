#ifndef DF_SET_FLOWID_HH
#define DF_SET_FLOWID_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
CLICK_DECLS

class DF_SetFlowID : public Element { public:

    DF_SetFlowID() CLICK_COLD;
    ~DF_SetFlowID() CLICK_COLD;

    const char *class_name() const		{ return "DF_SetFlowID"; }
    const char *port_count() const		{ return PORTS_1_1; }

    Packet *simple_action(Packet *);
    int configure(Vector<String> &, ErrorHandler *);

  private:
    
    uint32_t _flow_id; // 64bit are needed

};

CLICK_ENDDECLS
#endif
