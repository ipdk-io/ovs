lib_LTLIBRARIES += p4proto/libp4proto.la

p4proto_libp4proto_la_LDFLAGS = \
        $(OVS_LTINFO) \
        -Wl,--version-script=$(top_builddir)/p4proto/libp4proto.sym \
        $(AM_LDFLAGS)
#if P4SAI
EXTRA_DIST += p4proto/kctrl/switchlink
EXTRA_DIST += p4proto/p4src/linux_networking/headers.p4
EXTRA_DIST += p4proto/p4src/linux_networking/linux_networking.p4
EXTRA_DIST += p4proto/p4src/linux_networking/metadata.p4
EXTRA_DIST += p4proto/p4src/linux_networking/parser.p4
EXTRA_DIST += p4proto/p4src/linux_networking/routing.p4
EXTRA_DIST += p4proto/p4src/linux_networking/tunnel.p4
EXTRA_DIST += p4proto/p4src/linux_networking/lnw_ct.p4
EXTRA_DIST += p4proto/p4src/linux_networking/lnw.conf
EXTRA_DIST += p4proto/kctrl/README.md
EXTRA_DIST += p4proto/kctrl/switchapi/README.md
EXTRA_DIST += p4proto/kctrl/switchlink/README.md
EXTRA_DIST += p4proto/kctrl/switchsai/README.md
EXTRA_DIST += p4proto/p4src/linux_networking/README.md
EXTRA_DIST += p4proto/p4src/linux_networking/topology.PNG
#endif

p4proto_libp4proto_la_SOURCES = \
    p4proto/p4proto-provider.h \
    p4proto/p4proto.c \
    p4proto/p4proto.h

p4proto_libp4proto_la_CPPFLAGS = $(AM_CPPFLAGS)
p4proto_libp4proto_la_CPPFLAGS += -I /usr/include/libnl3
#if P4SAI
p4proto_libp4proto_la_CPPFLAGS += -I p4proto/kctrl/switchlink
#endif

p4proto_libp4proto_la_CFLAGS = $(AM_CFLAGS)

p4proto_libp4proto_la_LIBADD = p4proto/p4rt/libp4rt.la
p4proto_libp4proto_la_LIBADD += p4proto/bfIntf/libbfIntf.la

#if P4SAI
p4proto_libp4proto_la_LIBADD += p4proto/kctrl/switchlink/libswitchlink.la
#endif

p4proto_libp4proto_la_LIBADD += -lstdc++

pkgconfig_DATA += \
	p4proto/libp4proto.pc

CLEANFILES += p4proto/libp4proto.sym
CLEANFILES += p4proto/libp4proto.pc
