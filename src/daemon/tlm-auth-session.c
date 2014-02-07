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

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h> 
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <security/pam_appl.h>
#include <gio/gio.h>

#include "tlm-auth-session.h"
#include "tlm-log.h"
#include "tlm-utils.h"

G_DEFINE_TYPE (TlmAuthSession, tlm_auth_session, G_TYPE_OBJECT);

#define TLM_AUTH_SESSION_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        TLM_TYPE_AUTH_SESSION, TlmAuthSessionPrivate)

enum {
    PROP_0,
    PROP_SERVICE,
    PROP_USERNAME,
    PROP_PASSWORD,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

enum {
    SIG_AUTH_ERROR,
    SIG_AUTH_SUCCESS,
    SIG_SESSION_CREATED,
    SIG_SESSION_ERROR,
    SIG_MAX
};
static guint signals[SIG_MAX];

struct _TlmAuthSessionPrivate
{
    gchar *service;
    gchar *username;
    gchar *password;
    gchar *session_id; /* logind session path */
    pam_handle_t *pam_handle;
};

static void
_auth_session_stop (TlmAuthSession *auth_session)
{
    int res;

    g_return_if_fail (auth_session &&
                TLM_IS_AUTH_SESSION (auth_session));

    TlmAuthSessionPrivate *priv = TLM_AUTH_SESSION_PRIV (auth_session);

    g_return_if_fail (priv->pam_handle);
    res = pam_setcred (priv->pam_handle, PAM_DELETE_CRED);
    if (res != PAM_SUCCESS) {
        WARN ("Failed to remove credentials from pam session: %s",
              pam_strerror (priv->pam_handle, res));
    }

    res = pam_end (priv->pam_handle,
                   pam_close_session (priv->pam_handle, 0));
    if (res != PAM_SUCCESS) {
        WARN ("Failed to end pam session: %s",
            pam_strerror (priv->pam_handle, res));
    }

    priv->pam_handle = NULL;
}

static void
tlm_auth_session_dispose (GObject *self)
{
    TlmAuthSession *auth_session = TLM_AUTH_SESSION (self);
    TlmAuthSessionPrivate *priv = TLM_AUTH_SESSION_PRIV (auth_session);
    DBG ("disposing auth_session: %s:%s", priv->service, priv->username);

    if (priv->pam_handle)
        _auth_session_stop (auth_session);

    G_OBJECT_CLASS (tlm_auth_session_parent_class)->dispose (self);
}

static void
tlm_auth_session_finalize (GObject *self)
{
    TlmAuthSessionPrivate *priv = TLM_AUTH_SESSION (self)->priv;

    g_clear_string (&priv->service);
    g_clear_string (&priv->username);
    g_clear_string (&priv->password);

    G_OBJECT_CLASS (tlm_auth_session_parent_class)->finalize (self);
}

static void
_auth_session_set_property (GObject *obj,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    TlmAuthSession *auth_session = TLM_AUTH_SESSION (obj);
    TlmAuthSessionPrivate *priv = TLM_AUTH_SESSION_PRIV (auth_session);

    switch (property_id) {
        case PROP_SERVICE: 
            priv->service = g_value_dup_string (value);
            break;
        case PROP_USERNAME:
            priv->username = g_value_dup_string (value);
            break;
        case PROP_PASSWORD:
            priv->password = g_value_dup_string (value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
_auth_session_get_property (GObject *obj,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
    TlmAuthSession *auth_session = TLM_AUTH_SESSION(obj);
    TlmAuthSessionPrivate *priv = TLM_AUTH_SESSION_PRIV (auth_session);

    switch (property_id) {
        case PROP_SERVICE: 
            g_value_set_string (value, priv->service);
            break;
        case PROP_USERNAME:
            g_value_set_string (value, priv->username);
            break;
        case PROP_PASSWORD:
            g_value_set_string (value, priv->password);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
tlm_auth_session_class_init (TlmAuthSessionClass *klass)
{
    GObjectClass *g_klass = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof(TlmAuthSessionPrivate));

    g_klass->dispose = tlm_auth_session_dispose ;
    g_klass->finalize = tlm_auth_session_finalize;
    g_klass->set_property = _auth_session_set_property;
    g_klass->get_property = _auth_session_get_property;

    pspecs[PROP_SERVICE] =
        g_param_spec_string ("service",
                             "authentication service",
                             "Service",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_USERNAME] =
        g_param_spec_string ("username",
                             "username",
                             "Username",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);
    pspecs[PROP_PASSWORD] =
        g_param_spec_string ("password",
                             "password",
                             "Unix password for the user to login",
                             NULL,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);

    signals[SIG_AUTH_ERROR] = g_signal_new ("auth-error", TLM_TYPE_AUTH_SESSION,
                                G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                G_TYPE_NONE, 1, G_TYPE_ERROR);

    signals[SIG_AUTH_SUCCESS] = g_signal_new ("auth-success", 
                                TLM_TYPE_AUTH_SESSION, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                0, G_TYPE_NONE);

    signals[SIG_SESSION_CREATED] = g_signal_new ("session-created",
                                TLM_TYPE_AUTH_SESSION, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                1, G_TYPE_STRING);

    signals[SIG_SESSION_ERROR] = g_signal_new ("session-error",
                                TLM_TYPE_AUTH_SESSION, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                1, G_TYPE_ERROR);
                                
}

static void
tlm_auth_session_init (TlmAuthSession *auth_session)
{
    TlmAuthSessionPrivate *priv = TLM_AUTH_SESSION_PRIV (auth_session);
    
    priv->service = priv->username = NULL;

    auth_session->priv = priv;
}


static gchar *
_auth_session_get_logind_session_id (GError **error)
{
    GDBusConnection *bus;
    GVariant *result;
    gchar *session_id;

    bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, error);
    if (!bus)
        return NULL;

    DBG ("trying to get session id");
    result = g_dbus_connection_call_sync (bus,
                                          "org.freedesktop.login1",
                                          "/org/freedesktop/login1",
                                          "org.freedesktop.login1.Manager",
                                          "GetSessionByPID",
                                          g_variant_new("(u)", getpid()),
                                          G_VARIANT_TYPE("(o)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          error);
    g_object_unref (bus);
    if (!result) {
        DBG ("failed to get session id");
        return NULL;
    }

    g_variant_get (result, "(o)", &session_id);
    g_variant_unref (result);

    DBG ("logind session : %s", session_id);

    return session_id;
}

static int
_auth_session_pam_conversation_cb (int n_msgs,
                                   const struct pam_message **msgs,
                                   struct pam_response **resps,
                                   void *appdata_ptr)
{
    int i;
    TlmAuthSession *auth_session = TLM_AUTH_SESSION (appdata_ptr);

    (void) auth_session;
    DBG (" n_msgs : %d", n_msgs);

    *resps = calloc (n_msgs, sizeof(struct pam_response));
    for (i=0; i < n_msgs; i++) {
        const struct pam_message *msg = msgs[i];
        struct pam_response *resp = *resps + i;
        const char *login_prompt = "login";
        const char *pwd_prompt = "Password";

        DBG (" message string : '%s'", msg->msg);
        if (msg->msg_style == PAM_PROMPT_ECHO_ON &&
            strncmp(msg->msg, login_prompt, strlen(login_prompt)) == 0) {
            DBG ("  login prompt");
            resp->resp = strndup (auth_session->priv->username,
            		              PAM_MAX_RESP_SIZE - 1);
            resp->resp[PAM_MAX_RESP_SIZE - 1] = '\0';
        }
        else if (msg->msg_style == PAM_PROMPT_ECHO_OFF &&
                 strncmp(msg->msg, pwd_prompt, strlen(pwd_prompt)) == 0) {
            DBG ("  password prompt");
            resp->resp = strndup ("", PAM_MAX_RESP_SIZE - 1);
            resp->resp[PAM_MAX_RESP_SIZE - 1] = '\0';
        }
        else {
            resp->resp = NULL;
        }
        resp->resp_retcode = PAM_SUCCESS;
    }

    return PAM_SUCCESS;
}

gboolean
tlm_auth_session_putenv (
    TlmAuthSession *auth_session,
    const gchar *var,
    const gchar *value)
{
    int res;
    gchar *env_item;
    g_return_val_if_fail (
        auth_session && TLM_IS_AUTH_SESSION (auth_session), FALSE);
    g_return_val_if_fail (var, FALSE);

    if (value)
        env_item = g_strdup_printf ("%s=%s", var, value);
    else
        env_item = g_strdup_printf ("%s", var);
    res = pam_putenv (auth_session->priv->pam_handle, env_item);
    g_free (env_item);
    if (res != PAM_SUCCESS) {
        WARN ("pam putenv ('%s=%s') failed", var, value);
        return FALSE;
    }
    return TRUE;
}

gboolean
tlm_auth_session_start (TlmAuthSession *auth_session)
{
    int res;
    const char *pam_tty = NULL;
    const char *pam_ruser = NULL;
    GError *error = 0;
    g_return_val_if_fail (auth_session && 
                TLM_IS_AUTH_SESSION(auth_session), FALSE);

    TlmAuthSessionPrivate *priv = TLM_AUTH_SESSION_PRIV (auth_session);

    /*pam_tty = getenv ("DISPLAY");
    if (!pam_tty) {*/
        pam_tty = ttyname (0);
    //}
    DBG ("setting PAM_TTY to '%s'", pam_tty);
    if (pam_set_item (priv->pam_handle, PAM_TTY, pam_tty) != PAM_SUCCESS) {
            WARN ("pam_set_item(PAM_TTY, '%s')", pam_tty);
    }

    pam_ruser = tlm_user_get_name (geteuid());
    if (pam_set_item (priv->pam_handle, PAM_RUSER, pam_ruser) != PAM_SUCCESS) {
        WARN ("pam_set_item(PAM_RUSER, '%s')", pam_ruser);
    }

    if (pam_set_item (priv->pam_handle, PAM_RHOST, "localhost") !=
        PAM_SUCCESS) {
        WARN ("pam_set_item(PAM_RHOST)");
    }

    char *p_service = 0, *p_uname = 0;
    pam_get_item (priv->pam_handle, PAM_SERVICE, (const void **)&p_service);
    pam_get_item (priv->pam_handle, PAM_USER, (const void **)&p_uname);
    DBG ("PAM service : '%s', PAM username : '%s'", p_service, p_uname);
    DBG ("starting pam authentication for user '%s'", priv->username);
    if((res = pam_authenticate (priv->pam_handle, PAM_SILENT)) != PAM_SUCCESS) {
        WARN ("PAM authentication failure: %s",
              pam_strerror (priv->pam_handle, res));
        GError *error = g_error_new (TLM_AUTH_SESSION_ERROR,
                                     TLM_AUTH_SESSION_PAM_ERROR,
                                     "pam authenticaton failed : %s",
                                     pam_strerror (priv->pam_handle, res));
        g_signal_emit (auth_session, signals[SIG_AUTH_ERROR], 0, error);
        g_error_free (error); 
        return FALSE;
    }

    /*res = pam_acct_mgmt (priv->pam_handle, 0);
    if (res == PAM_NEW_AUTHTOK_REQD) {
        res = pam_chauthtok (priv->pam_handle, PAM_CHANGE_EXPIRED_AUTHTOK);
    }
    if (res != PAM_SUCCESS) {
        WARN ("Account validity check failed: %s",
              pam_strerror (priv->pam_handle, res));
        return FALSE;
    }*/

    g_signal_emit (auth_session, signals[SIG_AUTH_SUCCESS], 0);

    res = pam_setcred (priv->pam_handle, PAM_ESTABLISH_CRED);
    if (res != PAM_SUCCESS) {
        WARN ("Failed to establish pam credentials: %s", 
            pam_strerror (priv->pam_handle, res));
        return FALSE;
    }

    res = pam_open_session (priv->pam_handle, 0);
    if (res != PAM_SUCCESS) {
        WARN ("Failed to open pam session: %s",
            pam_strerror (priv->pam_handle, res));
        return FALSE;
    }

    res = pam_setcred (priv->pam_handle, PAM_REINITIALIZE_CRED);
    if (res != PAM_SUCCESS) {
        WARN ("Failed to reinitialize pam credentials: %s", 
            pam_strerror (priv->pam_handle, res));
        pam_close_session (priv->pam_handle, 0);
        return FALSE;
    }

    priv->session_id = _auth_session_get_logind_session_id (&error);
    if (!priv->session_id) {
        g_signal_emit (auth_session, signals[SIG_SESSION_ERROR], 0, error);
        g_error_free (error);
        pam_close_session (priv->pam_handle, 0);
        return FALSE;
    }
    g_signal_emit (auth_session, signals[SIG_SESSION_CREATED],
                   0, priv->session_id);
    return TRUE;
}

TlmAuthSession *
tlm_auth_session_new (const gchar *service,
                      const gchar *username,
                      const gchar *password)
{
    TlmAuthSession *auth_session = TLM_AUTH_SESSION (
        g_object_new (TLM_TYPE_AUTH_SESSION,
                      "service", service,
                      "username", username,
                      "password", password,
                      NULL));
    TlmAuthSessionPrivate *priv = TLM_AUTH_SESSION_PRIV (auth_session);

    int res;
    struct pam_conv conv = { _auth_session_pam_conversation_cb,
                             auth_session };
    DBG ("loading pam for service '%s'", priv->service);
    res = pam_start (priv->service, priv->username,
                     &conv, &priv->pam_handle);
    if (res != PAM_SUCCESS) {
        WARN ("pam initialization failed: %s", pam_strerror (NULL, res));
        g_object_unref (auth_session);
        return NULL;
    }

    return auth_session;
}

const gchar *
tlm_auth_session_get_username (TlmAuthSession *auth_session)
{
    g_return_val_if_fail (TLM_IS_AUTH_SESSION (auth_session), NULL);

    return auth_session->priv->username;
}

gchar **
tlm_auth_session_get_envlist (TlmAuthSession *auth_session)
{
	g_return_val_if_fail(TLM_IS_AUTH_SESSION(auth_session), NULL);

    return (gchar **)pam_getenvlist(auth_session->priv->pam_handle);
}
