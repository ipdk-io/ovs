// Copyright 2021-present Open Networking Foundation
// Copyright (c) 2021-2022 Intel Corporation.
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_BAREFOOT_BF_INTERFACE_H_
#define STRATUM_LIB_BAREFOOT_BF_INTERFACE_H_

// Define C functions to access TdiInterface C++ class.
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "osdep/p4_sde_osdep.h"
#include "bf_types/bf_types.h"
#ifdef P4TOFINO
#include "tofino/bf_pal/bf_pal_port_intf.h"
#else
#include "bf_pal/bf_pal_port_intf.h"
#endif

#define PORT_NAME_LEN 64
#define MAC_STRING_LEN 32

// Type for the binary representation of a Protobuf message.
typedef void* PackedProtobuf;

typedef enum port_type_t {
        TAP_PORT,
        LINK_PORT,
        SOURCE_PORT,
        SINK_PORT,
        ETHER_PORT,
        VIRTUAL_PORT
} port_type_t;

typedef struct port_properties_t {
        char port_name[PORT_NAME_LEN];         /*!< Port Name */
        char mac_in_use[MAC_STRING_LEN];       /*!< MAC in string format */
        uint32_t port_id;
        uint32_t port_in_id;       /*!< Port ID for Pipeline in Input Direction */
        uint32_t port_out_id;   /*!< Port ID for Pipeline in Output Direction */
        port_type_t port_type;            /*!< Port Type */
} port_properties_t;

int bf_p4_init(const char* bf_sde_install, const char* bf_switchd_cfg,
               bool bf_switchd_background);
int bf_p4_add_port(uint64_t device, int64_t port,
                   port_properties_t *port_props);

#ifdef __cplusplus
}  // extern "C"
#endif

// Define TdiInterface C++ class.

#ifdef __cplusplus

#include <vector>
#include <absl/status/status.h>
#include <absl/synchronization/mutex.h>

#include "p4/v1/p4runtime.pb.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"
#include "stratum/hal/lib/barefoot/bf_sde_wrapper.h"
#include "stratum/hal/lib/barefoot/bfrt_action_profile_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/bfrt_counter_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_node.h"
#include "stratum/hal/lib/barefoot/bfrt_packetio_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_pre_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_switch.h"
#include "bf_chassis_manager.h"

namespace stratum {
namespace barefoot {

using namespace ::stratum::hal;
using namespace ::stratum::hal::barefoot;

// TODO(bocon): The "TdiSdeInterface" class in HAL implements a shim layer
// around the Barefoot
class TdiInterface {
 public:
  ::absl::Status InitSde(const std::string& bf_sde_install,
                         const std::string& bf_switchd_cfg,
                         bool bf_switchd_background);

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static TdiInterface* CreateSingleton() LOCKS_EXCLUDED(init_lock_);

  // Return the singleton instance to be used in the SDE callbacks.
  static TdiInterface* GetSingleton() LOCKS_EXCLUDED(init_lock_);

  // TdiRt Managers.
  std::unique_ptr<TdiTableManager> tdi_table_manager_;
  std::unique_ptr<TdiActionProfileManager> tdi_action_profile_manager_;
  std::unique_ptr<TdiPacketioManager> tdi_packetio_manager_;
  std::unique_ptr<TdiPreManager> tdi_pre_manager_;
  std::unique_ptr<TdiCounterManager> tdi_counter_manager_;
  // TODO: We are going to extend tdi_node[] as an array
  std::unique_ptr<TdiNode> tdi_node_;
  //TODO: Linking device_id_to_tdi_node_ to point to proper tdi_node_.
  std::map<int, TdiNode*> device_id_to_tdi_node_;
  std::unique_ptr<TdiChassisManager> tdi_chassis_manager_;

 protected:

 protected:
  // RW mutex lock for protecting the singleton instance initialization and
  // reading it back from other threads. Unlike other singleton classes, we
  // use RW lock as we need the pointer to class to be returned.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static TdiInterface* singleton_ GUARDED_BY(init_lock_);
};

}  // namespace barefoot
}  // namespace stratum

#endif  //  __cplusplus

#endif  // STRATUM_LIB_BAREFOOT_BF_INTERFACE_H_
