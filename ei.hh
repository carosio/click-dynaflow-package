#ifndef EI_HH
#define EI_HH

#include <click/packet.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/string.hh>
#include <click/straccum.hh>
#include "ei.h"

CLICK_DECLS

struct ei_badarg : public std::exception
{
    const char * what () const throw () { return "Erlang Bad Argument"; }
};

class ei_x {
private:
    ei_x_buff x;

public:
    ei_x(long size);
    ~ei_x();

    inline void reset() { x.index = 0; };
    inline ei_x_buff *buffer() { return &x; };

    inline String unparse() { return unparse(x.index); }
    inline String unparse(int index)
    {
	char *s = NULL;
	String r;

	if (index == 0) {
	    int version;
	    ei_decode_version(x.buff, &index, &version);
	}
	ei_s_print_term(&s, x.buff, &index);
	r = s;
	free(s);

	return r;
    }

    // EI_X decoder
    String decode_string();
    String decode_binary_string();
    int decode_tuple_header();
    int decode_list_header();
    int decode_version();
    erlang_ref decode_ref();
    erlang_pid decode_pid();
    unsigned long decode_ulong();
    String decode_atom();
    void decode_binary(void *b, int len);
    IPAddress decode_ipaddress();
    void get_type(int *type, int *size);
    void skip_term();

    // EI_X encoder
    ei_x &encode_version();
    ei_x &encode_tuple_header(int arity);
    ei_x &encode_list_header(int arity);
    ei_x &encode_empty_list();
    ei_x &encode_ref(const erlang_ref &ref);
    ei_x &encode_long(long p);
    ei_x &encode_atom(const char *s);
    ei_x &encode_atom(const String &s);
    ei_x &encode_binary(const void *b, int len);
    ei_x &encode_binary(const String &b);
    ei_x &encode_ipaddress(const IPAddress &ip);

    // support for iostream like manipulators for ei_x
    ei_x &operator<<(ei_x& (*__pf)(ei_x&)) {
	return __pf(*this);
    }
};

/*
 * implement iostream like manipulators for ei_x
 */

inline ei_x &version(ei_x &x)
{
    return x.encode_version();
};

struct _Tuple { int __arity; };
inline _Tuple tuple(int arity) { return { arity }; };
inline ei_x &operator<<(ei_x &x, _Tuple __arity)
{
    return x.encode_tuple_header(__arity.__arity);
};

struct _List { int __arity; };
inline _List list(int arity) { return { arity }; };
inline ei_x &operator<<(ei_x &x, _List __arity)
{
    if (__arity.__arity == 0)
	return x.encode_empty_list();

   return x.encode_list_header(__arity.__arity);
};

inline ei_x &list(ei_x &x)
{
    return x.encode_empty_list();
};

inline ei_x &operator<<(ei_x &x, const erlang_ref &ref)
{
    return x.encode_ref(ref);
};

inline ei_x &operator<<(ei_x &x, const uint32_t p)
{
    return x.encode_long(p);
};

inline ei_x &operator<<(ei_x &x, const int p)
{
    return x.encode_long(p);
};

inline ei_x &operator<<(ei_x &x, const long p)
{
    return x.encode_long(p);
};

struct _CharAtom { const char * __s; };
inline _CharAtom atom(const char * s) { return { s }; };
inline ei_x &operator<<(ei_x &x, _CharAtom __s)
{
   return x.encode_atom(__s.__s);
};

struct _StringAtom { const String & __s; };
inline _StringAtom atom(const String & s) { return { s }; };
inline ei_x &operator<<(ei_x &x, const _StringAtom __s)
{
   return x.encode_atom(__s.__s);
};

struct _Binary { const void *__b; int __len; };
inline _Binary binary(const void *b, int len) { return { b, len }; };
inline ei_x &operator<<(ei_x &x, _Binary __b)
{
    return x.encode_binary(__b.__b, __b.__len);
};

struct _EtherBinary { const EtherAddress & __addr; };
inline _EtherBinary binary(const EtherAddress & addr) { return { addr }; };
inline ei_x &operator<<(ei_x &x, const _EtherBinary __addr)
{
    return x.encode_binary(__addr.__addr.data(), 6);
};

struct _IPBinary { const IPAddress & __ip; };
inline _IPBinary binary(const IPAddress & ip) { return { ip }; };
inline ei_x &operator<<(ei_x &x, const _IPBinary __ip)
{
    return x.encode_binary(__ip.__ip.data(), 4);
};

struct _StringBinary { const String & __s; };
inline _StringBinary binary(const String & s) { return { s }; };
inline ei_x &operator<<(ei_x &x, const _StringBinary __b)
{
   return x.encode_binary(__b.__s);
};

inline ei_x &operator<<(ei_x &x, const Packet *p)
{
    return x.encode_binary(p->data(), p->length());
};

inline ei_x &operator<<(ei_x &x, const IPAddress &ip)
{
    return x.encode_ipaddress(ip);
};

inline ei_x &ok(ei_x &x)
{
    return x.encode_atom("ok");
};

inline ei_x &error(ei_x &x)
{
    return x.encode_atom("error");
};

inline ei_x &badarg(ei_x &x)
{
    return x.encode_atom("badarg");
};

CLICK_ENDDECLS

#endif
