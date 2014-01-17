/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
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

#ifndef _TLM_ACCOUNT_PLUGIN_H
#define _TLM_ACCOUNT_PLUGIN_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TlmAccountPlugin          TlmAccountPlugin;
typedef struct _TlmAccountPluginInterface TlmAccountPluginInterface;

#define TLM_TYPE_ACCOUNT_PLUGIN           (tlm_account_plugin_get_type ())
#define TLM_ACCOUNT_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                            TLM_TYPE_ACCOUNT_PLUGIN, \
                                            TlmAccountPlugin))
#define TLM_IS_PLUGIN(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                            TLM_TYPE_ACCOUNT_PLUGIN))
#define TLM_ACCOUNT_PLUGIN_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj),\
                                            TLM_TYPE_ACCOUNT_PLUGIN, \
                                            TlmAccountPluginInterface))

struct _TlmAccountPluginInterface {
   GTypeInterface parent;

    gboolean (*setup_guest_user_account) (TlmAccountPlugin *self,
                                          const gchar *user_name);
    gboolean (*is_valid_user) (TlmAccountPlugin *self,
                               const gchar *user_name);

   gboolean  (*cleanup_guest_user) (TlmAccountPlugin *self,
                                    const gchar *guest_user,
                                    gboolean delete_account);
};


GType tlm_account_plugin_get_type (void);

gboolean
tlm_account_plugin_setup_guest_user_account (TlmAccountPlugin   *self,
                             const gchar *user_name);

gboolean
tlm_account_plugin_is_valid_user (TlmAccountPlugin *self,
                          const gchar *user_name);

gboolean
tlm_account_plugin_cleanup_guest_user (TlmAccountPlugin *self,
                               const gchar *user_name,
                               gboolean delete_account);

G_END_DECLS

#endif /* _TLM_ACCOUNT_PLUGIN_H */
