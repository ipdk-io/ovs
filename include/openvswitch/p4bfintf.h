/*
 * Copyright (c) 2021-2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Public interface to the bfintf module.
 */

#ifndef OPENVSWITCH_P4BFINTF_H
#define OPENVSWITCH_P4BFINTF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BF_PORT_NAME_LEN 64
#define BF_MAC_STRING_LEN 32

typedef enum port_type_t {
    TAP_PORT,
    LINK_PORT,
    SOURCE_PORT,
    SINK_PORT,
    ETHER_PORT,
    VIRTUAL_PORT
} port_type_t;

typedef struct port_properties_t {
    /** Port name. */
    char port_name[BF_PORT_NAME_LEN];

    /** MAC address in string format. */
    char mac_in_use[BF_MAC_STRING_LEN];

    //! @todo What distinguishes this port ID from the others?
    uint32_t port_id;

    /** Port ID for pipeline in input direction. */
    uint32_t port_in_id;

    /** Port ID for pipeline in output direction. */
    uint32_t port_out_id;

    /** Port type. */
    port_type_t port_type;
} port_properties_t;

int bf_p4_add_port(uint64_t device, int64_t port,
                   const port_properties_t *port_props);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OPENVSWITCH_P4BFINTF_H
