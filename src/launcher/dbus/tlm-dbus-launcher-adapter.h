/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
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
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __TLM_DBUS_LAUNCHER_ADAPTER_H_
#define __TLM_DBUS_LAUNCHER_ADAPTER_H_

#include <config.h>
#include <glib.h>
#include "common/dbus/tlm-dbus-launcher-gen.h"
#include "common/dbus/tlm-dbus-utils.h"
#include "launcher/tlm-dbus-launcher-observer.h"

G_BEGIN_DECLS

#define TLM_TYPE_LAUNCHER_ADAPTER                \
    (tlm_dbus_launcher_adapter_get_type())
#define TLM_DBUS_LAUNCHER_ADAPTER(obj)            \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), TLM_TYPE_LAUNCHER_ADAPTER, \
            TlmDbusLauncherAdapter))
#define TLM_DBUS_LAUNCHER_ADAPTER_CLASS(klass)    \
    (G_TYPE_CHECK_CLASS_CAST((klass), TLM_TYPE_LAUNCHER_ADAPTER, \
            TlmDbusLauncherAdapterClass))
#define TLM_IS_DBUS_LAUNCHER_ADAPTER(obj)         \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), TLM_TYPE_LAUNCHER_ADAPTER))
#define TLM_IS_DBUS_LAUNCHER_ADAPTER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), TLM_TYPE_LAUNCHER_ADAPTER))
#define TLM_DBUS_LAUNCHER_ADAPTER_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj), TLM_TYPE_LAUNCHER_ADAPTER, \
            TlmDbusLauncherAdapterClass))

typedef struct _TlmDbusLauncherAdapter TlmDbusLauncherAdapter;
typedef struct _TlmDbusLauncherAdapterClass TlmDbusLauncherAdapterClass;
typedef struct _TlmDbusLauncherAdapterPrivate TlmDbusLauncherAdapterPrivate;

struct _TlmDbusLauncherAdapter
{
    GObject parent;

    /* priv */
    TlmDbusLauncherAdapterPrivate *priv;
};

struct _TlmDbusLauncherAdapterClass
{
    GObjectClass parent_class;
};

GType tlm_dbus_launcher_adapter_get_type (void) G_GNUC_CONST;

TlmDbusLauncherAdapter *
tlm_dbus_launcher_adapter_new_with_connection (
		TlmDbusLauncherObserver *observer,
        GDBusConnection *connection);

G_END_DECLS

#endif /* __TLM_DBUS_LAUNCHER_ADAPTER_H_ */
