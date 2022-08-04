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
#include "switch_internal.h"
#include "switch_base_types.h"
#include "switch_pd_routing.h"
#include "switch_nhop_int.h"
#include <config.h>

#include "tdi/common/tdi_defs.h"
#include "tdi/common/c_frontend/tdi_init.h"
#include "tdi/common/c_frontend/tdi_session.h"
#include "tdi/common/c_frontend/tdi_info.h"
#include "tdi/common/c_frontend/tdi_table.h"
#include "tdi/common/c_frontend/tdi_table_info.h"
#include <port_mgr/dpdk/bf_dpdk_port_if.h>
#include "switch_pd_utils.h"
#include "switch_pd_p4_name_mapping.h"

VLOG_DEFINE_THIS_MODULE(switch_pd_routing);

switch_status_t switch_routing_table_entry (
        switch_device_t device,
        const switch_pd_routing_info_t *api_routing_info,
        bool entry_type)
{
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  VLOG_DBG("%s", __func__);

  //update nexthop table
  status = switch_pd_nexthop_table_entry(device, api_routing_info, entry_type);
  if (status != SWITCH_STATUS_SUCCESS){
     VLOG_ERR("nexthop table update failed, error: %d", status);
     return status;
  }

  //update neighbor mod table
  status = switch_pd_neighbor_table_entry(device, api_routing_info, entry_type);
  if (status != SWITCH_STATUS_SUCCESS){
      VLOG_ERR( "neighbor table update failed, error: %d", status);
      return status;
  }
  return status;
}

switch_status_t switch_pd_rmac_table_entry (
        switch_device_t device,
        switch_rmac_entry_t *rmac_entry,
        switch_handle_t rif_handle,
        bool entry_type)
{
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  VLOG_DBG("%s", __func__);

  if (!rmac_entry) {
      VLOG_ERR("Empty router_mac entry, error: %d", status);
      return status;
  }

  //update rif mod tables
  status = switch_pd_rif_mod_entry(device, rmac_entry, rif_handle, entry_type);
  if (status != SWITCH_STATUS_SUCCESS){
      VLOG_ERR("rid mod table entry failed, error: %d", status);
      return status;
  }
  return status;
}

switch_status_t switch_pd_nexthop_table_entry(
    switch_device_t device,
    const switch_pd_routing_info_t  *api_nexthop_pd_info,
    bool entry_add) {

    tdi_status_t status;

    tdi_id_t field_id = 0;
    tdi_id_t action_id = 0;
    tdi_id_t data_field_id = 0;

    tdi_dev_id_t dev_id = device;

    tdi_flags_hdl *flags_hdl = NULL;
    tdi_target_hdl *target_hdl = NULL;
    const tdi_device_hdl *dev_hdl = NULL;
    tdi_session_hdl *session = NULL;
    const tdi_info_hdl *info_hdl = NULL;
    tdi_table_key_hdl *key_hdl = NULL;
    tdi_table_data_hdl *data_hdl = NULL;
    const tdi_table_hdl *table_hdl = NULL;
    const tdi_table_info_hdl *table_info_hdl = NULL;

    VLOG_DBG("%s", __func__);

    status = tdi_flags_create(0, &flags_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create flags handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_device_get(dev_id, &dev_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get device handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_target_create(dev_hdl, &target_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create target handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_session_create(dev_hdl, &session);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create tdi session, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_info_get(dev_id, PROGRAM_NAME, &info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get tdi info handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_table_from_name_get(info_hdl,
                                       LNW_NEXTHOP_TABLE,
                                       &table_hdl);
    if (status != TDI_SUCCESS || !table_hdl) {
        VLOG_ERR("Unable to get table handle for: %s, error: %d",
                 LNW_NEXTHOP_TABLE, status);
        goto dealloc_resources;
    }

    status = tdi_table_key_allocate(table_hdl, &key_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to allocate key handle for: %s, error: %d",
                 LNW_NEXTHOP_TABLE, status);
        goto dealloc_resources;
    }

    status = tdi_table_info_get(table_hdl, &table_info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get table info handle for table, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_key_field_id_get(table_info_hdl,
                                  LNW_NEXTHOP_TABLE_KEY_NEXTHOP_ID,
                                  &field_id);
    if (status != TDI_SUCCESS) {
      VLOG_ERR("Unable to get field ID for key: %s, error: %d",
               LNW_NEXTHOP_TABLE_KEY_NEXTHOP_ID, status);
        goto dealloc_resources;
    }

    status = tdi_key_field_set_value(key_hdl, field_id,
                                     (api_nexthop_pd_info->nexthop_handle &
                                     ~(SWITCH_HANDLE_TYPE_NHOP <<
                                     SWITCH_HANDLE_TYPE_SHIFT)));
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to set value for key ID: %d for nexthop_table,"
                 " error: %d", field_id, status);
        goto dealloc_resources;
    }

    if (entry_add) {
        /* Add an entry to target */
        VLOG_INFO("Populate set_nexthop action in nexthop_table");

        status = tdi_action_name_to_id(table_info_hdl,
                                       LNW_NEXTHOP_TABLE_ACTION_SET_NEXTHOP,
                                       &action_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get action allocator ID for: %s, error: %d",
                     LNW_NEXTHOP_TABLE_ACTION_SET_NEXTHOP, status);
            goto dealloc_resources;
        }

        status = tdi_table_action_data_allocate(table_hdl, action_id, &data_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get action allocator for ID: %d, "
                     "error: %d", action_id, status);
            goto dealloc_resources;
        }

        status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                   LNW_ACTION_SET_NEXTHOP_PARAM_RIF,
                                                   action_id, &data_field_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                     LNW_ACTION_SET_NEXTHOP_PARAM_RIF, status);
            goto dealloc_resources;
        }

        status = tdi_data_field_set_value(data_hdl, data_field_id,
                                          (api_nexthop_pd_info->rif_handle &
                                          ~(SWITCH_HANDLE_TYPE_RIF <<
                                          SWITCH_HANDLE_TYPE_SHIFT)));
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to set action value for ID: %d, error: %d",
                     data_field_id, status);
            goto dealloc_resources;
        }

        status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                   LNW_ACTION_SET_NEXTHOP_PARAM_NEIGHBOR_ID,
                                                   action_id, &data_field_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                     LNW_ACTION_SET_NEXTHOP_PARAM_NEIGHBOR_ID, status);
            goto dealloc_resources;
        }

        status = tdi_data_field_set_value(data_hdl, data_field_id,
                                          (api_nexthop_pd_info->neighbor_handle &
                                          ~(SWITCH_HANDLE_TYPE_NEIGHBOR <<
                                          SWITCH_HANDLE_TYPE_SHIFT)));
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to set action value for ID: %d, error: %d",
                     data_field_id, status);
            goto dealloc_resources;
        }

        status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                   LNW_ACTION_SET_NEXTHOP_PARAM_EGRESS_PORT,
                                                   action_id, &data_field_id);
        if (status != TDI_SUCCESS) {
          VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                   LNW_ACTION_SET_NEXTHOP_PARAM_EGRESS_PORT, status);
            goto dealloc_resources;
        }

        status = tdi_data_field_set_value(data_hdl, data_field_id,
                                          api_nexthop_pd_info->port_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to set action value for ID: %d, error: %d", 
                     data_field_id, status);
            goto dealloc_resources;
        }

        status = tdi_table_entry_add(table_hdl, session, target_hdl,
                                     flags_hdl, key_hdl, data_hdl);
        if (status != TDI_SUCCESS) {
          VLOG_ERR("Unable to add %s entry, error: %d", 
                   LNW_NEXTHOP_TABLE, status);
            goto dealloc_resources;
        }

    } else {
        /* Delete an entry from target */
        VLOG_INFO("Delete nexthop_table entry");
        status = tdi_table_entry_del(table_hdl, session, target_hdl, 
                                     flags_hdl, key_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to delete %s entry, error: %d", 
                     LNW_NEXTHOP_TABLE, status);
            goto dealloc_resources;
        }
  }

dealloc_resources:
    status = tdi_switch_pd_deallocate_resources(flags_hdl, target_hdl,
                                                key_hdl, data_hdl,
                                                session, entry_add);
    return switch_pd_tdi_status_to_status(status);
}

switch_status_t switch_pd_neighbor_table_entry(
    switch_device_t device,
    const switch_pd_routing_info_t  *api_neighbor_pd_info,
    bool entry_add) {

    tdi_status_t status;

    tdi_id_t field_id = 0;
    tdi_id_t action_id = 0;
    tdi_id_t data_field_id = 0;

    tdi_dev_id_t dev_id = device;

    tdi_flags_hdl *flags_hdl = NULL;
    tdi_target_hdl *target_hdl = NULL;
    const tdi_device_hdl *dev_hdl = NULL;
    tdi_session_hdl *session = NULL;
    const tdi_info_hdl *info_hdl = NULL;
    tdi_table_key_hdl *key_hdl = NULL;
    tdi_table_data_hdl *data_hdl = NULL;
    const tdi_table_hdl *table_hdl = NULL;
    const tdi_table_info_hdl *table_info_hdl = NULL;

    VLOG_DBG("%s", __func__);

    status = tdi_flags_create(0, &flags_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create flags handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_device_get(dev_id, &dev_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get device handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_target_create(dev_hdl, &target_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create target handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_session_create(dev_hdl, &session);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create tdi session, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_info_get(dev_id, PROGRAM_NAME, &info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get tdi info handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_table_from_name_get(info_hdl,
                                     LNW_NEIGHBOR_MOD_TABLE,
                                     &table_hdl);
    if (status != TDI_SUCCESS || !table_hdl) {
        VLOG_ERR("Unable to get table handle for: %s, error: %d",
                 LNW_NEIGHBOR_MOD_TABLE, status);
        goto dealloc_resources;
    }

    status = tdi_table_key_allocate(table_hdl, &key_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to allocate key handle for: %s, error: %d",
                 LNW_NEIGHBOR_MOD_TABLE, status);
        goto dealloc_resources;
    }

    status = tdi_table_info_get(table_hdl, &table_info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get table info handle for table, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_key_field_id_get(table_info_hdl,
                                  LNW_NEIGHBOR_MOD_TABLE_KEY_VENDORMETA_MOD_DATA_PTR,
                                  &field_id);
    if (status != TDI_SUCCESS) {
      VLOG_ERR("Unable to get field ID for key: %s, error: %d",
               LNW_NEIGHBOR_MOD_TABLE_KEY_VENDORMETA_MOD_DATA_PTR, status);
        goto dealloc_resources;
    }

    status = tdi_key_field_set_value(key_hdl, field_id,
                                     (api_neighbor_pd_info->neighbor_handle &
                                     ~(SWITCH_HANDLE_TYPE_NEIGHBOR <<
                                     SWITCH_HANDLE_TYPE_SHIFT)));
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to set value for key ID: %d for neighbor_mod_table",
                 field_id);
        goto dealloc_resources;
    }

    if (entry_add) {
        /* Add an entry to target */
        VLOG_INFO("Populate set_outer_mac action in neighbor_mod_table for "
                  "neighbor handle %x",
                  (unsigned int) api_neighbor_pd_info->neighbor_handle);

        status = tdi_action_name_to_id(table_info_hdl,
                                       LNW_NEIGHBOR_MOD_TABLE_ACTION_SET_OUTER_MAC,
                                       &action_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get action allocator ID for: %s, error: %d",
                     LNW_NEIGHBOR_MOD_TABLE_ACTION_SET_OUTER_MAC, status);
            goto dealloc_resources;
        }

        status = tdi_table_action_data_allocate(table_hdl, action_id, &data_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get action allocator for ID: %d, "
                     "error: %d", action_id, status);
            goto dealloc_resources;
        }

        status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                   LNW_ACTION_SET_OUTER_MAC_PARAM_DST_MAC_ADDR,
                                                   action_id, &data_field_id);
        if (status != TDI_SUCCESS) {
          VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                   LNW_ACTION_SET_OUTER_MAC_PARAM_DST_MAC_ADDR, status);
            goto dealloc_resources;
        }

        status = tdi_data_field_set_value_ptr(data_hdl, data_field_id,
                                              (const uint8_t *)
                                              &api_neighbor_pd_info->dst_mac_addr.mac_addr,
                                              SWITCH_MAC_LENGTH);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to set action value for ID: %d, error: %d",
                     data_field_id, status);
            goto dealloc_resources;
        }

        status = tdi_table_entry_add(table_hdl, session, target_hdl,
                                     flags_hdl, key_hdl, data_hdl);
        if (status != TDI_SUCCESS) {
          VLOG_ERR("Unable to add neighbor_mod_table entry, error: %d", status);
            goto dealloc_resources;
        }
    } else {
        /* Delete an entry from target */
        VLOG_INFO("Delete neighbor_mod_table entry");
        status = tdi_table_entry_del(table_hdl, session, target_hdl, 
                                     flags_hdl, key_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to delete nexthop_table entry, error: %d", status);
            goto dealloc_resources;
        }
    }

dealloc_resources:
    status = tdi_switch_pd_deallocate_resources(flags_hdl, target_hdl,
                                                key_hdl, data_hdl,
                                                session, entry_add);
    return switch_pd_tdi_status_to_status(status);
}

switch_status_t switch_pd_rif_mod_entry(
  switch_device_t device,
  switch_rmac_entry_t *rmac_entry,
  switch_handle_t rif_handle,
  bool entry_add) {

    tdi_status_t status;

    tdi_id_t field_id = 0;
    tdi_id_t action_id = 0;
    tdi_id_t data_field_id = 0;

    tdi_dev_id_t dev_id = device;

    tdi_flags_hdl *flags_hdl = NULL;
    tdi_target_hdl *target_hdl = NULL;
    const tdi_device_hdl *dev_hdl = NULL;
    tdi_session_hdl *session = NULL;
    const tdi_info_hdl *info_hdl = NULL;
    tdi_table_key_hdl *key_hdl = NULL;
    tdi_table_data_hdl *data_hdl = NULL;
    const tdi_table_hdl *table_hdl = NULL;
    const tdi_table_info_hdl *table_info_hdl = NULL;

    VLOG_DBG("%s", __func__);

    status = tdi_flags_create(0, &flags_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create flags handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_device_get(dev_id, &dev_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get device handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_target_create(dev_hdl, &target_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create target handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_session_create(dev_hdl, &session);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create tdi session, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_info_get(dev_id, PROGRAM_NAME, &info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get tdi info handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_table_from_name_get(info_hdl,
                                     LNW_RIF_MOD_TABLE,
                                     &table_hdl);
    if (status != TDI_SUCCESS || !table_hdl) {
        VLOG_ERR("Unable to get table handle for: %s, error: %d",
                 LNW_RIF_MOD_TABLE, status);
        goto dealloc_resources;
    }

    status = tdi_table_key_allocate(table_hdl, &key_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to allocate key handle for: %s, error: %d",
                 LNW_RIF_MOD_TABLE, status);
        goto dealloc_resources;
    }

    status = tdi_table_info_get(table_hdl, &table_info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get table info handle for table, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_key_field_id_get(table_info_hdl,
                                  LNW_RIF_MOD_TABLE_KEY_RIF_MOD_MAP_ID,
                                  &field_id);
    if (status != TDI_SUCCESS) {
      VLOG_ERR("Unable to get field ID for key: %s, error: %d",
               LNW_RIF_MOD_TABLE_KEY_RIF_MOD_MAP_ID, status);
        goto dealloc_resources;
    }

    status = tdi_key_field_set_value(key_hdl, field_id,
                                     (rif_handle &
                                     ~(SWITCH_HANDLE_TYPE_RIF <<
                                     SWITCH_HANDLE_TYPE_SHIFT)));
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to set value for key ID: %d for rif_mod_table_start",
                 field_id);
        goto dealloc_resources;
    }

    if (entry_add) {
        /* Add an entry to target */
        VLOG_INFO("Populate set_src_mac_start action in rif_mod_table_start");

        status = tdi_action_name_to_id(table_info_hdl,
                                       LNW_RIF_MOD_TABLE_ACTION_SET_SRC_MAC,
                                       &action_id);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get action allocator ID for: %s, error: %d",
                     LNW_RIF_MOD_TABLE_ACTION_SET_SRC_MAC, status);
            goto dealloc_resources;
        }

        status = tdi_table_action_data_allocate(table_hdl, action_id, &data_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get action allocator for ID: %d, "
                     "error: %d", action_id, status);
            goto dealloc_resources;
        }

        status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                   LNW_ACTION_SET_SRC_MAC_PARAM_SRC_MAC_ADDR,
                                                   action_id, &data_field_id);
        if (status != TDI_SUCCESS) {
          VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                   LNW_ACTION_SET_SRC_MAC_PARAM_SRC_MAC_ADDR, status);
            goto dealloc_resources;
        }

        status = tdi_data_field_set_value_ptr(data_hdl, data_field_id,
                                (             const uint8_t *)
                                              &rmac_entry->mac.mac_addr,
                                              SWITCH_MAC_LENGTH);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to set action value for ID: %d, error: %d",
                     data_field_id, status);
            goto dealloc_resources;
        }

        status = tdi_table_entry_add(table_hdl, session, target_hdl,
                                     flags_hdl, key_hdl, data_hdl);
        if (status != TDI_SUCCESS) {
          VLOG_ERR("Unable to add rif_mod_table_start entry, error: %d", status);
            goto dealloc_resources;
        }
    } else {
        /* Delete an entry from target */
        VLOG_INFO("Delete rif_mod_table_start entry");
        status = tdi_table_entry_del(table_hdl, session, target_hdl, 
                                     flags_hdl, key_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to delete rif_mod_table_start entry, error: %d", status);
            goto dealloc_resources;
        }
    }

dealloc_resources:
    status = tdi_switch_pd_deallocate_resources(flags_hdl, target_hdl,
                                                key_hdl, data_hdl,
                                                session, entry_add);
    return switch_pd_tdi_status_to_status(status);
}

switch_status_t switch_pd_ipv4_table_entry (switch_device_t device,
    const switch_api_route_entry_t *api_route_entry,
    bool entry_add, switch_ipv4_table_action_t action)
{
    tdi_status_t status;

    tdi_id_t field_id = 0;
    tdi_id_t action_id = 0;
    tdi_id_t data_field_id = 0;

    tdi_dev_id_t dev_id = device;

    tdi_flags_hdl *flags_hdl = NULL;
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
        goto dealloc_resources;
    }

    status = tdi_device_get(dev_id, &dev_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get device handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_target_create(dev_hdl, &target_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create target handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_session_create(dev_hdl, &session);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create tdi session, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_info_get(dev_id, PROGRAM_NAME, &info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get tdi info handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_table_from_name_get(info_hdl,
                                     LNW_IPV4_TABLE,
                                     &table_hdl);
    if (status != TDI_SUCCESS || !table_hdl) {
        VLOG_ERR("Unable to get table handle for: %s, error: %d",
                LNW_IPV4_TABLE, status);
        goto dealloc_resources;
    }

    status = tdi_table_key_allocate(table_hdl, &key_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to allocate key handle for: %s, error: %d",
                 LNW_IPV4_TABLE, status);
        goto dealloc_resources;
    }

    status = tdi_table_info_get(table_hdl, &table_info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get table info handle for table, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_key_field_id_get(table_info_hdl,
                                  LNW_IPV4_TABLE_KEY_IPV4_DST_MATCH,
                                  &field_id);
    if (status != TDI_SUCCESS) {
      VLOG_ERR("Unable to get field ID for key: %s, error: %d",
               LNW_IPV4_TABLE_KEY_IPV4_DST_MATCH, status);
        goto dealloc_resources;
    }

    /* Use LPM API for LPM match type*/
    network_byte_order = ntohl(api_route_entry->ip_address.ip.v4addr);
    status = tdi_key_field_set_value_lpm_ptr(key_hdl, field_id,
                                             (const uint8_t *)&network_byte_order,
                                             (const uint16_t)api_route_entry->ip_address.prefix_len,
                                             sizeof(uint32_t));
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to set value for key ID: %d for ipv4_table, error: %d",
                 field_id, status);
        goto dealloc_resources;
    }

    if (entry_add) {
        if (action == SWITCH_ACTION_NHOP) {
            VLOG_INFO("Populate set_nexthop_id action in ipv4_table for "
                      "route handle %x",
                      (unsigned int) api_route_entry->route_handle);

            status = tdi_action_name_to_id(table_info_hdl,
                                           LNW_IPV4_TABLE_ACTION_SET_NEXTHOP_ID,
                                           &action_id);
            if (status != TDI_SUCCESS) {
                VLOG_ERR("Unable to get action allocator ID for: %s, error: %d",
                         LNW_IPV4_TABLE_ACTION_SET_NEXTHOP_ID, status);
                goto dealloc_resources;
            }

            status = tdi_table_action_data_allocate(table_hdl, action_id, &data_hdl);
            if (status != TDI_SUCCESS) {
                VLOG_ERR("Unable to get action allocator for ID: %d, "
                         "error: %d", action_id, status);
                goto dealloc_resources;
            }

            status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                       LNW_ACTION_SET_NEXTHOP_ID_PARAM_NEXTHOP_ID,
                                                       action_id, &data_field_id);
            if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                     LNW_ACTION_SET_NEXTHOP_ID_PARAM_NEXTHOP_ID, status);
                goto dealloc_resources;
            }

            status = tdi_data_field_set_value(data_hdl, data_field_id,
                                              (api_route_entry->nhop_handle &
                                              ~(SWITCH_HANDLE_TYPE_NHOP <<
                                              SWITCH_HANDLE_TYPE_SHIFT)));
            if (status != TDI_SUCCESS) {
                VLOG_ERR("Unable to set action value for ID: %d, error: %d", 
                         data_field_id, status);
                goto dealloc_resources;
            }
        }

        if (action == SWITCH_ACTION_ECMP) {
            status = tdi_action_name_to_id(table_info_hdl,
                                           LNW_IPV4_TABLE_ACTION_ECMP_HASH_ACTION,
                                           &action_id);
            if (status != TDI_SUCCESS) {
                VLOG_ERR("Unable to get action allocator ID for: %s, error: %d",
                         LNW_IPV4_TABLE_ACTION_ECMP_HASH_ACTION, status);
                goto dealloc_resources;
            }

            status = tdi_table_action_data_allocate(table_hdl, action_id, &data_hdl);
            if (status != TDI_SUCCESS) {
                VLOG_ERR("Unable to get action allocator for ID: %d, "
                         "error: %d", action_id, status);
                goto dealloc_resources;
            }

            status = tdi_data_field_id_with_action_get(table_info_hdl,
                                                       LNW_ACTION_ECMP_HASH_ACTION_PARAM_ECMP_GROUP_ID,
                                                       action_id, &data_field_id);
            if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to get data field id param for: %s, error: %d",
                     LNW_ACTION_ECMP_HASH_ACTION_PARAM_ECMP_GROUP_ID, status);
                goto dealloc_resources;
            }

            status = tdi_data_field_set_value(data_hdl, data_field_id,
                                              (api_route_entry->nhop_handle &
                                              ~(SWITCH_HANDLE_TYPE_ECMP_GROUP <<
                                               SWITCH_HANDLE_TYPE_SHIFT)));
            if (status != TDI_SUCCESS) {
                VLOG_ERR("Unable to set action value for ID: %d, error: %d",
                         data_field_id, status);
                goto dealloc_resources;
            }
        }

        status = tdi_table_entry_add(table_hdl, session, target_hdl,
                                     flags_hdl, key_hdl, data_hdl);
        if (status != TDI_SUCCESS) {
          VLOG_ERR("Unable to add rif_mod_table_start entry, error: %d", status);
            goto dealloc_resources;
        }
    } else {
        /* Delete an entry from target */
        VLOG_INFO("Delete ipv4_table entry");
        status = tdi_table_entry_del(table_hdl, session, target_hdl, 
                                     flags_hdl, key_hdl);
        if (status != TDI_SUCCESS) {
            VLOG_ERR("Unable to delete ipv4_table entry, error: %d", status);
            goto dealloc_resources;
        }
    }

dealloc_resources:
    status = tdi_switch_pd_deallocate_resources(flags_hdl, target_hdl,
                                                key_hdl, data_hdl,
                                                session, entry_add);
    return switch_pd_tdi_status_to_status(status);
}

switch_status_t switch_pd_ecmp_hash_table_entry(switch_device_t device,
    const switch_ecmp_info_t *ecmp_info, bool entry_add)
{
    tdi_status_t status;

    tdi_id_t field_id_group_id = 0;
    tdi_id_t field_id_hash = 0;
    tdi_id_t action_id = 0;
    tdi_id_t data_field_id = 0;

    tdi_dev_id_t dev_id = device;

    tdi_flags_hdl *flags_hdl = NULL;
    tdi_target_hdl *target_hdl = NULL;
    const tdi_device_hdl *dev_hdl = NULL;
    tdi_session_hdl *session = NULL;
    const tdi_info_hdl *info_hdl = NULL;
    tdi_table_key_hdl *key_hdl = NULL;
    tdi_table_data_hdl *data_hdl = NULL;
    const tdi_table_hdl *table_hdl = NULL;
    const tdi_table_info_hdl *table_info_hdl = NULL;
    uint32_t ecmp_list = 0;
    uint8_t nhop_count = 0;

    switch_node_t *node = NULL;
    switch_ecmp_member_t *ecmp_member = NULL;
    switch_handle_t ecmp_handle = SWITCH_API_INVALID_HANDLE;
    switch_handle_t nhop_handle = SWITCH_API_INVALID_HANDLE;

    VLOG_DBG("%s", __func__);

    status = tdi_flags_create(0, &flags_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create flags handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_device_get(dev_id, &dev_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get device handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_target_create(dev_hdl, &target_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create target handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_session_create(dev_hdl, &session);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to create tdi session, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_info_get(dev_id, PROGRAM_NAME, &info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Failed to get tdi info handle, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_table_from_name_get(info_hdl,
                                     LNW_ECMP_HASH_TABLE,
                                     &table_hdl);
    if (status != TDI_SUCCESS || !table_hdl) {
        VLOG_ERR("Unable to get table handle for: %s, error: %d",
                 LNW_ECMP_HASH_TABLE, status);
        goto dealloc_resources;
    }

    status = tdi_table_key_allocate(table_hdl, &key_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to allocate key handle for: %s, error: %d",
                 LNW_NEXTHOP_TABLE, status);
        goto dealloc_resources;
    }

    status = tdi_table_info_get(table_hdl, &table_info_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get table info handle for table, error: %d", status);
        goto dealloc_resources;
    }

    status = tdi_key_field_id_get(table_info_hdl,
                                  LNW_ECMP_HASH_TABLE_KEY_HOST_INFO_TX_EXTENDED_FLEX_0,
                                  &field_id_group_id);
    if (status != TDI_SUCCESS) {
      VLOG_ERR("Unable to get field ID for key: %s, error: %d",
               LNW_IPV4_TABLE_KEY_IPV4_DST_MATCH, status);
        goto dealloc_resources;
    }

    status = tdi_key_field_id_get(table_info_hdl,
                                  LNW_ECMP_HASH_TABLE_KEY_HASH,
                                  &field_id_hash);
    if (status != TDI_SUCCESS) {
      VLOG_ERR("Unable to get field ID for key: %s, error: %d",
               LNW_ECMP_HASH_TABLE_KEY_HASH, status);
        goto dealloc_resources;
    }

    status = tdi_action_name_to_id(table_info_hdl,
                                   LNW_ECMP_HASH_TABLE_ACTION_SET_NEXTHOP_ID,
                                   &action_id);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get action allocator ID for: %s, error: %d",
                 LNW_ECMP_HASH_TABLE_ACTION_SET_NEXTHOP_ID, status);
        goto dealloc_resources;
    }

    status = tdi_table_action_data_allocate(table_hdl, action_id, &data_hdl);
    if (status != TDI_SUCCESS) {
        VLOG_ERR("Unable to get action allocator for ID: %d, "
                 "error: %d", action_id, status);
        goto dealloc_resources;
    }

    status = tdi_data_field_id_with_action_get(table_info_hdl,
                                               LNW_ACTION_SET_NEXTHOP_ID_PARAM_NEXTHOP_ID,
                                               action_id, &data_field_id);
    if (status != TDI_SUCCESS) {
    VLOG_ERR("Unable to get data field id param for: %s, error: %d",
             LNW_ACTION_SET_NEXTHOP_ID_PARAM_NEXTHOP_ID, status);
        goto dealloc_resources;
    }

    ecmp_handle = ecmp_info->ecmp_group_handle;

    while (ecmp_list < LNW_ECMP_HASH_SIZE) {
        nhop_count = 0;
        FOR_EACH_IN_LIST(ecmp_info->members, node) {
            ecmp_member = (switch_ecmp_member_t *)node->data;
            nhop_handle = ecmp_member->nhop_handle;

            status = tdi_key_field_set_value(key_hdl, field_id_group_id,
                                             (ecmp_handle &
                                             ~(SWITCH_HANDLE_TYPE_ECMP_GROUP <<
                                             SWITCH_HANDLE_TYPE_SHIFT)));
            if (status != TDI_SUCCESS) {
                VLOG_ERR("Unable to set value for key ID: %d for ecmp_hash_table"
                         ", error: %d", field_id_group_id, status);
                goto dealloc_resources;
            }

            status = tdi_key_field_set_value(key_hdl, field_id_hash,
                                             ecmp_list + nhop_count);
            if (status != TDI_SUCCESS) {
                VLOG_ERR("Unable to set value for key ID: %d for ecmp_hash_table"
                         ", error: %d", field_id_hash, status);
                goto dealloc_resources;
            }

            if (entry_add) {
                status = tdi_data_field_set_value(data_hdl, data_field_id,
                                                  (nhop_handle &
                                                  ~(SWITCH_HANDLE_TYPE_NHOP <<
                                                  SWITCH_HANDLE_TYPE_SHIFT)));
                if (status != TDI_SUCCESS) {
                    VLOG_ERR("Unable to set action value for ID: %d, error: %d", 
                             data_field_id, status);
                    goto dealloc_resources;
                }

        status = tdi_table_entry_add(table_hdl, session, target_hdl,
                                     flags_hdl, key_hdl, data_hdl);
                if (status != TDI_SUCCESS) {
                VLOG_ERR("Unable to add ecmp_hash_table entry, error: %d", status);
                    goto dealloc_resources;
                }
          } else {
            /* Delete an entry from target */
            VLOG_INFO("Delete ecmp_hash_table entry");
        status = tdi_table_entry_del(table_hdl, session, target_hdl, 
                                     flags_hdl, key_hdl);
            if (status != TDI_SUCCESS) {
                VLOG_ERR("Unable to delete ecmp_hash_table entry, error: %d", status);
                goto dealloc_resources;
            }
        }
        nhop_count++;
     }
     FOR_EACH_IN_LIST_END();
     ecmp_list += nhop_count;
  }

dealloc_resources:
    status = tdi_switch_pd_deallocate_resources(flags_hdl, target_hdl,
                                                key_hdl, data_hdl,
                                                session, entry_add);
    return switch_pd_tdi_status_to_status(status);
}
