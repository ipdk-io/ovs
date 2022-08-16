/*
 * Copyright (c) 2021-2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Defines the public interface to an externally-supplied module
 * that permits OvS to communicate with the P4 control plane.
 */

#ifndef OPENVSWITCH_OVS_P4RT_H
#define OPENVSWITCH_OVS_P4RT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define OVS_P4_PORT_NAME_LEN 64
#define OVS_P4_MAC_STRING_LEN 32

enum ovs_p4_port_type {
    TAP_PORT,
    LINK_PORT,
    SOURCE_PORT,
    SINK_PORT,
    ETHER_PORT,
    VIRTUAL_PORT
};

struct ovs_p4_port_properties {
    /** Port name. */
    char port_name[OVS_P4_PORT_NAME_LEN];

    /** MAC address in string format. */
    char mac_in_use[OVS_P4_MAC_STRING_LEN];

    //! @todo What distinguishes this port ID from the others?
    uint32_t port_id;

    /** Port ID for pipeline in input direction. */
    uint32_t port_in_id;

    /** Port ID for pipeline in output direction. */
    uint32_t port_out_id;

    /** Port type. */
    enum ovs_p4_port_type port_type;
};

int ovs_p4_add_port(uint64_t device, int64_t port,
                    const struct ovs_p4_port_properties *port_props);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OPENVSWITCH_OVS_P4RT_H
