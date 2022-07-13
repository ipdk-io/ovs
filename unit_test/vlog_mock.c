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

#include <config.h>
#include <stdarg.h>
#include <stdlib.h>
#include "openvswitch/vlog.h"

void vlog_insert_module(struct ovs_list *vlog)
{
    return;
}

/* Same as ovs_fatal() except that the arguments are supplied as a va_list. */
void ovs_fatal_valist(int err_no, const char *format, va_list args)
{
//    ovs_error_valist(err_no, format, args);
    exit(EXIT_FAILURE);
    return;
}

/* Same as ovs_abort() except that the arguments are supplied as a va_list. */
void ovs_abort_valist(int err_no, const char *format, va_list args)
{
//    ovs_error_valist(err_no, format, args);
    abort();
    return;
}

void vlog_valist(const struct vlog_module *module, enum vlog_level level,
            const char *message, va_list args)
{
    return;
}

void vlog(const struct vlog_module *module, enum vlog_level level,
     const char *message, ...)
{
    va_list args;
    va_start(args, message);
    vlog_valist(module, level, message, args);
    va_end(args);
    return;
}

/* Logs 'message' to 'module' at maximum verbosity, then exits with a failure
 * exit code.  Always writes the message to stderr, even if the console
 * destination is disabled.
 *
 * Choose this function instead of vlog_abort_valist() if the daemon monitoring
 * facility shouldn't automatically restart the current daemon.  */
void vlog_fatal_valist(const struct vlog_module *module_,
                  const char *message, va_list args)
{
    struct vlog_module *module = CONST_CAST(struct vlog_module *, module_);
    /* Don't log this message to the console to avoid redundancy with the
     * message written by the later ovs_fatal_valist(). */
    module->levels[VLF_CONSOLE] = VLL_OFF;
    vlog_valist(module, VLL_EMER, message, args);
    ovs_fatal_valist(0, message, args);
    return;
}

/* Logs 'message' to 'module' at maximum verbosity, then exits with a failure
 * exit code.  Always writes the message to stderr, even if the console
 * destination is disabled.
 *
 * Choose this function instead of vlog_abort() if the daemon monitoring
 * facility shouldn't automatically restart the current daemon.  */
void vlog_fatal(const struct vlog_module *module, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    vlog_fatal_valist(module, message, args);
    va_end(args);
    return;
}

/* Logs 'message' to 'module' at maximum verbosity, then calls abort().  Always
 * writes the message to stderr, even if the console destination is disabled.
 *
 * Choose this function instead of vlog_fatal_valist() if the daemon monitoring
 * facility should automatically restart the current daemon.  */
void vlog_abort_valist(const struct vlog_module *module_,
                  const char *message, va_list args)
{
    struct vlog_module *module = (struct vlog_module *) module_;
    /* Don't log this message to the console to avoid redundancy with the
     * message written by the later ovs_abort_valist(). */
    module->levels[VLF_CONSOLE] = VLL_OFF;
    vlog_valist(module, VLL_EMER, message, args);
    ovs_abort_valist(0, message, args);
    return;
}

/* Logs 'message' to 'module' at maximum verbosity, then calls abort().  Always
 * writes the message to stderr, even if the console destination is disabled.
 *
 * Choose this function instead of vlog_fatal() if the daemon monitoring
 * facility should automatically restart the current daemon.  */
void vlog_abort(const struct vlog_module *module, const char *message, ...)
{
    va_list args;
    va_start(args, message);
    vlog_abort_valist(module, message, args);
    va_end(args);
    return;
}
