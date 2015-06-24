/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
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
#include "common/dbus/tlm-dbus-launcher-gen.h"
#include "common/tlm-utils.h"
#include "common/dbus/tlm-dbus-utils.h"

static GPid daemon_pid = 0;

//static GMainLoop *main_loop = NULL;

typedef struct {
    gchar *username;
    gchar *password;
    gchar *seatid;
    gchar **environment;
} TlmUser;


typedef struct {
    gchar *sessionid;
    gchar *command;
    pid_t pid;
} TlmLauncher;

static TlmUser *
_create_tlm_user ()
{
    return g_malloc0 (sizeof (TlmUser));
}

static void
_free_tlm_user (
        TlmUser *user)
{
    if (user) {
        g_free (user->username);
        g_free (user->password);
        g_free (user->seatid);
        g_strfreev (user->environment);
        g_free (user);
    }
}

static TlmLauncher *
_create_tlm_launcher ()
{
    return g_malloc0 (sizeof (TlmLauncher));
}

static void
_free_tlm_launcher (
        TlmLauncher *launcher)
{
    if (launcher) {
        g_free (launcher->sessionid);
        g_free (launcher->command);
        g_free (launcher);
    }
}

static gboolean
_setup_daemon ()
{
    DBG ("starting tlm daemon");

    GError *error = NULL;
    gchar *argv[2];

    const gchar *bin_path = TLM_BIN_DIR;

#ifdef ENABLE_DEBUG
    const gchar *env_val = g_getenv("TLM_BIN_DIR");
    if (env_val)
        bin_path = env_val;
#endif
    if(!bin_path) {
        WARN("No TLM daemon bin path found");
        return FALSE;
    }

    gchar *test_daemon_path = g_build_filename (bin_path, "tlm", NULL);
    if(!test_daemon_path) {
        WARN("No TLM daemon path found");
        return FALSE;
    }

    argv[0] = test_daemon_path;
    argv[1] = NULL;
    g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
            &daemon_pid, &error);
    g_free (test_daemon_path);
    if (error) {
        WARN("Failed to spawn daemon : %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        return FALSE;
    }
    sleep (5); /* 5 seconds */

    DBG ("Daemon PID = %d\n", daemon_pid);
    return TRUE;
}

static void
_teardown_daemon ()
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
    uid_t user_id = getuid ();

    if (user_id == 0) {
        return _get_root_socket_bus_connection (error);
    }

    /* get dbus connection for specific user only */
    gchar address[128];
    g_snprintf (address, 127, "unix:path=%s/%s-%d", TLM_DBUS_SOCKET_PATH,
            seat_id, user_id);
    return g_dbus_connection_new_for_address_sync (address,
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, NULL, NULL, error);
}

gchar *
_get_dbus_socket_path (
        GVariant *sessioninfo,
        const gchar *sessionid)
{
    GVariantIter iter;
    gchar *key = NULL;
    GVariant *value = NULL;
    gchar *path = NULL;

    g_return_val_if_fail (sessioninfo != NULL, NULL);

    g_variant_iter_init (&iter, sessioninfo);
    while (g_variant_iter_next (&iter, "{sv}", &key, &value)) {
        if (g_strcmp0 (key, "uid") == 0) {
            uid_t uid = g_variant_get_uint32 (value);
            path = g_strdup_printf ("unix:path=%s/%d/%s",
                    TLM_RUNTIME_DIR_PREFIX, uid, sessionid);
            break;
        }
    }
    return path;
}

TlmDbusLogin *
_get_login_object (
        GDBusConnection *connection,
        GError **error)
{
    return tlm_dbus_login_proxy_new_sync (connection, G_DBUS_PROXY_FLAGS_NONE,
            NULL, TLM_LOGIN_OBJECTPATH, NULL, error);
}

GDBusConnection *
_get_launcher_bus_connection (
        const gchar *sessionid,
        GError *error)
{
    GDBusConnection *connection = NULL;
    TlmDbusLogin *login_object = NULL;
    GVariant *sessioninfo = NULL;
    gchar *address = NULL;

    connection = _get_root_socket_bus_connection (&error);
    if (connection == NULL) {
        WARN("failed to get bus connection : error %s",
                error ? error->message : "(null)");
        goto _finished;
    }
    login_object = _get_login_object (connection, &error);
    if (login_object == NULL) {
        WARN("failed to get login object : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    tlm_dbus_login_call_get_session_info_sync (login_object, sessionid,
            &sessioninfo, NULL, &error);
    if (error) {
        WARN ("launcher dbusg session failed with error: %d:%s", error->code,
                error->message);
        g_error_free (error); error = NULL;
        if (connection) g_object_unref (connection);
        connection = NULL;
        goto _finished;
    }
    /* get dbus connection for specific launcher only */
    address = _get_dbus_socket_path (sessioninfo, sessionid);
    DBG ("get launcher dbus conn with add %s session id %s", address,
            sessionid);
    connection = g_dbus_connection_new_for_address_sync (
            address,
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, NULL, NULL, &error);
    g_variant_unref (sessioninfo);
    g_free (address);

_finished:
    if (error) g_error_free (error);
    if (login_object) g_object_unref (login_object);
    return connection;
}

TlmDbusLauncher *
_get_launcher_object (
        GDBusConnection *connection,
        GError **error)
{
    return tlm_dbus_launcher_proxy_new_sync (connection,
            G_DBUS_PROXY_FLAGS_NONE, NULL, TLM_LAUNCHER_OBJECTPATH, NULL,
            error);
}

static GVariant *
_convert_environ_to_variant (gchar **env) {

    GVariantBuilder *builder = NULL;
    GVariant *venv = NULL;
    gchar **penv = env;

    builder = g_variant_builder_new (((const GVariantType *) "a{ss}"));

    while (penv && *penv) {
        gchar *key = *penv++;
        gchar *value = *penv++;
        if (!key || !value) {
            break;
        }
        g_variant_builder_add (builder, "{ss}", key, value);
    }
    venv = g_variant_builder_end (builder);
    g_variant_builder_unref (builder);

    return venv;
}

static gboolean
_handle_user_login (
        TlmUser *user)
{
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    TlmDbusLogin *login_object = NULL;
    GVariant *venv = NULL;
    gchar *sessionid = NULL;
    gboolean success = FALSE;

    if (!user || !user->username || !user->password || !user->seatid) {
        WARN("Invalid username/password");
        return FALSE;
    }
    DBG ("username %s seatid %s", user->username, user->seatid);

    connection = _get_bus_connection (user->seatid, &error);
    if (connection == NULL) {
        WARN("failed to get bus connection : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    login_object = _get_login_object (connection, &error);
    if (login_object == NULL) {
        WARN("failed to get login object : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    venv = _convert_environ_to_variant (user->environment);
    if (venv == NULL) {
        WARN("failed to get user environemnt");
        goto _finished;
    }

    tlm_dbus_login_call_login_user_sync (login_object, user->seatid,
            user->username, user->password, venv, &sessionid, NULL, &error);
    if (error) {
        WARN ("login failed with error: %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    } else {
        DBG ("User logged in successfully with session id %s", sessionid);
        success = TRUE;
    }
    g_free (sessionid);

_finished:
    if (login_object) g_object_unref (login_object);
    if (connection) g_object_unref (connection);

    return success;
}

static gboolean
_handle_user_logout (
        TlmUser *user)
{
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    TlmDbusLogin *login_object = NULL;
    gboolean success = FALSE;

    if (!user || !user->seatid) {
        WARN("Invalid user/seatid");
        return FALSE;
    }
    DBG ("logout for seatid %s", user->seatid);

    connection = _get_bus_connection (user->seatid, &error);
    if (connection == NULL) {
        WARN("failed to get bus connection : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    login_object = _get_login_object (connection, &error);
    if (login_object == NULL) {
        WARN("failed to get login object : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    tlm_dbus_login_call_logout_user_sync (login_object, user->seatid,
            "", NULL, &error);
    if (error) {
        WARN ("logout failed with error: %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    } else {
        DBG ("User logged out successfully with seat id %s", user->seatid);
        success = TRUE;
    }

_finished:
    if (login_object) g_object_unref (login_object);
    if (connection) g_object_unref (connection);

    return success;
}

static gboolean
_handle_user_switch (
        TlmUser *user)
{
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    TlmDbusLogin *login_object = NULL;
    GVariant *venv = NULL;
    gchar *sessionid = NULL;
    gboolean success = FALSE;

    if (!user || !user->username || !user->password || !user->seatid) {
        WARN("Invalid username/password/seatid");
        return FALSE;
    }
    DBG ("username %s seatid %s", user->username, user->seatid);

    connection = _get_bus_connection (user->seatid, &error);
    if (connection == NULL) {
        WARN("failed to get bus connection : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    login_object = _get_login_object (connection, &error);
    if (login_object == NULL) {
        WARN("failed to get login object : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    venv = _convert_environ_to_variant (user->environment);
    if (venv == NULL) {
        WARN("failed to get user environemnt");
        goto _finished;
    }

    tlm_dbus_login_call_switch_user_sync (login_object, user->seatid,
            user->username, user->password, venv, &sessionid, NULL, &error);
    if (error) {
        WARN ("switch user failed with error: %d:%s", error->code,
                error->message);
        g_error_free (error);
        error = NULL;
    } else {
        DBG ("User switched in successfully with session id %s", sessionid);
        success = TRUE;
    }
    g_free (sessionid);

_finished:
    if (login_object) g_object_unref (login_object);
    if (connection) g_object_unref (connection);

    return success;
}

static gboolean
_handle_launch_process (
        TlmLauncher *launcher)
{
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    TlmDbusLauncher *launcher_object = NULL;
    guint procid = 0;
    gboolean success = FALSE;

    if (!launcher || !launcher->sessionid) {
        WARN("Invalid sessionid");
        return FALSE;
    }
    DBG ("launch process within sessionid %s", launcher->sessionid);

    connection = _get_launcher_bus_connection (launcher->sessionid,
            error);
    if (connection == NULL) {
        WARN("failed to get bus connection : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    launcher_object = _get_launcher_object (connection, &error);
    if (launcher_object == NULL) {
        WARN("failed to get luncher object : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    tlm_dbus_launcher_call_launch_process_sync (launcher_object,
            launcher->command, &procid, NULL, &error);
    if (error) {
        WARN ("launch process failed with error: %d:%s", error->code,
                error->message);
        g_error_free (error);
        error = NULL;
    } else {
        DBG ("Process launched successfully with id %d", procid);
        success = TRUE;
    }

_finished:
    if (error) g_error_free (error);
    if (launcher_object) g_object_unref (launcher_object);
    if (connection) g_object_unref (connection);

    return success;
}

static gboolean
_handle_stop_process (
        TlmLauncher *launcher)
{
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    TlmDbusLauncher *launcher_object = NULL;
    gboolean success = FALSE;

    if (!launcher || !launcher->sessionid || !launcher->pid) {
        WARN("Invalid pid");
        return FALSE;
    }
    DBG ("stop process within pid %d", launcher->pid);

    connection = _get_launcher_bus_connection (launcher->sessionid,
            error);
    if (connection == NULL) {
        WARN("failed to get bus connection : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    launcher_object = _get_launcher_object (connection, &error);
    if (launcher_object == NULL) {
        WARN("failed to get launcher object : error %s",
            error ? error->message : "(null)");
        goto _finished;
    }

    tlm_dbus_launcher_call_stop_process_sync (launcher_object,
            launcher->pid, NULL, &error);
    if (error) {
        WARN ("stop process failed with error: %d:%s", error->code,
                error->message);
        g_error_free (error);
        error = NULL;
    } else {
        DBG ("Process stopped successfully with id %d", launcher->pid);
        success = TRUE;
    }

_finished:
    if (error) g_error_free (error);
    if (launcher_object) g_object_unref (launcher_object);
    if (connection) g_object_unref (connection);

    return success;
}

int main (int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context;
    gboolean rval = FALSE;

    gboolean is_user_login_op = FALSE, is_user_logout_op = FALSE;
    gboolean is_user_switch_op = FALSE;
    gboolean run_tlm_daemon = FALSE;
    gboolean is_launch_proc_op = FALSE;
    gboolean is_stop_proc_op = FALSE;
    GOptionGroup* user_option = NULL;
    GOptionGroup* launcher_option = NULL;
    TlmLauncher *launcher = _create_tlm_launcher ();
    TlmUser *user = _create_tlm_user ();

    GOptionEntry main_entries[] =
    {
        { "login-user", 'l', 0, G_OPTION_ARG_NONE, &is_user_login_op,
                "login user -- username, password and seatid is mandatory",
                NULL },
        { "logout-user", 'o', 0, G_OPTION_ARG_NONE, &is_user_logout_op,
                "logout user -- seatid is mandatory",
                NULL },
        { "switch-user", 's', 0, G_OPTION_ARG_NONE, &is_user_switch_op,
                "switch user -- username, password and seatid is mandatory",
                NULL },
        { "launch-proc", 'a', 0, G_OPTION_ARG_NONE, &is_launch_proc_op,
                "launch process -- sessionid and command are mandatory",
                NULL },
        { "stop-proc", 'p', 0, G_OPTION_ARG_NONE, &is_stop_proc_op,
                "stop process -- sessionid and pid are mandatory",
                NULL },
        { "run-daemon", 'r', 0, G_OPTION_ARG_NONE, &run_tlm_daemon,
                "run tlm daemon (by default tlm daemon is not run)",
                NULL },
        { NULL }
    };

    GOptionEntry user_entries[] =
    {
        { "username", 0, 0, G_OPTION_ARG_STRING, &user->username,
                "user name", "user1" },
        { "password", 0, 0, G_OPTION_ARG_STRING, &user->password,
                "user password", "mypass" },
        { "seat", 0, 0, G_OPTION_ARG_STRING, &user->seatid,
                 "user seat", "seat0" },
        { "env", 0, 0, G_OPTION_ARG_STRING_ARRAY, &user->environment,
                "user environment", "a 1 b 2" },
        { NULL }
    };

    GOptionEntry launch_entries[] =
    {
        { "sessionid", 0, 0, G_OPTION_ARG_STRING, &launcher->sessionid,
                "sessionid", "sessionid" },
        { "command", 0, 0, G_OPTION_ARG_STRING, &launcher->command,
                "command", "command to execute" },
        { "pid", 0, 0, G_OPTION_ARG_INT, &launcher->pid,
                "pid", "process id to stop" },
        { NULL }
    };

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    context = g_option_context_new (" [tlm client Option]\n"
            "  e.g. To login a user, ./tlm-client -l --username user1 "
            "--password p1 --seat seat0 --env key1 val1 key2 val2");
    g_option_context_add_main_entries (context, main_entries, NULL);

    user_option = g_option_group_new ("user-options", "User specific options",
            "User specific options", NULL, NULL);
    g_option_group_add_entries (user_option, user_entries);
    g_option_context_add_group (context, user_option);

    launcher_option = g_option_group_new ("launch-options", "Launcher options",
            "Launcher specific options", NULL, NULL);
    g_option_group_add_entries (launcher_option, launch_entries);
    g_option_context_add_group (context, launcher_option);

    rval = g_option_context_parse (context, &argc, &argv, &error);
    g_option_context_free (context);
    if (!rval) {
        DBG ("option parsing failed: %s\n", error->message);
        _free_tlm_user (user);
        _free_tlm_launcher (launcher);
        return EXIT_FAILURE;
    }

    if (geteuid() != 0) {
        WARN("test-client can only be run with ROOT privileges");
        _free_tlm_user (user);
        _free_tlm_launcher (launcher);
        return EXIT_FAILURE;
    }

    if (run_tlm_daemon)
        _setup_daemon ();

    if (is_user_login_op) {
        rval = _handle_user_login (user);
    } else if (is_user_logout_op) {
        rval = _handle_user_logout (user);
    } else if (is_user_switch_op) {
        rval = _handle_user_switch (user);
    } else if (is_launch_proc_op) {
        rval = _handle_launch_process (launcher);
    } else if (is_stop_proc_op) {
        rval = _handle_stop_process (launcher);
    } else {
        WARN ("No option specified");
        rval = FALSE;
    }
    _free_tlm_user (user);
    _free_tlm_launcher (launcher);

    if (run_tlm_daemon)
        _teardown_daemon ();

    if (rval)
        return EXIT_SUCCESS;

    return EXIT_FAILURE;
}
