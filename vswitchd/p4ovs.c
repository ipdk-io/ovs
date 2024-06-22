/*
 * Copyright (c) 2024 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <config.h>
#include <string.h>

#include "openvswitch/p4ovs.h"
#include "util.h"

char p4ovs_grpc_addr[32] = "localhost:9559";
static const char grpc_port[] = ":9559";

void ovs_set_grpc_addr(const char* optarg) {
    if (strlen(optarg) + sizeof(grpc_port) >= sizeof(p4ovs_grpc_addr)) {
        ovs_fatal(0, "--grpc_addr is too long (> %lu characters)",
                  sizeof(p4ovs_grpc_addr) - sizeof(grpc_port));
    }
    strncpy(p4ovs_grpc_addr, optarg, sizeof(p4ovs_grpc_addr));
    strcat(p4ovs_grpc_addr, grpc_port);
}

