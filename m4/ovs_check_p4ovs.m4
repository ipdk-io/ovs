dnl OVS_CHECK_P4OVS - Process P4 options.

dnl Copyright(c) 2021-2022 Intel Corporation.
dnl SPDX-License-Identifier: Apache 2.0

AC_DEFUN([OVS_CHECK_P4OVS], [
  AC_ARG_WITH([p4ovs],
              [AC_HELP_STRING([--with-p4ovs], [Build for P4])],
              [have_p4ovs=true])
  AC_ARG_WITH([tofino],
              [AC_HELP_STRING([--with-tofino], [Build for Tofino target])],
              [with_tofino=yes])
  AC_ARG_WITH([sai],
              [AC_HELP_STRING([--with-sai], [Build for P4 with SAI])],
              [have_sai=true])
  AC_MSG_CHECKING([whether P4OVS is enabled])
  if test "$have_p4ovs" != true || test "$with_p4ovs" = no; then
    AC_MSG_RESULT([no])
    P4OVS_VALID=false
  else
    AC_MSG_RESULT([yes])
	P4OVS_VALID=true
    AC_DEFINE([P4OVS], [1], [System uses the P4PROTO module.])
    if test "$have_sai" = true; then
        AC_DEFINE([P4SAI], [1], [System uses the SWITCHLINK and SWITCHSAI modules.])
    fi
    if test "$with_tofino" = true; then
        AC_DEFINE([P4TOFINO], [1], [System is being built for Tofino target])
    fi
  fi
  dnl export automake conditionals
  AM_CONDITIONAL([P4OVS], test "$P4OVS_VALID" = true)
  AM_CONDITIONAL([P4SAI], test "$P4OVS_VALID" = true && test "$have_sai" = true)
  AM_CONDITIONAL([P4TOFINO], test "$P4OVS_VALID" = true && test "$with_tofino" = yes)
])
