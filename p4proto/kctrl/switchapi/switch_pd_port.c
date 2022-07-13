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

#include <stdio.h>
#include <config.h>
#include <openvswitch/vlog.h>
#include <bf_types/bf_types.h>

#include "switch_base_types.h"
#include "switch_port.h"
#include "switch_port_int.h"
#include "switch_internal.h"
#include "bf_pal/bf_pal_port_intf.h"

VLOG_DEFINE_THIS_MODULE(switch_pd_port);

enum switch_tuntap_type {
     SWITCH_PORT_ATTR_TYPE_UNKNOWN,
     SWITCH_PORT_ATTR_TYPE_TUN,
     SWITCH_PORT_ATTR_TYPE_TAP,
     SWITCH_PORT_ATTR_TYPE_MAX,
};

// Typedefs to compile stuff
typedef uint16_t switch_pd_table_id_t;
typedef uint16_t switch_pd_action_id_t;
typedef switch_status_t bf_status_t;
typedef switch_status_t switch_pd_status_t;
#define SWITCH_PD_STATUS_SUCCESS 0

switch_status_t switch_pd_device_port_add(switch_device_t device,
    switch_dev_port_t dev_port,
    switch_uint32_t mtu)
{
   bf_status_t bf_status = BF_SUCCESS;
   struct port_attributes_t port_attrib;
   bf_dev_id_t bf_dev_id;
   bf_dev_port_t bf_dev_port;
   char portNameDpdk[10];
   char portName[25] = "TAP";

   memset(&port_attrib, 0, sizeof(port_attrib));
   snprintf(portNameDpdk, sizeof(portNameDpdk), "%d", dev_port);
   bf_dev_id = (bf_dev_id_t)device;
   bf_dev_port = (bf_dev_port_t)dev_port;
   strncat(portName, portNameDpdk,10);
   strncpy(port_attrib.port_name, portName, sizeof(port_attrib.port_name));
   VLOG_DBG("port_attrib.port_name=%s\n", port_attrib.port_name);
   bf_status = bf_pal_port_add(bf_dev_id, bf_dev_port, &port_attrib);
   if (bf_status != BF_SUCCESS)
   {
       VLOG_ERR(
       "Failed to add the port "
       "on device %d \n",
       device);
       return bf_status;
   }
   return switch_pd_status_to_status(bf_status);
}
