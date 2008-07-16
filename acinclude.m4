dnl AS_AC_EXPAND(VAR, CONFIGURE_VAR)
dnl
dnl example
dnl AS_AC_EXPAND(SYSCONFDIR, $sysconfdir)
dnl will set SYSCONFDIR to /usr/local/etc if prefix=/usr/local

AC_DEFUN([AS_AC_EXPAND],
[
    EXP_VAR=[$1]
    FROM_VAR=[$2]

    dnl first expand prefix and exec_prefix if necessary
    prefix_save=$prefix
    exec_prefix_save=$exec_prefix

    dnl if no prefix given, then use /usr/local, the default prefix
    if test "x$prefix" = "xNONE"; then
        prefix=$ac_default_prefix
    fi
    dnl if no exec_prefix given, then use prefix
    if test "x$exec_prefix" = "xNONE"; then
        exec_prefix=$prefix
    fi

    full_var="$FROM_VAR"
    dnl loop until it doesn't change anymore
    while true; do
        new_full_var="`eval echo $full_var`"
        if test "x$new_full_var"="x$full_var"; then break; fi
        full_var=$new_full_var
    done

    dnl clean up
    full_var=$new_full_var
    AC_SUBST([$1], "$full_var")

    dnl restore prefix and exec_prefix
    prefix=$prefix_save
    exec_prefix=$exec_prefix_save
])

# vala.m4 serial 1 (vala @VERSION@)
dnl Autoconf scripts for the Vala compiler
dnl Copyright (C) 2007  Mathias Hasselmann
dnl
dnl This library is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU Lesser General Public
dnl License as published by the Free Software Foundation; either
dnl version 2 of the License, or (at your option) any later version.

dnl This library is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl Lesser General Public License for more details.

dnl You should have received a copy of the GNU Lesser General Public
dnl License along with this library; if not, write to the Free Software
dnl Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
dnl
dnl Author:
dnl 	Mathias Hasselmann <mathias.hasselmann@gmx.de>
dnl --------------------------------------------------------------------------

dnl VALA_PROG_VALAC([MINIMUM-VERSION])
dnl
dnl Check whether the Vala compiler exists in `PATH'. If it is found the
dnl variable VALAC is set. Optionally a minimum release number of the compiler
dnl can be requested.
dnl --------------------------------------------------------------------------
AC_DEFUN([VALA_PROG_VALAC],[
	AC_PATH_PROG([VALAC], [valac], [])

	if test -z "${VALAC}"; then
		AC_MSG_WARN([No Vala compiler found. You will not be able to recompile .vala source files.])
	elif test -n "$1"; then
		AC_REQUIRE([AC_PROG_AWK])
		AC_MSG_CHECKING([valac is at least version $1])

		# 7
		if "${VALAC}" --version | "${AWK}" -v r='$1' 'function vn(s) [{ if (3 == split(s,v,".")) return (v[1]*1000+v[2])*1000+v[3]; else exit 2; }] /^Vala / { exit vn(r) > vn([$]2) }'; then
			AC_MSG_RESULT([yes])
			AC_SUBST(VALAC)
		else
			AC_MSG_RESULT([no])
			unset VALAC
		fi
	fi
])
