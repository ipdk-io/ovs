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
    size_t maximum = sizeof(grpc_addr) - strlen(grpc_port) - 1;
    size_t actual = strlen(optarg);

    if (actual > maximum) {
        ovs_fatal(0, "--grpc-addr (%lu chars) is too long (> %lu chars)",
                  actual, maximum);
    }

    strncpy(grpc_addr, optarg, sizeof(grpc_addr));
    strcat(grpc_addr, grpc_port);
}

