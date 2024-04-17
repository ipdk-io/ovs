/*
 * Copyright (c) 2024 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <config.h>
#include <string.h>

#include "openvswitch/p4ovs.h"
#include "util.h"

const char grpc_addr[32] = "localhost:9559";
static const char grpc_port[] = ":9559";

void ovs_set_grpc_addr(const char* optarg) {
    if (strlen(optarg) + sizeof(grpc_port) >= sizeof(grpc_addr)) {
        ovs_fatal(0, "--grpc_addr is too long (> %lu characters)",
                  sizeof(grpc_addr) - sizeof(grpc_port));
    }
    // grpc_addr is immutable except for this function.
    // We must cast away const to initialize it.
    strncpy((char *)grpc_addr, optarg, sizeof(grpc_addr));
    strcat((char *)grpc_addr, grpc_port);
}

