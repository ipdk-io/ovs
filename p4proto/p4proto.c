/*
 * Copyright (c) 2021-2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#define __USE_GNU 1     // enable xxx_np pthread functions
#include <pthread.h>
#include <string.h>

#include <config.h>
#include "p4proto.h"
#include "lib/unixctl.h"
#include "openvswitch/dynamic-string.h"
#include "openvswitch/hmap.h"
#include "openvswitch/shash.h"
#include "openvswitch/vlog.h"
#include "vswitch-idl.h"

#include "p4proto-provider.h"
#include "openvswitch/ovs-p4rt.h"

VLOG_DEFINE_THIS_MODULE(p4proto);

/* All p4 devices, indexed by name. */
static struct hmap all_p4devices = HMAP_INITIALIZER(&all_p4devices);

static unixctl_cb_func p4proto_dump_cache;

/* Called by ovs-vswitchd:main() on startup. */
void p4proto_init(void)
{
    unixctl_command_register("p4device/dump-cache", "[p4-device-id/all]", 1, 1,
                             p4proto_dump_cache, NULL);
}

/* Handling all deinit functionality */
void p4proto_deinit(void)
{
}

/* Create p4proto structure and initialize structure members. */
void p4proto_create(uint64_t device_id)
{
    struct p4proto *p4p;

    p4p = xzalloc(sizeof *p4p);

    p4p->dev_id = device_id;
    hmap_init(&p4p->bridges);
    hmap_insert(&all_p4devices, &p4p->node, hash_uint64(p4p->dev_id));
    VLOG_DBG("[%s]: Created P4 device %"PRIu64, __func__, device_id);
}

/* Destroy p4proto structure and remove associated bridges for the device */
void p4proto_destroy(uint64_t device_id)
{
    struct p4proto *p4p;

    p4p = p4proto_device_lookup(device_id);

    if (p4p) {
        hmap_remove(&all_p4devices, &p4p->node);
        free(p4p->type);
        free(p4p->name);

        if (p4p->config_file) {
            /* Send a delete or some event to SDE */
            free(p4p->config_file);
        }
        hmap_destroy(&p4p->bridges);
        free(p4p);
        VLOG_DBG("[%s]: Removed P4 device %"PRIu64, __func__, device_id);
    }
}

// TODO: Use right protoype after stratum integration
// p4proto_delete(const char *name, const char *type)
int p4proto_delete(void)
{
    VLOG_DBG("Func called: %s", __func__);
    return 0;
}

/* Called by ovs-vswitchd:main() on shutdown. */
void p4proto_exit(void)
{
    VLOG_DBG("Func called: %s", __func__);
}

/* Find 'p4proto' structure from 'all_p4devices' based on device_id.
 * Return found structure or else return NULL */
struct p4proto*
p4proto_device_lookup(uint64_t device_id)
{
    struct p4proto *p4p;

    HMAP_FOR_EACH(p4p, node, &all_p4devices) {
        if (p4p->dev_id == device_id) {
            VLOG_DBG("[%s] Found P4 device %"PRIu64, __func__, device_id);
            return p4p;
        }
    }
    VLOG_DBG("[%s] Couldnt find P4 device %"PRIu64, __func__, device_id);
    return NULL;
}

uint64_t p4proto_get_device_id_from_bridge_name(const char *br_name)
{
    uint64_t device_id = 0;
    struct p4proto *p4p;
    HMAP_FOR_EACH(p4p, node, &all_p4devices) {
        if (hmap_first_with_hash(&p4p->bridges, hash_string(br_name, 0))) {
                device_id = p4p->dev_id;
                break;
        }
    }
     return device_id;
}

/* Based on list of p4devices in 'new_p4_devices' received from OVSDB,
 * either add or remove p4 device information from all_p4devices hmap.
 * Also, update list of bridges associated with each P4 device. */
void
p4proto_add_del_devices(const struct shash *new_p4_devices)
{
    const struct ovsrec_p4_device *device_cfg;
    const struct ovsrec_bridge *br_cfg;
    struct p4proto *device, *next_device;
    struct hmap all_p4device_bridges;
    struct shash_node *bridges;
    struct hmap_node *br_node;
    uint64_t device_id;
    size_t list;

    /* Delete old p4 device from cache. */
    HMAP_FOR_EACH_SAFE(device, next_device, node, &all_p4devices) {
        device_id = device->dev_id;
        char *key = xasprintf("%"PRIu64, device_id);
        if (!shash_find_data(new_p4_devices, key)) {
            p4proto_destroy(device_id);
        }
        free(key);
    }

    /* Add new p4 device to cache. */
    SHASH_FOR_EACH(bridges, new_p4_devices) {
        device_cfg = bridges->data;
        device_id = (uint64_t)*device_cfg->device_id;
        if (!hmap_first_with_hash(&all_p4devices, hash_uint64(device_id))) {
            p4proto_create(device_id);
        }

        if (device_cfg->config_file_path) {
            p4proto_update_config_file(device_id, device_cfg->config_file_path);
        }

        /* Check if any bridge is added or deleted to/from a p4 device.
         * if added, then add bridge node to p4 device bridge list.
         * if deleted, then remove bridge node from p4 device bridge list. */

        hmap_init(&all_p4device_bridges);

        for (list = 0; list < device_cfg->n_bridges; list++) {
            br_cfg = device_cfg->bridges[list];
            br_node = p4proto_get_bridge_node(br_cfg->name);
            if (br_node) {
                hmap_insert(&all_p4device_bridges, br_node,
                            hash_string(br_cfg->name, 0));
                p4proto_update_bridge(device_id, br_node, br_cfg->name);
            }
        }

        device = p4proto_device_lookup(device_id);

        if (device) {
            /* Loop through all bridges in a p4 device and validate which
             * bridge from p4 device is deleted in OVSDB and remove it from
             * p4device as well. */
            p4proto_delete_bridges(&device->bridges, &all_p4device_bridges,
                                   device->dev_id);
        }
        hmap_destroy(&all_p4device_bridges);
    }

}

/* Update config file path for a p4 device */
void
p4proto_update_config_file(uint64_t device_id, const char *file_path)
{
    struct p4proto *p4p;

    p4p = p4proto_device_lookup(device_id);

    if (p4p) {
        if (!p4p->config_file) {
            p4p->config_file = xstrdup(file_path);
            VLOG_DBG("[%s]: Added config file :%s: for P4 device %"PRIu64,
                     __func__, file_path, device_id);
            /* TODO send an Add event to SDE about the config file*/
        } else if (strcmp(file_path, p4p->config_file)) {
            /* TODO send an delete event to SDE about old config file*/
            VLOG_DBG("[%s]: Updated config file from :%s: to :%s: for "
                     "P4 device %"PRIu64, __func__, p4p->config_file,
                     file_path, device_id);
            free(p4p->config_file);
            p4p->config_file = xstrdup(file_path);
            /* TODO send an Add event to SDE about the new config file*/
        }
    }
}

/* Associate new bridge to the p4 device */
void
p4proto_update_bridge(uint64_t device_id, struct hmap_node *br_node,
                      const char *br_name)
{
    struct p4proto *p4p;

    p4p = p4proto_device_lookup(device_id);

    if (p4p && !hmap_first_with_hash(&p4p->bridges, hash_string(br_name, 0))) {
        hmap_insert(&p4p->bridges, br_node, hash_string(br_name, 0));
        VLOG_DBG("[%s]: Added bridge %s to P4 device %"PRIu64,
                 __func__, br_name, device_id);
        /* TODO Send an event regarding bridge add */
    }
}

/* Remove a bridge from P4 device */
void
p4proto_remove_bridge(struct hmap_node *br_node, const char *br_name)
{
    struct p4proto *p4p;

    HMAP_FOR_EACH(p4p, node, &all_p4devices) {
        if (hmap_first_with_hash(&p4p->bridges, hash_string(br_name, 0))) {
            VLOG_DBG("[%s]: Deleted bridge %s from P4 device %"PRIu64,
                     __func__, br_name, p4p->dev_id);
            hmap_remove(&p4p->bridges, br_node);
            /* TODO Send an event regarding bridge delete */
        }
    }
}

static void
p4proto_dump_device(struct ds *ds, struct p4proto *device)
{
    ds_put_format(ds, "\n\ttype=%s", device->type);
    ds_put_format(ds, "\n\tname=%s", device->type);
    ds_put_format(ds, "\n\tConfig file=%s", device->config_file);
    ds_put_format(ds, "\n\tTotal no of bridges=%lu",
                    hmap_count(&device->bridges));
    p4proto_dump_bridge_names(ds, &device->bridges);
    ds_put_format(ds, "\n");
}

/* Loop through all p4 devices and print particular p4 device's
 * local data or print for all available p4 devices */
static void
p4proto_dump_cache(struct unixctl_conn *conn, int argc OVS_UNUSED,
                   const char *argv[], void *aux OVS_UNUSED)
{
    struct ds results;
    struct p4proto *device;
    uint64_t device_id;
    bool search_all_devices = !strcmp(argv[1], "all") ? true : false;

    ds_init(&results);
    HMAP_FOR_EACH (device, node, &all_p4devices) {
        device_id = device->dev_id;
        if (!search_all_devices) {
            if (device->dev_id == atoi(argv[1])) {
                ds_put_format(&results, "\nCache for device_id : %"PRIu64,
                              device_id);
                p4proto_dump_device(&results, device);
                break;
            }
            continue;
        }
        ds_put_format(&results, "\nCache for device_id : %"PRIu64, device_id);
        p4proto_dump_device(&results, device);
    }
    unixctl_command_reply(conn, ds_cstr(&results));
    ds_destroy(&results);
}
