/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
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

#include "tlm-manager.h"
#include "tlm-seat.h"
#include "tlm-log.h"
#include "tlm-account-plugin.h"
#include "tlm-auth-plugin.h"
#include "tlm-config.h"
#include "tlm-config-general.h"
#include "tlm-config-seat.h"
#include "tlm-dbus-observer.h"
#include "tlm-utils.h"
#include "config.h"

#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/inotify.h>

G_DEFINE_TYPE (TlmManager, tlm_manager, G_TYPE_OBJECT);

#define TLM_MANAGER_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_MANAGER, TlmManagerPrivate)

#define LOGIND_BUS_NAME 	"org.freedesktop.login1"
#define LOGIND_OBJECT_PATH 	"/org/freedesktop/login1"
#define LOGIND_MANAGER_IFACE 	LOGIND_BUS_NAME".Manager"

struct _TlmManagerPrivate
{
    GDBusConnection *connection;
    TlmConfig *config;
    GHashTable *seats; /* { gchar*:TlmSeat* } */
    TlmDbusObserver *dbus_observer; /* dbus observer accessed by root only */
    TlmAccountPlugin *account_plugin;
    GList *auth_plugins;
    gboolean is_started;
    gchar *initial_user;

    guint seat_added_id;
    guint seat_removed_id;
};

enum {
    PROP_0,
    PROP_INITIAL_USER,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

enum {
    SIG_SEAT_ADDED,
    SIG_SEAT_REMOVED,
    SIG_MANAGER_STOPPED,
    SIG_MAX,
};

static guint signals[SIG_MAX];

typedef struct _TlmSeatWatchClosure
{
    TlmManager *manager;
    gchar *seat_id;
    gchar *seat_path;
} TlmSeatWatchClosure;

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

    if (manager->priv->dbus_observer) {
        g_object_unref (manager->priv->dbus_observer);
        manager->priv->dbus_observer = NULL;
    }

    if (manager->priv->is_started) {
        tlm_manager_stop (manager);
    }

    if (manager->priv->seats) {
        g_hash_table_unref (manager->priv->seats);
        manager->priv->seats = NULL;
    }

    g_clear_object (&manager->priv->account_plugin);
    g_clear_object (&manager->priv->config);

    if (manager->priv->auth_plugins) {
    	g_list_free_full(manager->priv->auth_plugins, _unref_auth_plugins);
    }

    g_clear_string (&manager->priv->initial_user);

    G_OBJECT_CLASS (tlm_manager_parent_class)->dispose (self);
}

static void
tlm_manager_finalize (GObject *self)
{
    G_OBJECT_CLASS (tlm_manager_parent_class)->finalize (self);
}

static void
_manager_set_property (GObject *obj,
                       guint property_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
    TlmManager *manager = TLM_MANAGER (obj);

    switch (property_id) {
        case PROP_INITIAL_USER:
            manager->priv->initial_user = g_value_dup_string (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
_manager_get_property (GObject *obj,
                       guint property_id,
                       GValue *value,
                       GParamSpec *pspec)
{
    TlmManager *manager = TLM_MANAGER (obj);

    switch (property_id) {
        case PROP_INITIAL_USER:
            g_value_set_string (value, manager->priv->initial_user);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
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

    g_type_class_add_private (klass, sizeof (TlmManagerPrivate));

    g_klass->constructor = tlm_manager_constructor;    
    g_klass->dispose = tlm_manager_dispose ;
    g_klass->finalize = tlm_manager_finalize;
    g_klass->set_property = _manager_set_property;
    g_klass->get_property = _manager_get_property;

    pspecs[PROP_INITIAL_USER] =
        g_param_spec_string ("initial-user",
                             "initial user",
                             "User name for initial auto-login",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);

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
    signals[SIG_MANAGER_STOPPED] = g_signal_new ("manager-stopped",
                                                 TLM_TYPE_MANAGER,
                                                 G_SIGNAL_RUN_LAST,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 G_TYPE_NONE,
                                                 0);
}

static gboolean
_manager_authenticate_cb (TlmAuthPlugin *plugin,
                          const gchar *seat_id,
                          const gchar *pam_service,
                          const gchar *username,
                          const gchar *password,
                          gpointer user_data)
{
    TlmManager *self = TLM_MANAGER (user_data);
    TlmSeat *seat = NULL;

    g_return_val_if_fail (self, FALSE);

    DBG ("'%s' '%s' '%s' '%s'", seat_id, pam_service, username, password);

    seat = g_hash_table_lookup (self->priv->seats, seat_id);
    if (!seat) {
        WARN ("Seat is not ready : %s", seat_id);
        return FALSE;
    }

    /* re login with new username */
    return tlm_seat_switch_user (seat, pam_service, username, password, NULL);
}

static GObject *
_load_plugin_file (const gchar *file_path, 
                   const gchar *plugin_name,
                   const gchar *plugin_type,
                   GHashTable *config)
{
    GObject *plugin = NULL;

    DBG("Loading plugin %s", file_path);
    GModule* plugin_module = g_module_open (file_path, G_MODULE_BIND_LAZY);
    if (plugin_module == NULL) {
        DBG("Plugin couldn't be opened: %s", g_module_error());
        return NULL;
    }

    gchar* get_type_func = g_strdup_printf("tlm_%s_plugin_%s_get_type", 
                                           plugin_type,
                                           plugin_name);
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
    plugin = g_object_new(plugin_get_type_f(), "config", config, NULL);
    if (plugin == NULL) {
        DBG("Plugin couldn't be created");
        g_module_close (plugin_module);
        return NULL;
    }
    g_module_make_resident (plugin_module);

    return plugin;
}

static const gchar*
_get_plugins_path ()
{
    const gchar *e_val = NULL;

    e_val = g_getenv ("TLM_PLUGINS_DIR");
    if (!e_val) return TLM_PLUGINS_DIR;

    return e_val;
}

static void
_load_accounts_plugin (TlmManager *self, const gchar *name)
{
    const gchar *plugins_path = NULL;
    gchar *plugin_file = NULL;
    gchar *plugin_file_name = NULL;
    GHashTable *accounts_config = NULL;

    plugins_path = _get_plugins_path ();

    accounts_config = tlm_config_get_group (self->priv->config, name);

    plugin_file_name = g_strdup_printf ("libtlm-plugin-%s", name);
    plugin_file = g_module_build_path(plugins_path, plugin_file_name);
    g_free (plugin_file_name);

    self->priv->account_plugin =  TLM_ACCOUNT_PLUGIN(
        _load_plugin_file (plugin_file, name, "account", accounts_config));

    g_free (plugin_file);
}

static void
_load_auth_plugins (TlmManager *self)
{
    const gchar *plugins_path = NULL;
    const gchar *plugin_file_name = NULL;
    GDir  *plugins_dir = NULL;
    GError *error = NULL;

    plugins_path = _get_plugins_path ();
    
    DBG("plugins_path : %s", plugins_path);
    plugins_dir = g_dir_open (plugins_path, 0, &error);
    if (!plugins_dir) {
        WARN ("Failed to open pluins folder '%s' : %s", plugins_path,
                 error ? error->message : "NULL");
        g_error_free (error);
        return;
    }

    while ((plugin_file_name = g_dir_read_name (plugins_dir)) != NULL)
    {
        DBG("Plugin File : %s", plugin_file_name);
        if (g_str_has_prefix (plugin_file_name, "libtlm-plugin-") &&
            g_str_has_suffix (plugin_file_name, ".so"))
        {
            gchar      *plugin_file_path = NULL;
            gchar      *plugin_name = NULL;
            GHashTable *plugin_config = NULL;
            GObject    *plugin = NULL;
        
            plugin_file_path = g_module_build_path(plugins_path, 
                                                   plugin_file_name);

            if (!g_file_test (plugin_file_path, 
                              G_FILE_TEST_IS_REGULAR && G_FILE_TEST_EXISTS)) {
                WARN ("Ingnoring plugin : %s", plugin_file_path);
                g_free (plugin_file_path);
                continue;
            }

            DBG ("loading auth plugin '%s'", plugin_file_name);
 
            plugin_name = g_strdup (plugin_file_name + 14); // truncate prefix
            plugin_name[strlen(plugin_name) - 3] = '\0' ; // truncate suffix

            plugin_config = tlm_config_get_group (self->priv->config,
                                                  plugin_name);
    
            plugin = _load_plugin_file (plugin_file_path,
                                        plugin_name, 
                                        "auth",
                                        plugin_config);
            if (plugin) {
                g_signal_connect (plugin, "authenticate",
                     G_CALLBACK(_manager_authenticate_cb), self);
                self->priv->auth_plugins = g_list_append (
                            self->priv->auth_plugins, plugin);
            }
            g_free (plugin_file_path);
            g_free (plugin_name);
        }
    }

    g_dir_close (plugins_dir);

}

static void
tlm_manager_init (TlmManager *manager)
{
    GError *error = NULL;
    TlmManagerPrivate *priv = TLM_MANAGER_PRIV (manager);
    
    priv->config = tlm_config_new ();
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

    _load_accounts_plugin (manager,
                           tlm_config_get_string_default (priv->config,
                                                          TLM_CONFIG_GENERAL,
                                                          TLM_CONFIG_GENERAL_ACCOUNTS_PLUGIN,
                                                          "default"));
    _load_auth_plugins (manager);

    /* delete tlm runtime directory */
    tlm_utils_delete_dir (TLM_DBUS_SOCKET_PATH);
    priv->dbus_observer = TLM_DBUS_OBSERVER (tlm_dbus_observer_new (manager,
            NULL, TLM_DBUS_ROOT_SOCKET_ADDRESS, getuid (),
            DBUS_OBSERVER_ENABLE_ALL));
}

static void
_prepare_user_login_cb (TlmSeat *seat, const gchar *user_name, gpointer user_data)
{
    TlmManager *manager = TLM_MANAGER(user_data);

    g_return_if_fail (user_data && TLM_IS_MANAGER(manager));

    if (tlm_config_get_boolean (manager->priv->config,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_PREPARE_DEFAULT,
                                FALSE)) {
        DBG ("prepare for login for '%s'", user_name);
        if (!tlm_manager_setup_guest_user (manager, user_name)) {
            WARN ("failed to prepare for '%s'", user_name);
        }
    }
}

static void
_prepare_user_logout_cb (TlmSeat *seat, const gchar *user_name, gpointer user_data)
{
    TlmManager *manager = TLM_MANAGER(user_data);

    g_return_if_fail (user_data && TLM_IS_MANAGER(manager));

    if (tlm_config_get_boolean (manager->priv->config,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_PREPARE_DEFAULT,
                                FALSE)) {
        DBG ("prepare for logout for '%s'", user_name);
        if (!tlm_account_plugin_cleanup_guest_user (
                manager->priv->account_plugin, user_name, FALSE)) {
            WARN ("failed to prepare for '%s'", user_name);
        }
    }
}

static void
_create_seat (TlmManager *manager,
              const gchar *seat_id, const gchar *seat_path)
{
    g_return_if_fail (manager && TLM_IS_MANAGER (manager));

    TlmManagerPrivate *priv = TLM_MANAGER_PRIV (manager);

    TlmSeat *seat = tlm_seat_new (priv->config,
                                  seat_id,
                                  seat_path);
    g_signal_connect (seat,
                      "prepare-user-login",
                      G_CALLBACK (_prepare_user_login_cb),
                      manager);
    g_signal_connect (seat,
                      "prepare-user-logout",
                      G_CALLBACK (_prepare_user_logout_cb),
                      manager);
    g_hash_table_insert (priv->seats, g_strdup (seat_id), seat);
    g_signal_emit (manager, signals[SIG_SEAT_ADDED], 0, seat, NULL);

    if (tlm_config_get_boolean (priv->config,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_AUTO_LOGIN,
                                TRUE) ||
        priv->initial_user) {
        DBG("intial auto-login for user '%s'", priv->initial_user);
        if (!tlm_seat_create_session (seat,
                                      NULL,
                                      priv->initial_user,
                                      NULL,
                                      NULL))
            WARN("Failed to create session for default user");
    }
}

static void
_seat_watch_cb (
    const gchar *watch_item,
    gboolean is_final,
    GError *error,
    gpointer user_data)
{
    g_return_if_fail (watch_item && user_data);

    TlmSeatWatchClosure *closure = (TlmSeatWatchClosure *) user_data;

    if (error) {
      WARN ("Error in notify %s on seat %s: %s", watch_item, closure->seat_id,
          error->message);
      g_error_free (error);
      return;
    }

    DBG ("seat %s notify for %s", closure->seat_id, watch_item);

    if (is_final) {
        _create_seat (closure->manager, closure->seat_id, closure->seat_path);
        g_object_unref (closure->manager);
        g_free (closure->seat_id);
        g_free (closure->seat_path);
        g_free (closure);
    }
}

static void
_add_seat (TlmManager *manager, const gchar *seat_id, const gchar *seat_path)
{
    g_return_if_fail (manager && TLM_IS_MANAGER (manager));

    TlmManagerPrivate *priv = TLM_MANAGER_PRIV (manager);

    if (!tlm_config_get_boolean (priv->config,
                                 seat_id,
                                 TLM_CONFIG_SEAT_ACTIVE,
                                 TRUE))
        return;

    guint nwatch = tlm_config_get_uint (priv->config,
                                        seat_id,
                                        TLM_CONFIG_SEAT_NWATCH,
                                        0);
    if (nwatch) {
        int x;
        int watch_id = 0;
        gchar **watch_items = g_new0 (gchar *, nwatch + 1);
        for (x = 0; x < nwatch; x++) {
          gchar *watchx = g_strdup_printf ("%s%u", TLM_CONFIG_SEAT_WATCHX, x);
          watch_items[x] = (char *)tlm_config_get_string (
              priv->config, seat_id, watchx);
          g_free (watchx);
        }
        watch_items[nwatch] = NULL;
        TlmSeatWatchClosure *watch_closure = 
            g_new0 (TlmSeatWatchClosure, 1);
        watch_closure->manager = g_object_ref (manager);
        watch_closure->seat_id = g_strdup (seat_id);
        watch_closure->seat_path = g_strdup (seat_path);

        watch_id = tlm_utils_watch_for_files (
            (const gchar **)watch_items, _seat_watch_cb, watch_closure);
        g_free (watch_items);
        if (watch_id <= 0) {
            WARN ("Failed to add watch on seat %s", seat_id);
        } else {
            return;
        }
    }

    _create_seat (manager, seat_id, seat_path);
}

static void
_manager_hashify_seats (TlmManager *manager, GVariant *hash_map)
{
    GVariantIter iter;
    gchar *id = 0, *path = 0;

    g_return_if_fail (manager);
    g_return_if_fail (hash_map);

    g_variant_iter_init (&iter, hash_map);
    while (g_variant_iter_next (&iter, "(so)", &id, &path)) {
        DBG("found seat %s:%s", id, path);
        _add_seat (manager, id, path);
        g_free (id);
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
        _add_seat (manager, id, path);
    }
    g_free (id);
    g_free (path);
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

    guint nseats = tlm_config_get_uint (manager->priv->config,
                                        TLM_CONFIG_GENERAL,
                                        TLM_CONFIG_GENERAL_NSEATS,
                                        0);
    if (nseats) {
        guint i;
        for (i = 0; i < nseats; i++) {
            gchar *id = g_strdup_printf("seat%u", i);
            DBG ("adding virtual seat '%s'", id);
            _add_seat (manager, id, NULL);
            g_free (id);
        }
    } else {
        _manager_sync_seats (manager);
        _manager_subscribe_seat_changes (manager);
    }

    manager->priv->is_started = TRUE;

    return TRUE;
}


static gboolean
_session_terminated_cb (GObject *emitter, const gchar *seat_id,
        TlmManager *manager)
{
    g_return_val_if_fail (manager && TLM_IS_MANAGER (manager), TRUE);
    DBG("seatid %s", seat_id);

    g_hash_table_remove (manager->priv->seats, seat_id);
    if (g_hash_table_size (manager->priv->seats) == 0) {
        DBG ("signalling stopped");
        g_signal_emit (manager, signals[SIG_MANAGER_STOPPED], 0);
    }

    return TRUE;
}

gboolean
tlm_manager_stop (TlmManager *manager)
{
    g_return_val_if_fail (manager && TLM_IS_MANAGER (manager), FALSE);

    _manager_unsubsribe_seat_changes (manager);

    GHashTableIter iter;
    gpointer key, value;
    gboolean delayed = FALSE;

    g_hash_table_iter_init (&iter, manager->priv->seats);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        DBG ("terminate seat '%s'", (const gchar *) key);
        g_signal_connect_after ((TlmSeat *) value,
                                  "session-terminated",
                                  G_CALLBACK (_session_terminated_cb),
                                  manager);
        if (!tlm_seat_terminate_session ((TlmSeat *) value)) {
            g_hash_table_remove (manager->priv->seats, key);
            g_hash_table_iter_init (&iter, manager->priv->seats);
        } else {
            delayed = TRUE;
        }
    }
    if (!delayed)
        g_signal_emit (manager, signals[SIG_MANAGER_STOPPED], 0);

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
        return tlm_account_plugin_setup_guest_user_account (
                    manager->priv->account_plugin, user_name);
    }
}

TlmManager *
tlm_manager_new (const gchar *initial_user)
{
    return g_object_new (TLM_TYPE_MANAGER,
                         "initial-user", initial_user,
                         NULL);
}

TlmSeat *
tlm_manager_get_seat (TlmManager *manager, const gchar *seat_id)
{
    g_return_val_if_fail (manager && TLM_IS_MANAGER (manager), NULL);

    return g_hash_table_lookup (manager->priv->seats, seat_id);
}

void
tlm_manager_sighup_received (TlmManager *manager)
{
    g_return_if_fail (manager && TLM_IS_MANAGER (manager));

    DBG ("sighup recvd. reload configuration and account plugin");
    tlm_config_reload (manager->priv->config);
    g_clear_object (&manager->priv->account_plugin);
    _load_accounts_plugin (manager,
                           tlm_config_get_string_default (manager->priv->config,
                                                          TLM_CONFIG_GENERAL,
                                                          TLM_CONFIG_GENERAL_ACCOUNTS_PLUGIN,
                                                          "default"));
}

