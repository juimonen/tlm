/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2014 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "config.h"
#include <check.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib-unix.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common/dbus/tlm-dbus.h"
#include "common/tlm-log.h"
#include "common/tlm-config.h"
#include "common/dbus/tlm-dbus-login-gen.h"
#include "common/tlm-utils.h"
#include "common/dbus/tlm-dbus-utils.h"

static gchar *exe_name = 0;
static GPid daemon_pid = 0;

static GMainLoop *main_loop = NULL;

static void
_stop_mainloop ()
{
    if (main_loop) {
        g_main_loop_quit (main_loop);
    }
}

static void
_create_mainloop ()
{
    if (main_loop == NULL) {
        main_loop = g_main_loop_new (NULL, FALSE);
    }
}

static void
_setup_daemon (void)
{
    DBG ("Programe name : %s\n", exe_name);

    GError *error = NULL;
    /* start daemon maually */
    gchar *argv[2];
    gchar *test_daemon_path = g_build_filename (g_getenv("TLM_BIN_DIR"),
            "tlm", NULL);
    fail_if (test_daemon_path == NULL, "No UM daemon path found");

    argv[0] = test_daemon_path;
    argv[1] = NULL;
    g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
            &daemon_pid, &error);
    g_free (test_daemon_path);
    fail_if (error != NULL, "Failed to span daemon : %s",
            error ? error->message : "");
    sleep (5); /* 5 seconds */

    DBG ("Daemon PID = %d\n", daemon_pid);
}

static void
_teardown_daemon (void)
{
    if (daemon_pid) kill (daemon_pid, SIGTERM);
}

GDBusConnection *
_get_root_socket_bus_connection (
        GError **error)
{
    gchar address[128];
    g_snprintf (address, 127, TLM_DBUS_ROOT_SOCKET_ADDRESS);
    return g_dbus_connection_new_for_address_sync (address,
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, NULL, NULL, error);
}

GDBusConnection *
_get_bus_connection (
        const gchar *seat_id,
        GError **error)
{
    uid_t ui_user_id = getuid ();

    if (ui_user_id == 0) {
        return _get_root_socket_bus_connection (error);
    }

    /* get dbus connection for specific user only */
    gchar address[128];
    g_snprintf (address, 127, "unix:path=%s/%s-%d", TLM_DBUS_SOCKET_PATH,
            seat_id, ui_user_id);
    return g_dbus_connection_new_for_address_sync (address,
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, NULL, NULL, error);
}

TlmDbusLogin *
_get_login_object (
        GDBusConnection *connection,
        GError **error)
{
    return tlm_dbus_login_proxy_new_sync (connection, G_DBUS_PROXY_FLAGS_NONE,
            NULL, TLM_LOGIN_OBJECTPATH, NULL, error);
}


static GVariant *
_get_session_property (
        GDBusProxy *proxy,
        const gchar *object_path,
        const gchar *prop_key)
{
    GError *error = NULL;
    GVariant *result = NULL;
    GVariant *prop_value = NULL;

    result = g_dbus_connection_call_sync (
            g_dbus_proxy_get_connection (proxy),
            g_dbus_proxy_get_name (proxy),
            object_path,
            "org.freedesktop.DBus.Properties",
            "GetAll",
            g_variant_new ("(s)",  "org.freedesktop.login1.Session"),
            G_VARIANT_TYPE ("(a{sv})"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &error);
    if (error) {
        DBG ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        return NULL;
    }

    if (g_variant_is_of_type (result, G_VARIANT_TYPE ("(a{sv})"))) {
        GVariantIter *iter = NULL;
        GVariant *item = NULL;
        g_variant_get (result, "(a{sv})",  &iter);
        while ((item = g_variant_iter_next_value (iter)))  {
            gchar *key;
            GVariant *value;
            g_variant_get (item, "{sv}", &key, &value);
            if (g_strcmp0 (key, prop_key) == 0) {
                prop_value = value;
                g_free (key); key = NULL;
                break;
            }
            g_free (key); key = NULL;
            g_variant_unref (value); value = NULL;
        }
    }
    g_variant_unref (result);
    return prop_value;
}

static GVariant *
_get_property (
        const gchar *prop_key)
{
    GError *error = NULL;
    GDBusProxy *proxy = NULL;
    GVariant *res = NULL;
    GVariant *prop_value = NULL;

    GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL,
            &error);
    if (error) goto _finished;

    proxy = g_dbus_proxy_new_sync (connection,
            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL,
            "org.freedesktop.login1", //destintation
            "/org/freedesktop/login1", //path
            "org.freedesktop.login1.Manager", //interface
            NULL, &error);
    if (error) goto _finished;

    res = g_dbus_proxy_call_sync (proxy, "GetSessionByPID",
            g_variant_new ("(u)", getpid()), G_DBUS_CALL_FLAGS_NONE, -1,
            NULL, &error);
    if (res) {
        gchar *obj_path = NULL;
        g_variant_get (res, "(o)", &obj_path);
        prop_value = _get_session_property (proxy, obj_path, prop_key);
        g_variant_unref (res); res = NULL;
        g_free (obj_path);
    }

_finished:
    if (error) {
        DBG ("failed to listen for login events: %s", error->message);
        g_error_free (error);
    }
    if (proxy) g_object_unref (proxy);
    if (connection) g_object_unref (connection);

    return prop_value;
}

/*
 * Login object test cases
 */
START_TEST (test_login_user)
{
    DBG ("\n");
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    TlmDbusLogin *login_object = NULL;
    GHashTable *environ = NULL;
    GVariant *venv = NULL;
    GVariant *vseat = NULL;
    gchar *seat = NULL;

    vseat = _get_property ("Seat");
    fail_if (vseat == NULL);
    g_variant_get (vseat, "(so)", &seat, NULL);
    g_variant_unref (vseat);

    connection = _get_bus_connection (seat, &error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");
    g_free (seat);

    login_object = _get_login_object (connection, &error);
    fail_if (login_object == NULL, "failed to get login object: %s",
            error ? error->message : "");

    environ = g_hash_table_new_full ((GHashFunc)g_str_hash,
            (GEqualFunc)g_str_equal,
            (GDestroyNotify)g_free,
            (GDestroyNotify)g_free);
    venv = tlm_dbus_utils_hash_table_to_variant (environ);

    g_hash_table_unref (environ);

    fail_if (tlm_dbus_login_call_login_user_sync (login_object,
            "seat0", "test01234567", "test1", venv, NULL, &error) == TRUE);

    if (error) {
        g_error_free (error);
        error = NULL;
    }
    g_object_unref (login_object);
    g_object_unref (connection);
}
END_TEST

Suite* daemon_suite (void)
{
    TCase *tc = NULL;

    Suite *s = suite_create ("Tlm daemon");
    
    tc = tcase_create ("Daemon tests");
    tcase_set_timeout(tc, 15);
    tcase_add_unchecked_fixture (tc, _setup_daemon, _teardown_daemon);
    tcase_add_checked_fixture (tc, _create_mainloop, _stop_mainloop);

    tcase_add_test (tc, test_login_user);
    suite_add_tcase (s, tc);

    return s;
}

int main (int argc, char *argv[])
{
    int number_failed;
    Suite *s = 0;
    SRunner *sr = 0;
   
#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    exe_name = argv[0];

    s = daemon_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);

    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
