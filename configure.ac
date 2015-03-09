dnl -*- mode: shell-script -*-
dnl 'configure.ac' file for the DynaFlow package
dnl Process this file with autoconf to produce a configure script.
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a
dnl copy of this software and associated documentation files (the "Software"),
dnl to deal in the Software without restriction, subject to the conditions
dnl listed in the Click LICENSE file. These conditions include: you must
dnl preserve this copyright notice, and you cannot mention the copyright
dnl holders in advertising related to the Software without their permission.
dnl The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
dnl notice is a summary of the Click LICENSE file; the license in that file is
dnl legally binding.

AC_INIT(click-dynaflow-package, 0.0.1)
AC_SUBST(ac_configure_args)

CLICKPACKAGENAME=dynaflow
AC_SUBST(CLICKPACKAGENAME)


dnl
dnl locate Click install directory
dnl

AC_ARG_WITH(click, [  --with-click[[=DIR]]      Click install path is DIR [[PREFIX]]],
    [clickprefix=$withval; if test -z "$withval" -o "$withval" = yes; then clickprefix=$prefix; fi],
    clickprefix=NONE)

dnl Preset $prefix and $exec_prefix.
test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
test "x$clickprefix" != xNONE -a "x$prefix" = xNONE && prefix="$clickprefix"
test "x$clickprefix" = xNONE && clickprefix="$prefix"

clickdatadir="${clickprefix}/share/click"

if test ! -r $clickdatadir/config.mk; then
    AC_MSG_ERROR([
==============================================

'$clickdatadir/config.mk' not found.
Are you sure '$clickprefix' is a Click install directory?

==============================================])
fi

clickbuild_prefix="$clickprefix"
. $clickdatadir/config.mk

AC_SUBST(clickbuild_prefix)
AC_SUBST(clickbuild_datadir)


dnl
dnl build userlevel? Linux module? FreeBSD module?
dnl

onezero () {
    { [[ "$1" = yes ]] && echo 1; } || echo 0
}

AC_ARG_ENABLE(userlevel, [  --enable-userlevel      enable user-level driver],
    [BUILD_USERLEVEL=`onezero $enableval`],
    [BUILD_USERLEVEL=$CLICK_HAVE_USERLEVEL_DRIVER])
AC_ARG_ENABLE(linuxmodule, [  --enable-linuxmodule    enable Linux kernel driver],
    [BUILD_LINUXMODULE=`onezero $enableval`],
    [BUILD_LINUXMODULE=$CLICK_HAVE_LINUXMODULE_DRIVER])
AC_ARG_ENABLE(bsdmodule, [  --enable-bsdmodule      enable FreeBSD kernel driver
                          [[Default drivers are those Click supports.]]],
    [BUILD_BSDMODULE=`onezero $enableval`],
    [BUILD_BSDMODULE=$CLICK_HAVE_BSDMODULE_DRIVER])

AC_SUBST(BUILD_USERLEVEL)
AC_SUBST(BUILD_LINUXMODULE)
AC_SUBST(BUILD_BSDMODULE)

AS_IF([test "x$BUILD_LINUXMODULE" == "xyes"],
    [AC_MSG_ERROR([Linux kernel module is incompatible with Erlang Node communication.])])

ERL_CRASH_DUMP=/dev/null
EIBASE="`erl -noshell -noinput -eval 'io:format (\"~s\", [[ code:lib_dir (erl_interface) ]])' -s erlang halt`"
AC_SUBST([EIBASE])

ERTSBASE="`erl -noshell -noinput -eval 'io:format (\"~s\", [[ \"/\" ++ filename:join (lists:reverse ([ \"erts-\" ++ erlang:system_info (version) | tl (lists:reverse (string:tokens (code:lib_dir (), \"/\"))) ])) ]]).' -s erlang halt `"
AC_SUBST([ERTSBASE])

ERL_INCLUDES="-I${EIBASE}/include -I${ERTSBASE}/include"
ERL_LDFLAGS="-L${EIBASE}/lib -L${ERTSBASE}/lib"

AC_CHECK_HEADER([ei.h], [], [AC_MSG_ERROR([can\'t find erl_interface.h; under debian you need to install erlang-src])], [])
AC_CHECK_LIB([ei], [ei_set_compat_rel], [], [AC_MSG_ERROR([can't find libei])], [])
dnl AC_CHECK_LIB([erl_interface], [erl_compare_ext], [], [AC_MSG_ERROR([can't find liberl_interface])], [])

AC_SUBST([ERL_INCLUDES])
AC_SUBST([ERL_LDFLAGS])

dnl
dnl Output
dnl

AC_OUTPUT(Makefile)
