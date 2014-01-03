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

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "tlm-session.h"
#include "tlm-auth-session.h"
#include "tlm-log.h"
#include "tlm-utils.h"

G_DEFINE_TYPE (TlmSession, tlm_session, G_TYPE_OBJECT);

#define TLM_SESSION_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_SESSION, TlmSessionPrivate)

enum {
    PROP_0,
    PROP_SERVICE,
    PROP_NOTIFY_FD,
    PROP_USERNAME,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

struct _TlmSessionPrivate
{
    gint notify_fd;
    pid_t child_pid;
    gchar *service;
    gchar *username;
    GHashTable *env_hash;
    TlmAuthSession *auth_session;
};

static GHashTable *notify_table = NULL;

static void
tlm_session_dispose (GObject *self)
{
    TlmSession *session = TLM_SESSION(self);
    DBG("disposing session: %s", session->priv->service);

    tlm_auth_session_stop (session->priv->auth_session);
    g_clear_object (&session->priv->auth_session);
    if (session->priv->env_hash) {
        g_hash_table_unref (session->priv->env_hash);
        session->priv->env_hash = NULL;
    }

    G_OBJECT_CLASS (tlm_session_parent_class)->dispose (self);
}

static void
tlm_session_finalize (GObject *self)
{
    TlmSession *session = TLM_SESSION(self);

    g_clear_string (&session->priv->service);
    g_clear_string (&session->priv->username);

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
        case PROP_SERVICE: 
            priv->service = g_value_dup_string (value);
            break;
        case PROP_NOTIFY_FD:
            priv->notify_fd = g_value_get_int (value);
            break;
        case PROP_USERNAME:
            priv->username = g_value_dup_string (value);
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
        case PROP_SERVICE: 
            g_value_set_string (value, priv->service);
            break;
        case PROP_NOTIFY_FD:
            g_value_set_int (value, priv->notify_fd);
            break;
        case PROP_USERNAME:
            g_value_set_string (value, priv->username);
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

    pspecs[PROP_SERVICE] =
        g_param_spec_string ("service",
                             "authentication service",
                             "PAM service",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_NOTIFY_FD] =
        g_param_spec_int ("notify-fd",
                          "notification descriptor",
                          "SIGCHLD notification file descriptor",
                          0,
                          INT_MAX,
                          0,
                          G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);
    pspecs[PROP_USERNAME] =
        g_param_spec_string ("username",
                             "user name",
                             "Unix user name of user to login",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);
}

static void
tlm_session_init (TlmSession *session)
{
    TlmSessionPrivate *priv = TLM_SESSION_PRIV (session);
    
    priv->service = NULL;
    priv->env_hash = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            g_free,
                                            g_free);
    priv->auth_session = NULL;

    session->priv = priv;

    if (!notify_table) {
        notify_table = g_hash_table_new (g_direct_hash,
                                         g_direct_equal);
        /* NOTE: the notify_table won't be freed ever */
    }
}

gboolean
tlm_session_putenv (TlmSession *session, const gchar *var, const gchar *val)
{
    g_return_val_if_fail (session && TLM_IS_SESSION (session), FALSE);
    if (!session->priv->auth_session) {
        g_hash_table_insert (session->priv->env_hash, 
                                g_strdup (var), g_strdup (val));
        return TRUE;
    }

    return tlm_auth_session_putenv (session->priv->auth_session, var, val);
}

static void
_putenv_to_auth_session (gpointer key, gpointer val, gpointer user_data)
{
    TlmAuthSession *auth_session = TLM_AUTH_SESSION (user_data);

    tlm_auth_session_putenv(auth_session, (const gchar*)key, (const gchar*)val);
}

static void
_session_on_auth_error (
    TlmAuthSession *session, 
    GError *error, 
    gpointer userdata)
{
    WARN ("ERROR : %s", error->message);
}

static void
_session_on_session_error (
    TlmAuthSession *session, 
    GError *error, 
    gpointer userdata)
{
    if (!error)
        WARN ("ERROR but error is NULL");
    else
        WARN ("ERROR : %s", error->message);
}

static gboolean
_set_terminal (TlmSessionPrivate *priv)
{
    int i;
    int tty_fd;
    const char *tty_dev;
    struct stat tty_stat;

    tty_dev = ttyname (0);
    DBG ("trying to setup TTY '%s'", tty_dev);
    if (!tty_dev) {
        WARN ("no TTY");
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
        WARN ("invalid TTY");
        return FALSE;
    }

    tty_fd = open (tty_dev, O_RDWR | O_NONBLOCK);
    if (tty_fd < 0) {
        WARN ("open() failed: %s", strerror(errno));
        return FALSE;
    }
    if (!isatty(tty_fd)) {
        close (tty_fd);
        WARN ("isatty() failed");
        return FALSE;
    }

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

    if (envlist) {
    	gchar **env = 0;
    	for (env = envlist; *env != NULL; ++env) {
    		DBG ("ENV : %s", *env);
    		putenv(*env);
    		g_free (*env);
    	}
    	g_free (envlist);
    }

    setenv ("PATH", "/usr/local/bin:/usr/bin:/bin", 1);
    setenv ("USER", priv->username, 1);
    setenv ("LOGNAME", priv->username, 1);
    setenv ("HOME", tlm_user_get_home_dir (priv->username), 1);
    setenv ("SHELL", tlm_user_get_shell (priv->username), 1);

    return TRUE;
}

static void
_signal_action (
    int signal_no,
    siginfo_t *signal_info,
    void *context)
{
    gpointer notify_ptr;

    switch (signal_no) {
        case SIGCHLD:
            DBG ("SIGCHLD received for %u status %d",
                 signal_info->si_pid,
                 signal_info->si_status);
            notify_ptr = g_hash_table_lookup (notify_table,
                                              GUINT_TO_POINTER (signal_info->si_pid));
            if (!notify_ptr) {
                WARN ("no notify entry found for child pid %u",
                      signal_info->si_pid);
                return;
            }
            if (write (GPOINTER_TO_INT (notify_ptr),
                   &signal_info->si_pid,
                   sizeof (pid_t)) < (ssize_t) sizeof (pid_t))
                WARN ("failed to send notification");
            g_hash_table_remove (notify_table, notify_ptr);
            break;
        default:
            DBG ("%s received for %u",
                 strsignal (signal_no),
                 signal_info->si_pid);
    }
}

static void
_session_on_session_created (
    TlmAuthSession *auth_session,
    const gchar *id,
    gpointer userdata)
{
    const char *home;
    const char *shell;
    TlmSession *session = TLM_SESSION (userdata);
    TlmSessionPrivate *priv = session->priv;

    priv = session->priv;
    if (!priv->username)
        priv->username =
            g_strdup (tlm_auth_session_get_username (auth_session));
    DBG ("session ID : %s", id);

    priv->child_pid = fork ();
    if (priv->child_pid) {
        DBG ("establish handler for the child pid %u", priv->child_pid);
        struct sigaction sa;
        memset (&sa, 0x00, sizeof (sa));
        sa.sa_sigaction = _signal_action;
        sigaddset (&sa.sa_mask, SIGCHLD);
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        if (sigaction (SIGCHLD, &sa, NULL))
            WARN ("Failed to establish watch for %u", priv->child_pid);

        g_hash_table_insert (notify_table,
                             GUINT_TO_POINTER (priv->child_pid),
                             GINT_TO_POINTER (priv->notify_fd));
        return;
    }

    /* this is child process here onwards */

    _set_terminal (priv);

    setsid ();
    uid_t target_uid = tlm_user_get_uid (priv->username);
    gid_t target_gid = tlm_user_get_gid (priv->username);
    if (initgroups (priv->username, target_gid))
        WARN ("initgroups() failed: %s", strerror(errno));
    if (setregid (target_gid, target_gid))
        WARN ("setregid() failed: %s", strerror(errno));
    if (setreuid (target_uid, target_uid))
        WARN ("setreuid() failed: %s", strerror(errno));

    DBG (" state:\n\truid=%d, euid=%d, rgid=%d, egid=%d (%s)",
         getuid(), geteuid(), getgid(), getegid(), priv->username);
    _set_environment (priv);

    home = getenv("HOME");
    if (home) {
        DBG ("changing directory to : %s", home);
    	if (chdir (home) < 0)
            WARN ("Failed to change directroy : %s", strerror (errno));
    } else WARN ("Could not get home directory");

    shell = getenv("SHELL");
    if (shell) {
        DBG ("starting shell %s", shell);
        execlp (shell, shell, (const char *) NULL);
    } else {
        DBG ("starting systemd user session");
        execlp ("systemd", "systemd", "--user", (const char *) NULL);
    }
    DBG ("execl(): %s", strerror(errno));
}

static gboolean
_start_session (TlmSession *session, const gchar *password)
{
    g_return_val_if_fail (session && TLM_IS_SESSION(session), FALSE);

    session->priv->auth_session = 
        tlm_auth_session_new (session->priv->service,
                              session->priv->username,
                              password);

    if (!session->priv->auth_session) return FALSE;

    g_signal_connect (session->priv->auth_session, "auth-error", 
                G_CALLBACK(_session_on_auth_error), (gpointer)session);
    g_signal_connect (session->priv->auth_session, "session-created",
                G_CALLBACK(_session_on_session_created), (gpointer)session);
    g_signal_connect (session->priv->auth_session, "session-error",
                G_CALLBACK (_session_on_session_error), (gpointer)session);

    g_hash_table_foreach (session->priv->env_hash, 
                          _putenv_to_auth_session,
                          session->priv->auth_session);

    return tlm_auth_session_start (session->priv->auth_session);
}

TlmSession *
tlm_session_new (const gchar *service, gint notify_fd,
                 const gchar *username, const gchar *password,
                 const gchar *seat_id)
{
    TlmSession *session =
        g_object_new (TLM_TYPE_SESSION,
                      "service", service,
                      "notify-fd", notify_fd,
                      "username", username,
                      NULL);
    tlm_session_putenv (session, "XDG_SEAT", seat_id);
    if (!_start_session (session, password)) {
        g_object_unref (session);
        return NULL;
    }
    return session;
}

void
tlm_session_terminate (TlmSession *session)
{
    g_return_if_fail (session && TLM_IS_SESSION(session));

    if (kill(session->priv->child_pid, SIGTERM) < 0)
        WARN ("kill(%u, SIGTERM): %s",
              session->priv->child_pid, strerror(errno));
}

