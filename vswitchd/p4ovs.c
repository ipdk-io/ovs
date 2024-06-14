/*
 * Copyright (c) 2024 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <config.h>
#include <string.h>

#include "openvswitch/p4ovs.h"
#include "util.h"

char grpc_addr[32] = "localhost:9559";
static const char grpc_port[] = ":9559";

void ovs_set_grpc_addr(const char* optarg) {
    if (strlen(optarg) + sizeof(grpc_port) >= sizeof(grpc_addr)) {
        ovs_fatal(0, "--grpc_addr is too long (> %lu characters)",
                  sizeof(grpc_addr) - sizeof(grpc_port));
    }
    strncpy(grpc_addr, optarg, sizeof(grpc_addr) - 1);
    grpc_addr[sizeof(grpc_addr) - 1] = '\0';
    strcat(grpc_addr, grpc_port);
}

