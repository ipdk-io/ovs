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
#include <fcntl.h>
#include <unistd.h>
#include <netlink/netlink.h>
#include <netlink/msg.h>
#include <netlink/route/nexthop.h>
#include <linux/if_bridge.h>
#include <linux/if.h>
#include <linux/version.h>

#include "util.h"
#include "switchlink.h"
#include "switchlink_int.h"
#include "switchlink_link.h"
#include "switchlink_neigh.h"
#include "switchlink_db.h"
#include "switchlink_sai.h"
#include "openvswitch/vlog.h"
#include "openvswitch/dynamic-string.h"
#include "unixctl.h"

// static unixctl_cb_func vxlan_dump_cache;

switchlink_handle_t g_default_vrf_h = 0;
switchlink_handle_t g_default_bridge_h = 0;
switchlink_handle_t g_cpu_rx_nhop_h = 0;

VLOG_DEFINE_THIS_MODULE(switchlink_link);

/*
 * Routine Description:
 *    Wrapper function to create interface
 *
 * Arguments:
 *    [in] intf - interface info
 *
 * Return Values:
 *    void
 */

static void interface_create(switchlink_db_interface_info_t *intf) {
  switchlink_db_status_t status;
  switchlink_db_interface_info_t ifinfo;

  status = switchlink_db_interface_get_info(intf->ifindex, &ifinfo);
  if (status == SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND) {
    // create the interface
    VLOG_DBG("Switchlink Interface Create: %s", intf->ifname);

    status = switchlink_interface_create(intf, &(intf->intf_h));
    if (status) {
      VLOG_ERR("newlink: Failed to create switchlink interface, error: %d\n",
               status);
      return;
    }

    // add the mapping to the db
    switchlink_db_interface_add(intf->ifindex, intf);
  } else {
    // interface has already been created
    if (memcmp(&(ifinfo.mac_addr),
               &(intf->mac_addr),
               sizeof(switchlink_mac_addr_t))) {
       memcpy(&(ifinfo.mac_addr), &(intf->mac_addr),
              sizeof(switchlink_mac_addr_t));

      // Delete if RMAC is configured previously, and create this new RMAC.
      status = switchlink_interface_create(&ifinfo, &ifinfo.intf_h);
      if (status) {
        VLOG_ERR("newlink: Failed to create switchlink interface, error: %d\n",
                 status);
        return;
      }

      switchlink_db_interface_update(intf->ifindex, &ifinfo);
    }
    intf->intf_h = ifinfo.intf_h;
  }
}

/*
 * Routine Description:
 *    Wrapper function to delete interface
 *
 * Arguments:
 *    [in] ifindex - interface index
 *
 * Return Values:
 *    void
 */

static void interface_delete(uint32_t ifindex) {
  switchlink_db_interface_info_t intf;
  if (switchlink_db_interface_get_info(ifindex, &intf) ==
      SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND) {
    return;
  }

  // delete the interface from backend and DB
  switchlink_interface_delete(&intf, intf.intf_h);
  switchlink_db_interface_delete(intf.ifindex);
}

/*
 * Routine Description:
 *    Create tunnel interface
 *
 * Arguments:
 *    [in] tnl_intf - tunnel interface info
 *
 * Return Values:
 *    void
 */

static void tunnel_interface_create(
                             switchlink_db_tunnel_interface_info_t *tnl_intf) {
  switchlink_db_status_t status;
  switchlink_db_tunnel_interface_info_t tnl_ifinfo;

  status = switchlink_db_tunnel_interface_get_info(tnl_intf->ifindex,
                                                   &tnl_ifinfo);
  if (status == SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND) {

    VLOG_DBG("Switchlink tunnel interface: %s", tnl_intf->ifname);
    status = switchlink_tunnel_interface_create(tnl_intf,
                                                &(tnl_intf->orif_h),
                                                &(tnl_intf->tnl_term_h));
    if (status) {
      VLOG_ERR("newlink: Failed to create switchlink tunnel interface :%s, "
               "error: %d", tnl_intf->ifname, status);
      return;
    }

    // add the mapping to the db
    switchlink_db_tunnel_interface_add(tnl_intf->ifindex, tnl_intf);
    return;
  }
  VLOG_DBG("Switchlink DB already has tunnel config for "
           "interface: %s", tnl_intf->ifname);
  return;
}

/*
 * Routine Description:
 *    Delete tunnel interface
 *
 * Arguments:
 *    [in] ifindex - interface index
 *
 * Return Values:
 *    void
 */

static void tunnel_interface_delete(uint32_t ifindex) {
  switchlink_db_tunnel_interface_info_t tnl_intf;
  if (switchlink_db_tunnel_interface_get_info(ifindex, &tnl_intf) ==
      SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND) {
      VLOG_INFO("Trying to delete a tunnel which is not "
                "available");
    return;
  }

  VLOG_DBG("Switchlink tunnel interface: %s", tnl_intf.ifname);

  // delete the interface from backend and in DB
  switchlink_tunnel_interface_delete(&tnl_intf);
  switchlink_db_tunnel_interface_delete(ifindex);

  return;
}

/*
 * Routine Description:
 *    Get the link type
 *
 * Arguments:
 *    [in] info_kind - link related info
 *
 * Return Values:
 *    link type
 */

static switchlink_link_type_t get_link_type(char *info_kind) {
  switchlink_link_type_t link_type = SWITCHLINK_LINK_TYPE_ETH;

  if (!strcmp(info_kind, "bridge")) {
    link_type = SWITCHLINK_LINK_TYPE_BRIDGE;
  } else if (!strcmp(info_kind, "vxlan")) {
    link_type = SWITCHLINK_LINK_TYPE_VXLAN;
  } else if (!strcmp(info_kind, "bond")) {
    link_type = SWITCHLINK_LINK_TYPE_BOND;
  } else if (!strcmp(info_kind, "tun")) {
    link_type = SWITCHLINK_LINK_TYPE_TUN;
  }

  return link_type;
}

/*
TODO: P4-OVS: Process Received Netlink messages here
*/

// Support Port(tuntap), Routing and Vxlan features
/*
 * Routine Description:
 *    Process link netlink messages
 *
 * Arguments:
 *    [in] nlmsg - netlink msg header
 *    [in] type - type of netlink messages
 *
 * Return Values:
 *    void
 */

void process_link_msg(struct nlmsghdr *nlmsg, int type) {
  int hdrlen, attrlen;
  struct nlattr *attr, *nest_attr, *nest_attr_new;
  struct ifinfomsg *ifmsg;
  int nest_attr_type;

  switchlink_db_interface_info_t intf_info;
  switchlink_db_tunnel_interface_info_t tnl_intf_info;
  switchlink_link_type_t link_type = SWITCHLINK_LINK_TYPE_NONE;

  uint32_t vni_id = 0;
  switchlink_ip_addr_t remote_ip_addr;
  switchlink_ip_addr_t src_ip_addr;
  uint32_t vxlan_dst_port = 0;
  uint8_t ttl = 0;

  ovs_assert((type == RTM_NEWLINK) || (type == RTM_DELLINK));
  ifmsg = nlmsg_data(nlmsg);
  hdrlen = sizeof(struct ifinfomsg);

  VLOG_DBG("%slink: family = %d, type = %d, ifindex = %d, flags = 0x%x,"\
           "change = 0x%x\n", ((type == RTM_NEWLINK) ? "new" : "del"),
           ifmsg->ifi_family, ifmsg->ifi_type, ifmsg->ifi_index,
           ifmsg->ifi_flags, ifmsg->ifi_change);

  memset(&intf_info, 0, sizeof(switchlink_db_interface_info_t));
  memset(&tnl_intf_info, 0, sizeof(switchlink_db_tunnel_interface_info_t));
  attrlen = nlmsg_attrlen(nlmsg, hdrlen);
  attr = nlmsg_attrdata(nlmsg, hdrlen);

  while (nla_ok(attr, attrlen)) {
    int attr_type = nla_type(attr);
    switch (attr_type) {
      case IFLA_IFNAME:
        ovs_strzcpy(intf_info.ifname,
                nla_get_string(attr),
                SWITCHLINK_INTERFACE_NAME_LEN_MAX);
        VLOG_DBG("Interface name is %s\n", intf_info.ifname);
        break;
      case IFLA_LINKINFO:
        nla_for_each_nested(nest_attr, attr, attrlen) {
          nest_attr_type = nla_type(nest_attr);
          switch (nest_attr_type) {
            case IFLA_INFO_KIND:
              link_type = get_link_type(nla_get_string(nest_attr));
              break;
            case IFLA_INFO_DATA:
              nla_for_each_nested(nest_attr_new, nest_attr, attrlen) {
                  int nest_attr_type_new = nla_type(nest_attr_new);
                  switch (nest_attr_type_new) {
                      case IFLA_VXLAN_ID:
                        vni_id = *(uint32_t *) nla_data(nest_attr_new);
                        VLOG_DBG("Interface VNI ID: %d\n", vni_id);
                        break;
                      case IFLA_VXLAN_PORT:
                        vxlan_dst_port =
                            htons(*(uint16_t *) nla_data(nest_attr_new));
                        VLOG_DBG("Interface Dst port: %d\n", vxlan_dst_port);
                        break;
                      case IFLA_VXLAN_GROUP:
                        memset(&remote_ip_addr, 0,
                               sizeof(switchlink_ip_addr_t));
                        remote_ip_addr.family = AF_INET;
                        remote_ip_addr.ip.v4addr.s_addr =
                            ntohl(nla_get_u32(nest_attr_new));
                        remote_ip_addr.prefix_len = 32;
                        VLOG_DBG("Remote Ipv4 address: 0x%x\n",
                                   remote_ip_addr.ip.v4addr.s_addr);
                        break;
                      case IFLA_VXLAN_LOCAL:
                        memset(&src_ip_addr, 0, sizeof(switchlink_ip_addr_t));
                        src_ip_addr.family = AF_INET;
                        src_ip_addr.ip.v4addr.s_addr =
                            ntohl(nla_get_u32(nest_attr_new));
                        src_ip_addr.prefix_len = 32;
                        VLOG_DBG("Src Ipv4 address: 0x%x\n",
                                   src_ip_addr.ip.v4addr.s_addr);
                        break;
                      case IFLA_VXLAN_TTL:
                        ttl = nla_get_u8(nest_attr_new);
                        VLOG_DBG("TTL: %d\n", ttl);
                        break;
                    default:
                      break;
                  }
              }
              break;
            default:
              break;
          }
        }
        break;
      case IFLA_ADDRESS: {
        ovs_assert(nla_len(attr) == sizeof(switchlink_mac_addr_t));
        memcpy(&(intf_info.mac_addr), nla_data(attr), nla_len(attr));

        VLOG_DBG("Interface Mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
               (unsigned char) intf_info.mac_addr[0],
               (unsigned char) intf_info.mac_addr[1],
               (unsigned char) intf_info.mac_addr[2],
               (unsigned char) intf_info.mac_addr[3],
               (unsigned char) intf_info.mac_addr[4],
               (unsigned char) intf_info.mac_addr[5]
               );

        break;
      }
      case IFLA_MASTER:
        break;
      case IFLA_PROTINFO:
      case IFLA_AF_SPEC:
        break;
      default:
        break;
    }
    attr = nla_next(attr, &attrlen);
  }

  if (type == RTM_NEWLINK) {
    switch (link_type) {
      case SWITCHLINK_LINK_TYPE_BRIDGE:
      case SWITCHLINK_LINK_TYPE_BOND:
      case SWITCHLINK_LINK_TYPE_NONE:
      case SWITCHLINK_LINK_TYPE_ETH:
        break;

      case SWITCHLINK_LINK_TYPE_TUN:
          intf_info.ifindex = ifmsg->ifi_index;
          intf_info.vrf_h = g_default_vrf_h;
          intf_info.intf_type = SWITCHLINK_INTF_TYPE_L3;

          interface_create(&intf_info);
        break;

      case SWITCHLINK_LINK_TYPE_VXLAN: {
        ovs_strzcpy(tnl_intf_info.ifname, intf_info.ifname,
                    SWITCHLINK_INTERFACE_NAME_LEN_MAX);
        tnl_intf_info.dst_ip = remote_ip_addr;
        tnl_intf_info.src_ip = src_ip_addr;
        tnl_intf_info.link_type = link_type;
        tnl_intf_info.ifindex = ifmsg->ifi_index;
        tnl_intf_info.vni_id = vni_id;
        tnl_intf_info.dst_port = vxlan_dst_port;
        tnl_intf_info.ttl = ttl;

        tunnel_interface_create(&tnl_intf_info);
      }
      break;
      default:
        break;
    }
  } else {
    ovs_assert(type == RTM_DELLINK);
    if (link_type == SWITCHLINK_LINK_TYPE_VXLAN) {
        tunnel_interface_delete(ifmsg->ifi_index);
    } else if (link_type == SWITCHLINK_LINK_TYPE_TUN) {
      interface_delete(ifmsg->ifi_index);
    } else {
      VLOG_DBG("Unhandled link type");
    }
  }
}

#if 0
/* Loop through all p4 devices and print particular p4 device's
 * local data or print for all available p4 devices */
static void
vxlan_dump_cache(struct unixctl_conn *conn, int argc OVS_UNUSED,
                 const char *argv[], void *aux OVS_UNUSED)
{

  // TODO, dump cache crashes after dumping the context. Fix
  // the issue and then remove this return.
  return;
#if 0
  struct ds results;
  ds_init(&results);
  unixctl_command_reply(conn, ds_cstr(&results));
  ds_destroy(&results);
#endif
  switchlink_db_tunnel_interface_info_t tnl_ifinfo;
  struct ds results = DS_EMPTY_INITIALIZER;
  int if_index = if_nametoindex(argv[1]);
  switchlink_db_status_t status;

  if (if_index == 0) {
    return;
  }

  status = switchlink_db_tunnel_interface_get_info(if_index, &tnl_ifinfo);
  if (status == SWITCHLINK_DB_STATUS_ITEM_NOT_FOUND) {
    ds_put_format(&results, "\nCannot find config for interface %s", argv[1]);
  } else {
    //    ds_put_format(&results, "\nConfig for VxLAN port %s is:", argv[1]);
    ds_put_format(&results, "\n\tDestination port ID: %d", tnl_ifinfo.dst_port);
    ds_put_format(&results, "\n\tDestination IP: %x", tnl_ifinfo.dst_ip.ip.v4addr.s_addr);
    ds_put_format(&results, "\n\tSource IP: %x", tnl_ifinfo.src_ip.ip.v4addr.s_addr);
    ds_put_format(&results, "\n\tIfindex: %d", tnl_ifinfo.ifindex);
    ds_put_format(&results, "\n\tVNI ID: %d", tnl_ifinfo.vni_id);
    ds_put_format(&results, "\n\tTTL : %d", tnl_ifinfo.ttl);
  }

  unixctl_command_reply(conn, ds_cstr(&results));
  ds_destroy(&results);
}
#endif

void switchlink_link_init(void) {
  /* P4OVS: create default vrf*/
  switchlink_vrf_create(&g_default_vrf_h);

  //unixctl_command_register("p4vxlan/dump-cache", "[kernel-intf-name/all]", 1, 1,
  //                           vxlan_dump_cache, NULL);
}
