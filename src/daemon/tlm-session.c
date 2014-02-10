/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
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
#include <utmp.h>
#include <paths.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>

#include "tlm-session.h"
#include "tlm-auth-session.h"
#include "tlm-log.h"
#include "tlm-utils.h"
#include "tlm-config-general.h"

G_DEFINE_TYPE (TlmSession, tlm_session, G_TYPE_OBJECT);

#define TLM_SESSION_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_SESSION, TlmSessionPrivate)

#define HOST_NAME_SIZE 256

enum {
    PROP_0,
    PROP_CONFIG,
    PROP_SEAT,
    PROP_SERVICE,
    PROP_NOTIFY_FD,
    PROP_USERNAME,
    PROP_ENVIRONMENT,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

struct _TlmSessionPrivate
{
    TlmConfig *config;
    gint notify_fd;
    pid_t child_pid;
    uid_t tty_uid;
    gid_t tty_gid;
    struct termios tty_state;
    gchar *seat_id;
    gchar *service;
    gchar *username;
    GHashTable *env_hash;
    TlmAuthSession *auth_session;
    int last_sig;
    guint timer_id;
};

static GHashTable *notify_table = NULL;

static void
tlm_session_dispose (GObject *self)
{
    TlmSession *session = TLM_SESSION(self);
    DBG("disposing session: %s", session->priv->service);

    if (session->priv->timer_id)
        g_source_remove (session->priv->timer_id);

    g_clear_object (&session->priv->auth_session);
    if (session->priv->env_hash) {
        g_hash_table_unref (session->priv->env_hash);
        session->priv->env_hash = NULL;
    }
    g_clear_object (&session->priv->config);

    G_OBJECT_CLASS (tlm_session_parent_class)->dispose (self);
}

static void
tlm_session_finalize (GObject *self)
{
    TlmSession *session = TLM_SESSION(self);

    g_clear_string (&session->priv->seat_id);
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
        case PROP_NOTIFY_FD:
            priv->notify_fd = g_value_get_int (value);
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
        case PROP_NOTIFY_FD:
            g_value_set_int (value, priv->notify_fd);
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
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
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
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_NOTIFY_FD] =
        g_param_spec_int ("notify-fd",
                          "notification descriptor",
                          "SIGCHLD notification file descriptor",
                          0,
                          INT_MAX,
                          0,
                          G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_USERNAME] =
        g_param_spec_string ("username",
                             "user name",
                             "Unix user name of user to login",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_ENVIRONMENT] =
        g_param_spec_pointer ("environment",
                              "environment variables",
                              "Environment variables for the session",
                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);
}

static void
tlm_session_init (TlmSession *session)
{
    TlmSessionPrivate *priv = TLM_SESSION_PRIV (session);
    
    priv->service = NULL;
    priv->env_hash = NULL;
    priv->auth_session = NULL;

    session->priv = priv;

    if (!notify_table) {
        notify_table = g_hash_table_new (g_direct_hash,
                                         g_direct_equal);
        /* NOTE: the notify_table won't be freed ever */
    }

    struct stat tty_stat;

    if (fstat (0, &tty_stat) == 0) {
        priv->tty_uid = tty_stat.st_uid;
        priv->tty_gid = tty_stat.st_gid;
    } else {
        priv->tty_uid = 0;
        priv->tty_gid = 0;
    }

    if (tcgetattr (0, &priv->tty_state))
        WARN ("Failed to retrieve initial terminal state");
}

static void
_setenv_to_session (gpointer key, gpointer val, gpointer user_data)
{
    /*TlmSessionPrivate *priv = (TlmSessionPrivate *) user_data;*/

    setenv ((const char *) key, (const char *) val, 1);
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
    pid_t tty_pid;
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
    tty_pid = getpid ();
    if (ioctl (tty_fd, TIOCSPGRP, &tty_pid)) {
        WARN ("ioctl(TIOCSPGRP) failed: %s", strerror(errno));
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

    const gchar *path = tlm_config_get_string (priv->config,
                                               TLM_CONFIG_GENERAL,
                                               TLM_CONFIG_GENERAL_SESSION_PATH);
    if (!path)
        path = "/usr/local/bin:/usr/bin:/bin";
    setenv ("PATH", path, 1);

    setenv ("USER", priv->username, 1);
    setenv ("LOGNAME", priv->username, 1);
    setenv ("HOME", tlm_user_get_home_dir (priv->username), 1);
    setenv ("SHELL", tlm_user_get_shell (priv->username), 1);
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
    gint i;
    const gchar *pattern = "('.*?'|\".*?\"|\\S+)";
    const char *home;
    const char *shell;
    gchar **args = NULL;
    gchar **args_iter = NULL;
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

    /* ==================================
     * this is child process here onwards
     * ================================== */

    if (tlm_config_get_boolean (priv->config,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_SETUP_TERMINAL,
                                FALSE)) {
        /* usually terminal settings are handled by PAM */
        _set_terminal (priv);
    }

    if (getppid() == 1) {
        setsid ();
        if (ioctl (0, TIOCSCTTY, 1)) {
            WARN ("ioctl(TIOCSCTTY) failed: %s", strerror(errno));
        }
    }
    uid_t target_uid = tlm_user_get_uid (priv->username);
    gid_t target_gid = tlm_user_get_gid (priv->username);

    if (fchown (0, target_uid, -1)) {
        WARN ("Changing TTY access rights failed");
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

    shell = tlm_config_get_string (priv->config,
                                   TLM_CONFIG_GENERAL,
                                   TLM_CONFIG_GENERAL_SESSION_CMD);
    if (shell) {
        DBG ("Session command : %s", shell);
        gchar **temp_strv = g_regex_split_simple (pattern,
                                                  shell,
                                                  0,
                                                  G_REGEX_MATCH_NOTEMPTY);
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
        }
    } else {
        args = g_new0 (gchar *, 3);
        shell = getenv("SHELL");
        if (shell) {
            /* use shell if no override configured */
            args[0] = g_strdup (shell);
        } else {
            /* in case shell is not defined, fall back to systemd --user */
            args[0] = g_strdup ("systemd");
            args[1] = g_strdup ("--user");
        }
    }
    DBG ("executing: ");
    for (args_iter = args, i = 0;
         *args_iter != NULL;
         args_iter++, i++) {
        DBG ("\targv[%d]: %s", i, *args_iter);
    }
    execvp (args[0], args);
    /* we reach here only in case of error */
    g_strfreev (args);
    DBG ("execl(): %s", strerror(errno));
}

static gboolean
_start_session (TlmSession *session,
                const gchar *password)
{
    g_return_val_if_fail (session && TLM_IS_SESSION(session), FALSE);
    TlmSessionPrivate *priv = TLM_SESSION_PRIV(session);

    priv->auth_session =
        tlm_auth_session_new (priv->service,
                              priv->username,
                              password);

    if (!priv->auth_session) return FALSE;

    g_signal_connect (priv->auth_session, "auth-error",
                G_CALLBACK(_session_on_auth_error), (gpointer)session);
    g_signal_connect (priv->auth_session, "session-created",
                G_CALLBACK(_session_on_session_created), (gpointer)session);
    g_signal_connect (priv->auth_session, "session-error",
                G_CALLBACK (_session_on_session_error), (gpointer)session);

    tlm_auth_session_putenv (priv->auth_session, "XDG_SEAT", priv->seat_id);

    return tlm_auth_session_start (priv->auth_session);
}

static gchar *
_get_tty_id (
        const gchar *tty_name)
{
    gchar *id = NULL;
    const gchar *tmp = tty_name;

    while (tmp) {
        if (isdigit(*tmp)) {
            id = g_strdup (tmp);
            break;
        }
        tmp++;
    }
    return id;
}

static gchar *
_get_host_address (
        const gchar *hostname)
{
    gchar *hostaddress = NULL;
    struct addrinfo hints, *info = NULL;

    if (!hostname) return NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG;

    if (getaddrinfo(hostname, NULL, &hints, &info) == 0) {
        if (info) {
            if (info->ai_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *) info->ai_addr;
                hostaddress = g_malloc0 (sizeof(sa->sin_addr));
                memcpy(hostaddress, &(sa->sin_addr), sizeof(sa->sin_addr));
            } else if (info->ai_family == AF_INET6) {
                struct sockaddr_in6 *sa = (struct sockaddr_in6 *) info->ai_addr;
                hostaddress = g_malloc0 (sizeof(sa->sin6_addr));
                memcpy(hostaddress, &(sa->sin6_addr), sizeof(sa->sin6_addr));
            }
            freeaddrinfo(info);
        }
    }
    return hostaddress;
}

static gboolean
_is_tty_same (
        const gchar *tty1_name,
        const gchar *tty2_name)
{
    gchar *tty1 = NULL, *tty2 = NULL;
    gboolean res = FALSE;

    if (tty1_name == tty2_name) return TRUE;
    if (!tty1_name || !tty2_name) return FALSE;

    if (*tty1_name == '/') tty1 = g_strdup (tty1_name);
    else tty1 = g_strdup_printf ("/dev/%s", tty1_name);
    if (*tty2_name == '/') tty2 = g_strdup (tty2_name);
    else tty2 = g_strdup_printf ("/dev/%s", tty2_name);

    res = (g_strcmp0 (tty1_name, tty2_name) == 0);

    g_free (tty1);
    g_free (tty2);
    return res;
}

static gchar *
_get_host_name ()
{
    gchar *name = g_malloc0 (HOST_NAME_SIZE);
    if (gethostname(name, HOST_NAME_SIZE) != 0) {
        g_free (name);
        return NULL;
    }
    return name;
}

static void
_log_utmp_entry (TlmSession *self)
{
    struct timeval tv;
    pid_t pid;
    struct utmp ut_ent;
    struct utmp *ut_tmp = NULL;
    gchar *hostname = NULL, *hostaddress = NULL;
    const gchar *tty_name = NULL;
    gchar *tty_no_dev_name = NULL, *tty_id = NULL;

    hostname = _get_host_name ();
    hostaddress = _get_host_address (hostname);
    tty_name = ttyname (0);
    tty_no_dev_name = g_strdup (strncmp(tty_name, "/dev/", 5) == 0 ?
            tty_name + 5 : tty_name);
    tty_id = _get_tty_id (tty_no_dev_name);
    pid = getpid ();
    utmpname(_PATH_UTMP);

    setutent();
    while ((ut_tmp = getutent())) {
        if ( (ut_tmp->ut_pid == pid) &&
             (ut_tmp->ut_id[0] != '\0') &&
             (ut_tmp->ut_type == LOGIN_PROCESS ||
                     ut_tmp->ut_type == USER_PROCESS) &&
             (_is_tty_same (ut_tmp->ut_line, tty_name))) {
            break;
        }
    }

    if (ut_tmp) memcpy(&ut_ent, ut_tmp, sizeof(ut_ent));
    else        memset(&ut_ent, 0, sizeof(ut_ent));

    ut_ent.ut_type = USER_PROCESS;
    ut_ent.ut_pid = pid;
    if (tty_id)
        strncpy(ut_ent.ut_id, tty_id, sizeof(ut_ent.ut_id));
    if (self->priv->username)
        strncpy(ut_ent.ut_user, self->priv->username, sizeof(ut_ent.ut_user));
    if (tty_no_dev_name)
        strncpy(ut_ent.ut_line, tty_no_dev_name, sizeof(ut_ent.ut_line));
    if (hostname)
        strncpy(ut_ent.ut_host, hostname, sizeof(ut_ent.ut_host));
    if (hostaddress)
        memcpy(&ut_ent.ut_addr_v6, hostaddress, sizeof(ut_ent.ut_addr_v6));

    ut_ent.ut_session = getsid (0);
    gettimeofday(&tv, NULL);
#ifdef _HAVE_UT_TV
    ut_ent.ut_tv.tv_sec = tv.tv_sec;
    ut_ent.ut_tv.tv_usec = tv.tv_usec;
#else
    ut_ent.ut_time = tv.tv_sec;
#endif

    pututline(&ut_ent);
    endutent();

    updwtmp(_PATH_WTMP, &ut_ent);

    g_free (hostaddress);
    g_free (hostname);
    g_free (tty_no_dev_name);
    g_free (tty_id);
}

TlmSession *
tlm_session_new (TlmConfig *config,
                 const gchar *seat_id, const gchar *service,
                 const gchar *username, const gchar *password,
                 GHashTable *environment, gint notify_fd)
{
    DBG ("Session New");

    TlmSession *session =
        g_object_new (TLM_TYPE_SESSION,
                      "config", config,
                      "seat", seat_id,
                      "service", service,
                      "notify-fd", notify_fd,
                      "username", username,
                      "environment", environment,
                      NULL);
    if (!_start_session (session, password)) {
        WARN ("Session startup failed");
        g_object_unref (session);
        return NULL;
    }

    _log_utmp_entry (session);
    return session;
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
            priv->last_sig = SIGTERM;
            if (kill (priv->child_pid, SIGTERM))
                WARN ("kill(%u, SIGTERM): %s", priv->child_pid, strerror(errno));
            return G_SOURCE_CONTINUE;
        case SIGTERM:
            DBG ("child %u didn't respond to SIGTERM, sending SIGKILL",
                 priv->child_pid);
            priv->last_sig = SIGKILL;
            if (kill (priv->child_pid, SIGKILL))
                WARN ("kill(%u, SIGKILL): %s", priv->child_pid, strerror(errno));
            return G_SOURCE_CONTINUE;
        case SIGKILL:
            DBG ("child %u didn't respond to SIGKILL, process is stuck in kernel",
                 priv->child_pid);
            priv->timer_id = 0;
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

    if (kill (priv->child_pid, SIGHUP) < 0)
        WARN ("kill(%u, SIGHUP): %s",
              priv->child_pid, strerror(errno));
    priv->last_sig = SIGHUP;
    priv->timer_id = g_timeout_add_seconds (
            tlm_config_get_uint (priv->config, TLM_CONFIG_GENERAL,
                    TLM_CONFIG_GENERAL_TERMINATE_TIMEOUT, 10),
            _terminate_timeout,
            session);
}


void
tlm_session_reset_tty (TlmSession *session)
{
    TlmSessionPrivate *priv = TLM_SESSION_PRIV(session);

    if (fchown (0, priv->tty_uid, priv->tty_gid))
        WARN ("Changing TTY access rights failed");
    if (tcflush (0, TCIOFLUSH))
        WARN ("Flushing stdio failed");
    if (tcsetpgrp (0, getpid ()))
        WARN ("Change TTY controlling process failed");
    if (tcsetattr (0, TCSANOW, &priv->tty_state))
        WARN ("Restoring TTY settings failed");
}

