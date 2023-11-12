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

#define VPORT_ID_OFFSET 16
#define MAX_P4_BRIDGE_ID 256
#define P4_VXLAN_SOURCE_PORT_OFFSET 2048

/* This is a replica of port_vlan_mode in ofproto.h */
enum p4_vlan_mode {
  P4_PORT_VLAN_ACCESS,
  P4_PORT_VLAN_TRUNK,
  P4_PORT_VLAN_NATIVE_TAGGED,
  P4_PORT_VLAN_NATIVE_UNTAGGED,
  P4_PORT_VLAN_DOT1Q_TUNNEL,
  P4_PORT_VLAN_UNSUPPORTED
};

struct p4_ipaddr {
  uint8_t family;
  uint8_t prefix_len;
  union {
    struct in_addr v4addr;
    struct in6_addr v6addr;
  } ip;
};

struct port_vlan_info {
  enum p4_vlan_mode port_vlan_mode;
  int port_vlan;
};

struct tunnel_info {
    uint32_t ifindex;
    uint32_t port_id;
    uint32_t src_port;
    struct p4_ipaddr local_ip;
    struct p4_ipaddr remote_ip;
    uint16_t dst_port;
    uint16_t vni;
    struct port_vlan_info vlan_info;
    uint8_t bridge_id;
};

struct src_port_info {
    uint8_t bridge_id;
    uint16_t vlan_id;
    uint32_t src_port;
};

struct vlan_info {
    uint32_t vlan_id;
};

struct mac_learning_info {
    bool is_tunnel;
    bool is_vlan;
    uint8_t mac_addr[6];
    uint8_t bridge_id;
    uint32_t src_port;
    struct port_vlan_info vlan_info;
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
extern void ConfigTunnelSrcPortTableEntry(struct src_port_info tnl_sp,
                                          bool insert_entry);
extern void ConfigSrcPortTableEntry(struct src_port_info vsi_sp,
                                    bool insert_entry);
extern void ConfigVlanTableEntry(uint16_t vlan_id,
                                 bool insert_entry);
extern void ConfigIpTunnelTermTableEntry(struct tunnel_info tunnel_info,
                                         bool insert_entry);
extern void ConfigRxTunnelSrcTableEntry(struct tunnel_info tunnel_info,
                                        bool insert_entry);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // OPENVSWITCH_OVS_P4RT_H

