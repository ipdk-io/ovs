/*
 * Copyright (c) 2023 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Defines the P4 OvS specific definitions. These need be used under
 * if defined(P4OVS) scope only.
 */

#ifndef OPENVSWITCH_P4OVS_H
#define OPENVSWITCH_P4OVS_H

#include <stdint.h>
#include "openvswitch/thread.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct ovs_mutex p4ovs_fdb_entry_lock;

/* Control OvS offload with an environment variable during runtime.
 * If env variable OVS_P4_OFFLOAD=false, then disable OVS offload, else
 * if OVS_P4_OFFLOAD is not set or OVS_P4_OFFLOAD is any value other
 * than false, then by default enable OVS offload.
 */
static inline bool ovs_p4_offload_enabled(void) {
    const char* offload = getenv("OVS_P4_OFFLOAD");
    return (offload == NULL) || strcmp(offload, "false") != 0;
}

/* OvS creates multiple handler and revalidator threads based on the number of
 * CPU cores. These threading mechanism also associated with bridges that
 * are created in OvS. During multiple bridge scenarios, we are seeing
 * issues when a mutiple MAC's are learnt on different bridges at the same time.
 * Creating a mutex and with this we are controlling p4runtime calls for each
 * MAC learn.
 */
static inline void p4ovs_lock_init(const struct ovs_mutex *p4ovs_lock) {
    return ovs_mutex_init(p4ovs_lock);
}

static inline void p4ovs_lock_destroy(const struct ovs_mutex *p4ovs_lock) {
    return ovs_mutex_destroy(p4ovs_lock);
}

static inline void p4ovs_lock(const struct ovs_mutex *p4ovs_lock) {
    return ovs_mutex_lock(p4ovs_lock);
}

static inline void p4ovs_unlock(const struct ovs_mutex *p4ovs_lock) {
    return ovs_mutex_unlock(p4ovs_lock) OVS_RELEASES(p4ovs_lock);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // OPENVSWITCH_P4OVS_H
