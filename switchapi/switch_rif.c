/*
Copyright 2013-present Barefoot Networks, Inc.
Copyright(c) 2021 Intel Corporation.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <config.h>
#include "switch_rif.h"
#include <openvswitch/util.h>
#include <openvswitch/vlog.h>
#include <net/if.h>

/* Local header includes */
#include "switch_internal.h"
#include "switch_rif_int.h"
#include "switch_base_types.h"
#include "switch_status.h"
#include "switch_device.h"
#include "switch_pd_utils.h"

#include <bf_types/bf_types.h>
#include "bf_pal/bf_pal_port_intf.h"

VLOG_DEFINE_THIS_MODULE(switch_rif);

/*
 * Routine Description:
 *   @brief initialize rif context and structs
 *
 * Arguments:
 *   @param[in] device - device
 *
 * Return Values:
 *    @return  SWITCH_STATUS_SUCCESS on success
 *             Failure status code on error
 */
switch_status_t switch_rif_init(switch_device_t device) {
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  VLOG_INFO("%s", __func__);

  status =
      switch_handle_type_init(device, SWITCH_HANDLE_TYPE_RIF, SWITCH_RIF_MAX);
  CHECK_RET(status != SWITCH_STATUS_SUCCESS, status);

  VLOG_INFO("rif init successful on device %d\n", device);

  return status;
}

/*
 * Routine Description:
 *   @brief uninitialize vrf context and structs
 *
 * Arguments:
 *   @param[in] device - device
 *
 * Return Values:
 *    @return  SWITCH_STATUS_SUCCESS on success
 *             Failure status code on error
 */
switch_status_t switch_rif_free(switch_device_t device) {
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  VLOG_INFO("%s", __func__);

  status = switch_handle_type_free(device, SWITCH_HANDLE_TYPE_RIF);
  CHECK_RET(status != SWITCH_STATUS_SUCCESS, status);

  VLOG_INFO("RIF free successful on device %d\n", device);

  return status;
}

switch_status_t switch_api_rif_attribute_get(
    const switch_device_t device,
    const switch_handle_t rif_handle,
    const switch_uint64_t rif_flags,
    switch_api_rif_info_t *api_rif_info) {
  switch_rif_info_t *rif_info = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  VLOG_INFO("%s", __func__);

  if (!SWITCH_RIF_HANDLE(rif_handle)) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    VLOG_ERR(
        "rif attribute get failed on device %d "
        "rif handle 0x%lx: "
        "rif handle invalid(%s)\n",
        device,
        rif_handle,
        switch_error_to_string(status));
    return status;
  }

  status = switch_rif_get(device, rif_handle, &rif_info);
  CHECK_RET(status != SWITCH_STATUS_SUCCESS, status);

  /* just get all attributes? */
  *api_rif_info = rif_info->api_rif_info;

  return status;
}

switch_status_t switch_api_rif_create(
    switch_device_t device,
    switch_api_rif_info_t *api_rif_info,
    switch_handle_t *rif_handle) {
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  switch_rif_info_t *rif_info = NULL;

  VLOG_INFO("%s", __func__);

  if (api_rif_info->rmac_handle == SWITCH_API_INVALID_HANDLE) {
    status = switch_api_device_default_rmac_handle_get(
        device, &api_rif_info->rmac_handle);
    CHECK_RET(status != SWITCH_STATUS_SUCCESS, status);
  }

  if (!SWITCH_RMAC_HANDLE(api_rif_info->rmac_handle)) {
    status = SWITCH_STATUS_INVALID_HANDLE;
    VLOG_ERR(
        "rif create failed on device %d: "
        "rmac or vrf handle invalid(%s)\n",
        device,
        switch_error_to_string(device));
    return status;
  }

  *rif_handle = switch_rif_handle_create(device);
  CHECK_RET(*rif_handle == SWITCH_API_INVALID_HANDLE, SWITCH_STATUS_NO_MEMORY);

  status = switch_rif_get(device, *rif_handle, &rif_info);
  CHECK_RET(status != SWITCH_STATUS_SUCCESS, status);

  // When multipipe support is available in P4-OVS, make port_id as 
  // in_port_id and out_port_id. Use accordingly for respective pipelines.
  api_rif_info->port_id = switch_pd_to_get_port_id(api_rif_info->rif_ifindex);

  SWITCH_MEMCPY(&rif_info->api_rif_info,
                api_rif_info,
                sizeof(switch_api_rif_info_t));
  
  return status;
}

switch_status_t switch_api_rif_delete(switch_device_t device,
                                               switch_handle_t rif_handle) {
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  switch_rif_info_t *rif_info = NULL;

  VLOG_INFO("%s", __func__);

  status = switch_rif_get(device, rif_handle, &rif_info);
  CHECK_RET(status != SWITCH_STATUS_SUCCESS, status);

  status = switch_rif_handle_delete(device, rif_handle);
  CHECK_RET(status != SWITCH_STATUS_SUCCESS, status);

  return status;
}
