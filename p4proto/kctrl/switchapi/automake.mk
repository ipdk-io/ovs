AM_CPPFLAGS += -I $(top_srcdir)/p4proto/kctrl/switchlink/submodules/SAI/inc
AM_CFLAGS += -I $(top_srcdir)/p4proto/kctrl/switchlink/submodules/SAI/inc

EXTRA_DIST += $(top_srcdir)/p4proto/kctrl/switchlink/submodules/SAI

lib_LTLIBRARIES += p4proto/kctrl/switchapi/libswitchapi.la

p4proto_kctrl_switchapi_libswitchapi_la_LDFLAGS = \
        $(OVS_LTINFO) \
        -Wl,--version-script=$(top_builddir)/p4proto/kctrl/switchapi/libswitchapi.sym \
        $(AM_LDFLAGS)

p4proto_kctrl_switchapi_libswitchapi_la_SOURCES = \
    p4proto/kctrl/switchapi/switch_base_types.h \
    p4proto/kctrl/switchapi/switch_internal.h \
    p4proto/kctrl/switchapi/switch_interface.h \
    p4proto/kctrl/switchapi/switch_types_int.h \
    p4proto/kctrl/switchapi/switch_port_int.h \
    p4proto/kctrl/switchapi/switch_status.h \
    p4proto/kctrl/switchapi/switch_id.h \
    p4proto/kctrl/switchapi/switch_id.c \
    p4proto/kctrl/switchapi/switch_table.h \
    p4proto/kctrl/switchapi/switch_table.c \
    p4proto/kctrl/switchapi/switch_config_int.h \
    p4proto/kctrl/switchapi/switch_config.h \
    p4proto/kctrl/switchapi/switch_config.c \
    p4proto/kctrl/switchapi/switch_device_int.h \
    p4proto/kctrl/switchapi/switch_device.h \
    p4proto/kctrl/switchapi/switch_device.c \
    p4proto/kctrl/switchapi/switch_handle_int.h \
    p4proto/kctrl/switchapi/switch_handle.h \
    p4proto/kctrl/switchapi/switch_handle.c \
    p4proto/kctrl/switchapi/switch_utils.c \
    p4proto/kctrl/switchapi/switch_port.h \
    p4proto/kctrl/switchapi/switch_port.c \
    p4proto/kctrl/switchapi/switch_pd_port.c \
    p4proto/kctrl/switchapi/switch_tunnel_int.h \
    p4proto/kctrl/switchapi/switch_tunnel.h \
    p4proto/kctrl/switchapi/switch_tunnel.c \
    p4proto/kctrl/switchapi/switch_pd_tunnel.c \
    p4proto/kctrl/switchapi/switch_rif.c \
    p4proto/kctrl/switchapi/switch_rif.h \
    p4proto/kctrl/switchapi/switch_rif_int.h \
    p4proto/kctrl/switchapi/switch_rmac.c \
    p4proto/kctrl/switchapi/switch_rmac_int.h \
    p4proto/kctrl/switchapi/switch_rmac.h \
    p4proto/kctrl/switchapi/switch_neighbor.c \
    p4proto/kctrl/switchapi/switch_neighbor.h \
    p4proto/kctrl/switchapi/switch_neighbor_int.h \
    p4proto/kctrl/switchapi/switch_nhop.c \
    p4proto/kctrl/switchapi/switch_nhop_int.h \
    p4proto/kctrl/switchapi/switch_nhop.h \
    p4proto/kctrl/switchapi/switch_pd_routing.c \
    p4proto/kctrl/switchapi/switch_pd_routing.h \
    p4proto/kctrl/switchapi/switch_fdb.h \
    p4proto/kctrl/switchapi/switch_fdb.c \
    p4proto/kctrl/switchapi/switch_pd_fdb.c \
    p4proto/kctrl/switchapi/switch_l3.c \
    p4proto/kctrl/switchapi/switch_l3.h \
    p4proto/kctrl/switchapi/switch_l3_int.h \
    p4proto/kctrl/switchapi/switch_vrf.c \
    p4proto/kctrl/switchapi/switch_vrf.h \
    p4proto/kctrl/switchapi/switch_pd_utils.c \
    p4proto/kctrl/switchapi/switch_pd_utils.h \
    p4proto/kctrl/switchapi/switch_pd_p4_name_mapping.h


p4proto_kctrl_switchapi_libswitchapi_la_CPPFLAGS = $(AM_CPPFLAGS)
p4proto_kctrl_switchapi_libswitchapi_la_CFLAGS = $(AM_CFLAGS)

pkgconfig_DATA += \
    p4proto/kctrl/switchapi/libswitchapi.pc

CLEANFILES += p4proto/kctrl/switchapi/libswitchapi.sym
CLEANFILES += p4proto/kctrl/switchapi/libswitchapi.pc
