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
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>
#include <target-utils/third-party/tommyds/tommyds/tommytrieinp.h>
#include <target-utils/third-party/tommyds/tommyds/tommyhashlin.h>
#include <target-utils/third-party/tommyds/tommyds/tommylist.h>

#include "util.h"
#include "xxhash.h"
#include "switchlink.h"
#include "switchlink_link.h"
#include "switchlink_neigh.h"
#include "switchlink_route.h"
#include "switchlink_db.h"
#include "switchlink_db_int.h"
#include "switchlink_int.h"

#define SWITCHLINK_MAC_KEY_LEN 14

static tommy_trie_inplace switchlink_db_handle_obj_map;
static tommy_trie_inplace switchlink_db_tuntap_obj_map;
static tommy_trie_inplace switchlink_db_interface_obj_map;
static tommy_trie_inplace switchlink_db_tunnel_obj_map;
static tommy_trie_inplace switchlink_db_bridge_obj_map;
static tommy_hashlin switchlink_db_mac_obj_hash;
static tommy_list switchlink_db_mac_obj_list;
static tommy_list switchlink_db_neigh_obj_list;
static tommy_list switchlink_db_nexthop_obj_list;
static tommy_list switchlink_db_ecmp_obj_list;
static tommy_list switchlink_db_route_obj_list;

/*
 * Routine Description:
 *   Get object from database matching the handle
 *
 * Arguments:
 *   [in] h - switchlink handle
 *
 * Return Values:
 *    Object from the database matching the handle h
 */

static void *switchlink_db_handle_get_obj(switchlink_handle_t h) {
  void *obj;
  obj = tommy_trie_inplace_search(&switchlink_db_handle_obj_map, h);
  return obj;
}
/*
 * Routine Description:
 *   Add interface info to database
 *
 * Arguments:
 *    [in] ifindex - interface index
 *    [in] intf_info - interface info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 */

switchlink_db_status_t switchlink_db_interface_add(
    uint32_t ifindex, switchlink_db_interface_info_t *intf_info) {
  switchlink_db_intf_obj_t *obj =
      switchlink_malloc(sizeof(switchlink_db_intf_obj_t), 1);
  ovs_assert(obj);
  obj->ifindex = ifindex;
  memcpy(&(obj->intf_info), intf_info, sizeof(switchlink_db_interface_info_t));
  tommy_trie_inplace_insert(
      &switchlink_db_interface_obj_map, &obj->ifindex_node, obj, obj->ifindex);
  tommy_trie_inplace_insert(&switchlink_db_handle_obj_map,
                            &obj->handle_node,
                            obj,
                            obj->intf_info.intf_h);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *   Add tunnel tap interface info to database
 *
 * Arguments:
 *    [in] ifindex - interface index
 *    [in] tunp_info - tunnel tap interface info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 */

switchlink_db_status_t switchlink_db_tuntap_add(
    uint32_t ifindex, switchlink_db_tuntap_info_t *tunp_info) {
  switchlink_db_tuntap_obj_t *obj =
      switchlink_malloc(sizeof(switchlink_db_tuntap_obj_t), 1);
  ovs_assert(obj);
  obj->ifindex = ifindex;
  memcpy(&(obj->tunp_info), tunp_info, sizeof(switchlink_db_tuntap_info_t));
  tommy_trie_inplace_insert(
      &switchlink_db_tuntap_obj_map, &obj->ifindex_node, obj, obj->ifindex);
  tommy_trie_inplace_insert(&switchlink_db_handle_obj_map,
                            &obj->handle_node,
                            obj,
                            obj->tunp_info.tunp_h);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *   Get tunnel tap interface info from database
 *
 * Arguments:
 *    [in] ifindex - interface index
 *   [out] tunp_info - tunnel tap interface info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_tuntap_get_info(
    uint32_t ifindex, switchlink_db_tuntap_info_t *tunp_info) {
  ovs_assert(tunp_info);
  switchlink_db_tuntap_obj_t *obj;
  obj = tommy_trie_inplace_search(&switchlink_db_tuntap_obj_map, ifindex);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  if (tunp_info) {
    memcpy(
        tunp_info, &(obj->tunp_info), sizeof(switchlink_db_tuntap_info_t));
  }
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *   Get interface info from database
 *
 * Arguments:
 *    [in] ifindex - interface index
 *   [out] intf_info - interface info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_interface_get_info(
    uint32_t ifindex, switchlink_db_interface_info_t *intf_info) {
  ovs_assert(intf_info);
  switchlink_db_intf_obj_t *obj;
  obj = tommy_trie_inplace_search(&switchlink_db_interface_obj_map, ifindex);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  if (intf_info) {
    memcpy(
        intf_info, &(obj->intf_info), sizeof(switchlink_db_interface_info_t));
  }
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *   Get ifindex from database
 *
 * Arguments:
 *    [in] intf_h - interface handle
 *   [out] ifindex - interface index
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_interface_get_ifindex(
    switchlink_handle_t intf_h, uint32_t *ifindex) {
  switchlink_db_intf_obj_t *obj;
  obj = switchlink_db_handle_get_obj(intf_h);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }

  *ifindex = obj->ifindex;
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *   Update interface info in database
 *
 * Arguments:
 *    [in] ifindex - interface index
 *    [in] intf_info - interface info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_interface_update(
    uint32_t ifindex, switchlink_db_interface_info_t *intf_info) {
  switchlink_db_intf_obj_t *obj;
  obj = tommy_trie_inplace_search(&switchlink_db_interface_obj_map, ifindex);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  memcpy(&(obj->intf_info), intf_info, sizeof(switchlink_db_interface_info_t));
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *   Delete tunnel interface info from database
 *
 * Arguments:
 *    [in] ifindex - interface index
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_interface_delete(uint32_t ifindex) {
  switchlink_db_intf_obj_t *obj;
  obj = tommy_trie_inplace_remove(&switchlink_db_interface_obj_map, ifindex);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  tommy_trie_inplace_remove_existing(&switchlink_db_handle_obj_map,
                                     &obj->handle_node);
  switchlink_free(obj);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *   Add tunnel interface info to database
 *
 * Arguments:
 *    [in] ifindex - interface index
 *    [in] tunnel_intf_info - tunnel interface info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 */

switchlink_db_status_t switchlink_db_tunnel_interface_add(
    uint32_t ifindex, switchlink_db_tunnel_interface_info_t *tnl_intf_info) {
  switchlink_db_tunnel_intf_obj_t *obj =
      switchlink_malloc(sizeof(switchlink_db_tunnel_intf_obj_t), 1);
  ovs_assert(obj);
  obj->ifindex = ifindex;
  memcpy(&(obj->tnl_intf_info), tnl_intf_info,
         sizeof(switchlink_db_tunnel_interface_info_t));
  tommy_trie_inplace_insert(
      &switchlink_db_tunnel_obj_map, &obj->ifindex_node, obj, obj->ifindex);
  tommy_trie_inplace_insert(&switchlink_db_handle_obj_map,
                            &obj->handle_node,
                            obj,
                            obj->tnl_intf_info.urif_h);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *   Get tunnel interface info from database
 *
 * Arguments:
 *    [in] ifindex - interface index
 *   [out] tunnel_intf_info - tunnel interface info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_tunnel_interface_get_info(
    uint32_t ifindex, switchlink_db_tunnel_interface_info_t *tunnel_intf_info) {
  ovs_assert(tunnel_intf_info);
  switchlink_db_tunnel_intf_obj_t *obj;
  obj = tommy_trie_inplace_search(&switchlink_db_tunnel_obj_map, ifindex);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  if (tunnel_intf_info) {
    memcpy(tunnel_intf_info, &(obj->tnl_intf_info),
           sizeof(switchlink_db_tunnel_interface_info_t));
  }
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *   Delete tunnel interface info from database
 *
 * Arguments:
 *    [in] ifindex - interface index
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_tunnel_interface_delete(uint32_t ifindex) {
  switchlink_db_tunnel_intf_obj_t *obj;
  obj = tommy_trie_inplace_remove(&switchlink_db_tunnel_obj_map, ifindex);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  tommy_trie_inplace_remove_existing(&switchlink_db_handle_obj_map,
                                     &obj->handle_node);
  switchlink_free(obj);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Get the hash for mac address 
 *
 * Arguments:
 *    [in] mac_addr - MAC address
 *    [in] bridge_h - bridge handle
 *   [out] key - key used for hashing
 *   [out] hash - hash computed from the key
 *
 * Return Values:
 *    void
 */

static inline void switchlink_db_mac_key_hash(switchlink_mac_addr_t mac_addr,
                                              switchlink_handle_t bridge_h,
                                              uint8_t *key,
                                              uint32_t *hash) {
  memset(key, 0, SWITCHLINK_MAC_KEY_LEN);
  memcpy(&key[0], &bridge_h, min(sizeof(bridge_h), (uint32_t)8));
  memcpy(&key[8], mac_addr, 6);
  if (hash) {
    *hash = XXH32(key, SWITCHLINK_MAC_KEY_LEN, 0x98761234);
  }
}

/*
 * Routine Description:
 *    Compare the mac address with the one in database 
 *
 * Arguments:
 *    [in] mac_addr - MAC address
 *    [in] bridge_h - bridge handle
 *   [out] key - key used for hashing
 *   [out] hash - hash computed from the key
 *
 * Return Values:
 *    0 if mac address matches
 *    > 1 if key1 is greater than key2
 *    < 1 if key1 is smaller than key2
 */

static inline int switchlink_db_mac_cmp(const void *key1, const void *arg) {
  switchlink_db_mac_obj_t *obj = (switchlink_db_mac_obj_t *)arg;
  uint8_t key2[SWITCHLINK_MAC_KEY_LEN];

  switchlink_db_mac_key_hash(obj->addr, obj->bridge_h, key2, NULL);
  return (memcmp(key1, key2, SWITCHLINK_MAC_KEY_LEN));
}

/*
 * Routine Description:
 *    Add mac entry to switchlink database
 *
 * Arguments:
 *    [in] mac_addr - MAC address
 *    [in] bridge_h - bridge handle
 *    [in] intf_h - interface handle
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 */

switchlink_db_status_t switchlink_db_mac_add(switchlink_mac_addr_t mac_addr,
                                             switchlink_handle_t bridge_h,
                                             switchlink_handle_t intf_h) {
  switchlink_db_mac_obj_t *obj =
      switchlink_malloc(sizeof(switchlink_db_mac_obj_t), 1);
  memcpy(obj->addr, mac_addr, sizeof(switchlink_mac_addr_t));
  obj->bridge_h = bridge_h;
  obj->intf_h = intf_h;

  uint32_t hash;
  uint8_t key[SWITCHLINK_MAC_KEY_LEN];
  switchlink_db_mac_key_hash(mac_addr, bridge_h, key, &hash);
  tommy_hashlin_insert(&switchlink_db_mac_obj_hash, &obj->hash_node, obj, hash);
  tommy_list_insert_tail(&switchlink_db_mac_obj_list, &obj->list_node, obj);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Get interface handle for mac address from database
 *
 * Arguments:
 *    [in] mac_addr - MAC address
 *    [in] bridge_h - bridge handle
 *   [out] intf_h - interface handle
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_mac_get_intf(
    switchlink_mac_addr_t mac_addr,
    switchlink_handle_t bridge_h,
    switchlink_handle_t *intf_h) {
  switchlink_db_mac_obj_t *obj;
  uint32_t hash;
  uint8_t key[SWITCHLINK_MAC_KEY_LEN];
  switchlink_db_mac_key_hash(mac_addr, bridge_h, key, &hash);

  obj = tommy_hashlin_search(
      &switchlink_db_mac_obj_hash, switchlink_db_mac_cmp, key, hash);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  *intf_h = obj->intf_h;
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Delete mac entry from switchlink database
 *
 * Arguments:
 *    [in] bridge_h - bridge handle
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_mac_delete(switchlink_mac_addr_t mac_addr,
                                                switchlink_handle_t bridge_h) {
  switchlink_db_mac_obj_t *obj;
  uint32_t hash;
  uint8_t key[SWITCHLINK_MAC_KEY_LEN];
  switchlink_db_mac_key_hash(mac_addr, bridge_h, key, &hash);

  obj = tommy_hashlin_search(
      &switchlink_db_mac_obj_hash, switchlink_db_mac_cmp, key, hash);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  tommy_hashlin_remove_existing(&switchlink_db_mac_obj_hash, &obj->hash_node);
  tommy_list_remove_existing(&switchlink_db_mac_obj_list, &obj->list_node);
  switchlink_free(obj);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Add neighbor entry to switchlink database
 *
 * Arguments:
 *    [in] neigh_info - neigh info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_neighbor_add(
    switchlink_db_neigh_info_t *neigh_info) {
  switchlink_db_neigh_obj_t *obj =
      switchlink_malloc(sizeof(switchlink_db_neigh_obj_t), 1);
  memcpy(&(obj->neigh_info), neigh_info, sizeof(switchlink_db_neigh_info_t));
  tommy_list_insert_tail(&switchlink_db_neigh_obj_list, &obj->list_node, obj);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Get neighbor entry from switchlink database
 *
 * Arguments:
 *    [out] neigh_info - neigh info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_neighbor_get_info(
    switchlink_db_neigh_info_t *neigh_info) {
  tommy_node *node = tommy_list_head(&switchlink_db_neigh_obj_list);
  while (node) {
    switchlink_db_neigh_obj_t *obj = node->data;
    node = node->next;
    if ((memcmp(&(neigh_info->ip_addr),
                &(obj->neigh_info.ip_addr),
                sizeof(switchlink_ip_addr_t)) == 0) &&
        (neigh_info->vrf_h == obj->neigh_info.vrf_h) &&
        (neigh_info->intf_h == obj->neigh_info.intf_h)) {
      if (neigh_info) {
        memcpy(
            neigh_info, &(obj->neigh_info), sizeof(switchlink_db_neigh_info_t));
      }
      return SWITCHLINK_DB_STATUS_SUCCESS;
    }
  }
  return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
}

/*
 * Routine Description:
 *    Delete neighbor entry from switchlink database
 *
 * Arguments:
 *    [in] neigh_info - neigh info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_neighbor_delete(
    switchlink_db_neigh_info_t *neigh_info) {
  tommy_node *node = tommy_list_head(&switchlink_db_neigh_obj_list);
  while (node) {
    switchlink_db_neigh_obj_t *obj = node->data;
    node = node->next;
    if ((memcmp(&(neigh_info->ip_addr),
                &(obj->neigh_info.ip_addr),
                sizeof(switchlink_ip_addr_t)) == 0) &&
        (neigh_info->intf_h == obj->neigh_info.intf_h)) {
      tommy_list_remove_existing(&switchlink_db_neigh_obj_list,
                                 &obj->list_node);
      switchlink_free(obj);
      return SWITCHLINK_DB_STATUS_SUCCESS;
    }
  }
  return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
}

/*
 * Routine Description:
 *    Add nexthop entry to switchlink database
 *
 * Arguments:
 *    [in] nexthop_info - nexthop info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_nexthop_add(
    switchlink_db_nexthop_info_t *nexthop_info) {
  switchlink_db_nexthop_obj_t *obj =
      switchlink_malloc(sizeof(switchlink_db_nexthop_obj_t), 1);
  ovs_assert(obj);
  memcpy(&(obj->nexthop_info), nexthop_info,
         sizeof(switchlink_db_nexthop_info_t));
  tommy_list_insert_tail(&switchlink_db_nexthop_obj_list, &obj->list_node, obj);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Get nexthop entry from switchlink database
 *
 * Arguments:
 *    [out] nexthop_info - nexthop info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_nexthop_get_info(
    switchlink_db_nexthop_info_t *nexthop_info) {
  ovs_assert(nexthop_info);
  tommy_node *node = tommy_list_head(&switchlink_db_nexthop_obj_list);
  while (node) {
    switchlink_db_nexthop_obj_t *obj = node->data;
    node = node->next;
    if ((memcmp(&(nexthop_info->ip_addr),
                &(obj->nexthop_info.ip_addr),
                sizeof(switchlink_ip_addr_t)) == 0) &&
        (nexthop_info->vrf_h == obj->nexthop_info.vrf_h) &&
        (nexthop_info->intf_h == obj->nexthop_info.intf_h)) {
        memcpy(nexthop_info, &(obj->nexthop_info),
               sizeof(switchlink_db_nexthop_info_t));
      return SWITCHLINK_DB_STATUS_SUCCESS;
    }
  }
  return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
}

/*
 * Routine Description:
 *   Update nexthop info in database
 *
 * Arguments:
 *    [in] nexthop_info - interface info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */
switchlink_db_status_t switchlink_db_nexthop_update_using_by(
    switchlink_db_nexthop_info_t *nexthop_info) {
  ovs_assert(nexthop_info);
  tommy_node *node = tommy_list_head(&switchlink_db_nexthop_obj_list);
  while (node) {
    switchlink_db_nexthop_obj_t *obj = node->data;
    node = node->next;
    if ((memcmp(&(nexthop_info->ip_addr),
                &(obj->nexthop_info.ip_addr),
                sizeof(switchlink_ip_addr_t)) == 0) &&
        (nexthop_info->vrf_h == obj->nexthop_info.vrf_h) &&
        (nexthop_info->intf_h == obj->nexthop_info.intf_h)) {
        obj->nexthop_info.using_by = nexthop_info->using_by;
      return SWITCHLINK_DB_STATUS_SUCCESS;
    }
  }

  return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
}

/*
 * Routine Description:
 *    Get nexthop entry from switchlink database
 *
 * Arguments:
 *    [in] nhop_h - hexthop handler
 *    [out] nexthop_info - nexthop info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_nexthop_handle_get_info(
    switchlink_handle_t nhop_h, switchlink_db_nexthop_info_t *nexthop_info) {
  tommy_node *node = tommy_list_head(&switchlink_db_nexthop_obj_list);
  while (node) {
    switchlink_db_nexthop_obj_t *obj = node->data;
    node = node->next;
    if (nhop_h == obj->nexthop_info.nhop_h) {
      if (nexthop_info) {
        memcpy(nexthop_info, &(obj->nexthop_info),
               sizeof(switchlink_db_nexthop_info_t));
      }
      return SWITCHLINK_DB_STATUS_SUCCESS;
    }
  }
  return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
}

/*
 * Routine Description:
 *    Delete nexthop entry from switchlink database
 *
 * Arguments:
 *    [in] nexthop_info - nexthop info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_nexthop_delete(
    switchlink_db_nexthop_info_t *nexthop_info) {
  tommy_node *node = tommy_list_head(&switchlink_db_nexthop_obj_list);
  while (node) {
    switchlink_db_nexthop_obj_t *obj = node->data;
    node = node->next;
    if ((memcmp(&(nexthop_info->ip_addr),
                &(obj->nexthop_info.ip_addr),
                sizeof(switchlink_ip_addr_t)) == 0) &&
        (nexthop_info->intf_h == obj->nexthop_info.intf_h)) {
      tommy_list_remove_existing(&switchlink_db_nexthop_obj_list,
                                 &obj->list_node);
      switchlink_free(obj);
      return SWITCHLINK_DB_STATUS_SUCCESS;
    }
  }
  return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
}


/*
 * Routine Description:
 *    Add ecmp info to switchlink database
 *
 * Arguments:
 *    [in] ecmp_info - ecmp info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 */

switchlink_db_status_t switchlink_db_ecmp_add(
    switchlink_db_ecmp_info_t *ecmp_info) {
  ovs_assert(ecmp_info->num_nhops < SWITCHLINK_ECMP_NUM_MEMBERS_MAX);
  switchlink_db_ecmp_obj_t *obj =
      switchlink_malloc(sizeof(switchlink_db_ecmp_obj_t), 1);
  memcpy(&(obj->ecmp_info), ecmp_info, sizeof(switchlink_db_ecmp_info_t));
  obj->ref_count = 0;
  tommy_list_insert_tail(&switchlink_db_ecmp_obj_list, &obj->list_node, obj);
  tommy_trie_inplace_insert(&switchlink_db_handle_obj_map,
                            &obj->handle_node,
                            obj,
                            obj->ecmp_info.ecmp_h);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Get ecmp info from switchlink database
 *
 * Arguments:
 *    [out] ecmp_info - ecmp info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_ecmp_get_info(
    switchlink_db_ecmp_info_t *ecmp_info) {
  tommy_node *node = tommy_list_head(&switchlink_db_ecmp_obj_list);
  while (node) {
    switchlink_db_ecmp_obj_t *obj = node->data;
    node = node->next;
    if (obj->ecmp_info.num_nhops == ecmp_info->num_nhops) {
      int i, j;
      for (i = 0; i < ecmp_info->num_nhops; i++) {
        bool match_found = false;
        for (j = 0; j < ecmp_info->num_nhops; j++) {
          if (obj->ecmp_info.nhops[i] == ecmp_info->nhops[j]) {
            match_found = true;
            break;
          }
        }
        if (!match_found) {
          return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
        }
      }
      if (ecmp_info) {
        memcpy(ecmp_info, &(obj->ecmp_info), sizeof(switchlink_db_ecmp_info_t));
      }
      return SWITCHLINK_DB_STATUS_SUCCESS;
    }
  }
  return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
}

/*
 * Routine Description:
 *    Get ecmp handle info from switchlink database
 *
 * Arguments:
 *    [in] ecmp_h - ecmp handle
 *    [out] ecmp_info - ecmp info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_ecmp_handle_get_info(
    switchlink_handle_t ecmp_h, switchlink_db_ecmp_info_t *ecmp_info) {
  switchlink_db_ecmp_obj_t *obj;
  obj = switchlink_db_handle_get_obj(ecmp_h);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  if (ecmp_info) {
    memcpy(ecmp_info, &(obj->ecmp_info), sizeof(switchlink_db_ecmp_info_t));
  }
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Increase ecmp reference from switchlink database
 *
 * Arguments:
 *    [in] ecmp_h - ecmp handle
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_ecmp_ref_inc(switchlink_handle_t ecmp_h) {
  switchlink_db_ecmp_obj_t *obj;
  obj = switchlink_db_handle_get_obj(ecmp_h);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  ovs_assert(obj->ref_count >= 0);
  obj->ref_count++;
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Decrease ecmp reference from switchlink database
 *
 * Arguments:
 *    [in] ecmp_h - ecmp handle
 *    [in/out] ref_count - reference count
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_ecmp_ref_dec(switchlink_handle_t ecmp_h,
                                                  int *ref_count) {
  switchlink_db_ecmp_obj_t *obj;
  obj = switchlink_db_handle_get_obj(ecmp_h);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  ovs_assert(obj->ref_count >= 0);
  if (obj->ref_count != 0) {
    obj->ref_count--;
  }
  *ref_count = obj->ref_count;
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Delete ecmp entry from switchlink database
 *
 * Arguments:
 *    [in] ecmp_h - ecmp handle
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_ecmp_delete(switchlink_handle_t ecmp_h) {
  switchlink_db_ecmp_obj_t *obj;
  obj = switchlink_db_handle_get_obj(ecmp_h);
  if (!obj) {
    return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
  }
  ovs_assert(obj->ref_count == 0);
  tommy_trie_inplace_remove_existing(&switchlink_db_handle_obj_map,
                                     &obj->handle_node);
  tommy_list_remove_existing(&switchlink_db_ecmp_obj_list, &obj->list_node);
  switchlink_free(obj);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Add route entry to switchlink database
 *
 * Arguments:
 *    [in] route_info - switchlink database route info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 */

switchlink_db_status_t switchlink_db_route_add(
    switchlink_db_route_info_t *route_info) {
  switchlink_db_route_obj_t *obj =
      switchlink_malloc(sizeof(switchlink_db_route_obj_t), 1);
  memcpy(&(obj->route_info), route_info, sizeof(switchlink_db_route_info_t));
  tommy_list_insert_tail(&switchlink_db_route_obj_list, &obj->list_node, obj);
  return SWITCHLINK_DB_STATUS_SUCCESS;
}

/*
 * Routine Description:
 *    Delete route entry from switchlink database
 *
 * Arguments:
 *    [in] route_info - switchlink database route info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_route_delete(
    switchlink_db_route_info_t *route_info) {
  tommy_node *node = tommy_list_head(&switchlink_db_route_obj_list);
  while (node) {
    switchlink_db_route_obj_t *obj = node->data;
    node = node->next;
    if ((obj->route_info.vrf_h == route_info->vrf_h) &&
        (memcmp(&(obj->route_info.ip_addr),
                &(route_info->ip_addr),
                sizeof(switchlink_ip_addr_t)) == 0)) {
      tommy_list_remove_existing(&switchlink_db_route_obj_list,
                                 &obj->list_node);
      switchlink_free(obj);
      return SWITCHLINK_DB_STATUS_SUCCESS;
    }
  }
  return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
}

/*
 * Routine Description:
 *    Get route info from switchlink database
 *
 * Arguments:
 *    [in/out] route_info - switchlink database route info
 *
 * Return Values:
 *    SWITCHLINK_DB_STATUS_SUCCESS on success
 *    SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND otherwise
 */

switchlink_db_status_t switchlink_db_route_get_info(
    switchlink_db_route_info_t *route_info) {
  tommy_node *node = tommy_list_head(&switchlink_db_route_obj_list);
  while (node) {
    switchlink_db_route_obj_t *obj = node->data;
    node = node->next;
    if ((obj->route_info.vrf_h == route_info->vrf_h) &&
        (memcmp(&(obj->route_info.ip_addr),
                &(route_info->ip_addr),
                sizeof(switchlink_ip_addr_t)) == 0)) {
      if (route_info) {
        memcpy(
            route_info, &(obj->route_info), sizeof(switchlink_db_route_info_t));
      }
      return SWITCHLINK_DB_STATUS_SUCCESS;
    }
  }
  return SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND;
}

/*
 * Routine Description:
 *    Initialize switchlink database
 *
 * Arguments:
 *    void
 *
 * Return Values:
 *    void
 */

void switchlink_db_init(void) {
  tommy_trie_inplace_init(&switchlink_db_handle_obj_map);
  tommy_trie_inplace_init(&switchlink_db_interface_obj_map);
  tommy_trie_inplace_init(&switchlink_db_tuntap_obj_map);
  tommy_trie_inplace_init(&switchlink_db_tunnel_obj_map);
  tommy_trie_inplace_init(&switchlink_db_bridge_obj_map);
  tommy_hashlin_init(&switchlink_db_mac_obj_hash);
  tommy_list_init(&switchlink_db_mac_obj_list);
  tommy_list_init(&switchlink_db_neigh_obj_list);
  tommy_list_init(&switchlink_db_ecmp_obj_list);
  tommy_list_init(&switchlink_db_route_obj_list);
}
