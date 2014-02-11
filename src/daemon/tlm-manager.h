/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2013-2014 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
 *          Jussi Laako <jussi.laako@linux.intel.com>
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

#ifndef _TLM_MANAGER_H
#define _TLM_MANAGER_H

#include <glib-object.h>
#include "tlm-types.h"

G_BEGIN_DECLS

#define TLM_TYPE_MANAGER            (tlm_manager_get_type())
#define TLM_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                     TLM_TYPE_MANAGER, TlmManager))
#define TLM_IS_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                     TLM_TYPE_MANAGER))
#define TLM_MANAGER_CLASS(cls)      (G_TYPE_CHECK_CLASS_CAST((cls), \
                                     TLM_TYPE_MANAGER))
#define TLM_IS_MANAGER_CLASS(cls)   (G_TYPE_CHECK_CLASS_TYPE((cls), \
                                     TLM_TYPE_MANAGER))

typedef struct _TlmManagerPrivate TlmManagerPrivate;

struct _TlmManager
{
    GObject parent;
    TlmManagerPrivate *priv;
};

struct _TlmManagerClass
{
    GObjectClass parent_class;
};

GType tlm_manager_get_type (void);

TlmManager * tlm_manager_new (const gchar *initial_user);

gboolean
tlm_manager_start(TlmManager *manager);

gboolean 
tlm_manager_stop(TlmManager *manager);

gboolean
tlm_manager_setup_guest_user (TlmManager *manager, const gchar *name);

TlmSeat *
tlm_manager_get_seat (TlmManager *manager, const gchar *seat_id);

void
tlm_manager_sighup_received (TlmManager *manager);

G_END_DECLS

#endif /* _TLM_MANAGER_H */
