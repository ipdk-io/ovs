/*
 * Copyright (c) 2022 Intel Corporation.
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
#include <linux/if_ether.h>
#include <netlink/netlink.h>
#include <netlink/msg.h>
#include <netlink/route/neighbour.h>
#include <net/if.h>

#include "util.h"
#include "switchlink.h"
#include "switchlink_link.h"
#include "switchlink_neigh.h"
#include "switchlink_route.h"
#include "switchlink_db.h"
#include "switchlink_sai.h"
#include "openvswitch/vlog.h"

VLOG_DEFINE_THIS_MODULE(switchlink_neigh)

/*
 * Routine Description:
 *    Validate if any other feature is using this NHOP
 *
 * Arguments:
 *    [in] using_by - Flag which has consolidated features using this nhop.
 *    [in] type - Feature enum type, to be checked if this feature is the only
 *                feature referring to this NHOP.
 *
 * Return Values:
 *    true: if this NHOP can be deleted.
 *    false: if this NHOP is referred by other features.
 */

bool
validate_nexthop_delete(uint32_t using_by,
                        switchlink_nhop_using_by_e type) {
  return (using_by & ~(type)) ? false : true;
}

/*
 * Routine Description:
 *    Delete MAC entry
 *
 * Arguments:
 *    [in] mac_addr - MAC address associated with entry
 *    [in] bridge_h - bridge handle
 *
 * Return Values:
 *    void
 */

static void mac_delete(switchlink_mac_addr_t mac_addr,
                       switchlink_handle_t bridge_h) {
  switchlink_handle_t intf_h;
  switchlink_db_status_t status;
  status = switchlink_db_mac_get_intf(mac_addr, bridge_h, &intf_h);
  if (status != SWITCHLINK_DB_STATUS_SUCCESS) {
    return;
  }
  VLOG_INFO("Delete a FDB entry: %x:%x:%x:%x:%x:%x", mac_addr[0], mac_addr[1],
                                                     mac_addr[2], mac_addr[3],
                                                     mac_addr[4], mac_addr[5]);
  switchlink_mac_delete(mac_addr, bridge_h);
  switchlink_db_mac_delete(mac_addr, bridge_h);
}

/*
 * Routine Description:
 *    Create MAC entry
 *
 * Arguments:
 *    [in] mac_addr - MAC address associated with entry
 *    [in] bridge_h - bridge handle
 *    [in] intf_h - interface handle
 *
 * Return Values:
 *    void
 */

static void mac_create(switchlink_mac_addr_t mac_addr,
                       switchlink_handle_t bridge_h,
                       switchlink_handle_t intf_h) {
  switchlink_handle_t old_intf_h;
  switchlink_db_status_t status;
  status = switchlink_db_mac_get_intf(mac_addr, bridge_h, &old_intf_h);
  if (status == SWITCHLINK_DB_STATUS_SUCCESS) {
    if (old_intf_h != intf_h) {
      mac_delete(mac_addr, bridge_h);
    } else {
      VLOG_DBG("FDB entry already exist");
      return;
    }
  }
  VLOG_INFO("Create a FDB entry: %x:%x:%x:%x:%x:%x", mac_addr[0], mac_addr[1],
                                                     mac_addr[2], mac_addr[3],
                                                     mac_addr[4], mac_addr[5]);

  switchlink_mac_create(mac_addr, bridge_h, intf_h);
  switchlink_db_mac_add(mac_addr, bridge_h, intf_h);
}

/*
 * Routine Description:
 *    Wrapper function to delete neighbor, nexthop, route entry
 *
 * Arguments:
 *    [in] vrf_h - vrf handle
 *    [in] ipaddr - IP address associated with neighbor entry
 *    [in] intf_h - interface handle
 *
 * Return Values:
 *    void
 */

static void neigh_delete(switchlink_handle_t vrf_h,
                         switchlink_ip_addr_t *ipaddr,
                         switchlink_handle_t intf_h) {
  switchlink_db_nexthop_info_t nexthop_info;
  switchlink_db_neigh_info_t neigh_info;
  switchlink_db_status_t status;

  memset(&neigh_info, 0, sizeof(switchlink_db_neigh_info_t));
  neigh_info.vrf_h = vrf_h;
  neigh_info.intf_h = intf_h;
  memcpy(&(neigh_info.ip_addr), ipaddr, sizeof(switchlink_ip_addr_t));
  status = switchlink_db_neighbor_get_info(&neigh_info);
  if (status != SWITCHLINK_DB_STATUS_SUCCESS) {
    return;
  }

  memset(&nexthop_info, 0, sizeof(switchlink_db_nexthop_info_t));
  nexthop_info.vrf_h = vrf_h;
  nexthop_info.intf_h = intf_h;
  memcpy(&(nexthop_info.ip_addr), ipaddr, sizeof(switchlink_ip_addr_t));

  mac_delete(neigh_info.mac_addr, g_default_bridge_h);
  VLOG_INFO("Delete a neighbor entry: 0x%x", ipaddr->ip.v4addr.s_addr);
  switchlink_neighbor_delete(&neigh_info);
  switchlink_db_neighbor_delete(&neigh_info);

  status = switchlink_db_nexthop_get_info(&nexthop_info);
  if (status == SWITCHLINK_DB_STATUS_SUCCESS) {
      if (validate_nexthop_delete(nexthop_info.using_by,
                                  SWITCHLINK_NHOP_FROM_NEIGHBOR)) {
          VLOG_DBG("Deleting nhop with neighbor delete 0x%lx", nexthop_info.nhop_h);
          switchlink_nexthop_delete(nexthop_info.nhop_h);
          switchlink_db_nexthop_delete(&nexthop_info);
      } else {
          VLOG_DBG("Removing Neighbor learn from nhop");
          nexthop_info.using_by &= ~SWITCHLINK_NHOP_FROM_NEIGHBOR;
          switchlink_db_nexthop_update_using_by(&nexthop_info);
      }
  }

  // delete the host route
  route_delete(g_default_vrf_h, ipaddr);
}

/*
 * Routine Description:
 *    Wrapper function to create neighbor, nexthop, route entry
 *
 * Arguments:
 *    [in] vrf_h - vrf handle
 *    [in] ipaddr - IP address associated with neighbor entry
 *    [in] mac_addr - MAC address associated with neighbor entry
 *    [in] intf_h - interface handle
 *
 * Return Values:
 *    void
 */

void neigh_create(switchlink_handle_t vrf_h,
                  switchlink_ip_addr_t *ipaddr,
                  switchlink_mac_addr_t mac_addr,
                  switchlink_handle_t intf_h) {
  bool nhop_available = false;
  switchlink_db_status_t status;
  switchlink_db_neigh_info_t neigh_info;
  switchlink_db_nexthop_info_t nexthop_info;

  if ((ipaddr->family == AF_INET6) &&
      IN6_IS_ADDR_MULTICAST(&(ipaddr->ip.v6addr))) {
    return;
  }

  memset(&neigh_info, 0, sizeof(switchlink_db_neigh_info_t));
  neigh_info.vrf_h = vrf_h;
  neigh_info.intf_h = intf_h;
  memcpy(&(neigh_info.ip_addr), ipaddr, sizeof(switchlink_ip_addr_t));

  status = switchlink_db_neighbor_get_info(&neigh_info);
  if (status == SWITCHLINK_DB_STATUS_SUCCESS) {
    if (memcmp(neigh_info.mac_addr, mac_addr, sizeof(switchlink_mac_addr_t)) ==
        0) {
      // no change
      return;
    }

    // update, currently handled as a delete followed by add
    neigh_delete(vrf_h, ipaddr, intf_h);
  }

  memcpy(neigh_info.mac_addr, mac_addr, sizeof(switchlink_mac_addr_t));

  memset(&nexthop_info, 0, sizeof(switchlink_db_nexthop_info_t));
  nexthop_info.vrf_h = vrf_h;
  nexthop_info.intf_h = intf_h;
  memcpy(&(nexthop_info.ip_addr), ipaddr, sizeof(switchlink_ip_addr_t));

  status = switchlink_db_nexthop_get_info(&nexthop_info);
  if (status == SWITCHLINK_DB_STATUS_SUCCESS) {
      nhop_available = true;
  }

  if (!nhop_available &&
      switchlink_nexthop_create(&nexthop_info) == -1) {
    return;
  }

  VLOG_INFO("Create a neighbor entry: 0x%x", ipaddr->ip.v4addr.s_addr);
  if (switchlink_neighbor_create(&neigh_info) == -1) {
    if (!nhop_available) {
        switchlink_nexthop_delete(nexthop_info.nhop_h);
    }
    return;
  }

  switchlink_db_neighbor_add(&neigh_info);

  nexthop_info.using_by |= SWITCHLINK_NHOP_FROM_NEIGHBOR;
  if (!nhop_available) {
    switchlink_db_nexthop_add(&nexthop_info);
  } else {
    switchlink_db_nexthop_update_using_by(&nexthop_info);
  }

  // add a host route
  route_create(g_default_vrf_h, ipaddr, ipaddr, 0, intf_h);
}

/* TODO: P4-OVS: Dummy Processing of Netlink messages received
 * Support IPv4 neigh/arp
 */

/*
 * Routine Description:
 *    Process neighbor netlink messages
 *
 * Arguments:
 *    [in] nlmsg - netlink msg header
 *    [in] type - type of entry (RTM_NEWNEIGH/RTM_DELNEIGH)
 *
 * Return Values:
 *    void
 */

void process_neigh_msg(struct nlmsghdr *nlmsg, int type) {
  int hdrlen, attrlen;
  struct nlattr *attr;
  struct ndmsg *nbh;
  switchlink_mac_addr_t mac_addr;
  bool mac_addr_valid = false;
  bool ipaddr_valid = false;
  switchlink_ip_addr_t ipaddr;

  ovs_assert((type == RTM_NEWNEIGH) || (type == RTM_DELNEIGH));
  nbh = nlmsg_data(nlmsg);
  hdrlen = sizeof(struct ndmsg);

  if (nbh->ndm_family == AF_INET6) {
    VLOG_DBG("Ignoring IPv6 neighbors, as IPv6 support is not available");
    return;
  }

  VLOG_DBG("%sneigh: family = %d, ifindex = %d, state = 0x%x, \
       flags = 0x%x, type = %u\n",
       ((type == RTM_NEWNEIGH) ? "new" : "del"),
       nbh->ndm_family,
       nbh->ndm_ifindex,
       nbh->ndm_state,
       nbh->ndm_flags,
       nbh->ndm_type);

  switchlink_db_interface_info_t ifinfo;
  if (switchlink_db_interface_get_info(nbh->ndm_ifindex, &ifinfo) !=
      SWITCHLINK_DB_STATUS_SUCCESS) {
    char intf_name[16] = {0};
    if (!if_indextoname(nbh->ndm_ifindex, intf_name)) {
        VLOG_ERR("Failed to get ifname for the index: %d", nbh->ndm_ifindex);
        return;
    }
    if_indextoname(nbh->ndm_ifindex, intf_name);
    VLOG_DBG("neigh: Failed to get switchlink database interface info "
             "for :%s\n", intf_name);
    return;
  }

  memset(&ipaddr, 0, sizeof(switchlink_ip_addr_t));
  attrlen = nlmsg_attrlen(nlmsg, hdrlen);
  attr = nlmsg_attrdata(nlmsg, hdrlen);
  while (nla_ok(attr, attrlen)) {
    int attr_type = nla_type(attr);
    switch (attr_type) {
      case NDA_DST:
        if ((nbh->ndm_state == NUD_REACHABLE) ||
            (nbh->ndm_state == NUD_PERMANENT) ||
            (nbh->ndm_state == NUD_STALE) ||
            (nbh->ndm_state == NUD_FAILED)) {
            ipaddr_valid = true;
            ipaddr.family = nbh->ndm_family;
            if (nbh->ndm_family == AF_INET) {
              ipaddr.ip.v4addr.s_addr = ntohl(nla_get_u32(attr));
              ipaddr.prefix_len = 32;
            } else {
              memcpy(&(ipaddr.ip.v6addr), nla_data(attr), nla_len(attr));
              ipaddr.prefix_len = 128;
            }
        } else {
            VLOG_DBG("Ignoring unused neighbor states for attribute type %d\n",
                     attr_type);
            return;
        }
        break;
      case NDA_LLADDR: {
        ovs_assert(nla_len(attr) == sizeof(switchlink_mac_addr_t));
        mac_addr_valid = true;
        memcpy(mac_addr, nla_data(attr), nla_len(attr));
        break;
      }
      default:
        VLOG_DBG("neigh: skipping attr %d\n", attr_type);
        break;
    }
    attr = nla_next(attr, &attrlen);
  }

  switchlink_handle_t intf_h = ifinfo.intf_h;
  switchlink_handle_t bridge_h = g_default_bridge_h;
  if (ifinfo.intf_type == SWITCHLINK_INTF_TYPE_L2_ACCESS) {
    bridge_h = ifinfo.bridge_h;
    ovs_assert(bridge_h);
  }

  if (type == RTM_NEWNEIGH) {
    if (bridge_h && mac_addr_valid) {
      mac_create(mac_addr, bridge_h, intf_h);
    } else if(mac_addr_valid && ifinfo.intf_type == SWITCHLINK_INTF_TYPE_L3) {
      /* Here we are creating FDB entry from neighbor table, check for
       * type as SWITCHLINK_INTF_TYPE_L3 */
      mac_create(mac_addr, bridge_h, intf_h);
    }
    if (ipaddr_valid) {
      if (mac_addr_valid) {
        neigh_create(g_default_vrf_h, &ipaddr, mac_addr, intf_h);
      } else {
        /* mac address is not valid, remove the neighbor entry */
        neigh_delete(g_default_vrf_h, &ipaddr, intf_h);
      }
    }
  } else {
    if (ipaddr_valid) {
      neigh_delete(g_default_vrf_h, &ipaddr, intf_h);
    }
  }
}
