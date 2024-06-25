dnl OVS_CHECK_OVSP4RT - Process P4 options.

dnl Copyright(c) 2021-2024 Intel Corporation.
dnl SPDX-License-Identifier: Apache 2.0

AC_DEFUN([OVS_CHECK_OVSP4RT], [
  AC_ARG_WITH([ovsp4rt],
              [AC_HELP_STRING([--with-ovsp4rt@<:@=stubs@:>@],
              [Build with OVSP4RT support])],
              [have_ovsp4rt=true],
              [have_ovsp4rt=false])

  AC_ARG_WITH([p4ovs],
              [AC_HELP_STRING([--with-p4ovs],
              [Build with P4 support (legacy mode)])],
              [have_p4ovs=true],
              [have_p4ovs=false])

  if test $have_ovsp4rt = true; then
    if test "$with_ovsp4rt" = "stubs"; then
      PKG_CHECK_MODULES([ovsp4rtstubs], [libovsp4rt_stubs])
      OVSP4RT_CFLAGS=$ovsp4rtstubs_CFLAGS
      OVSP4RT_LIBS=$ovsp4rtstubs_LIBS
    else
      PKG_CHECK_MODULES([ovsp4rt], [libovsp4rt])
      OVSP4RT_CFLAGS=$ovsp4rt_CFLAGS
      OVSP4RT_LIBS=$ovsp4rt_LIBS
    fi
    have_p4ovs=true
  fi

  legacy_p4ovs=$have_p4ovs
  if test $have_ovsp4rt = true; then
    legacy_p4ovs=false
  fi

  dnl config.h variable
  if test $have_p4ovs = true; then
    AC_DEFINE([P4OVS], [1], [System includes P4 support.])
  fi

  dnl export autoconf variables
  AC_SUBST([OVSP4RT_CFLAGS])
  AC_SUBST([OVSP4RT_LIBS])

  dnl export automake conditionals
  AM_CONDITIONAL([P4OVS], test $have_p4ovs = true)
  AM_CONDITIONAL([OVSP4RT], test $have_ovsp4rt = true)
  AM_CONDITIONAL([LEGACY_P4OVS], test $legacy_p4ovs = true)
])
