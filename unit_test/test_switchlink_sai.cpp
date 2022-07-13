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

//This is an example test case file with dummy function calls.

#include "gmock/gmock.h"
#include "gmock-global.h"
#include "p4proto/kctrl/switchlink/switchlink_sai.c"

#define MAC_LENGTH 6

sai_status_t mock_fdb_remove_entry( const sai_fdb_entry_t *fdb_entry)
{
    if (fdb_entry->mac_address[0] == 0xff)
        return -1;
    else 
        return 0;
}

sai_status_t mock_fdb_create_entry( const sai_fdb_entry_t *fdb_entry)
{
    if (fdb_entry->mac_address[0] == 0xff)
        return -1;
    else
        return 0;
}

sai_status_t mock_vrf_create_entry( const sai_virtual_router_api_t *vrf_entry)
{
    return 0;
}

sai_status_t mock_vrf_create_entry_2( const sai_virtual_router_api_t *vrf_entry)
{
    return -1;
}

TEST(macdelete, case1) {
    uint8_t test_mac_addr_1[MAC_LENGTH] = {00,00,00,11,11,11};
    uint8_t test_mac_addr_2[MAC_LENGTH] = {0xff,00,00,11,11,11};
    uint64_t test_handle = 0x001;
    sai_fdb_api_t fdb_api_op = {.remove_fdb_entry = mock_fdb_remove_entry};
    fdb_api = &fdb_api_op;
    ASSERT_EQ(0, switchlink_mac_delete(test_mac_addr_1, test_handle));
    ASSERT_EQ(-1, switchlink_mac_delete(test_mac_addr_2, test_handle));
}

TEST(maccreate, case2) {
    uint8_t test_mac_addr_1[MAC_LENGTH] = {00,00,00,11,11,11};
    uint8_t test_mac_addr_2[MAC_LENGTH] = {0xff,00,00,11,11,11};
    uint64_t test_handle_1 = 0x001;
    uint64_t test_handle_2 = 0x010;
    sai_fdb_api_t fdb_api_op = {.create_fdb_entry = mock_fdb_create_entry};
    fdb_api = &fdb_api_op;
    ASSERT_EQ(0, switchlink_mac_create(test_mac_addr_1, test_handle_1, test_handle_2));
    ASSERT_EQ(-1, switchlink_mac_create(test_mac_addr_2, test_handle_1, test_handle_2));
}

TEST(vrfcreate, case1) {
    uint64_t test_vrf_handle = 0x001;
    sai_virtual_router_api_t vrf_api_op = {.create_virtual_router = mock_vrf_create_entry};
    vrf_api = &vrf_api_op;
    ASSERT_EQ(0, switchlink_vrf_create(test_vrf_handle));
}

TEST(vrfcreate, case2) {
    uint64_t test_vrf_handle = 0x001;
    sai_virtual_router_api_t vrf_api_op = {.create_virtual_router = mock_vrf_create_entry_2};
    vrf_api = &vrf_api_op;
    ASSERT_EQ(-1, switchlink_vrf_create(test_vrf_handle));
}
