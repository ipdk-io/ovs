AM_CPPFLAGS += -I /usr/include/libnl3
AM_CPPFLAGS += -I $(top_srcdir)/p4proto/kctrl/switchlink/xxhash/include
AM_CPPFLAGS += -I $(top_srcdir)/p4proto/kctrl/switchlink/submodules/SAI/inc

AM_CFLAGS += -I /usr/include/libnl3
AM_CFLAGS += -I $(top_srcdir)/p4proto/kctrl/switchlink/xxhash/include
AM_CFLAGS += -I $(top_srcdir)/p4proto/kctrl/switchlink/submodules/SAI/inc

EXTRA_DIST += $(top_srcdir)/p4proto/kctrl/switchlink/submodules/SAI
EXTRA_DIST += p4proto/kctrl/switchsai

NETLINK_LIBS = -lnl-route-3 -lnl-3

lib_LTLIBRARIES += p4proto/kctrl/switchlink/libswitchlink.la

p4proto_kctrl_switchlink_libswitchlink_la_LDFLAGS = \
        $(OVS_LTINFO) \
        -Wl,--version-script=$(top_builddir)/p4proto/kctrl/switchlink/libswitchlink.sym \
        $(AM_LDFLAGS)

p4proto_kctrl_switchlink_libswitchlink_la_SOURCES = \
   p4proto/kctrl/switchlink/switchlink_address.c \
   p4proto/kctrl/switchlink/switchlink_db.c \
   p4proto/kctrl/switchlink/switchlink_db.h \
   p4proto/kctrl/switchlink/switchlink_db_int.h \
   p4proto/kctrl/switchlink/switchlink.h \
   p4proto/kctrl/switchlink/switchlink_int.h \
   p4proto/kctrl/switchlink/switchlink_link.c \
   p4proto/kctrl/switchlink/switchlink_link.h \
   p4proto/kctrl/switchlink/switchlink_main.c \
   p4proto/kctrl/switchlink/switchlink_neigh.c \
   p4proto/kctrl/switchlink/switchlink_neigh.h \
   p4proto/kctrl/switchlink/switchlink_route.c \
   p4proto/kctrl/switchlink/switchlink_route.h \
   p4proto/kctrl/switchlink/switchlink_sai.c \
   p4proto/kctrl/switchlink/switchlink_sai.h \
   p4proto/kctrl/switchlink/xxhash/include/xxhash.h \
   p4proto/kctrl/switchlink/xxhash/src/xxhash.c

p4proto_kctrl_switchlink_libswitchlink_la_CPPFLAGS = $(AM_CPPFLAGS)
p4proto_kctrl_switchlink_libswitchlink_la_CPPFLAGS += -DHAVE_NLA_BITFIELD32
p4proto_kctrl_switchlink_libswitchlink_la_CPPFLAGS += -I p4proto/kctrl/switchsai

p4proto_kctrl_switchlink_libswitchlink_la_CFLAGS = $(AM_CFLAGS)
p4proto_kctrl_switchlink_libswitchlink_la_CFLAGS += -DHAVE_NLA_BITFIELD32
p4proto_kctrl_switchlink_libswitchlink_la_CFLAGS += -I p4proto/kctrl/switchsai

p4proto_kctrl_switchlink_libswitchlink_la_LIBADD = $(NETLINK_LIBS)
p4proto_kctrl_switchlink_libswitchlink_la_LIBADD += p4proto/kctrl/switchsai/libswitchsai.la

pkgconfig_DATA += \
    p4proto/kctrl/switchlink/libswitchlink.pc

CLEANFILES += p4proto/kctrl/switchlink/libswitchlink.sym
CLEANFILES += p4proto/kctrl/switchlink/libswitchlink.pc
