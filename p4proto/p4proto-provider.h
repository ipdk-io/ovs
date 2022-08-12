/*
 * Copyright (c) 2021-2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef P4PROTO_PROVIDER_H
#define P4PROTO_PROVIDER_H 1

#include "openvswitch/hmap.h"

/* Maximum number of P4 devices?? (Eg, PI = 256) */
// #define MAX_PROGS 256

struct p4proto {
    struct hmap_node node;      /* In global 'all_p4devices' hmap. */
    const struct p4proto_class *p4proto_class;

    char *type;                 /* Datapath type. */
    char *name;                 /* Datapath name. */

    // TODO: Placeholder - P4Info describing a P4 program.

    uint64_t dev_id;            /* Device ID used by P4Runtime. */
    char *config_file;          /* config file path. */

    struct hmap bridges;        /* "struct bridge"s indexed by name. */
};

#endif /* p4proto-provider.h */
