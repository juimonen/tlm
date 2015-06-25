/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
 *
 * Copyright (C) 2015 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _TLM_PROCESS_MANAGER_H
#define _TLM_PROCESS_MANAGER_H

#include <glib-object.h>
#include "common/tlm-config.h"

G_BEGIN_DECLS

#define TLM_TYPE_PROCESS_MANAGER       (tlm_process_manager_get_type())
#define TLM_PROCESS_MANAGER(obj)       (G_TYPE_CHECK_INSTANCE_CAST((obj), \
            TLM_TYPE_PROCESS_MANAGER, TlmProcessManager))
#define TLM_IS_PROCESS_MANAGER(obj)    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
            TLM_TYPE_PROCESS_MANAGER))
#define TLM_PROCESS_MANAGER_CLASS(kls) (G_TYPE_CHECK_CLASS_CAST((kls), \
            TLM_TYPE_PROCESS_MANAGER))
#define TLM_PROCESS_MANAGER_IS_CLASS(kls)  (G_TYPE_CHECK_CLASS_TYPE((kls), \
            TLM_TYPE_PROCESS_MANAGER))

typedef struct _TlmProcessManager TlmProcessManager;
typedef struct _TlmProcessManagerClass TlmProcessManagerClass;
typedef struct _TlmProcessManagerPrivate TlmProcessManagerPrivate;

struct _TlmProcessManager
{
    GObject parent;
    /* Private */
    TlmProcessManagerPrivate *priv;
};

struct _TlmProcessManagerClass
{
    GObjectClass parent_class;
};

GType tlm_process_manager_get_type(void);

TlmProcessManager *
tlm_process_manager_new (
		TlmConfig *config,
        const gchar *dbus_address,
        uid_t uid);

gboolean
tlm_process_manager_launch_process (
        TlmProcessManager *self,
        const gchar *command,
        gboolean is_leader,
        guint *procid,
        GError **error);

gboolean
tlm_process_manager_stop_process (
        TlmProcessManager *self,
        guint procid,
        GError **error);

gboolean
tlm_process_manager_list_processes (
        TlmProcessManager *self);

G_END_DECLS

#endif /* _TLM_PROCESS_MANAGER_H */
