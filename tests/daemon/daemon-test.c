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
_get_bus_connection (
        GError **error)
{
    gchar address[128];
    g_snprintf (address, 127, TLM_DBUS_ROOT_SOCKET_ADDRESS);
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

    connection = _get_bus_connection (&error);
    fail_if (connection == NULL, "failed to get bus connection : %s",
            error ? error->message : "(null)");

    login_object = _get_login_object (connection, &error);
    fail_if (login_object == NULL, "failed to get login object: %s",
            error ? error->message : "");

    environ = g_hash_table_new_full ((GHashFunc)g_str_hash,
            (GEqualFunc)g_str_equal,
            (GDestroyNotify)g_free,
            (GDestroyNotify)g_free);
    g_hash_table_insert (environ, "KEY1", "VALUE1");

    fail_if (tlm_dbus_login_call_login_user_sync (login_object,
            "seat0", "test0", "test1", environ, NULL, &error) == TRUE);

    g_hash_table_unref (environ);

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
