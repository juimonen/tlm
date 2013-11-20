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

#ifndef _TLM_PLUGIN_H
#define _TLM_PLUGIN_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
   TLM_PLUGIN_TYPE_NONE = 0x00,
   /*
    * Accounts plugin: Which provides the user account information
    * GUM, AccountsService, etc.,
    */
   TLM_PLUGIN_TYPE_ACCOUNT = 0x01,
   /*
    * Authentication Plugin: Which requests (re-)authentication tlm
    * Bluetooth, NFC, Keyfob, etc., 
    */
   TLM_PLUGIN_TYPE_AUTH = 0x02

} TlmPluginTypeFlags;

typedef struct _TlmPlugin          TlmPlugin;
typedef struct _TlmPluginInterface TlmPluginInterface;

#define TLM_TYPE_PLUGIN           (tlm_plugin_get_type ())
#define TLM_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), TLM_TYPE_PLUGIN, TlmPlugin))
#define TLM_IS_PLUGIN(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TLM_TYPE_PLUGIN))
#define TLM_PLUGIN_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), TLM_TYPE_PLUGIN, TlmPluginInterface))

struct _TlmPluginInterface {
   GTypeInterface parent;

   /*
    * ACCOUNT METHODS/SIGNALS
    */
   /* 
    * Create/setup a user account
    * Called by tlm for every seat
    */
    gboolean (*setup_guest_user_account) (TlmPlugin *self,
                                          const gchar *user_name);
    /**
     * Check if user is existing with given name
     */
    gboolean (*is_valid_user) (TlmPlugin *self,
                               const gchar *user_name);
    
   /*
    * Cleanup guest user
    * Called by tlm when a guest user logs out
    */
   gboolean  (*cleanup_guest_user) (TlmPlugin *self,
                                    const gchar *guest_user,
                                    gboolean delete_account);
};


GType tlm_plugin_get_type (void);

TlmPluginTypeFlags
tlm_plugin_get_plugin_type (TlmPlugin *self);

gboolean
tlm_plugin_setup_guest_user (TlmPlugin   *self,
                             const gchar *user_name);

gboolean
tlm_plugin_is_valid_user (TlmPlugin *self,
                          const gchar *user_name);

gboolean
tlm_plugin_cleanup_guest_user (TlmPlugin *self,
                               const gchar *user_name,
                               gboolean delete_account);

void
tlm_plugin_start_authentication (TlmPlugin *self,
                                 const gchar *service,
                                 const gchar *user_name,
                                 const gchar *seat_id);

G_END_DECLS

#endif /* _TLM_PLUGIN_H */
