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
#include <openvswitch/util.h>
#include <openvswitch/vlog.h>

/* Local header includes */
#include "switch_nhop.h"
#include "switch_nhop_int.h"
#include "switch_handle_int.h"
#include "switch_internal.h"
#include "switch_rif_int.h"
#include "switch_rif.h"
#include "switch_neighbor_int.h"
#include "switch_pd_routing.h"

VLOG_DEFINE_THIS_MODULE(switch_nhop);

//add corresponding delete functions

switch_status_t switch_nhop_ecmp_member_list_add(
    switch_device_t device,
    switch_nhop_info_t *nhop_info,
    switch_handle_t ecmp_mem_handle) {
  PWord_t PValue;

  JLI(PValue, SWITCH_NHOP_ECMP_MEMBER_REF_LIST(nhop_info), ecmp_mem_handle);
  if (PValue == PJERR) {
    VLOG_ERR(
        "nhop add ecmp member Failed on device %d, "
        "nhop handle 0x%lx: , ecmp mem handle 0x%lx\n",
        device,
        nhop_info->nhop_handle,
        ecmp_mem_handle);
    return SWITCH_STATUS_FAILURE;
  }

  SWITCH_NHOP_NUM_ECMP_MEMBER_REF(nhop_info) += 1;
  VLOG_INFO(
      "nhop add ecmp member success on device %d, "
      "nhop handle 0x%lx: , ecmp mem handle 0x%lx: ref_cnt: %d\n",
      device,
      nhop_info->nhop_handle,
      ecmp_mem_handle,
      SWITCH_NHOP_NUM_ECMP_MEMBER_REF(nhop_info));

  return SWITCH_STATUS_SUCCESS;
}

switch_status_t switch_ecmp_member_handle_init(switch_device_t device) {
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  status = switch_handle_type_init(
      device, SWITCH_HANDLE_TYPE_ECMP_MEMBER, ECMP_HASH_TABLE_SIZE);

  if (status != SWITCH_STATUS_SUCCESS) {
     VLOG_ERR("ecmp member handle init Failed for device %d: %s\n",
                     device,
                     switch_error_to_string(status));
    return status;
  }

  return SWITCH_STATUS_SUCCESS;
}

switch_status_t switch_nhop_hash_key_init(void *args,
                                          switch_uint8_t *key,
                                          switch_uint32_t *len) {
  switch_nhop_key_t *nhop_key = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  if (!args || !key || !len) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    return status;
  }

  nhop_key = (switch_nhop_key_t *)args;
  *len = 0;

  SWITCH_MEMCPY((key + *len), &nhop_key->handle, sizeof(switch_handle_t));
  *len += sizeof(switch_handle_t);

  SWITCH_MEMCPY((key + *len), &nhop_key->ip_addr, sizeof(switch_ip_addr_t));
  *len += sizeof(switch_ip_addr_t);

  SWITCH_MEMCPY((key + *len), &nhop_key->vni, sizeof(switch_vni_t));
  *len += sizeof(switch_vni_t);

  SWITCH_ASSERT(*len == SWITCH_NHOP_HASH_KEY_SIZE);

  return status;
}

switch_int32_t switch_nhop_hash_compare(const void *key1, const void *key2) {
  switch_nhop_info_t *nhop_info = (switch_nhop_info_t *) key2;
  switch_nhop_key_t nhop_key;

  SWITCH_MEMSET(&nhop_key, 0x0, sizeof(switch_nhop_key_t));

  if (nhop_info) {
      SWITCH_MEMCPY(&nhop_key, &nhop_info->spath.nhop_key,
                    sizeof(switch_nhop_key_t));
  }

  return SWITCH_MEMCMP(key1, &nhop_key, SWITCH_NHOP_HASH_KEY_SIZE);
}

switch_status_t switch_nhop_init(switch_device_t device) {
  switch_nhop_context_t *nhop_ctx = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  nhop_ctx = SWITCH_MALLOC(device, sizeof(switch_nhop_context_t), 0x1);
  if (!nhop_ctx) {
    status = SWITCH_STATUS_NO_MEMORY;
    VLOG_ERR("nhop init: Failed to allocate memory for switch_nhop_context_t"
             " for device %d, error: %s\n",
             device,
             switch_error_to_string(status));
    return status;
  }

  status = switch_device_api_context_set(
      device, SWITCH_API_TYPE_NHOP, (void *)nhop_ctx);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("nhop init: Failed to set device api context for device %d,"
             " error: %s\n",
             device,
             switch_error_to_string(status));
    return status;
  }
  
  nhop_ctx->nhop_hashtable.size = NEXTHOP_TABLE_SIZE;
  nhop_ctx->nhop_hashtable.compare_func = switch_nhop_hash_compare;
  nhop_ctx->nhop_hashtable.key_func = switch_nhop_hash_key_init;
  nhop_ctx->nhop_hashtable.hash_seed = SWITCH_NHOP_HASH_SEED;
  
  status = SWITCH_HASHTABLE_INIT(&nhop_ctx->nhop_hashtable);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("nhop init Failed for device %d\n",
                     device);
    return status;
  }

  status = switch_handle_type_init(device, SWITCH_HANDLE_TYPE_NHOP,
                                   NEXTHOP_TABLE_SIZE);

  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("nhop init Failed for device %d\n",
                     device);
    return status;
  }

  status = switch_handle_type_init(device, SWITCH_HANDLE_TYPE_ECMP_GROUP,
                                   ECMP_HASH_TABLE_SIZE);

  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("ECMP group init Failed for device %d\n",
                     device);
    return status;
  }

  status = switch_ecmp_member_handle_init(device);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("ecmp member init Failed for device %d: %s\n",
                     device,
                     switch_error_to_string(status));
    return status;
  }
  
  return SWITCH_STATUS_SUCCESS;
}

switch_status_t switch_nhop_free(switch_device_t device) {
  switch_nhop_context_t *nhop_ctx = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  status = switch_device_api_context_get(
      device, SWITCH_API_TYPE_NHOP, (void **)&nhop_ctx);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("nhop free Failed for device %d\n",
                     device);
    return status;
  }

  status = SWITCH_HASHTABLE_DONE(&nhop_ctx->nhop_hashtable);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("nhop free Failed for device %d: , error: %s\n",
                     device,
                     switch_error_to_string(status));
  }

  status = switch_handle_type_free(device, SWITCH_HANDLE_TYPE_NHOP);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("nhop free Failed for device %d\n",
                     device);
  }

  status = switch_handle_type_free(device, SWITCH_HANDLE_TYPE_ECMP_GROUP);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("ECMP group free Failed for device %d\n",
                     device);
  }

  status = switch_handle_type_free(device, SWITCH_HANDLE_TYPE_ECMP_MEMBER);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("nhop free Failed for device %d: %s\n",
                     device,
                     switch_error_to_string(status));
    return status;
  }
  
  SWITCH_FREE(device, nhop_ctx);
  status = switch_device_api_context_set(device, SWITCH_API_TYPE_NHOP, NULL);
  SWITCH_ASSERT(status == SWITCH_STATUS_SUCCESS);
  return status;
}

switch_status_t switch_api_nhop_handle_get(
    const switch_device_t device,
    const switch_nhop_key_t *nhop_key,
    switch_handle_t *nhop_handle) {
  switch_nhop_context_t *nhop_ctx = NULL;
  switch_nhop_info_t *nhop_info = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  if (!nhop_key || !nhop_handle) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    VLOG_ERR("nhop key find Failed \n");
    return status;
  }

  status = switch_device_api_context_get(
      device, SWITCH_API_TYPE_NHOP, (void **)&nhop_ctx);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("nhop_key find Failed \n");
    return status;
  }

  status = SWITCH_HASHTABLE_SEARCH(
      &nhop_ctx->nhop_hashtable, (void *)nhop_key, (void **)&nhop_info);
  if (status == SWITCH_STATUS_SUCCESS) {
    *nhop_handle = nhop_info->nhop_handle;
  } else {
    VLOG_DBG("Unable to find the entry in hashtable");
  }

  return status;
}

switch_status_t switch_api_neighbor_handle_get (
    const switch_device_t device,
    const switch_handle_t nhop_handle,
    switch_handle_t *neighbor_handle) {
  switch_nhop_info_t *nhop_info = NULL;
  switch_spath_info_t *spath_info = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  
  if (!SWITCH_NHOP_HANDLE(nhop_handle)) {
    status = SWITCH_STATUS_INVALID_HANDLE;
    VLOG_ERR(
        "neighbor handle get Failed for "
        "device %d handle 0x%lx: %s\n",
        device,
        nhop_handle,
        switch_error_to_string(status));
    return status;
  }

  status = switch_nhop_get(device, nhop_handle, &nhop_info);
  if (status != SWITCH_STATUS_SUCCESS) {
    status = SWITCH_STATUS_INVALID_HANDLE;
    VLOG_ERR(
        "neighbor handle get Failed for "
        "device %d handle 0x%lx: %s\n",
        device,
        nhop_handle,
        switch_error_to_string(status));
    return status;
  }

  spath_info = &(SWITCH_NHOP_SPATH_INFO(nhop_info));
  *neighbor_handle = spath_info->neighbor_handle;

  return status;
}

switch_status_t switch_api_ecmp_create (const switch_device_t device,
                                                switch_handle_t *ecmp_handle) {
  switch_ecmp_info_t *ecmp_info = NULL;
  switch_handle_t handle = SWITCH_API_INVALID_HANDLE;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  handle = switch_ecmp_group_handle_create(device, 0);
  if (handle == SWITCH_API_INVALID_HANDLE) {
    status = SWITCH_STATUS_NO_MEMORY;
    VLOG_ERR(
        "ecmp create Failed on device %d "
        "handle create Failed:(%s)\n",
        device,
        switch_error_to_string(status));
    return status;
  }

  status = switch_ecmp_group_get(device, handle, &ecmp_info);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR(
        "ecmp create Failed on device %d "
        "ecmp get Failed:(%s)\n",
        device,
        switch_error_to_string(status));
    return status;
  }

  ecmp_info->ecmp_group_handle = handle;

  SWITCH_LIST_INIT(&(ecmp_info->members));

  *ecmp_handle = handle;

  VLOG_INFO("ecmp handle created on device %d ecmp handle 0x%lx\n",
                   device,
                   *ecmp_handle);

  return status;
}

switch_status_t switch_api_ecmp_member_add (
    const switch_device_t device,
    const switch_handle_t ecmp_handle,
    const switch_uint32_t num_nhops,
    const switch_handle_t *nhop_handles,
    switch_handle_t *member_handle) {
  switch_uint32_t index = 0;
  switch_handle_t nhop_handle = 0;
  switch_ecmp_info_t *ecmp_info = NULL;
  switch_nhop_info_t *nhop_info = NULL;
  switch_ecmp_member_t *ecmp_member = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  switch_handle_t ecmp_member_handle = SWITCH_API_INVALID_HANDLE;

  if (num_nhops == 0 || !nhop_handles || !ecmp_handle) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    VLOG_ERR(
        "ecmp member add Failed on device %d ecmp handle 0x%lx: "
        "parameters invalid:(%s)\n",
        device,
        ecmp_handle,
        switch_error_to_string(status));
    return status;
  }

  SWITCH_ASSERT(SWITCH_ECMP_HANDLE(ecmp_handle));
  status = switch_ecmp_group_get(device, ecmp_handle, &ecmp_info);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR(
        "ecmp member add Failed on device %d ecmp handle 0x%lx: "
        "ecmp get Failed:(%s)\n",
        device,
        ecmp_handle,
        switch_error_to_string(status));
    return status;
  }

  for (index = 0; index < num_nhops; index++) {
    nhop_handle = nhop_handles[index];

    SWITCH_ASSERT(SWITCH_NHOP_HANDLE(nhop_handle));
    status = switch_nhop_get(device, nhop_handle, &nhop_info);
    if (status != SWITCH_STATUS_SUCCESS) {
      status = SWITCH_STATUS_INVALID_HANDLE;
      VLOG_ERR(
          "ecmp member add Failed on device %d ecmp handle 0x%lx "
          "nhop handle 0x%lx: "
          "nhop get Failed:(%s)\n",
          device,
          ecmp_handle,
          nhop_handle,
          switch_error_to_string(status));
      return status;
    }

    ecmp_member_handle = switch_ecmp_member_handle_create(device);
    if (ecmp_member_handle == SWITCH_API_INVALID_HANDLE) {
      status = SWITCH_STATUS_NO_MEMORY;
      VLOG_ERR(
          "ecmp member add Failed on device %d ecmp handle 0x%lx "
          "nhop handle 0x%lx: "
          "ecmp member create Failed:(%s)\n",
          device,
          ecmp_handle,
          nhop_handle,
          switch_error_to_string(status));
      return status;
    }

    status = switch_ecmp_member_get(device, ecmp_member_handle, &ecmp_member);
    if (status != SWITCH_STATUS_SUCCESS) {
      VLOG_ERR(
          "ecmp member add Failed on device %d ecmp handle 0x%lx "
          "nhop handle 0x%lx: "
          "ecmp member get Failed:(%s)\n",
          device,
          ecmp_handle,
          nhop_handle,
          switch_error_to_string(status));
      return status;
    }

    SWITCH_ECMP_MEMBER_INIT(ecmp_member);
    ecmp_member->member_handle = ecmp_member_handle;
    ecmp_member->ecmp_handle = ecmp_handle;
    ecmp_member->nhop_handle = nhop_handle;
    ecmp_member->active = TRUE;

    status = SWITCH_LIST_INSERT(
        &(ecmp_info->members), &(ecmp_member->node), ecmp_member);
    SWITCH_ASSERT(status == SWITCH_STATUS_SUCCESS);

    status =
        switch_nhop_ecmp_member_list_add(device, nhop_info, ecmp_member_handle);
    if (status != SWITCH_STATUS_SUCCESS) {
      VLOG_ERR(
          "Failed to add ecmp member_handle to nhop , device %d"
          " nhop handle 0x%lx: "
          " ecmp_member_handle: 0x%lx"
          " with status (%s)\n",
          device,
          nhop_info->nhop_handle,
          ecmp_member_handle,
          switch_error_to_string(status));
      return status;
    }
  }
  *member_handle = ecmp_member_handle;
  /* TODO: add the code to distribute the hash among different neighbor IDs
   * based on number of nhops here update the table for each hash-nhop entry */
  VLOG_INFO(
      "ecmp member add on device %d ecmp handle 0x%lx num nhops %d\n",
      device,
      ecmp_handle,
      num_nhops);

  return status;
}

switch_status_t switch_api_ecmp_delete(const switch_device_t device,
                                       const switch_handle_t ecmp_handle) {
  switch_uint32_t index = 0;
  switch_node_t *node = NULL;
  switch_uint32_t num_nhops = 0;
  switch_handle_t *nhop_handles = NULL;
  switch_ecmp_info_t *ecmp_info = NULL;
  switch_ecmp_member_t *ecmp_member = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  SWITCH_ASSERT(SWITCH_ECMP_HANDLE(ecmp_handle));
  if (!SWITCH_ECMP_HANDLE(ecmp_handle)) {
    status = SWITCH_STATUS_INVALID_HANDLE;
    VLOG_ERR(
        "ecmp delete failed on device %d ecmp handle 0x%lx: "
        "ecmp handle invalid:(%s)\n",
        device,
        ecmp_handle,
        switch_error_to_string(status));
    return status;
  }

  status = switch_ecmp_group_get(device, ecmp_handle, &ecmp_info);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR(
        "ecmp delete failed on device %d ecmp handle 0x%lx: "
        "ecmp get failed:(%s)\n",
        device,
        ecmp_handle,
        switch_error_to_string(status));
    return status;
  }

  num_nhops = SWITCH_LIST_COUNT(&ecmp_info->members);

  if (num_nhops) {
    nhop_handles = SWITCH_MALLOC(device, sizeof(switch_handle_t), num_nhops);
    if (!nhop_handles) {
      status = SWITCH_STATUS_NO_MEMORY;
      VLOG_ERR(
          "ecmp delete failed on device %d ecmp handle 0x%lx: "
          "memory allocation failed:(%s)\n",
          device,
          ecmp_handle,
          switch_error_to_string(status));
      return status;
    }

    FOR_EACH_IN_LIST(ecmp_info->members, node) {
      ecmp_member = (switch_ecmp_member_t *)node->data;
      nhop_handles[index++] = ecmp_member->nhop_handle;
    }
    FOR_EACH_IN_LIST_END();

    status = switch_api_ecmp_member_delete(
        device, ecmp_handle, num_nhops, nhop_handles);
    if (status != SWITCH_STATUS_SUCCESS) {
      VLOG_ERR(
          "ecmp delete failed on device %d ecmp handle 0x%lx: "
          "ecmp member delete failed:(%s)\n",
          device,
          ecmp_handle,
          switch_error_to_string(status));
      SWITCH_FREE(device, nhop_handles);
      return status;
    }
    SWITCH_FREE(device, nhop_handles);
  }

  status = switch_ecmp_handle_delete(device, ecmp_handle);
  SWITCH_ASSERT(status == SWITCH_STATUS_SUCCESS);

  VLOG_DBG("ecmp handle deleted on device %d ecmp handle 0x%lx\n",
                   device,
                   ecmp_handle);

  return status;
}

switch_status_t switch_api_ecmp_nhop_by_member_get(
    const switch_device_t device,
    const switch_handle_t ecmp_member_handle,
    switch_handle_t *ecmp_handle,
    switch_handle_t *nhop_handle) {
  switch_ecmp_member_t *ecmp_member = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  SWITCH_ASSERT(SWITCH_ECMP_MEMBER_HANDLE(ecmp_member_handle));
  status = switch_ecmp_member_get(device, ecmp_member_handle, &ecmp_member);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR(
        "ecmp member get failed on device %d "
        "ecmp handle 0x%lx: invalid ecmp handle(%s)",
        device,
        ecmp_member_handle,
        switch_error_to_string(status));
    return status;
  }

  *ecmp_handle = ecmp_member->ecmp_handle;
  *nhop_handle = ecmp_member->nhop_handle;

  return status;
}

switch_status_t switch_ecmp_member_get_from_nhop(
    const switch_device_t device,
    const switch_handle_t ecmp_handle,
    const switch_handle_t nhop_handle,
    switch_ecmp_member_t **ecmp_member) {
  switch_ecmp_info_t *ecmp_info = NULL;
  switch_ecmp_member_t *tmp_ecmp_member = NULL;
  switch_node_t *node = NULL;
  bool member_found = FALSE;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  if (!ecmp_member) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    VLOG_ERR("Failed to get ecmp member on device %d: %s",
                     device,
                     switch_error_to_string(status));
    return status;
  }

  SWITCH_ASSERT(SWITCH_ECMP_HANDLE(ecmp_handle));
  if (!SWITCH_ECMP_HANDLE(ecmp_handle)) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    VLOG_ERR("Failed to get ecmp member on device %d: %s",
                     device,
                     switch_error_to_string(status));
    return status;
  }

  SWITCH_ASSERT(SWITCH_NHOP_HANDLE(nhop_handle));
  if (!SWITCH_NHOP_HANDLE(nhop_handle)) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    VLOG_ERR("Failed to get ecmp member on device %d: %s",
                     device,
                     switch_error_to_string(status));
    return status;
  }

  status = switch_ecmp_group_get(device, ecmp_handle, &ecmp_info);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("Failed to get ecmp member on device %d: %s",
                     device,
                     switch_error_to_string(status));
    return status;
  }

  FOR_EACH_IN_LIST(ecmp_info->members, node) {
    tmp_ecmp_member = (switch_ecmp_member_t *)node->data;
    if (tmp_ecmp_member->nhop_handle == nhop_handle) {
      member_found = TRUE;
      *ecmp_member = tmp_ecmp_member;
      break;
    }
  }
  FOR_EACH_IN_LIST_END();

  if (!member_found) {
    *ecmp_member = NULL;
    status = SWITCH_STATUS_ITEM_NOT_FOUND;
  }

  return status;
}

switch_status_t switch_nhop_ecmp_member_list_remove(
    switch_device_t device,
    switch_nhop_info_t *nhop_info,
    switch_handle_t ecmp_mem_handle) {
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  int Rc_int;
  JLD(Rc_int, SWITCH_NHOP_ECMP_MEMBER_REF_LIST(nhop_info), ecmp_mem_handle);
  if (Rc_int != 1) {
    VLOG_ERR(
        "nhop remove ecmp mem Failed on device %d, "
        "nhop handle 0x%lx: , ecmp mem handle 0x%lx\n",
        device,
        nhop_info->nhop_handle,
        ecmp_mem_handle);
    return SWITCH_STATUS_FAILURE;
  }

  SWITCH_NHOP_NUM_ECMP_MEMBER_REF(nhop_info) -= 1;
  VLOG_INFO(
      "nhop remove ecmp mem success on device %d, "
      "nhop handle 0x%lx: , ecmp mem handle 0x%lx: ref_cnt: %d\n",
      device,
      nhop_info->nhop_handle,
      ecmp_mem_handle,
      SWITCH_NHOP_NUM_ECMP_MEMBER_REF(nhop_info));

  return status;
}

switch_status_t switch_api_ecmp_member_delete(
    const switch_device_t device,
    const switch_handle_t ecmp_handle,
    const switch_uint32_t num_nhops,
    const switch_handle_t *nhop_handles) {
  switch_nhop_info_t *nhop_info = NULL;
  switch_ecmp_info_t *ecmp_info = NULL;
  switch_ecmp_member_t *ecmp_member = NULL;
  switch_handle_t nhop_handle = 0;
  switch_handle_t member_handle = SWITCH_API_INVALID_HANDLE;
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  switch_uint32_t index = 0;

  if (num_nhops == 0 || !nhop_handles) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    VLOG_ERR(
        "ecmp member delete Failed on device %d ecmp handle 0x%lx: "
        "parameters invalid:(%s)\n",
        device,
        ecmp_handle,
        switch_error_to_string(status));
    return status;
  }

  if (!SWITCH_ECMP_HANDLE(ecmp_handle)) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    VLOG_ERR(
        "ecmp member delete Failed on device %d ecmp handle 0x%lx: "
        "ecmp handle invalid:(%s)\n",
        device,
        ecmp_handle,
        switch_error_to_string(status));
    return status;
  }

  status = switch_ecmp_group_get(device, ecmp_handle, &ecmp_info);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR(
        "ecmp member delete Failed on device %d ecmp handle 0x%lx: "
        "ecmp get Failed:(%s)\n",
        device,
        ecmp_handle,
        switch_error_to_string(status));
    return status;
  }

  for (index = 0; index < num_nhops; index++) {
    nhop_handle = nhop_handles[index];

    if (!SWITCH_NHOP_HANDLE(nhop_handle)) {
      status = SWITCH_STATUS_INVALID_HANDLE;
      VLOG_ERR(
          "ecmp member delete Failed on device %d ecmp handle 0x%lx "
          "nhop handle 0x%lx: "
          "nhop handle invalid:(%s)\n",
          device,
          ecmp_handle,
          nhop_handle,
          switch_error_to_string(status));
      return status;
    }

    status = switch_nhop_get(device, nhop_handle, &nhop_info);
    if (status != SWITCH_STATUS_SUCCESS) {
      VLOG_ERR(
          "ecmp member delete Failed on device %d ecmp handle 0x%lx "
          "nhop handle 0x%lx: "
          "nhop get Failed:(%s)\n",
          device,
          ecmp_handle,
          nhop_handle,
          switch_error_to_string(status));
      return status;
    }

    status = switch_ecmp_member_get_from_nhop(
        device, ecmp_handle, nhop_handle, &ecmp_member);
    if (status != SWITCH_STATUS_SUCCESS) {
      VLOG_ERR(
          "ecmp member delete Failed on device %d ecmp handle 0x%lx "
          "nhop handle 0x%lx: "
          "ecmp member from nhop Failed:(%s)\n",
          device,
          ecmp_handle,
          nhop_handle,
          switch_error_to_string(status));
      return status;
    }

    member_handle = ecmp_member->member_handle;
    status =
        switch_nhop_ecmp_member_list_remove(device, nhop_info, member_handle);
    if (status != SWITCH_STATUS_SUCCESS) {
      VLOG_ERR(
          "ecmp member delete Failed on device %d ecmp handle 0x%lx "
          "nhop handle 0x%lx: "
          "ecmp member list remove Failed:(%s)\n",
          device,
          ecmp_handle,
          nhop_handle,
          switch_error_to_string(status));
      return status;
    }

    status = SWITCH_LIST_DELETE(&(ecmp_info->members), &(ecmp_member->node));
    SWITCH_ASSERT(status == SWITCH_STATUS_SUCCESS);

    status = switch_ecmp_member_handle_delete(device, member_handle);
    SWITCH_ASSERT(status == SWITCH_STATUS_SUCCESS);
  }

  VLOG_INFO(
      "ecmp member deleted on device %d ecmp handle 0x%lx num nhops %d\n",
      device,
      ecmp_handle,
      num_nhops);

  return status;
}

switch_status_t switch_api_ecmp_members_delete (
    switch_device_t device, switch_handle_t ecmp_handle) {
  switch_uint32_t index = 0;
  switch_uint32_t num_nhops = 0;
  switch_node_t *node = NULL;
  switch_ecmp_info_t *ecmp_info = NULL;
  switch_handle_t *nhop_handles = NULL;
  switch_ecmp_member_t *ecmp_member = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  SWITCH_ASSERT(SWITCH_ECMP_HANDLE(ecmp_handle));
  if (!SWITCH_ECMP_HANDLE(ecmp_handle)) {
    status = SWITCH_STATUS_INVALID_HANDLE;
    VLOG_ERR(
        "ecmp members delete Failed on device %d ecmp handle 0x%lx: "
        "ecmp handle invalid:(%s)\n",
        device,
        ecmp_handle,
        switch_error_to_string(status));
    return status;
  }

  status = switch_ecmp_group_get(device, ecmp_handle, &ecmp_info);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR(
        "ecmp members delete Failed on device %d ecmp handle 0x%lx: "
        "ecmp get Failed:(%s)\n",
        device,
        ecmp_handle,
        switch_error_to_string(status));
    return status;
  }

  if (ecmp_info->id_type != SWITCH_NHOP_ID_TYPE_ECMP) {
    status = SWITCH_STATUS_INVALID_HANDLE;
    VLOG_ERR(
        "ecmp members delete Failed on device %d ecmp handle 0x%lx: "
        "handle type not ecmp:(%s)\n",
        device,
        ecmp_handle,
        switch_error_to_string(status));
    return status;
  }

  num_nhops = SWITCH_LIST_COUNT(&ecmp_info->members);

  if (num_nhops) {
    nhop_handles = SWITCH_MALLOC(device, sizeof(switch_handle_t), num_nhops);
    if (!nhop_handles) {
      status = SWITCH_STATUS_NO_MEMORY;
      VLOG_ERR(
          "ecmp members delete Failed on device %d ecmp handle 0x%lx: "
          "nhop handles malloc Failed:(%s)\n",
          device,
          ecmp_handle,
          switch_error_to_string(status));
      return status;
    }

    FOR_EACH_IN_LIST(ecmp_info->members, node) {
      ecmp_member = (switch_ecmp_member_t *)node->data;
      nhop_handles[index++] = ecmp_member->nhop_handle;
    }
    FOR_EACH_IN_LIST_END();

    status = switch_api_ecmp_member_delete(
        device, ecmp_handle, num_nhops, nhop_handles);
    if (status != SWITCH_STATUS_SUCCESS) {
      VLOG_ERR(
          "ecmp members delete Failed on device %d ecmp handle 0x%lx: "
          "ecmp member delete Failed:(%s)\n",
          device,
          ecmp_handle,
          switch_error_to_string(status));
      SWITCH_FREE(device, nhop_handles);
      return status;
    }
    SWITCH_FREE(device, nhop_handles);
  }

  VLOG_INFO("ecmp members deleted on device %d ecmp handle 0x%lx\n",
                   device,
                   ecmp_handle);

  return status;
}

switch_status_t switch_api_nhop_create(
    const switch_device_t device,
    const switch_api_nhop_info_t *api_nhop_info,
    switch_handle_t *nhop_handle) {
  switch_nhop_context_t *nhop_ctx = NULL;
  switch_nhop_info_t *nhop_info = NULL;
  switch_spath_info_t *spath_info = NULL;
  switch_nhop_key_t nhop_key = {0};
  switch_handle_t handle = SWITCH_API_INVALID_HANDLE;
  switch_status_t status = SWITCH_STATUS_SUCCESS;
  switch_pd_routing_info_t pd_routing_info; //TODO

  memset(&pd_routing_info, 0, sizeof(switch_pd_routing_info_t));

  status = switch_device_api_context_get(
      device, SWITCH_API_TYPE_NHOP, (void **)&nhop_ctx);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR(
        "nhop create Failed on device %d: "
        "nhop context get Failed:(%s)\n",
        device,
        switch_error_to_string(status));
    return status;
  }

  SWITCH_ASSERT(api_nhop_info != NULL);
  SWITCH_ASSERT(nhop_handle != NULL);
  if (!api_nhop_info || !nhop_handle) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    VLOG_ERR(
        "nhop create Failed on device %d: "
        "parameters invalid:(%s)\n",
        device,
        switch_error_to_string(status));
    return status;
  }

  *nhop_handle = SWITCH_API_INVALID_HANDLE;

  SWITCH_NHOP_KEY_GET(api_nhop_info, nhop_key);
  status = switch_api_nhop_handle_get(device, &nhop_key, &handle);
  if (status != SWITCH_STATUS_SUCCESS &&
      status != SWITCH_STATUS_ITEM_NOT_FOUND) {
    VLOG_ERR(
        "nhop create Failed on device %d: "
        "nhop get Failed:(%s)\n",
        device,
        switch_error_to_string(status));
    return status;
  }

  if (status == SWITCH_STATUS_SUCCESS) {
    VLOG_INFO(
        "nhop create Failed on device %d nhop handle 0x%lx: "
        "nhop already exists\n",
        device,
        handle);
    status = switch_nhop_get(device, handle, &nhop_info);
    SWITCH_ASSERT(status == SWITCH_STATUS_SUCCESS);
    nhop_info->nhop_ref_count++;
    *nhop_handle = handle;
    return status;
  }

  handle = switch_nhop_handle_create(device, 1);
  if (handle == SWITCH_API_INVALID_HANDLE) {
    status = SWITCH_STATUS_NO_MEMORY;
    VLOG_ERR(
        "nhop create Failed on device %d "
        "nhop handle create Failed:(%s)\n",
        device,
        switch_error_to_string(status));
    return status;
  }

  status = switch_nhop_get(device, handle, &nhop_info);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR(
        "nhop create Failed on device %d "
        "nhop get Failed:(%s)\n",
        device,
        switch_error_to_string(status));
    return status;
  }

  SWITCH_NHOP_NUM_ECMP_MEMBER_REF(nhop_info) = 0;
  SWITCH_NHOP_ECMP_MEMBER_REF_LIST(nhop_info) = (Pvoid_t)NULL;
  nhop_info->id_type = SWITCH_NHOP_ID_TYPE_ONE_PATH;
  nhop_info->nhop_ref_count = 1;
  nhop_info->flags = 0;

  /* currently support export/import of only one nhop per rif */
  nhop_info->nhop_handle = handle;
  pd_routing_info.nexthop_handle = handle;

  spath_info = &(SWITCH_NHOP_SPATH_INFO(nhop_info));
  spath_info->nhop_handle = handle;
  spath_info->tunnel = FALSE;

  SWITCH_MEMCPY(&spath_info->api_nhop_info,
                api_nhop_info,
                sizeof(switch_api_nhop_info_t));
  SWITCH_MEMCPY(&spath_info->nhop_key, &nhop_key, sizeof(nhop_key));

 /* Backend programming for nhop create is not needed because Neighbor
  * create will call a nexthop create always before creating neighbor.
  * So, backend programming should happen with Neighbor create only,
  * by the time neighbor create happens already nexthop would have been created. 
  * Vice-versa will not happen */
  SWITCH_MEMCPY(&nhop_info->switch_pd_routing_info,
                &pd_routing_info,
                sizeof(switch_pd_routing_info_t));

  status = SWITCH_HASHTABLE_INSERT(&nhop_ctx->nhop_hashtable,
                                   &(nhop_info->node),
                                   (void *)&nhop_key,
                                   (void *)nhop_info);
  SWITCH_ASSERT(status == SWITCH_STATUS_SUCCESS);

  *nhop_handle = handle;

  VLOG_INFO(
      "nhop created on device %d nhop handle 0x%lx\n", device, handle);

  return status;
}

switch_status_t switch_api_nhop_delete(
    const switch_device_t device, const switch_handle_t nhop_handle) {
  switch_nhop_context_t *nhop_ctx = NULL;
  switch_nhop_info_t *nhop_info = NULL;
  switch_api_nhop_info_t *api_nhop_info = NULL;
  switch_spath_info_t *spath_info = NULL;
  switch_nhop_key_t nhop_key = {0};
  switch_status_t status = SWITCH_STATUS_SUCCESS;



  status = switch_device_api_context_get(
      device, SWITCH_API_TYPE_NHOP, (void **)&nhop_ctx);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR(
        "nhop delete Failed on device %d nhop handle 0x%lx: "
        "nhop device context get Failed:(%s)\n",
        device,
        nhop_handle,
        switch_error_to_string(status));
    return status;
  }

  if (!SWITCH_NHOP_HANDLE(nhop_handle)) {
    status = SWITCH_STATUS_INVALID_HANDLE;
    VLOG_ERR(
        "nhop delete Failed on device %d nhop handle 0x%lx: "
        "nhop handle invalid:(%s)\n",
        device,
        nhop_handle,
        switch_error_to_string(status));
    return status;
  }

  status = switch_nhop_get(device, nhop_handle, &nhop_info);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR(
        "nhop delete Failed on device %d nhop handle 0x%lx: "
        "nhop get Failed:(%s)\n",
        device,
        nhop_handle,
        switch_error_to_string(status));
    return status;
  }

  if (nhop_info->id_type != SWITCH_NHOP_ID_TYPE_ONE_PATH) {
    status = SWITCH_STATUS_INVALID_HANDLE;
    VLOG_ERR(
        "nhop delete Failed on device %d nhop handle 0x%lx: "
        "nhop id type invalid:(%s)\n",
        device,
        nhop_handle,
        switch_error_to_string(status));
    return status;
  }

  if (nhop_info->nhop_ref_count > 1) {
    nhop_info->nhop_ref_count--;
    return status;
  }

  spath_info = &(SWITCH_NHOP_SPATH_INFO(nhop_info));
  api_nhop_info = &spath_info->api_nhop_info;
  SWITCH_NHOP_KEY_GET(api_nhop_info, nhop_key);
  if (SWITCH_NHOP_NUM_ECMP_MEMBER_REF(nhop_info) > 0) {
    nhop_info->flags |= SWITCH_NHOP_MARK_TO_BE_DELETED;
    status = SWITCH_STATUS_RESOURCE_IN_USE;
    VLOG_ERR(
        "nhop delete Failed on device %d nhop handle 0x%lx: "
        "nhop is still in use, mark to free: %s\n",
        device,
        nhop_handle,
        switch_error_to_string(status));
    return status;
  }

  status = SWITCH_HASHTABLE_DELETE(
      &nhop_ctx->nhop_hashtable, (void *)(&nhop_key), (void **)&nhop_info);
  SWITCH_ASSERT(status == SWITCH_STATUS_SUCCESS);

  status = switch_nhop_handle_delete(device, nhop_handle, 1);
  SWITCH_ASSERT(status == SWITCH_STATUS_SUCCESS);

  VLOG_INFO(
      "nhop deleted on device %d nhop handle 0x%lx\n", device, nhop_handle);

  return status;
}

switch_status_t switch_api_nhop_id_type_get(
    const switch_device_t device,
    const switch_handle_t nhop_handle,
    switch_nhop_id_type_t *nhop_type) {
  switch_nhop_info_t *nhop_info = NULL;
  switch_status_t status = SWITCH_STATUS_SUCCESS;

  if (!nhop_type || !nhop_handle) {
    status = SWITCH_STATUS_INVALID_PARAMETER;
    VLOG_ERR("nhop id type find Failed due to invalid parameters \n");
    return status;
  }

  *nhop_type = SWITCH_NHOP_ID_TYPE_NONE;

  if (!SWITCH_NHOP_HANDLE(nhop_handle)) {
    status = SWITCH_STATUS_INVALID_HANDLE;
    VLOG_ERR(
        "nhop id type get Failed for "
        "device %d handle 0x%lx: %s\n",
        device,
        nhop_handle,
        switch_error_to_string(status));
    return status;
  }

  status = switch_nhop_get(device, nhop_handle, &nhop_info);
  if (status != SWITCH_STATUS_SUCCESS) {
    VLOG_ERR("Failed to get nhop info on handle 0x%lx, error: %s\n",
             nhop_handle,
             switch_error_to_string(status));
    return status;
  }

  if(nhop_info) {
     *nhop_type = nhop_info->id_type;
  } else {
     VLOG_ERR("nhop info was null, status = %d", status);
  }

  return status;
}
