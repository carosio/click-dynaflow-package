#ifndef EI_HH
#define EI_HH

#include <click/ipaddress.hh>
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
};

CLICK_ENDDECLS

#endif
