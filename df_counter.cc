#include <click/config.h>
#include "df_counter.hh"
#include "df.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
#include <click/handler.hh>
CLICK_DECLS

void Simple_Counter::reset() {
    _pkt_count = 0;
    _byte_count = 0;
}

Simple_Counter::Simple_Counter() {
    reset();
}

DF_Counter::DF_Counter()
{
}

DF_Counter::~DF_Counter()
{
    for(Table::iterator it = _table.begin(); it; it++) {
        Simple_Counter_Container *scc = _table.erase(it);
        _my_alloc.deallocate(scc);
    }
}

Packet *
DF_Counter::simple_action(Packet *p)
{
    uint64_t flow_id = DF_FLOWID_ANNO(p);
    Simple_Counter_Container *scc = NULL;
    Table::iterator it = _table.find(flow_id);
    if(!it) {
        void *x = _my_alloc.allocate();
        if(!x)
            return p;
        scc = new(x) Simple_Counter_Container(flow_id);
        _table.set(it, scc);
        _table.balance();
    } else
        scc = it.get();

    scc->counter._pkt_count += 1;
    scc->counter._byte_count += p->length();

    return p;
}

enum { H_BYTE_COUNT, H_PKT_COUNT, H_BYTE_COUNT_ALL, H_PKT_COUNT_ALL, H_RESET_ALL };

String DF_Counter::pkt_count_all_handler(Element* e, void * thunk)
{
    DF_Counter *self = (DF_Counter *)e;
    String s = String();
    for(Table::iterator it = self->_table.begin(); it; it++) {
        s += String(it->_flow_id);
        s += ":";
        s += String(it->counter.pkt_count());
        s += String("\n");
    }
    return s;
}

String DF_Counter::byte_count_all_handler(Element* e, void * thunk)
{
    DF_Counter *self = (DF_Counter *)e;
    String s = String();
    for(Table::iterator it = self->_table.begin(); it; it++) {
        s += String(it->_flow_id);
        s += ":";
        s += String(it->counter.byte_count());
        s += String("\n");
    }
    return s;
}

String DF_Counter::reset_all_handler(Element* e, void * thunk)
{
    DF_Counter *self = (DF_Counter *)e;
    for(Table::iterator it = self->_table.begin(); it; it++) {
        it->counter.reset();
    }
    return String();
}

void
DF_Counter::add_handlers()
{
    add_read_handler("byte_count_all", byte_count_all_handler, H_BYTE_COUNT_ALL);
    add_read_handler("pkt_count_all", pkt_count_all_handler, H_PKT_COUNT_ALL);
    add_read_handler("reset_all", reset_all_handler, H_PKT_COUNT_ALL);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DF_Counter)
