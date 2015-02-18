/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
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

#ifndef _TLM_AUTH_PLUGIN_H
#define _TLM_AUTH_PLUGIN_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TlmAuthPlugin          TlmAuthPlugin;
typedef struct _TlmAuthPluginInterface TlmAuthPluginInterface;

#define TLM_TYPE_AUTH_PLUGIN           (tlm_auth_plugin_get_type ())
#define TLM_AUTH_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                         TLM_TYPE_AUTH_PLUGIN, \
                                         TlmAuthPlugin))
#define TLM_IS_AUTH_PLUGIN(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                         TLM_TYPE_AUTH_PLUGIN))
#define TLM_AUTH_PLUGIN_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
                                         TLM_TYPE_AUTH_PLUGIN, \
                                         TlmAuthPluginInterface))

struct _TlmAuthPluginInterface {
   GTypeInterface parent;
};


GType tlm_auth_plugin_get_type (void);

gboolean
tlm_auth_plugin_start_authentication (TlmAuthPlugin *self,
                                      const gchar *seat_id, 
                                      const gchar *pam_service,
                                      const gchar *user_name,
                                      const gchar *password);

G_END_DECLS

#endif /* _TLM_AUTH_PLUGIN_H */
