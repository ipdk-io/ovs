dnl OVS_CHECK_P4OVS - Process P4 options.

dnl Copyright(c) 2021-2022 Intel Corporation.
dnl SPDX-License-Identifier: Apache 2.0

AC_DEFUN([OVS_CHECK_P4OVS], [
  AC_ARG_WITH([p4ovs],
              [AC_HELP_STRING([--with-p4ovs], [Build with P4 support])],
              [have_p4ovs=true])
  AC_MSG_CHECKING([whether P4OVS is enabled])
  if test "$have_p4ovs" != true || test "$with_p4ovs" = no; then
    AC_MSG_RESULT([no])
    P4OVS_VALID=false
  else
    AC_MSG_RESULT([yes])
    P4OVS_VALID=true
    AC_DEFINE([P4OVS], [1], [System includes P4 support.])
  fi
  dnl export automake conditional
  AM_CONDITIONAL([P4OVS], test "$P4OVS_VALID" = true)
])
