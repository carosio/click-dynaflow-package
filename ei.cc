#include <click/config.h>
#include <click/string.hh>
#include <click/straccum.hh>
#include "ei.hh"

#define DEBUG

CLICK_DECLS

extern "C" {

void* ei_malloc (long size);

}

ei_x::ei_x(long size)
{
#define BLK_MAX 127

    size = (size + BLK_MAX) & ~BLK_MAX;

#undef BLK_MAX

    x.buff = (char *)ei_malloc(size);
    x.buffsz = size;
    x.index = 0;

    if (!x.buff)
	throw std::bad_alloc();
}

ei_x::~ei_x()
{
    ei_x_free(&x);
}

// EI_X decoder

String ei_x::decode_string()
{
    int type;
    int size;

    get_type(&type, &size);

    String str = String::make_uninitialized(size);
    // NB: mutable_c_str() creates space for the terminating null character
    if (char *s = str.mutable_c_str()) {
	if (::ei_decode_string(x.buff, &x.index, s) != 0)
	    throw ei_badarg();
    } else
	    throw std::bad_alloc();

    return str;
}

String ei_x::decode_binary_string()
{
    int type;
    int size;
    long bin_size;

    get_type(&type, &size);

    if (type != ERL_BINARY_EXT)
	throw ei_badarg();

    String str = String::make_uninitialized(size);
    // NB: mutable_c_str() creates space for the terminating null character
    if (char *s = str.mutable_c_str()) {
	if (::ei_decode_binary(x.buff, &x.index, s, &bin_size) != 0)
	    throw ei_badarg();
    } else
	throw std::bad_alloc();

    return str;
}
int ei_x::decode_tuple_header()
{
    int arity;

    if (::ei_decode_tuple_header(x.buff, &x.index, &arity) != 0)
	throw ei_badarg();

    return arity;
}

int ei_x::decode_list_header()
{
    int arity;

    if (::ei_decode_list_header(x.buff, &x.index, &arity) != 0)
	throw ei_badarg();

    return arity;
}

int ei_x::decode_version()
{
    int version;

    if (::ei_decode_version(x.buff, &x.index, &version) != 0)
	throw ei_badarg();

    return version;
}

erlang_ref ei_x::decode_ref()
{
    erlang_ref p;

    if (::ei_decode_ref(x.buff, &x.index, &p) != 0)
	throw ei_badarg();

    return p;
}

erlang_pid ei_x::decode_pid()
{
    erlang_pid p;

    if (::ei_decode_pid(x.buff, &x.index, &p) != 0)
	throw ei_badarg();

    return p;
}

unsigned long ei_x::decode_ulong()
{
    unsigned long p;

    if (::ei_decode_ulong(x.buff, &x.index, &p) != 0)
	throw ei_badarg();

    return p;
}

String ei_x::decode_atom()
{
    char atom[MAXATOMLEN+1] = {0};

    if (::ei_decode_atom(x.buff, &x.index, atom) != 0)
	throw ei_badarg();

    return atom;
}

void ei_x::decode_binary(void *b, int len)
{
    int type;
    int size;
    long bin_size;

    get_type(&type, &size);
    if (type != ERL_BINARY_EXT
	|| size != len
	|| ::ei_decode_binary(x.buff, &x.index, b, &bin_size) != 0
	|| bin_size != len)
	throw ei_badarg();
}

IPAddress ei_x::decode_ipaddress()
{
    uint32_t a = 0;

    if (decode_tuple_header() != 4)
	throw ei_badarg();

    for (int i = 0; i < 4; i++) {
	a = a << 8;
	a |= decode_ulong();
    }

    return IPAddress(htonl(a));
}

void ei_x::get_type(int *type, int *size)
{
    if (::ei_get_type(x.buff, &x.index, type, size) != 0)
	throw ei_badarg();
}

void ei_x::skip_term()
{
    if (::ei_skip_term(x.buff, &x.index) != 0)
	throw ei_badarg();
}

// EI_X encoder

ei_x & ei_x::encode_version()
{
    ei_x_encode_version(&x);
    return *this;
}

ei_x & ei_x::encode_tuple_header(int arity)
{
    ei_x_encode_tuple_header(&x, arity);
    return *this;
}

ei_x & ei_x::encode_list_header(int arity)
{
    ei_x_encode_list_header(&x, arity);
    return *this;
}

ei_x & ei_x::encode_empty_list()
{
    ei_x_encode_empty_list(&x);
    return *this;
}

ei_x & ei_x::encode_ref(const erlang_ref &ref)
{
    ei_x_encode_ref(&x, &ref);
    return *this;
}

ei_x & ei_x::encode_long(long p)
{
    ei_x_encode_long(&x, p);
    return *this;
}

ei_x & ei_x::encode_atom(const char *s)
{
    ei_x_encode_atom(&x, s);
    return *this;
}

ei_x & ei_x::encode_atom(const String &s)
{
    ei_x_encode_atom(&x, s.c_str());
    return *this;
}

ei_x & ei_x::encode_binary(const void *b, int len)
{
    ei_x_encode_binary(&x, b, len);
    return *this;
}

ei_x & ei_x::encode_binary(const String &s)
{
    ei_x_encode_binary(&x, s.c_str(), s.length());
    return *this;
}

ei_x & ei_x::encode_ipaddress(const IPAddress &ip)
{
    encode_tuple_header(4);
    encode_long(ip.data()[0]);
    encode_long(ip.data()[1]);
    encode_long(ip.data()[2]);
    encode_long(ip.data()[3]);
    return *this;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(ei_x)
