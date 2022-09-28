/*
 * Copyright (c) 2013-2021 Barefoot Networks, Inc.
 * Copyright (c) 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <config.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sai.h>
#include "util.h"
#include "switchlink_sai.h"
#include <linux/if_ether.h>
#include "openvswitch/vlog.h"
#include "saiinternal.h"

VLOG_DEFINE_THIS_MODULE(switchlink_sai);

extern sai_status_t sai_initialize(void);

static sai_port_api_t *port_api = NULL;
static sai_virtual_router_api_t *vrf_api = NULL;
static sai_fdb_api_t *fdb_api = NULL;
static sai_router_interface_api_t *rintf_api = NULL;
static sai_neighbor_api_t *neigh_api = NULL;
static sai_next_hop_api_t *nhop_api = NULL;
static sai_next_hop_group_api_t *nhop_group_api = NULL;
static sai_route_api_t *route_api = NULL;
static sai_hostif_api_t *host_intf_api = NULL;
static sai_tunnel_api_t *tunnel_api = NULL;

// This object ID is not used.
// Introduced this variable to be inline with submodules/SAI declarations
//static sai_object_id_t obj_id = 0;

static inline uint32_t ipv4_prefix_len_to_mask(uint32_t prefix_len) {
  return (prefix_len ? (((uint32_t)0xFFFFFFFF) << (32 - prefix_len)) : 0);
}

static inline struct in6_addr ipv6_prefix_len_to_mask(uint32_t prefix_len) {
  struct in6_addr mask;
  memset(&mask, 0, sizeof(mask));
  ovs_assert(prefix_len <= 128);

  int i;
  for (i = 0; i < 4; i++) {
    if (prefix_len > 32) {
      mask.s6_addr32[i] = 0xFFFFFFFF;
    } else {
      mask.s6_addr32[i] = htonl(ipv4_prefix_len_to_mask(prefix_len));
      break;
    }
    prefix_len -= 32;
  }
  return mask;
}

/*
 * Routine Description:
 *    Remove FDB entry
 *
 * Arguments:
 *    [in] fdb_entry - fdb entry
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */

int switchlink_vrf_create(switchlink_handle_t *vrf_h) {
  sai_status_t status = SAI_STATUS_SUCCESS;
  sai_attribute_t attr_list[2];

  memset(attr_list, 0, sizeof(attr_list));
  attr_list[0].id = SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE;
  attr_list[0].value.booldata = true;
  attr_list[1].id = SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V6_STATE;
  attr_list[1].value.booldata = true;

  status = vrf_api->create_virtual_router(vrf_h, 0, 2, attr_list);
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Create router interface
 *
 * Arguments:
 *    [in] intf - router interface info
 *    [out] intf_h - router interface handle
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_interface_create(switchlink_db_interface_info_t *intf,
                                switchlink_handle_t *intf_h) {
  sai_status_t status = SAI_STATUS_SUCCESS;

  if (intf->intf_type == SWITCHLINK_INTF_TYPE_L3) {
    sai_attribute_t attr_list[10];
    int ac = 0;
    memset(attr_list, 0, sizeof(attr_list));
    attr_list[ac].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
    attr_list[ac].value.oid = intf->vrf_h;
    ac++;
    attr_list[ac].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    attr_list[ac].value.oid = 0;
    if (intf_h) {
        attr_list[ac].value.oid = *intf_h;
    }
    ac++;
    attr_list[ac].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
    memcpy(attr_list[ac].value.mac, intf->mac_addr, sizeof(sai_mac_t));
    ac++;
    attr_list[ac].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    attr_list[ac].value.u32 = intf->ifindex;
    ac++;

    status =
        rintf_api->create_router_interface(intf_h, 0, ac++, attr_list);
  }
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Remove router interface
 *
 * Arguments:
 *    [in] intf - router interface info
 *    [in] intf_h - router interface handle
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_interface_delete(switchlink_db_interface_info_t *intf,
                                switchlink_handle_t intf_h) {
  sai_status_t status = SAI_STATUS_SUCCESS;
  if (intf->intf_type == SWITCHLINK_INTF_TYPE_L3) {
    status = rintf_api->remove_router_interface(intf_h);
  }
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Create tunnel
 *
 * Arguments:
 *    [in] tnl_intf - tunnel interface info
 *    [in] tnl_intf_h - tunnel interface handle
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */

sai_status_t switchlink_create_tunnel(
          switchlink_db_tunnel_interface_info_t *tnl_intf,
          switchlink_handle_t *tnl_intf_h) {
    sai_attribute_t attr_list[10];
    int ac = 0;
    memset(attr_list, 0, sizeof(attr_list));


    // TODO looks like remote is valid only for PEER_MODE = P2P
    if (tnl_intf->src_ip.family == AF_INET) {
        attr_list[ac].id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
        attr_list[ac].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        attr_list[ac].value.ipaddr.addr.ip4 =
                        htonl(tnl_intf->src_ip.ip.v4addr.s_addr);
        ac++;
    }

    // TODO looks like remote is valid only for PEER_MODE = P2P
    if (tnl_intf->dst_ip.family == AF_INET) {
        attr_list[ac].id = SAI_TUNNEL_ATTR_ENCAP_DST_IP;
        attr_list[ac].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        attr_list[ac].value.ipaddr.addr.ip4 =
                        htonl(tnl_intf->dst_ip.ip.v4addr.s_addr);
        ac++;
    }

    attr_list[ac].id = SAI_TUNNEL_ATTR_VXLAN_UDP_SPORT;
    attr_list[ac].value.u16 = tnl_intf->dst_port;
    ac++;

    return tunnel_api->create_tunnel(tnl_intf_h, 0, ac, attr_list);
}

/*
 * Routine Description:
 *    Create tunnel termination table entry
 *
 * Arguments:
 *    [in] tnl_intf - tunnel term interface info
 *    [in] tnl_term_intf_h - tunnel term interface handle
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */

sai_status_t switchlink_create_term_table_entry(
                            switchlink_db_tunnel_interface_info_t *tnl_intf,
                            switchlink_handle_t *tnl_term_intf_h) {
    sai_attribute_t attr_list[10];
    memset(attr_list, 0, sizeof(attr_list));
    int ac = 0;

    if (tnl_intf->dst_ip.family == AF_INET) {
        attr_list[ac].id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP;
        attr_list[ac].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        attr_list[ac].value.ipaddr.addr.ip4 =
                        htonl(tnl_intf->dst_ip.ip.v4addr.s_addr);
        ac++;
    }

    if (tnl_intf->src_ip.family == AF_INET) {
        attr_list[ac].id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP;
        attr_list[ac].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        attr_list[ac].value.ipaddr.addr.ip4 =
                        htonl(tnl_intf->src_ip.ip.v4addr.s_addr);
        ac++;
    }

    attr_list[ac].id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID;
    attr_list[ac].value.u32 = tnl_intf->vni_id;
    ac++;

    return tunnel_api->create_tunnel_term_table_entry(tnl_term_intf_h, 0, ac,
                                                      attr_list);
}

/*
 * Routine Description:
 *    Wrapper function to create tunnel interface and create tunnel 
 *    term table entry
 *
 * Arguments:
 *    [in] tnl_intf - tunnel interface info
 *    [in] tnl_intf_h - tunnel interface handle
 *    [in] tnl_term_h - tunnel term handle
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_tunnel_interface_create(
                                switchlink_db_tunnel_interface_info_t *tnl_intf,
                                switchlink_handle_t *tnl_intf_h,
                                switchlink_handle_t *tnl_term_h) {
    sai_status_t status = SAI_STATUS_SUCCESS;

    status = switchlink_create_tunnel(tnl_intf, tnl_intf_h);
    if (status != SAI_STATUS_SUCCESS) {
        VLOG_ERR("Cannot create tunnel for interface: %s", tnl_intf->ifname);
        return -1;
    }
    VLOG_INFO("Created tunnel interface: %s", tnl_intf->ifname);

    status = switchlink_create_term_table_entry(tnl_intf, tnl_term_h);
    if (status != SAI_STATUS_SUCCESS) {
        VLOG_ERR("Cannot create tunnel termination table entry for "
                 "interface: %s", tnl_intf->ifname);
        return -1;
    }
    VLOG_INFO("Created tunnel termination entry for "
              "interface: %s", tnl_intf->ifname);

    return 0;
}

/*
 * Routine Description:
 *    Remove tunnel term table entry
 *
 * Arguments:
 *    [in] tnl_intf - tunnel interface info
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */

sai_status_t switchlink_remove_tunnel_term_table_entry(
                        switchlink_db_tunnel_interface_info_t *tnl_intf) {
    return tunnel_api->remove_tunnel_term_table_entry(tnl_intf->tnl_term_h);
}

/*
 * Routine Description:
 *    Remove tunnel interface
 *
 * Arguments:
 *    [in] tnl_intf - tunnel interface info
 *
 * Return Values:
 *    SAI_STATUS_SUCCESS on success
 *    Failure status code on error
 */

sai_status_t switchlink_remove_tunnel(
                        switchlink_db_tunnel_interface_info_t *tnl_intf) {

  return tunnel_api->remove_tunnel(tnl_intf->orif_h);
}

/*
 * Routine Description:
 *    Wrapper function to delete tunnel interface and delete tunnel 
 *    term table entry
 *
 * Arguments:
 *    [in] tnl_intf - tunnel interface info
 *    [in] tnl_intf_h - tunnel interface handle
 *    [in] tnl_term_h - tunnel term handle
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_tunnel_interface_delete(switchlink_db_tunnel_interface_info_t
                                       *tnl_intf) {
  sai_status_t status = SAI_STATUS_SUCCESS;

  status = switchlink_remove_tunnel_term_table_entry(tnl_intf);
  if (status != SAI_STATUS_SUCCESS) {
      VLOG_ERR("Cannot remove tunnel termination entry for "
               "interface: %s", tnl_intf->ifname);
      return -1;
  }
  VLOG_INFO("Removed tunnel termination entry for "
            "interface: %s", tnl_intf->ifname);

  status = switchlink_remove_tunnel(tnl_intf);
  if (status != SAI_STATUS_SUCCESS) {
      VLOG_ERR("Cannot remove tunnel entry for "
               "interface: %s", tnl_intf->ifname);
      return -1;
  }

  VLOG_INFO("Removed tunnel entry for interface: %s", tnl_intf->ifname);
  // Add further code to remove tunnel dependent params here.

  return 0;
}

/*
 * Routine Description:
 *    Create FDB entry
 *
 * Arguments:
 *    [in] mac_addr - MAC address
 *    [in] bridge_h - bridge handle
 *    [in] intf_h - interface handle
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_mac_create(switchlink_mac_addr_t mac_addr,
                          switchlink_handle_t bridge_h,
                          switchlink_handle_t intf_h) {
  sai_status_t status = SAI_STATUS_SUCCESS;
  sai_fdb_entry_t fdb_entry;
  memset(&fdb_entry, 0, sizeof(fdb_entry));
  memcpy(fdb_entry.mac_address, mac_addr, sizeof(sai_mac_t));
  fdb_entry.bv_id = bridge_h;

  sai_attribute_t attr_list[3];
  memset(&attr_list, 0, sizeof(attr_list));

  attr_list[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
  attr_list[0].value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
  attr_list[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
  attr_list[1].value.oid = intf_h;
  attr_list[2].id = SAI_FDB_ENTRY_ATTR_META_DATA;
  attr_list[2].value.u16 = SAI_L2_FWD_LEARN_PHYSICAL_INTERFACE;

  status = fdb_api->create_fdb_entry(&fdb_entry, 3, attr_list);
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Delete FDB entry
 *
 * Arguments:
 *    [in] mac_addr - MAC address
 *    [in] bridge_h - bridge handle
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_mac_delete(switchlink_mac_addr_t mac_addr,
                          switchlink_handle_t bridge_h) {
  sai_status_t status = SAI_STATUS_SUCCESS;
  sai_fdb_entry_t fdb_entry;
  memset(&fdb_entry, 0, sizeof(fdb_entry));
  memcpy(fdb_entry.mac_address, mac_addr, sizeof(sai_mac_t));
  fdb_entry.bv_id = bridge_h;

  status = fdb_api->remove_fdb_entry(&fdb_entry);
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Create nexthop entry
 *
 * Arguments:
 *    [in] nexthop_info - nexthop interface info
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_nexthop_create(switchlink_db_nexthop_info_t *nexthop_info) {
  sai_status_t status = SAI_STATUS_SUCCESS;

  sai_attribute_t attr_list[3];
  memset(attr_list, 0, sizeof(attr_list));
  attr_list[0].id = SAI_NEXT_HOP_ATTR_TYPE;
  attr_list[0].value.s32 = SAI_NEXT_HOP_TYPE_IP;
  attr_list[1].id = SAI_NEXT_HOP_ATTR_IP;
  if (nexthop_info->ip_addr.family == AF_INET) {
    attr_list[1].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    attr_list[1].value.ipaddr.addr.ip4 =
        htonl(nexthop_info->ip_addr.ip.v4addr.s_addr);
  } else {
    attr_list[1].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    memcpy(attr_list[1].value.ipaddr.addr.ip6,
           &(nexthop_info->ip_addr.ip.v6addr),
           sizeof(sai_ip6_t));
  }
  attr_list[2].id = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID;
  attr_list[2].value.oid = nexthop_info->intf_h;
  status = nhop_api->create_next_hop(&(nexthop_info->nhop_h), 0, 3, attr_list);
  if (status != SAI_STATUS_SUCCESS) {
    VLOG_ERR("Unable to create nexthop");
    return -1;
  }

  memset(attr_list, 0x0, sizeof(attr_list));
  attr_list[0].id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID;
  attr_list[0].value.oid = nexthop_info->nhop_h;
  status = nhop_group_api->create_next_hop_group_member(
           &(nexthop_info->nhop_member_h), 0, 1, attr_list);
  if (status != SAI_STATUS_SUCCESS) {
      VLOG_ERR("Unable to add member to nexthop");
      return -1;
  }

  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Delete nexthop entry
 *
 * Arguments:
 *    [in] nhop handler
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_nexthop_delete(switchlink_handle_t nhop_h) {
  sai_status_t status = SAI_STATUS_SUCCESS;
  status = nhop_api->remove_next_hop(nhop_h);
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Create neighbor entry
 *
 * Arguments:
 *    [in] neigh_info - neighbor interface info
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_neighbor_create(switchlink_db_neigh_info_t *neigh_info) {
  sai_status_t status = SAI_STATUS_SUCCESS;

  sai_attribute_t attr_list[1];
  memset(attr_list, 0, sizeof(attr_list));
  attr_list[0].id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;
  memcpy(attr_list[0].value.mac, neigh_info->mac_addr, sizeof(sai_mac_t));

  sai_neighbor_entry_t neighbor_entry;
  memset(&neighbor_entry, 0, sizeof(neighbor_entry));
  neighbor_entry.rif_id = neigh_info->intf_h;
  if (neigh_info->ip_addr.family == AF_INET) {
    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    neighbor_entry.ip_address.addr.ip4 =
        htonl(neigh_info->ip_addr.ip.v4addr.s_addr);
  } else {
    ovs_assert(neigh_info->ip_addr.family == AF_INET6);
    neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    memcpy(neighbor_entry.ip_address.addr.ip6,
           &(neigh_info->ip_addr.ip.v6addr),
           sizeof(sai_ip6_t));
  }

  status = neigh_api->create_neighbor_entry(&neighbor_entry, 1, attr_list);
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Remove neighbor entry
 *
 * Arguments:
 *    [in] neigh_info - neighbor interface info
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_neighbor_delete(switchlink_db_neigh_info_t *neigh_info) {
  sai_status_t status = SAI_STATUS_SUCCESS;

  sai_neighbor_entry_t neighbor_entry;
  memset(&neighbor_entry, 0, sizeof(neighbor_entry));
  neighbor_entry.rif_id = neigh_info->intf_h;
  neighbor_entry.ip_address.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
  neighbor_entry.ip_address.addr.ip4 =
      htonl(neigh_info->ip_addr.ip.v4addr.s_addr);

  status = neigh_api->remove_neighbor_entry(&neighbor_entry);
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Create route entry
 *
 * Arguments:
 *    [in] route_info - route info
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_route_create(switchlink_db_route_info_t *route_info) {
  sai_status_t status = SAI_STATUS_SUCCESS;

  sai_route_entry_t route_entry;
  memset(&route_entry, 0, sizeof(route_entry));
  route_entry.vr_id = route_info->vrf_h;
  if (route_info->ip_addr.family == AF_INET) {
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 =
        htonl(route_info->ip_addr.ip.v4addr.s_addr);
    route_entry.destination.mask.ip4 =
        htonl(ipv4_prefix_len_to_mask(route_info->ip_addr.prefix_len));
  } else {
    ovs_assert(route_info->ip_addr.family == AF_INET6);
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    memcpy(route_entry.destination.addr.ip6,
           &(route_info->ip_addr.ip.v6addr),
           sizeof(sai_ip6_t));
    struct in6_addr mask =
        ipv6_prefix_len_to_mask(route_info->ip_addr.prefix_len);
    memcpy(route_entry.destination.mask.ip6, &mask, sizeof(sai_ip6_t));
  }

  sai_attribute_t attr_list[1];
  memset(attr_list, 0, sizeof(attr_list));
  if (route_info->nhop_h == g_cpu_rx_nhop_h) {
    attr_list[0].id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    attr_list[0].value.s32 = SAI_PACKET_ACTION_TRAP;
  } else {
    attr_list[0].id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    attr_list[0].value.oid = route_info->nhop_h;
  }

  VLOG_INFO("Switch SAI route create API is triggered");
  status = route_api->create_route_entry(&route_entry, 1, attr_list);
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Delete route entry
 *
 * Arguments:
 *    [in] route_info - route info
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_route_delete(switchlink_db_route_info_t *route_info) {
  sai_status_t status = SAI_STATUS_SUCCESS;

  sai_route_entry_t route_entry;
  memset(&route_entry, 0, sizeof(route_entry));
  route_entry.vr_id = route_info->vrf_h;
  if (route_info->ip_addr.family == AF_INET) {
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
    route_entry.destination.addr.ip4 =
        htonl(route_info->ip_addr.ip.v4addr.s_addr);
    route_entry.destination.mask.ip4 =
        htonl(ipv4_prefix_len_to_mask(route_info->ip_addr.prefix_len));
  } else {
    ovs_assert(route_info->ip_addr.family == AF_INET6);
    route_entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
    memcpy(route_entry.destination.addr.ip6,
           &(route_info->ip_addr.ip.v6addr),
           sizeof(sai_ip6_t));
    struct in6_addr mask =
        ipv6_prefix_len_to_mask(route_info->ip_addr.prefix_len);
    memcpy(route_entry.destination.mask.ip6, &mask, sizeof(sai_ip6_t));
  }

  VLOG_INFO("Switch SAI route delete API is triggered");
  status = route_api->remove_route_entry(&route_entry);
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Create ecmp by creating nexthop group
 *
 * Arguments:
 *    [in] ecmp_info - ecmp_info
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_ecmp_create(switchlink_db_ecmp_info_t *ecmp_info) {
  sai_status_t status = SAI_STATUS_SUCCESS;
  uint8_t index = 0;
  sai_attribute_t attr_list[1];
  sai_attribute_t attr_member_list[2];

  memset(attr_list, 0, sizeof(attr_list));
  attr_list[0].id = SAI_NEXT_HOP_GROUP_ATTR_TYPE;
  attr_list[0].value.s32 = SAI_NEXT_HOP_GROUP_TYPE_ECMP;

  status = nhop_group_api->create_next_hop_group(
      &(ecmp_info->ecmp_h), 0, 0x1, attr_list);
  if (status != SAI_STATUS_SUCCESS) {
    VLOG_ERR("Unable to create nexthop group for ECMP");
    return -1;
  }

  for (index = 0; index < ecmp_info->num_nhops; index++) {
    memset(attr_member_list, 0x0, sizeof(attr_member_list));
    attr_member_list[0].id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID;
    attr_member_list[0].value.oid = ecmp_info->ecmp_h;
    attr_member_list[1].id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID;
    attr_member_list[1].value.oid = ecmp_info->nhops[index];
    status = nhop_group_api->create_next_hop_group_member(
        &ecmp_info->nhop_member_handles[index], 0, 0x2, attr_member_list);
    if (status != SAI_STATUS_SUCCESS) {
        VLOG_ERR("Unable to add members to nexthop group for ECMP");
        return -1;
    }
  }

  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Delete ecmp by deleting nexthop group
 *
 * Arguments:
 *    [in] ecmp_info - ecmp_info
 *
 * Return Values:
 *    0 on success
 *   -1 in case of error
 */

int switchlink_ecmp_delete(switchlink_db_ecmp_info_t *ecmp_info) {
  sai_status_t status = SAI_STATUS_SUCCESS;
  /* hile deleting group, we oop through list of members and delete
   * member entries as well
   * */
  status = nhop_group_api->remove_next_hop_group(ecmp_info->ecmp_h);
  return ((status == SAI_STATUS_SUCCESS) ? 0 : -1);
}

/*
 * Routine Description:
 *    Initialize SAI API's
 *
 * Arguments:
 *    void
 *
 * Return Values:
 *    void
 */

void switchlink_api_init(void) {
  sai_status_t status = SAI_STATUS_SUCCESS;

  status = sai_initialize();
  ovs_assert(status == SAI_STATUS_SUCCESS);

  status = sai_api_query(SAI_API_PORT, (void **)&port_api);
  ovs_assert(status == SAI_STATUS_SUCCESS);
  status = sai_api_query(SAI_API_VIRTUAL_ROUTER, (void **)&vrf_api);
  ovs_assert(status == SAI_STATUS_SUCCESS);
  status = sai_api_query(SAI_API_FDB, (void **)&fdb_api);
  ovs_assert(status == SAI_STATUS_SUCCESS);
  status = sai_api_query(SAI_API_ROUTER_INTERFACE, (void **)&rintf_api);
  ovs_assert(status == SAI_STATUS_SUCCESS);
  status = sai_api_query(SAI_API_NEIGHBOR, (void **)&neigh_api);
  ovs_assert(status == SAI_STATUS_SUCCESS);
  status = sai_api_query(SAI_API_NEXT_HOP, (void **)&nhop_api);
  ovs_assert(status == SAI_STATUS_SUCCESS);
  status = sai_api_query(SAI_API_ROUTE, (void **)&route_api);
  ovs_assert(status == SAI_STATUS_SUCCESS);
  status = sai_api_query(SAI_API_HOSTIF, (void **)&host_intf_api);
  ovs_assert(status == SAI_STATUS_SUCCESS);
  status = sai_api_query(SAI_API_TUNNEL, (void **)&tunnel_api);
  ovs_assert(status == SAI_STATUS_SUCCESS);
  status = sai_api_query(SAI_API_NEXT_HOP_GROUP, (void **)&nhop_group_api);
  ovs_assert(status == SAI_STATUS_SUCCESS);
  return;
}
