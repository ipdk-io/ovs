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

#include <openvswitch/vlog.h>
#include "switch_tunnel.h"
#include "switch_internal.h"
#include <config.h>

#include "tdi/common/tdi_defs.h"
#include "tdi/common/c_frontend/tdi_init.h"
#include "tdi/common/c_frontend/tdi_session.h"
#include "tdi/common/c_frontend/tdi_info.h"
#include <port_mgr/dpdk/bf_dpdk_port_if.h>
#include "switch_pd_utils.h"
#include "switch_pd_p4_name_mapping.h"

VLOG_DEFINE_THIS_MODULE(switch_pd_tunnel);

switch_status_t switch_pd_tunnel_entry(
    switch_device_t device,
    const switch_api_tunnel_info_t *api_tunnel_info_t,
    bool entry_add) {

    tdi_status_t status;

    tdi_id_t field_id = 0;
    tdi_id_t action_id = 0;
    tdi_id_t data_field_id = 0;

    tdi_dev_id_t dev_id = device;

    const tdi_flags_hdl *flags_hdl = NULL;
    tdi_target_hdl *target_hdl = NULL;
    const tdi_device_hdl *dev_hdl = NULL;
    tdi_session_hdl *session = NULL;
    const tdi_info_hdl *info_hdl = NULL;
    tdi_table_key_hdl *key_hdl = NULL;
    tdi_table_data_hdl *data_hdl = NULL;
    const tdi_table_hdl *table_hdl = NULL;
    const tdi_table_info_hdl *table_info_hdl = NULL;
    uint32_t network_byte_order;

    VLOG_DBG("%s", __func__);

    status = tdi_flags_create(0, &flags_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create flags handle, error: %d", status);
        return switch_pd_tdi_status_to_status(status);
    }

    status = tdi_device_get(dev_id, &dev_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get device handle, error: %d", status);
        return switch_pd_tdi_status_to_status(status);
    }

    status = tdi_target_create(dev_hdl, &target_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create target handle, error: %d", status);
        return switch_pd_tdi_status_to_status(status);
    }
    
    status = tdi_session_create(dev_hdl, &session);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create tdi session, error: %d", status);
        return status;
    }

    status = tdi_info_get(dev_id, PROGRAM_NAME, &info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get tdi info handle, error: %d", status);
        return status;
    }

    status = tdi_table_from_name_get(info_hdl,
                                     LNW_VXLAN_ENCAP_MOD_TABLE,
                                     &table_hdl);
    if (status != TDI_SUCCESS || !table_hdl) {
        VLOG_ERR("Unable to get table handle for: %s, error: %d",
                 LNW_VXLAN_ENCAP_MOD_TABLE, status);
        goto dealloc_handle_session;
    }

    status = tdi_table_key_allocate(table_hdl, &key_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to allocate key handle for: %s, error: %d",
                 LNW_VXLAN_ENCAP_MOD_TABLE, status);
        goto dealloc_handle_session;
    }

    status = tdi_table_info_get(table_hdl, &table_info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get table info handle for table, error: %d", status);
        goto dealloc_handle_session;
    }

    status = tdi_key_field_id_get(table_info_hdl,
                                  LNW_VXLAN_ENCAP_MOD_TABLE_KEY_VENDORMETA_MOD_DATA_PTR,
                                  &field_id);
    if (status != TDI_SUCCESS) {
      VLOG_ERR("Unable to get field ID for key: %s, error: %d",
               LNW_VXLAN_ENCAP_MOD_TABLE_KEY_VENDORMETA_MOD_DATA_PTR, status);
        goto dealloc_handle_session;
    }

    status = tdi_key_field_set_value(key_hdl, field_id, 0 /*vni value*/);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to set value for key ID: %d for vxlan_encap_mod_table"
                 ", error: %d", field_id, status);
        goto dealloc_handle_session;
    }

    if (entry_add) {
        /* Add an entry to target */
        VLOG_INFO("Populate vxlan encap action in vxlan_encap_mod_table for "
                  "tunnel interface %x",
                  (unsigned int) api_tunnel_info_t->overlay_rif_handle);

        status = tdi_action_name_to_id(table_info_hdl,
                                       LNW_VXLAN_ENCAP_MOD_TABLE_ACTION_VXLAN_ENCAP,
                                       &action_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get action allocator ID for: %s, error: %d",
                     LNW_VXLAN_ENCAP_MOD_TABLE_ACTION_VXLAN_ENCAP, status);
            goto dealloc_handle_session;
        }

        status = tdi_table_action_data_allocate(table_hdl, action_id, &data_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get action allocator for ID: %s, "
                     "error: %d", LNW_VXLAN_ENCAP_MOD_TABLE_ACTION_VXLAN_ENCAP, status);
            goto dealloc_handle_session;
        }

        status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                   LNW_ACTION_VXLAN_ENCAP_PARAM_SRC_ADDR,
                                                   action_id, &data_field_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                     LNW_ACTION_VXLAN_ENCAP_PARAM_SRC_ADDR, status);
            goto dealloc_handle_session;
        }

        network_byte_order = ntohl(api_tunnel_info_t->src_ip.ip.v4addr);
        status = tdi_data_field_set_value_ptr(data_hdl, data_field_id,
                                              (const uint8_t *)&network_byte_order,
                                              sizeof(uint32_t));
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to set action value for ID: %d, error: %d",
                     data_field_id, status);
            goto dealloc_handle_session;
        }

        status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                   LNW_ACTION_VXLAN_ENCAP_PARAM_DST_ADDR,
                                                   action_id, &data_field_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                     LNW_ACTION_VXLAN_ENCAP_PARAM_DST_ADDR, status);
            goto dealloc_handle_session;
        }

        network_byte_order = ntohl(api_tunnel_info_t->dst_ip.ip.v4addr);
        status = tdi_data_field_set_value_ptr(data_hdl, data_field_id,
                                              (const uint8_t *)&network_byte_order,
                                              sizeof(uint32_t));
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to set action value for ID: %d, error: %d",
                     data_field_id, status);
            goto dealloc_handle_session;
        }

        status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                   LNW_ACTION_VXLAN_ENCAP_PARAM_DST_PORT,
                                                   action_id, &data_field_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                     LNW_ACTION_VXLAN_ENCAP_PARAM_DST_PORT, status);
            goto dealloc_handle_session;
        }

        uint16_t network_byte_order_udp = ntohs(api_tunnel_info_t->udp_port);
        status = tdi_data_field_set_value_ptr(data_hdl, data_field_id,
                                              (const uint8_t *)&network_byte_order_udp,
                                              sizeof(uint16_t));
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to set action value for ID: %d, error: %d",
                     data_field_id, status);
            goto dealloc_handle_session;
        }

        status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                   LNW_ACTION_VXLAN_ENCAP_PARAM_VNI,
                                                   action_id, &data_field_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                     LNW_ACTION_VXLAN_ENCAP_PARAM_VNI, status);
            goto dealloc_handle_session;
        }

        status = tdi_data_field_set_value_ptr(data_hdl, data_field_id, 0,
                                              sizeof(uint32_t));
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to set action value for ID: %d, error: %d",
                     data_field_id, status);
            goto dealloc_handle_session;
        }

        status = tdi_table_entry_add(table_hdl, session, target_hdl,
                                     flags_hdl, key_hdl, data_hdl);
        if (status != TDI_SUCCESS) {
          VLOG_ERR("Unable to add %s entry, error: %d", 
                   LNW_VXLAN_ENCAP_MOD_TABLE, status);
            goto dealloc_handle_session;
        }
    } else {
        /* Delete an entry from target */
        VLOG_INFO("Delete vxlan_encap_mod_table entry");
        status = tdi_table_entry_del(table_hdl, session, target_hdl, 
                                     flags_hdl, key_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to delete %s entry, error: %d", 
                     LNW_VXLAN_ENCAP_MOD_TABLE, status);
            goto dealloc_handle_session;
        }
    }

dealloc_handle_session:

    status = tdi_flags_delete(flags_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to deallocate flags handle, error: %d", status);
    }

    status = tdi_target_delete(target_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to deallocate target handle, error: %d", status);
    }

    status = tdi_switch_pd_deallocate_handle_session(key_hdl, data_hdl,
                                                     session, entry_add);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to deallocate session and handles");
        return switch_pd_tdi_status_to_status(status);
    }

    return switch_pd_tdi_status_to_status(status);
}

switch_status_t switch_pd_tunnel_term_entry(
    switch_device_t device,
    const switch_api_tunnel_term_info_t *api_tunnel_term_info_t,
    bool entry_add) {

    tdi_status_t status;

    tdi_id_t field_id = 0;
    tdi_id_t action_id = 0;
    tdi_id_t data_field_id = 0;

    tdi_dev_id_t dev_id = device;

    const tdi_flags_hdl *flags_hdl = NULL;
    tdi_target_hdl *target_hdl = NULL;
    const tdi_device_hdl *dev_hdl = NULL;
    tdi_session_hdl *session = NULL;
    const tdi_info_hdl *info_hdl = NULL;
    tdi_table_key_hdl *key_hdl = NULL;
    tdi_table_data_hdl *data_hdl = NULL;
    const tdi_table_hdl *table_hdl = NULL;
    const tdi_table_info_hdl *table_info_hdl = NULL;
    uint32_t network_byte_order;

    VLOG_DBG("%s", __func__);

    status = tdi_flags_create(0, &flags_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create flags handle, error: %d", status);
        return switch_pd_tdi_status_to_status(status);
    }

    status = tdi_device_get(dev_id, &dev_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get device handle, error: %d", status);
        return switch_pd_tdi_status_to_status(status);
    }

    status = tdi_target_create(dev_hdl, &target_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create target handle, error: %d", status);
        return switch_pd_tdi_status_to_status(status);
    }
    
    status = tdi_session_create(dev_hdl, &session);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create tdi session, error: %d", status);
        return status;
    }

    status = tdi_info_get(dev_id, PROGRAM_NAME, &info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get tdi info handle, error: %d", status);
        return status;
    }

    status = tdi_table_from_name_get(info_hdl,
                                     LNW_IPV4_TUNNEL_TERM_TABLE,
                                     &table_hdl);
    if (status != TDI_SUCCESS || !table_hdl) {
        VLOG_ERR("Unable to get table handle for: %s, error: %d",
                 LNW_IPV4_TUNNEL_TERM_TABLE, status);
        goto dealloc_handle_session;
    }

    status = tdi_table_key_allocate(table_hdl, &key_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to allocate key handle for: %s, error: %d",
                 LNW_IPV4_TUNNEL_TERM_TABLE, status);
        goto dealloc_handle_session;
    }

    status = tdi_table_info_get(table_hdl, &table_info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get table info handle for table");
        goto dealloc_handle_session;
    }

    status = tdi_key_field_id_get(table_info_hdl,
                                  LNW_IPV4_TUNNEL_TERM_TABLE_KEY_TUNNEL_TYPE,
                                  &field_id);
    if (status != TDI_SUCCESS) {
      VLOG_ERR("Unable to get field ID for key: %s, error: %d",
               LNW_IPV4_TUNNEL_TERM_TABLE_KEY_TUNNEL_TYPE, status);
        goto dealloc_handle_session;
    }

    /* From p4 file the value expected is TUNNEL_TYPE_VXLAN=2 */
    status = tdi_key_field_set_value(key_hdl, field_id, 2);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to set value for key ID: %d of ipv4_tunnel_term_table"
                 ", error: %d", field_id, status);
        goto dealloc_handle_session;
    }

    status = tdi_key_field_id_get(table_info_hdl,
                                  LNW_IPV4_TUNNEL_TERM_TABLE_KEY_IPV4_SRC,
                                  &field_id);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get field ID for key: %s",
                 LNW_IPV4_TUNNEL_TERM_TABLE_KEY_IPV4_SRC);
        goto dealloc_handle_session;
    }

    /* This refers to incoming packet fields, where SIP will be the remote_ip
     * configured while creating tunnel */
    network_byte_order = ntohl(api_tunnel_term_info_t->dst_ip.ip.v4addr);
    status = tdi_key_field_set_value_ptr(key_hdl, field_id,
                                         (const uint8_t *)&network_byte_order,
                                         sizeof(uint32_t));
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to set value for key ID: %d", field_id);
        goto dealloc_handle_session;
    }

    status = tdi_key_field_id_get(table_info_hdl,
                                  LNW_IPV4_TUNNEL_TERM_TABLE_KEY_IPV4_DST,
                                  &field_id);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get field ID for key: %s",
                 LNW_IPV4_TUNNEL_TERM_TABLE_KEY_IPV4_DST);
        goto dealloc_handle_session;
    }

    /* This refers to incoming packet fields, where DIP will be the local_ip
     * configured while creating tunnel */
    network_byte_order = ntohl(api_tunnel_term_info_t->src_ip.ip.v4addr);
    status = tdi_key_field_set_value_ptr(key_hdl, field_id,
                                         (const uint8_t *)&network_byte_order,
                                         sizeof(uint32_t));
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to set value for key ID: %d", field_id);
        goto dealloc_handle_session;
    }

    if (entry_add) {
        VLOG_INFO("Populate decap_outer_ipv4 action in ipv4_tunnel_term_table "
                  "for tunnel interface %x",
                   (unsigned int) api_tunnel_term_info_t->tunnel_handle);

        status = tdi_action_name_to_id(table_info_hdl,
                                       LNW_IPV4_TUNNEL_TERM_TABLE_ACTION_DECAP_OUTER_IPV4,
                                       &action_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get action allocator ID for: %s, error: %d",
                     LNW_IPV4_TUNNEL_TERM_TABLE_ACTION_DECAP_OUTER_IPV4, status);
            goto dealloc_handle_session;
        }

        /* Add an entry to target */
        status = tdi_table_action_data_allocate(table_hdl, action_id, &data_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get action allocator for ID: %d, "
                     "error: %d", action_id, status);
            goto dealloc_handle_session;
        }

        status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                   LNW_ACTION_DECAP_OUTER_IPV4_PARAM_TUNNEL_ID,
                                                   action_id, &data_field_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                     LNW_ACTION_DECAP_OUTER_IPV4_PARAM_TUNNEL_ID, status);
            goto dealloc_handle_session;
        }

        status = tdi_data_field_set_value(data_hdl, data_field_id,
                                          api_tunnel_term_info_t->tunnel_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to set action value for ID: %d, error: %d",
                     data_field_id, status);
            goto dealloc_handle_session;
        }

        status = tdi_table_entry_add(table_hdl, session, target_hdl,
                                     flags_hdl, key_hdl, data_hdl);
        if (status != TDI_SUCCESS) {
          VLOG_ERR("Unable to add %s entry, error: %d", 
                   LNW_IPV4_TUNNEL_TERM_TABLE, status);
            goto dealloc_handle_session;
        }

    } else {
        /* Delete an entry from target */
        VLOG_INFO("Delete ipv4_tunnel_term_table entry");
        status = tdi_table_entry_del(table_hdl, session, target_hdl, 
                                     flags_hdl, key_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to delete %s entry, error: %d", 
                     LNW_IPV4_TUNNEL_TERM_TABLE, status);
            goto dealloc_handle_session;
        }
    }

dealloc_handle_session:

    status = tdi_flags_delete(flags_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to deallocate flags handle, error: %d", status);
    }

    status = tdi_target_delete(target_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to deallocate target handle, error: %d", status);
    }

    status = tdi_switch_pd_deallocate_handle_session(key_hdl, data_hdl,
                                                     session, entry_add);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to deallocate session and handles");
        return switch_pd_tdi_status_to_status(status);
    }

    return switch_pd_tdi_status_to_status(status);
}
