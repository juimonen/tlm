/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2014 Intel Corporation.
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

#ifndef _TLM_DBUS_OBSERVER_H
#define _TLM_DBUS_OBSERVER_H

#include <glib-object.h>

#include "tlm-types.h"

G_BEGIN_DECLS

#define TLM_TYPE_DBUS_OBSERVER       (tlm_dbus_observer_get_type())
#define TLM_DBUS_OBSERVER(obj)       (G_TYPE_CHECK_INSTANCE_CAST((obj), \
            TLM_TYPE_DBUS_OBSERVER, TlmDbusObserver))
#define TLM_IS_DBUS_OBSERVER(obj)    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
            TLM_TYPE_DBUS_OBSERVER))
#define TLM_DBUS_OBSERVER_CLASS(kls) (G_TYPE_CHECK_CLASS_CAST((kls), \
            TLM_TYPE_DBUS_OBSERVER))
#define TLM_DBUS_OBSERVER_IS_CLASS(kls)  (G_TYPE_CHECK_CLASS_TYPE((kls), \
            TLM_TYPE_DBUS_OBSERVER))

typedef struct _TlmDbusObserver TlmDbusObserver;
typedef struct _TlmDbusObserverClass TlmDbusObserverClass;
typedef struct _TlmDbusObserverPrivate TlmDbusObserverPrivate;

struct _TlmDbusObserver
{
    GObject parent;
    /* Private */
    TlmDbusObserverPrivate *priv;
};

struct _TlmDbusObserverClass
{
    GObjectClass parent_class;
};

typedef enum {
    DBUS_OBSERVER_ENABLE_LOGIN_USER = 0x01,
    DBUS_OBSERVER_ENABLE_LOGOUT_USER = 0x02,
    DBUS_OBSERVER_ENABLE_SWITCH_USER = 0x04,
    DBUS_OBSERVER_ENABLE_ALL = 0x0F,
} DbusObserverEnableFlags;

GType tlm_dbus_observer_get_type(void);

TlmDbusObserver *
tlm_dbus_observer_new (
        TlmManager *manager,
        TlmSeat *seat,
        const gchar *address,
        uid_t uid,
        DbusObserverEnableFlags enable_flags);

G_END_DECLS

#endif /* _TLM_DBUS_OBSERVER_H */
