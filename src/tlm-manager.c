/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2013 Intel Corporation.
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

#include "tlm-manager.h"
#include "tlm-seat.h"
#include "tlm-log.h"
#include "tlm-account-plugin.h"
#include "tlm-auth-plugin.h"
#include "config.h"

#include <glib.h>
#include <gio/gio.h>
#include <stdio.h>

G_DEFINE_TYPE (TlmManager, tlm_manager, G_TYPE_OBJECT);

#define TLM_MANAGER_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_MANAGER, TlmManagerPrivate)

#define LOGIND_BUS_NAME 	"org.freedesktop.login1"
#define LOGIND_OBJECT_PATH 	"/org/freedesktop/login1"
#define LOGIND_MANAGER_IFACE 	LOGIND_BUS_NAME".Manager"

struct _TlmManagerPrivate
{
    GDBusConnection *connection;
    GHashTable *seats; /* { gchar*:TlmSeat* } */
    TlmAccountPlugin  *account_plugin;
    GList      *auth_plugins;
    gboolean is_started;

    guint seat_added_id;
    guint seat_removed_id;
};

enum {
    SIG_SEAT_ADDED,
    SIG_SEAT_REMOVED,
    SIG_MAX,
};

static guint signals[SIG_MAX];

static void
_unref_auth_plugins (gpointer data)
{
	GObject *plugin = G_OBJECT (data);

	g_object_unref (plugin);
}

static void
tlm_manager_dispose (GObject *self)
{
    TlmManager *manager = TLM_MANAGER(self);

    DBG("disposing manager");

    if (manager->priv->is_started) {
        tlm_manager_stop (manager);
    }

    if (manager->priv->seats) {
        g_hash_table_unref (manager->priv->seats);
        manager->priv->seats = NULL;
    }

    g_clear_object (&manager->priv->account_plugin);

    if (manager->priv->auth_plugins) {
    	g_list_free_full(manager->priv->auth_plugins, _unref_auth_plugins);
    }

    G_OBJECT_CLASS (tlm_manager_parent_class)->dispose (self);
}

static void
tlm_manager_finalize (GObject *self)
{
    G_OBJECT_CLASS (tlm_manager_parent_class)->finalize (self);
}

static GObject *
tlm_manager_constructor (GType gtype, guint n_prop, GObjectConstructParam *prop)
{
    static GObject *manager = NULL; /* Singleton */

    if (manager != NULL) return g_object_ref (manager);

    manager = G_OBJECT_CLASS (tlm_manager_parent_class)->
                                    constructor (gtype, n_prop, prop);
    g_object_add_weak_pointer (G_OBJECT(manager), (gpointer*)&manager);
    
    return manager;
}

static void
tlm_manager_class_init (TlmManagerClass *klass)
{
    GObjectClass *g_klass = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof(TlmManagerPrivate));

    g_klass->constructor = tlm_manager_constructor;    
    g_klass->dispose = tlm_manager_dispose ;
    g_klass->finalize = tlm_manager_finalize;

    signals[SIG_SEAT_ADDED] =  g_signal_new ("seat-added",
                TLM_TYPE_MANAGER,
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                1,
                TLM_TYPE_SEAT);

    signals[SIG_SEAT_REMOVED] =  g_signal_new ("seat-removed",
                TLM_TYPE_MANAGER,
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                1,
                G_TYPE_STRING);
}

static TlmAccountPlugin *
_load_account_plugin_file (const gchar *file_path, const gchar *plugin_name)
{
    TlmAccountPlugin *plugin = NULL;

    DBG("Loading plugin %s", file_path);
    GModule* plugin_module = g_module_open (file_path, G_MODULE_BIND_LAZY);
    if (plugin_module == NULL) {
        DBG("Plugin couldn't be opened: %s", g_module_error());
        return NULL;
    }

    gchar* get_type_func = g_strdup_printf("tlm_account_plugin_%s_get_type", plugin_name);
    gpointer p;

    DBG("Resolving symbol %s", get_type_func);
    gboolean symfound = g_module_symbol (plugin_module, get_type_func, &p);
    g_free(get_type_func);
    if (!symfound) {
        DBG("Symbol couldn't be resolved");
        g_module_close (plugin_module);
        return NULL;
    }

    DBG("Creating plugin object");
    GType (*plugin_get_type_f)(void) = p;
    plugin = g_object_new(plugin_get_type_f(), NULL);
    if (plugin == NULL) {
        DBG("Plugin couldn't be created");
        g_module_close (plugin_module);
        return NULL;
    }
    g_module_make_resident (plugin_module);

    return plugin;
}

static void
_load_accounts_plugin (TlmManager *self, const gchar *name)
{
    /* FIXME : plugins path priority
                 i) environment
                ii) configuration
               iii) $(libdir)/tlm-plugins
     */
    const gchar *plugins_path = TLM_PLUGINS_DIR;
    const gchar *env_val = NULL;
    gchar *plugin_file = NULL;
    gchar *plugin_file_name = NULL;

    env_val = g_getenv ("TLM_PLUGINS_DIR");
    if (env_val)
        plugins_path = env_val;

    plugin_file_name = g_strdup_printf ("libtlm-plugin-%s", name);
    plugin_file = g_module_build_path(plugins_path, plugin_file_name);
    g_free (plugin_file_name);

    self->priv->account_plugin =  _load_account_plugin_file (plugin_file, name);

    g_free (plugin_file);
}

#if 0
static void
_load_auth_plugins (TlmManager *self)
{
    const gchar *plugin_path = TLM_PLUGINS_DIR;
    gchar *plugin_file = NULL;

    env_val = g_getenv ("TLM_PLUGINS_DIR");
    if (env_val)
        plugins_path = env_val;
}
#endif

static void
tlm_manager_init (TlmManager *manager)
{
    GError *error = NULL;
    TlmManagerPrivate *priv = TLM_MANAGER_PRIV (manager);
    
    priv->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!priv->connection) {
        CRITICAL ("error getting system bus: %s", error->message);
        g_error_free (error);
        return;
    }

    priv->seats = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                         (GDestroyNotify)g_object_unref);

    priv->account_plugin = NULL;
    priv->auth_plugins = NULL;

    manager->priv = priv;

    /* FIXME: findout account plugin from configuration */
    _load_accounts_plugin (manager, "gumd");
    //_load_auth_plugins ();
}

static void
_manager_hashify_seats (TlmManager *manager, GVariant *hash_map)
{
    GVariantIter iter;
    gchar *id = 0, *path = 0;
    TlmManagerPrivate *priv = NULL;

    g_return_if_fail (manager);
    g_return_if_fail (hash_map);

    priv = manager->priv;

    g_variant_iter_init (&iter, hash_map);
    while (g_variant_iter_next (&iter, "(so)", &id, &path)) {
        DBG("found seat %s:%s", id, path);
        TlmSeat *seat = tlm_seat_new (id, path);

        g_hash_table_insert (priv->seats, id, seat);

        g_signal_emit (manager, signals[SIG_SEAT_ADDED], 0, seat, NULL);
        g_free (path);
    }
}

static void
_manager_sync_seats (TlmManager *manager)
{
    GError *error = NULL;
    GVariant *reply = NULL;
    GVariant *hash_map = NULL;

    g_return_if_fail (manager && manager->priv->connection);

    reply = g_dbus_connection_call_sync (manager->priv->connection,
                                         LOGIND_BUS_NAME,
                                         LOGIND_OBJECT_PATH,
                                         LOGIND_MANAGER_IFACE,
                                         "ListSeats",
                                         g_variant_new("()"),
                                         G_VARIANT_TYPE_TUPLE,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    if (!reply) {
        WARN ("failed to get attached seats: %s", error->message);
        g_error_free (error);
        return;
    }

    g_variant_get (reply, "(@a(so))", &hash_map);
    g_variant_unref (reply);

    _manager_hashify_seats (manager, hash_map);

    g_variant_unref (hash_map);
}

static void
_manager_on_seat_added (GDBusConnection *connection,
                        const gchar *sender,
                        const gchar *object_path,
                        const gchar *iface,
                        const gchar *signal_name,
                        GVariant *params,
                        gpointer userdata)
{
    gchar *id = NULL, *path = NULL;
    TlmManager *manager = TLM_MANAGER (userdata);

    g_return_if_fail (manager);
    g_return_if_fail (params);

    g_variant_get (params, "(&s&o)", &id, &path);
    g_variant_unref (params);

    DBG("Seat added: %s:%s", id, path);

    if (!g_hash_table_contains (manager->priv->seats, id)) {
        TlmSeat *seat = tlm_seat_new (id, path);

        g_hash_table_insert (manager->priv->seats,(gpointer)id, (gpointer)seat);
        g_signal_emit (manager, signals[SIG_SEAT_ADDED], 0, seat, NULL);
        g_free (path);
    } 
    else {
        g_free (id);
        g_free (path);
    }
}

static void
_manager_on_seat_removed (GDBusConnection *connection,
                        const gchar *sender,
                        const gchar *object_path,
                        const gchar *iface,
                        const gchar *signal_name,
                        GVariant *params,
                        gpointer userdata)
{
    gchar *id = NULL, *path = NULL;
    TlmManager *manager = TLM_MANAGER (userdata);

    g_return_if_fail (manager);
    g_return_if_fail (params);

    g_variant_get (params, "(&s&o)", &id, path);
    g_variant_unref (params);

    DBG("Seat removed: %s:%s", id, path);

    if (!g_hash_table_contains (manager->priv->seats, id)) {
        g_hash_table_remove (manager->priv->seats, id);
        g_signal_emit (manager, signals[SIG_SEAT_REMOVED], 0, id, NULL);

    } 
    g_free (id);
    g_free (path);
}

static void
_manager_subscribe_seat_changes (TlmManager *manager)
{
    TlmManagerPrivate *priv = manager->priv;

    priv->seat_added_id = g_dbus_connection_signal_subscribe (
                              priv->connection,
                              LOGIND_BUS_NAME,
                              LOGIND_MANAGER_IFACE,
                              "SeatNew",
                              LOGIND_OBJECT_PATH,
                              NULL,
                              G_DBUS_SIGNAL_FLAGS_NONE,
                              _manager_on_seat_added,
                              manager, NULL);

    priv->seat_removed_id = g_dbus_connection_signal_subscribe (
                                priv->connection,
                                LOGIND_BUS_NAME,
                                LOGIND_MANAGER_IFACE,
                                "SeatRemoved",
                                LOGIND_OBJECT_PATH,
                                NULL,
                                G_DBUS_SIGNAL_FLAGS_NONE,
                                _manager_on_seat_removed,
                                manager, NULL);
}

static void
_manager_unsubsribe_seat_changes (TlmManager *manager)
{
    if (manager->priv->seat_added_id) {
        g_dbus_connection_signal_unsubscribe (manager->priv->connection,
                                              manager->priv->seat_added_id);
        manager->priv->seat_added_id = 0;
    }

    if (manager->priv->seat_removed_id) {
        g_dbus_connection_signal_unsubscribe (manager->priv->connection,
                                              manager->priv->seat_removed_id);
        manager->priv->seat_removed_id = 0;
    }
}

gboolean
tlm_manager_start (TlmManager *manager)
{
    g_return_val_if_fail (manager && TLM_IS_MANAGER (manager), FALSE);

    _manager_sync_seats (manager);
    _manager_subscribe_seat_changes (manager);

    manager->priv->is_started = TRUE;

    return TRUE;
}

gboolean
tlm_manager_stop (TlmManager *manager)
{
    g_return_val_if_fail (manager && TLM_IS_MANAGER (manager), FALSE);

    _manager_unsubsribe_seat_changes (manager);
    
    manager->priv->is_started = FALSE;

    return TRUE;
}

gboolean
tlm_manager_setup_guest_user (TlmManager *manager, const gchar *user_name)
{
    g_return_val_if_fail (manager && TLM_IS_MANAGER (manager), FALSE);
    g_return_val_if_fail (manager->priv->account_plugin, FALSE);

    if (tlm_account_plugin_is_valid_user (
            manager->priv->account_plugin, user_name)) {
        DBG("user account '%s' already existing, cleaning the home folder",
                 user_name);
        return tlm_account_plugin_cleanup_guest_user (
                    manager->priv->account_plugin, user_name, FALSE);
    }
    else {
        DBG("Asking plugin to setup guest user '%s'", user_name); 
        return tlm_account_plugin_setup_guest_user (
                    manager->priv->account_plugin, user_name);
    }
}

TlmManager *
tlm_manager_new ()
{
    return g_object_new (TLM_TYPE_MANAGER, NULL);
}

