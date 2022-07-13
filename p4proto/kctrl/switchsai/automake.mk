AM_CPPFLAGS += -I $(top_srcdir)/p4proto/kctrl/switchlink/submodules/SAI/inc
AM_CFLAGS += -I $(top_srcdir)/p4proto/kctrl/switchlink/submodules/SAI/inc

EXTRA_DIST += $(top_srcdir)/p4proto/kctrl/switchlink/submodules/SAI
EXTRA_DIST += p4proto/kctrl/switchapi

lib_LTLIBRARIES += p4proto/kctrl/switchsai/libswitchsai.la

p4proto_kctrl_switchsai_libswitchsai_la_LDFLAGS = \
        $(OVS_LTINFO) \
        -Wl,--version-script=$(top_builddir)/p4proto/kctrl/switchsai/libswitchsai.sym \
        $(AM_LDFLAGS)

p4proto_kctrl_switchsai_libswitchsai_la_SOURCES = \
   p4proto/kctrl/switchsai/sai.c \
   p4proto/kctrl/switchsai/saiinternal.h \
   p4proto/kctrl/switchsai/saiport.c \
   p4proto/kctrl/switchsai/saifdb.c \
   p4proto/kctrl/switchsai/saineighbor.c \
   p4proto/kctrl/switchsai/sainexthop.c \
   p4proto/kctrl/switchsai/sairoute.c \
   p4proto/kctrl/switchsai/sairouterinterface.c \
   p4proto/kctrl/switchsai/saitunnel.c \
   p4proto/kctrl/switchsai/saiutils.c \
   p4proto/kctrl/switchsai/saivirtualrouter.c \
   p4proto/kctrl/switchsai/sainexthopgroup.c

p4proto_kctrl_switchsai_libswitchsai_la_CPPFLAGS = $(AM_CPPFLAGS)
p4proto_kctrl_switchsai_libswitchsai_la_CPPFLAGS += -I ./switchapi

p4proto_kctrl_switchsai_libswitchsai_la_CFLAGS = $(AM_CFLAGS)
p4proto_kctrl_switchsai_libswitchsai_la_CFLAGS += -I ./switchapi

p4proto_kctrl_switchsai_libswitchsai_la_LIBADD = p4proto/kctrl/switchapi/libswitchapi.la

pkgconfig_DATA += \
    p4proto/kctrl/switchsai/libswitchsai.pc

CLEANFILES += p4proto/kctrl/switchsai/libswitchsai.sym
CLEANFILES += p4proto/kctrl/switchsai/libswitchsai.pc
