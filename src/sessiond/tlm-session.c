/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2013-2014 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
 *          Jussi Laako <jussi.laako@linux.intel.com>
 *          Imran Zaman <imran.zaman@intel.com>
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

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <stdio.h>
#include <signal.h>
#include <termios.h>
#include <libintl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>

#include "tlm-session.h"
#include "tlm-auth-session.h"
#include "common/tlm-log.h"
#include "common/tlm-utils.h"
#include "common/tlm-error.h"
#include "common/tlm-config-general.h"

G_DEFINE_TYPE (TlmSession, tlm_session, G_TYPE_OBJECT);

#define TLM_SESSION_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_SESSION, TlmSessionPrivate)

enum {
    PROP_0,
    PROP_CONFIG,
    PROP_SEAT,
    PROP_SERVICE,
    PROP_USERNAME,
    PROP_ENVIRONMENT,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

enum {
    SIG_SESSION_CREATED,
    SIG_SESSION_TERMINATED,
    SIG_AUTHENTICATED,
    SIG_SESSION_ERROR,
    SIG_MAX
};
static guint signals[SIG_MAX];

struct _TlmSessionPrivate
{
    TlmConfig *config;
    pid_t child_pid;
    uid_t tty_uid;
    gid_t tty_gid;
    struct termios stdin_state, stdout_state, stderr_state;
    gchar *seat_id;
    gchar *service;
    gchar *username;
    GHashTable *env_hash;
    TlmAuthSession *auth_session;
    int last_sig;
    guint timer_id;
    guint child_watch_id;
    gchar *sessionid;
    gboolean can_emit_signal;
    gboolean is_child_up;
};

static void
_clear_session (TlmSession *session)
{
    tlm_session_reset_tty (session);

    if (session->priv->timer_id) {
        g_source_remove (session->priv->timer_id);
        session->priv->timer_id = 0;
    }

    if (session->priv->child_watch_id) {
        g_source_remove (session->priv->child_watch_id);
        session->priv->child_watch_id = 0;
    }

    if (session->priv->auth_session)
        g_clear_object (&session->priv->auth_session);

    if (session->priv->env_hash) {
        g_hash_table_unref (session->priv->env_hash);
        session->priv->env_hash = NULL;
    }
    g_clear_string (&session->priv->seat_id);
    g_clear_string (&session->priv->service);
    g_clear_string (&session->priv->username);
    g_clear_string (&session->priv->sessionid);
}

static void
tlm_session_dispose (GObject *self)
{
    TlmSession *session = TLM_SESSION(self);
    DBG("disposing session: %s", session->priv->service);
    session->priv->can_emit_signal = FALSE;

    if (session->priv->is_child_up) {
        tlm_session_terminate (session);
        while (session->priv->is_child_up)
            g_main_context_iteration(NULL, TRUE);
        DBG ("child DESTROYED");
    }

    g_clear_object (&session->priv->config);

    G_OBJECT_CLASS (tlm_session_parent_class)->dispose (self);
}

static void
tlm_session_finalize (GObject *self)
{
    //TlmSession *session = TLM_SESSION(self);
    G_OBJECT_CLASS (tlm_session_parent_class)->finalize (self);
}

static void
_session_set_property (GObject *obj,
                       guint property_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
    TlmSession *session = TLM_SESSION(obj);
    TlmSessionPrivate *priv = TLM_SESSION_PRIV (session);

    switch (property_id) {
        case PROP_CONFIG:
            priv->config = g_value_dup_object (value);
            break;
        case PROP_SEAT:
            g_free (priv->seat_id);
            priv->seat_id = g_value_dup_string (value);
            break;
        case PROP_SERVICE: 
            priv->service = g_value_dup_string (value);
            break;
        case PROP_USERNAME:
            priv->username = g_value_dup_string (value);
            break;
        case PROP_ENVIRONMENT:
            priv->env_hash = (GHashTable *) g_value_get_pointer (value);
            if (priv->env_hash)
                g_hash_table_ref (priv->env_hash);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
_session_get_property (GObject *obj,
                       guint property_id,
                       GValue *value,
                       GParamSpec *pspec)
{
    TlmSession *session = TLM_SESSION(obj);
    TlmSessionPrivate *priv = TLM_SESSION_PRIV (session);

    switch (property_id) {
        case PROP_CONFIG:
            g_value_set_object (value, priv->config);
            break;
        case PROP_SEAT:
            g_value_set_string (value, priv->seat_id);
            break;
        case PROP_SERVICE: 
            g_value_set_string (value, priv->service);
            break;
        case PROP_USERNAME:
            g_value_set_string (value, priv->username);
            break;
        case PROP_ENVIRONMENT:
            g_value_set_pointer (value, priv->env_hash);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
tlm_session_class_init (TlmSessionClass *klass)
{
    GObjectClass *g_klass = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (TlmSessionPrivate));

    g_klass->dispose = tlm_session_dispose ;
    g_klass->finalize = tlm_session_finalize;
    g_klass->set_property = _session_set_property;
    g_klass->get_property = _session_get_property;

    pspecs[PROP_CONFIG] =
        g_param_spec_object ("config",
                             "config object",
                             "Configuration object",
                             TLM_TYPE_CONFIG,
                             G_PARAM_READWRITE|
                             G_PARAM_STATIC_STRINGS);
    pspecs[PROP_SEAT] =
        g_param_spec_string ("seat",
                             "seat id",
                             "Seat id string",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_SERVICE] =
        g_param_spec_string ("service",
                             "authentication service",
                             "PAM service",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_USERNAME] =
        g_param_spec_string ("username",
                             "user name",
                             "Unix user name of user to login",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_ENVIRONMENT] =
        g_param_spec_pointer ("environment",
                              "environment variables",
                              "Environment variables for the session",
                              G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);

    signals[SIG_SESSION_CREATED] = g_signal_new ("session-created",
    							TLM_TYPE_SESSION, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                1, G_TYPE_STRING);

    signals[SIG_SESSION_TERMINATED] = g_signal_new ("session-terminated",
    							TLM_TYPE_SESSION, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                0, G_TYPE_NONE);

    signals[SIG_AUTHENTICATED] = g_signal_new ("authenticated",
    							TLM_TYPE_SESSION, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                0, G_TYPE_NONE);

    signals[SIG_SESSION_ERROR] = g_signal_new ("session-error",
    							TLM_TYPE_SESSION, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                1, G_TYPE_ERROR);

}

static void
tlm_session_init (TlmSession *session)
{
    TlmSessionPrivate *priv = TLM_SESSION_PRIV (session);
    
    priv->service = NULL;
    priv->env_hash = NULL;
    priv->auth_session = NULL;
    priv->sessionid = NULL;
    priv->child_watch_id = 0;
    priv->is_child_up = FALSE;
    priv->can_emit_signal = TRUE;
    priv->config = tlm_config_new ();

    session->priv = priv;

    struct stat tty_stat;

    if (fstat (0, &tty_stat) == 0) {
        priv->tty_uid = tty_stat.st_uid;
        priv->tty_gid = tty_stat.st_gid;
    } else {
        priv->tty_uid = 0;
        priv->tty_gid = 0;
    }

    if (tcgetattr (0, &priv->stdin_state) ||
        tcgetattr (1, &priv->stdout_state) ||
        tcgetattr (2, &priv->stderr_state))
        WARN ("Failed to retrieve initial terminal state");
}

static void
_setenv_to_session (gpointer key, gpointer val, gpointer user_data)
{
    /*TlmSessionPrivate *priv = (TlmSessionPrivate *) user_data;*/
    setenv ((const char *) key, (const char *) val, 1);
}

static gboolean
_set_terminal (TlmSessionPrivate *priv)
{
    int i;
    int tty_fd;
    pid_t tty_pgid;
    const char *tty_dev;
    struct stat tty_stat;

    tty_dev = ttyname (0);
    DBG ("trying to setup TTY '%s'", tty_dev);
    if (!tty_dev) {
        WARN ("No TTY");
        return FALSE;
    }
    if (access (tty_dev, R_OK|W_OK)) {
        WARN ("TTY not accessible: %s", strerror(errno));
        return FALSE;
    }
    if (lstat (tty_dev, &tty_stat)) {
        WARN ("lstat() failed: %s", strerror(errno));
        return FALSE;
    }
    if (tty_stat.st_nlink > 1 ||
        !S_ISCHR (tty_stat.st_mode) ||
        strncmp (tty_dev, "/dev/", 5)) {
        WARN ("Invalid TTY");
        return FALSE;
    }

    tty_fd = open (tty_dev, O_RDWR | O_NONBLOCK);
    if (tty_fd < 0) {
        WARN ("open() failed: %s", strerror(errno));
        return FALSE;
    }
    if (!isatty (tty_fd)) {
        close (tty_fd);
        WARN ("isatty() failed");
        return FALSE;
    }
    if (ioctl (tty_fd, TIOCSCTTY, 1))
        WARN ("ioctl(TIOCSCTTY) failed: %s", strerror(errno));
    tty_pgid = getpgid (getpid ());
    if (ioctl (tty_fd, TIOCSPGRP, &tty_pgid)) {
        WARN ("ioctl(TIOCSPGRP) failed: %s", strerror(errno));
    }
    /*if (tcsetpgrp (tty_fd, getpgrp ()))
        WARN ("tcsetpgrp() failed: %s", strerror(errno));*/

    // close all old handles
    for (i = 0; i < tty_fd; i++)
        close (i);
    dup2 (tty_fd, 0);
    dup2 (tty_fd, 1);
    dup2 (tty_fd, 2);
    close (tty_fd);

    return TRUE;
}

static gboolean
_set_environment (TlmSessionPrivate *priv)
{
	gchar **envlist = tlm_auth_session_get_envlist(priv->auth_session);
	const gchar *home_dir=NULL, *shell=NULL;

    if (envlist) {
    	gchar **env = 0;
    	for (env = envlist; *env != NULL; ++env) {
    		DBG ("ENV : %s", *env);
    		putenv(*env);
    		g_free (*env);
    	}
    	g_free (envlist);
    }

    const gchar *path = tlm_config_get_string (priv->config,
                                               TLM_CONFIG_GENERAL,
                                               TLM_CONFIG_GENERAL_SESSION_PATH);
    if (!path)
        path = "/usr/local/bin:/usr/bin:/bin";
    setenv ("PATH", path, 1);

    setenv ("USER", priv->username, 1);
    setenv ("LOGNAME", priv->username, 1);
    home_dir = tlm_user_get_home_dir (priv->username);
    if (home_dir) setenv ("HOME", home_dir, 1);
    shell = tlm_user_get_shell (priv->username);
    if (shell) setenv ("SHELL", shell, 1);
    setenv ("XDG_SEAT", priv->seat_id, 1);

    const gchar *xdg_data_dirs =
        tlm_config_get_string (priv->config,
                               TLM_CONFIG_GENERAL,
                               TLM_CONFIG_GENERAL_DATA_DIRS);
    if (!xdg_data_dirs)
        xdg_data_dirs = "/usr/share:/usr/local/share";
    setenv ("XDG_DATA_DIRS", xdg_data_dirs, 1);

    if (priv->env_hash)
        g_hash_table_foreach (priv->env_hash,
                              _setenv_to_session,
                              priv);

    return TRUE;
}

static void
_on_child_down_cb (
        GPid  pid,
        gint  status,
        gpointer data)
{
    g_spawn_close_pid (pid);

    TlmSession *session = TLM_SESSION (data);

    DBG ("Sessiond(%p) with pid (%d) closed with status %d", session, pid,
            status);

    session->priv->child_pid = 0;
    session->priv->is_child_up = FALSE;
    _clear_session (session);
    if (session->priv->can_emit_signal)
        g_signal_emit (session, signals[SIG_SESSION_TERMINATED], 0);
}

static void
_exec_user_session (
		TlmSession *session)
{
    gint i;
    const gchar *pattern = "('.*?'|\".*?\"|\\S+)";
    const char *home;
    const char *shell = NULL;
    const char *env_shell = NULL;
    gchar **args = NULL;
    gchar **args_iter = NULL;
    TlmSessionPrivate *priv = session->priv;
    gchar **temp_strv = NULL;

    priv = session->priv;
    if (!priv->username)
        priv->username = g_strdup (tlm_auth_session_get_username (
        		priv->auth_session));
    DBG ("session ID : %s", priv->sessionid);

    priv->child_pid = fork ();
    if (priv->child_pid) {
        DBG ("establish handler for the child pid %u", priv->child_pid);
        session->priv->child_watch_id = g_child_watch_add (priv->child_pid,
                    (GChildWatchFunc)_on_child_down_cb, session);
        session->priv->is_child_up = TRUE;
        return;
    }

    /* ==================================
     * this is child process here onwards
     * ================================== */
    gint open_max;
    gint fd;

    //close all open descriptors other than stdin, stdout, stderr
    open_max = sysconf (_SC_OPEN_MAX);
    for (fd = 3; fd < open_max; fd++)
    	fcntl (fd, F_SETFD, FD_CLOEXEC);

    uid_t target_uid = tlm_user_get_uid (priv->username);
    gid_t target_gid = tlm_user_get_gid (priv->username);

    if (fchown (0, target_uid, -1)) {
        WARN ("Changing TTY access rights failed");
    }

    /*if (getppid() == 1) {
        if (setsid () == (pid_t) -1)
            WARN ("setsid() failed: %s", strerror (errno));
    } else {
        if (setpgrp ())
            WARN ("setpgrp() failed: %s", strerror (errno));
    }*/

    DBG ("old pgid=%u", getpgrp ());
    if (setsid () == (pid_t) -1)
        WARN ("setsid() failed: %s", strerror (errno));
    DBG ("new pgid=%u", getpgrp());

    if (tlm_config_get_boolean (priv->config,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_SETUP_TERMINAL,
                                FALSE)) {
        /* usually terminal settings are handled by PAM */
        _set_terminal (priv);
    }

    if (initgroups (priv->username, target_gid))
        WARN ("initgroups() failed: %s", strerror(errno));
    if (setregid (target_gid, target_gid))
        WARN ("setregid() failed: %s", strerror(errno));
    if (setreuid (target_uid, target_uid))
        WARN ("setreuid() failed: %s", strerror(errno));

    int grouplist_len = NGROUPS_MAX;
    gid_t grouplist[NGROUPS_MAX];
    grouplist_len = getgroups (grouplist_len, grouplist);
    DBG ("group membership:");
    for (i = 0; i < grouplist_len; i++)
        DBG ("\t%s", getgrgid (grouplist[i])->gr_name);

    DBG (" state:\n\truid=%d, euid=%d, rgid=%d, egid=%d (%s)",
         getuid(), geteuid(), getgid(), getegid(), priv->username);
    _set_environment (priv);

    home = getenv("HOME");
    if (home) {
        DBG ("changing directory to : %s", home);
    	if (chdir (home) < 0)
            WARN ("Failed to change directroy : %s", strerror (errno));
    } else WARN ("Could not get home directory");

    if (tlm_config_get_boolean (priv->config,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_PAUSE_SESSION,
                                FALSE)) {
        pause ();
        exit (0);
        return;  /* this should be unreachable */
    }

    shell = tlm_config_get_string (priv->config,
                                   TLM_CONFIG_GENERAL,
                                   TLM_CONFIG_GENERAL_SESSION_CMD);
    if (shell) {
        DBG ("Session command : %s", shell);
        temp_strv = g_regex_split_simple (pattern,
                                          shell,
                                          0,
                                          G_REGEX_MATCH_NOTEMPTY);
    }

    if (temp_strv) {
        gchar **temp_iter;

        args = g_new0 (gchar *, g_strv_length (temp_strv));
        for (temp_iter = temp_strv, args_iter = args;
                *temp_iter != NULL;
                temp_iter++) {
            size_t item_len = 0;
            gchar *item = g_strstrip (*temp_iter);

            item_len = strlen (item);
            if (item_len == 0) {
                continue;
            }
            if ((item[0] == '\"' && item[item_len - 1] == '\"') ||
                    (item[0] == '\'' && item[item_len - 1] == '\'')) {
                item[item_len - 1] = '\0';
                memmove (item, item + 1, item_len - 1);
            }
            *args_iter = g_strcompress (item);
            args_iter++;
        }
        g_strfreev (temp_strv);
    } else if ((env_shell = getenv("SHELL"))){
        /* use shell if no override configured */
        args = g_new0 (gchar *, 2);
        args[0] = g_strdup (env_shell);
    } else {
        /* in case shell is not defined, fall back to systemd --user */
        args = g_new0 (gchar *, 3);
        args[0] = g_strdup ("systemd");
        args[1] = g_strdup ("--user");
    }

    DBG ("executing: ");
    args_iter = args;
    while (args_iter && *args_iter) {
        DBG ("\targv[%d]: %s", i, *args_iter);
        args_iter++; i++;
    }
    execvp (args[0], args);
    /* we reach here only in case of error */
    g_strfreev (args);
    DBG ("execl(): %s", strerror(errno));
    exit (0);
}

TlmSession *
tlm_session_new ()
{
    DBG ("Session New");
    return g_object_new (TLM_TYPE_SESSION, NULL);
}

gboolean
tlm_session_start (TlmSession *session,
                   const gchar *seat_id, const gchar *service,
                   const gchar *username, const gchar *password,
                   GHashTable *environment)
{
	GError *error = NULL;
	g_return_val_if_fail (session && TLM_IS_SESSION(session), FALSE);
    TlmSessionPrivate *priv = TLM_SESSION_PRIV(session);

    g_object_set (G_OBJECT (session), "seat", seat_id, "service", service,
            "username", username, "environment", environment, NULL);

    priv->auth_session = tlm_auth_session_new (priv->service,
            priv->username, password);

    if (!priv->auth_session) {
    	error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_SESSION_CREATION_FAILURE,
    			"Unable to create PAM sesssion");
    	g_signal_emit (session, signals[SIG_SESSION_ERROR], 0, error);
    	g_error_free (error);
    	return FALSE;
    }

    tlm_auth_session_putenv (priv->auth_session, "XDG_SEAT", priv->seat_id);

    if (!tlm_auth_session_authenticate (priv->auth_session, &error)) {
    	if (error) {
    	    //consistant error message flow
    		GError *err = TLM_GET_ERROR_FOR_ID (
    		        TLM_ERROR_SESSION_CREATION_FAILURE,
        			"%d:%s", error->code, error->message);
    		g_error_free (error);
    		error = err;
    	} else {
            error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_SESSION_CREATION_FAILURE,
                    "Unable to authenticate PAM sesssion");
    	}
    	g_signal_emit (session, signals[SIG_SESSION_ERROR], 0, error);
    	g_error_free (error);
    	return FALSE;
    }
    g_signal_emit (session, signals[SIG_AUTHENTICATED], 0);

    if (!tlm_auth_session_open (priv->auth_session, &error)) {
    	if (!error) {
    		error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_SESSION_CREATION_FAILURE,
        			"Unable to open PAM sesssion");
    	}
    	g_signal_emit (session, signals[SIG_SESSION_ERROR], 0, error);
    	g_error_free (error);
    	return FALSE;
    }
    priv->sessionid = g_strdup (tlm_auth_session_get_sessionid (
    		priv->auth_session));
    tlm_utils_log_utmp_entry (priv->username);
    _exec_user_session (session);
    g_signal_emit (session, signals[SIG_SESSION_CREATED], 0, priv->sessionid);
    return TRUE;
}

static gboolean
_terminate_timeout (gpointer user_data)
{
    TlmSession *session = TLM_SESSION(user_data);
    TlmSessionPrivate *priv = TLM_SESSION_PRIV(session);

    switch (priv->last_sig)
    {
        case SIGHUP:
            DBG ("child %u didn't respond to SIGHUP, sending SIGTERM",
                 priv->child_pid);
            if (killpg (getpgid (priv->child_pid), SIGTERM))
                WARN ("killpg(%u, SIGTERM): %s",
                      getpgid (priv->child_pid),
                      strerror(errno));
            priv->last_sig = SIGTERM;
            return G_SOURCE_CONTINUE;
        case SIGTERM:
            DBG ("child %u didn't respond to SIGTERM, sending SIGKILL",
                 priv->child_pid);
            if (killpg (getpgid (priv->child_pid), SIGKILL))
                WARN ("killpg(%u, SIGKILL): %s",
                      getpgid (priv->child_pid),
                      strerror(errno));
            priv->last_sig = SIGKILL;
            return G_SOURCE_CONTINUE;
        case SIGKILL:
            DBG ("child %u didn't respond to SIGKILL, process is stuck in kernel",
                 priv->child_pid);
            priv->timer_id = 0;
            _clear_session (session);
            if (session->priv->can_emit_signal) {
                GError *error = TLM_GET_ERROR_FOR_ID (
                        TLM_ERROR_SESSION_TERMINATION_FAILURE,
                        "Unable to terminate session - process is stuck"
                        " in kernel");
                g_signal_emit (session, signals[SIG_SESSION_ERROR], 0, error);
                g_error_free (error);
            }
            return G_SOURCE_REMOVE;
        default:
            WARN ("%d has unknown signaling state %d",
                  priv->child_pid,
                  priv->last_sig);
    }
    return G_SOURCE_REMOVE;
}

void
tlm_session_terminate (TlmSession *session)
{
    g_return_if_fail (session && TLM_IS_SESSION(session));
    TlmSessionPrivate *priv = TLM_SESSION_PRIV(session);

    DBG ("Session Terminate");

    if (!priv->is_child_up) {
        DBG ("no child process is running - closing pam session");
        _clear_session (session);
        if (session->priv->can_emit_signal)
            g_signal_emit (session, signals[SIG_SESSION_TERMINATED], 0);
        return;
    }

    if (killpg (getpgid (priv->child_pid), SIGHUP) < 0)
        WARN ("kill(%u, SIGHUP): %s",
              getpgid (priv->child_pid),
              strerror(errno));
    priv->last_sig = SIGHUP;
    priv->timer_id = g_timeout_add_seconds (
            tlm_config_get_uint (priv->config, TLM_CONFIG_GENERAL,
                    TLM_CONFIG_GENERAL_TERMINATE_TIMEOUT, 3),
            _terminate_timeout,
            session);
}

void
tlm_session_reset_tty (TlmSession *session)
{
    TlmSessionPrivate *priv = TLM_SESSION_PRIV(session);

    if (fchown (0, priv->tty_uid, priv->tty_gid))
        WARN ("Changing TTY access rights failed");
    if (tcflush (0, TCIOFLUSH) ||
        tcflush (1, TCIOFLUSH) ||
        tcflush (2, TCIOFLUSH))
        WARN ("Flushing stdio failed");
    pid_t pgid = getpgid (getpid ());
    if (tcsetpgrp (0, pgid) || tcsetpgrp (1, pgid) || tcsetpgrp (2, pgid))
        WARN ("Change TTY controlling process failed");
    if (tcsetattr (0, TCSANOW, &priv->stdin_state) ||
        tcsetattr (1, TCSANOW, &priv->stdout_state) ||
        tcsetattr (2, TCSANOW, &priv->stderr_state))
        WARN ("Restoring TTY settings failed");
}

