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

#ifndef _TLM_SEAT_H
#define _TLM_SEAT_H

#include <glib-object.h>
#include <tlm-config.h>
#include "tlm-types.h"

G_BEGIN_DECLS

#define TLM_TYPE_SEAT       (tlm_seat_get_type())
#define TLM_SEAT(obj)       (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                              TLM_TYPE_SEAT, TlmSeat))
#define TLM_IS_SEAT(obj)    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                             TLM_TYPE_SEAT))
#define TLM_SEAT_CLASS(kls) (G_TYPE_CHECK_CLASS_CAST((kls), \
                             TLM_TYPE_SEAT))
#define TLM_SEAT_IS_CLASS(kls)  (G_TYPE_CHECK_CLASS_TYPE((kls), \
                                 TLM_TYPE_SEAT))

typedef struct _TlmSeatPrivate TlmSeatPrivate;

struct _TlmSeat
{
    GObject parent;
    /* Private */
    TlmSeatPrivate *priv;
};

struct _TlmSeatClass
{
    GObjectClass parent_class;
};

GType tlm_seat_get_type(void);

TlmSeat *
tlm_seat_new (TlmConfig *config,
              const gchar *id,
              const gchar *path);

const gchar *
tlm_seat_get_id (TlmSeat *seat);

gboolean
tlm_seat_switch_user (TlmSeat *seat,
                      const gchar *service,
                      const gchar *username,
                      const gchar *password,
                      GHashTable *environment);

gboolean
tlm_seat_create_session (TlmSeat *seat,
                         const gchar *service, 
                         const gchar *username,
                         const gchar *password,
                         GHashTable *environment);

gboolean
tlm_seat_terminate_session (TlmSeat *seat);

G_END_DECLS

#endif /* _TLM_SEAT_H */
