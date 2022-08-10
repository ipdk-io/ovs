/*
 * Copyright (c) 2021-2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef P4PROTO_H
#define P4PROTO_H 1

#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct ds;
struct hmap;
struct hmap_node;
struct p4proto;
struct shash;

/* FIXME: Use right prototypes after stratum integration */

void p4proto_add_del_devices(const struct shash *new_p4_devices);

void p4proto_create(uint64_t device_id);

void p4proto_deinit(void);

int p4proto_delete(void);

void p4proto_delete_bridges(struct hmap *bridges,
                            struct hmap *new_p4device_bridges,
                            uint64_t device_id);

void p4proto_destroy(uint64_t device_id);

void p4proto_dump_bridge_names(struct ds *ds, const struct hmap *bridges);

struct p4proto * p4proto_device_lookup(uint64_t device_id);

void p4proto_exit(void);

struct hmap_node * p4proto_get_bridge_node(const char *br_name);

uint64_t p4proto_get_device_id_from_bridge_name(const char *br_name);

void p4proto_init(void);

void p4proto_remove_bridge(struct hmap_node *br_node,
                           const char *br_name);

void p4proto_run(void);

void p4proto_update_config_file(uint64_t device_id, const char *file_path);

void p4proto_update_bridge(uint64_t device_id, struct hmap_node *br_node,
                           const char *br_name);

#ifdef  __cplusplus
}
#endif

#endif /* P4PROTO_H */
