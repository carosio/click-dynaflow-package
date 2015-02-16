#ifndef TP_COUNTER_HH
#define TP_COUNTER_HH
#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/atomic.hh>
CLICK_DECLS

struct Simple_Counter {
    atomic_uint32_t _pkt_count;
    atomic_uint32_t _byte_count; // too small?

    uint32_t byte_count() const     { return _byte_count; }
    uint32_t pkt_count() const     { return _pkt_count; }
    
    Simple_Counter();
    void reset();
};

struct Simple_Counter_Container {
    struct Simple_Counter counter;
    uint64_t _flow_id;
    Simple_Counter_Container *_hashnext;

    typedef uint64_t key_type;
    typedef uint64_t key_const_reference;

    Simple_Counter_Container(uint64_t flowid) : _flow_id(flowid), _hashnext(0) {};

    key_const_reference hashkey() const {
        return _flow_id;
    }
};

class TP_Counter : public Element { public:

    TP_Counter() CLICK_COLD;
    ~TP_Counter() CLICK_COLD;

    const char *class_name() const		{ return "TP_Counter"; }
    const char *port_count() const		{ return PORTS_1_1; }

    Packet *simple_action(Packet *);
    void add_handlers() CLICK_COLD;

  protected:

    static String pkt_count_all_handler(Element* e, void * thunk);
    static String byte_count_all_handler(Element* e, void * thunk);
    static String reset_all_handler(Element* e, void * thunk);

  private:

    typedef HashContainer<struct Simple_Counter_Container> Table;
    Table _table;

    SizedHashAllocator<sizeof(Simple_Counter_Container)> _my_alloc;

};

CLICK_ENDDECLS
#endif
