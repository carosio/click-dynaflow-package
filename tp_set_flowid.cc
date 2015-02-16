#include <click/config.h>
#include "tp_set_flowid.hh"
#include "tp.hh"
#include <click/hashallocator.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
CLICK_DECLS

TP_SetFlowID::TP_SetFlowID()
{
}

TP_SetFlowID::~TP_SetFlowID()
{
}

int
TP_SetFlowID::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (Args(conf, this, errh)
      /* TODO: how can it readed as 64bit?*/
      .read_m("FLOW_ID",IntArg(), _flow_id).complete() < 0 )
    return -1;

    return 0;
}

Packet *
TP_SetFlowID::simple_action(Packet *p)
{
    SET_TP_FLOWID_ANNO(p, reinterpret_cast<unsigned int&>(_flow_id));
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TP_SetFlowID)
