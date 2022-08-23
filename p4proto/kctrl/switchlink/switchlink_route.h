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

#ifndef __SWITCHLINK_ROUTE_H__
#define __SWITCHLINK_ROUTE_H__

extern void route_create(switchlink_handle_t vrf_h,
                         switchlink_ip_addr_t *dst,
                         switchlink_ip_addr_t *gateway,
                         switchlink_handle_t ecmp_h,
                         switchlink_handle_t intf_h);

extern void route_delete(switchlink_handle_t vrf_h, switchlink_ip_addr_t *dst);

extern bool validate_nexthop_delete(uint32_t using_by,
                                    switchlink_nhop_using_by_e type);

void process_route_msg(struct nlmsghdr *nlmsg, int type);

#endif /* __SWITCHLINK_ROUTE_H__ */
