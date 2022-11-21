/*
 * Copyright (c) 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Defines the public interface to an externally-supplied module
 * that permits OvS to communicate with the P4 control plane.
 */

#ifndef OPENVSWITCH_OVS_P4RT_H
#define OPENVSWITCH_OVS_P4RT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct p4_ipaddr {
  uint8_t family;
  uint8_t prefix_len;
  uint32_t v4addr;
};

struct tunnel_info {
    uint32_t ifindex;
    uint32_t port_id;
    struct p4_ipaddr local_ip;
    struct p4_ipaddr remote_ip;
    uint16_t dst_port;
    uint16_t vni;
};

struct vlan_info {
    uint32_t vlan_id;
};

struct mac_learning_info {
    bool is_tunnel;
    bool is_vlan;
    uint8_t mac_addr[6];
    union {
        struct tunnel_info tnl_info;
        struct vlan_info vln_info;
    };
};

// Function declarations
extern void ConfigFdbTableEntry(struct mac_learning_info learn_info,
                                bool insert_entry);
extern void ConfigTunnelTableEntry(struct tunnel_info tunnel_info,
                                   bool insert_entry);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OPENVSWITCH_OVS_P4RT_H

