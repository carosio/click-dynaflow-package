#include <click/config.h>
#include "df_set_flowid.hh"
#include "df.hh"
#include <click/hashallocator.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
CLICK_DECLS

DF_SetFlowID::DF_SetFlowID()
{
}

DF_SetFlowID::~DF_SetFlowID()
{
}

int
DF_SetFlowID::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (Args(conf, this, errh)
      /* TODO: how can it readed as 64bit?*/
      .read_m("FLOW_ID",IntArg(), _flow_id).complete() < 0 )
    return -1;

    return 0;
}

Packet *
DF_SetFlowID::simple_action(Packet *p)
{
    SET_DF_FLOWID_ANNO(p, reinterpret_cast<unsigned int&>(_flow_id));
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_SetFlowID)
