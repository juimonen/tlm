#include "tlm-auth-session.h"
#include "tlm-log.h"
#include "tlm-utils.h"

#include <security/pam_appl.h>
#include <stdio.h>
#include <malloc.h> /* calloc() */
#include <string.h> /* strlen() */
#include <gio/gio.h>

G_DEFINE_TYPE (TlmAuthSession, tlm_auth_session, G_TYPE_OBJECT);

#define TLM_AUTH_SESSION_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        TLM_TYPE_AUTH_SESSION, TlmAuthSessionPrivate)

enum {
    PROP_0,
    PROP_SERVICE,
    PROP_USERNAME,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

enum {
    SIG_AUTH_ERROR,
    SIG_AUTH_SUCCESS,
    SIG_SESSION_OPEND,
    SIG_SESSION_ERROR,
    SIG_MAX
};
static guint signals[SIG_MAX];

struct _TlmAuthSessionPrivate
{
    gchar *service;
    gchar *username;
    gchar *session_id; /* logind session path */
    pam_handle_t *pam_handle;
};

static void
tlm_auth_session_dispose (GObject *self)
{
    TlmAuthSessionPrivate *priv = TLM_AUTH_SESSION(self)->priv;
    DBG("disposing auth_session: %s:%s", priv->service, priv->username);

    if (priv->pam_handle) {
        pam_end (priv->pam_handle, 0);
        priv->pam_handle = 0;
    }

    G_OBJECT_CLASS (tlm_auth_session_parent_class)->dispose (self);
}

static void
tlm_auth_session_finalize (GObject *self)
{
    TlmAuthSessionPrivate *priv = TLM_AUTH_SESSION(self)->priv;

    g_clear_string (&priv->service);
    g_clear_string (&priv->username);

    G_OBJECT_CLASS (tlm_auth_session_parent_class)->finalize (self);
}

static void
_auth_session_set_property (GObject *obj,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
    TlmAuthSession *auth_session = TLM_AUTH_SESSION(obj);

    switch (property_id) {
        case PROP_SERVICE: 
        auth_session->priv->service = g_value_dup_string (value);
        break;

        case PROP_USERNAME:
        auth_session->priv->username = g_value_dup_string (value);
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

    switch (property_id) {
        case PROP_SERVICE: 
        g_value_set_string (value, auth_session->priv->service);
        break;

        case PROP_USERNAME:
        g_value_set_string (value, auth_session->priv->username);
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

    pspecs[PROP_SERVICE] = g_param_spec_string ("service", 
                        "authentication service", "Service", NULL, 
                        G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);
    pspecs[PROP_USERNAME] = g_param_spec_string ("username", 
                        "username", "Username", NULL, 
                        G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);

    signals[SIG_AUTH_ERROR] = g_signal_new ("auth-error", TLM_TYPE_AUTH_SESSION,
                                G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                                G_TYPE_NONE, 1, G_TYPE_ERROR);

    signals[SIG_AUTH_SUCCESS] = g_signal_new ("auth-success", 
                                TLM_TYPE_AUTH_SESSION, G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                0, G_TYPE_NONE);

    signals[SIG_SESSION_OPEND] = g_signal_new ("session-opened",
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
    if (!bus) {
        return NULL;
    }

    result = g_dbus_connection_call_sync (bus,
                                          "org.freedesktop.login1",
                                          "/org/freedesktop/login1",
                                          "ofg.freedesktop.login1.Manager",
                                          "GetSessionByPID",
                                          g_variant_new("(u)", getpid()),
                                          G_VARIANT_TYPE("(o)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          error);
    g_object_unref (bus);
    if (error) {
        return NULL;
    }

    g_variant_get (result, "(o)", &session_id);
    g_object_unref (result);

    DBG ("Logind session : %s", session_id);

    return session_id;
}

static int
_auth_session_pam_conversation_cb (
    int n_msgs,
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
        struct pam_response *resp = resps[i];

        DBG ("Message string : %s", msg->msg);
        if (msg->msg_style  == PAM_PROMPT_ECHO_OFF) {
            resp->resp = strndup ("", PAM_MAX_RESP_SIZE - 1);
            resp->resp[PAM_MAX_RESP_SIZE-1]='\0';
            resp->resp_retcode = PAM_SUCCESS;
        }
        else {
            resp->resp = NULL;
            resp->resp_retcode = PAM_SUCCESS;
        }
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
    char env_item[1024];
    g_return_val_if_fail (
        auth_session && TLM_IS_AUTH_SESSION (auth_session), FALSE);
    g_return_val_if_fail (var, FALSE);

    snprintf (env_item, 1023, "%s=%s", var, value ? value : "");
    res = pam_putenv (auth_session->priv->pam_handle, env_item);
    if (res != PAM_SUCCESS) {
        WARN ("pam putenv ('%s=%s') failed", var, value);
        return FALSE;
    }
    return TRUE;
}

/*
 * FIXME-NOTES: 
 *
 *   i) The pam authentication session should be handled
 * in a forked process(authentication-helper daemon).
 *
 *  ii)  Few environment variables are hard coded for testing purpose.
 *
 * iii) Guest user creation is not yet implemented, it should be done
 * by using gumd/accountsservice etc,
 *
 *  iv) opening logind session (org.freedesktop.logind1.Manager.CreateSession
 * which is called from pam_systemd) was failing with error 'Access deined',
 * this should be investigated.
 *
 *   v) Expected flow is, once the pam_open_session() is success the 
 *   authentication helper daemon should reply to the login-manager,
 *   then login-manager should start a wayland-session.
 */
gboolean
tlm_auth_session_start (TlmAuthSession *auth_session)
{
    TlmAuthSessionPrivate *priv = NULL;
    int res ;
    GError *error = 0;
    g_return_val_if_fail (auth_session && 
                TLM_IS_AUTH_SESSION(auth_session), FALSE);

    priv = auth_session->priv;

    if (!priv->pam_handle) {
        struct pam_conv conv = { _auth_session_pam_conversation_cb,
                                 auth_session };
        DBG ("Loading pam for service '%s'", priv->service);
        res = pam_start (priv->service, priv->username,
                             &conv, &priv->pam_handle);
        if (res != PAM_SUCCESS) {
            WARN ("pam initialization failed: %s", pam_strerror (NULL, res));
            GError *error = g_error_new (TLM_AUTH_SESSION_ERROR,
                                 TLM_AUTH_SESSION_PAM_ERROR,
                                "pam inititalization failed : %s",
                                 pam_strerror (NULL, res));
            g_signal_emit (auth_session, signals[SIG_AUTH_ERROR], 0, error);
            g_error_free (error);
            return FALSE;
        }
    }
    char *p_service = 0, *p_uname = 0;
    pam_get_item (priv->pam_handle, PAM_SERVICE, (const void **)&p_service);
    pam_get_item (priv->pam_handle, PAM_USER, (const void **)&p_uname);
    DBG ("PAM Service : '%s', PAM username : '%s'", p_service, p_uname);
    //res = pam_set_item (priv->pam_handle, PAM_RUSER, "root");
    //if (res != PAM_SUCCESS) {
    //    WARN ("Failed to set PAM_RUSER: %s", 
    //        pam_strerror (priv->pam_handle, res));
    //}
    DBG ("Starting pam authentication for user '%s'", priv->username); 
    res = pam_authenticate (priv->pam_handle, PAM_SILENT);
    if (res != PAM_SUCCESS) {
        WARN ("pam authentication failure: %s", 
            pam_strerror (priv->pam_handle, res));
        GError *error = g_error_new (TLM_AUTH_SESSION_ERROR,
                                 TLM_AUTH_SESSION_PAM_ERROR,
                                "pam authenticaton failed : %s",
                                 pam_strerror (priv->pam_handle, res));
        g_signal_emit (auth_session, signals[SIG_AUTH_ERROR], 0, error);
        g_error_free (error); 
        return FALSE;
    }

    g_signal_emit (auth_session, signals[SIG_AUTH_SUCCESS], 0);

    tlm_auth_session_putenv (auth_session, "PATH", 
                        "/usr/local/bin:/usr/bin:/bin");
    tlm_auth_session_putenv (auth_session, "USER", priv->username);
    tlm_auth_session_putenv (auth_session, "LOGNAME", priv->username);
    tlm_auth_session_putenv (auth_session, "HOME", 
                        tlm_user_get_home_dir (priv->username));
    tlm_auth_session_putenv (auth_session, "SHELL",
                        tlm_user_get_shell (priv->username));
    tlm_auth_session_putenv (auth_session, "XDG_SESSION_CLASS", "greeter");
    tlm_auth_session_putenv (auth_session, "XDG_VTNR", "1");

    res = pam_setcred (priv->pam_handle, 0);
    if (res != PAM_SUCCESS) {
        WARN ("Failed to establish pam credentials: %s", 
            pam_strerror (priv->pam_handle, res));
        return FALSE;
    }

    res = pam_open_session (priv->pam_handle, PAM_SILENT);
    if (res != PAM_SUCCESS) {
        WARN ("Failed to open pam session: %s",
            pam_strerror (priv->pam_handle, res));
        return FALSE;
    }

    priv->session_id = _auth_session_get_logind_session_id (&error);
    if (!priv->session_id) {
        g_signal_emit (auth_session, signals[SIG_SESSION_ERROR], 0, error);
        g_error_free (error);
        return FALSE;
    }
    g_signal_emit (auth_session, signals[SIG_SESSION_OPEND],
                         0, priv->session_id);
    return TRUE;
}

gboolean
tlm_auth_session_stop (TlmAuthSession *auth_session)
{
    int res;
    TlmAuthSessionPrivate *priv = NULL;
    g_return_val_if_fail (auth_session &&
                TLM_IS_AUTH_SESSION(auth_session), FALSE);

    priv = auth_session->priv;
    res = pam_close_session (priv->pam_handle, PAM_SILENT);
    if (res != PAM_SUCCESS) {
        WARN ("Failed to close pam session: %s",
            pam_strerror (priv->pam_handle, res));
        return FALSE;
    }

    return TRUE;
}

TlmAuthSession *
tlm_auth_session_new (const gchar *service, const gchar *username)
{
    TlmAuthSession *auth_session = TLM_AUTH_SESSION(g_object_new 
                (TLM_TYPE_AUTH_SESSION,
                 "service", service, "username", username, NULL));

    struct pam_conv conv = { _auth_session_pam_conversation_cb,
                            auth_session };
    int res;

    res = pam_start (service, username, &conv, &auth_session->priv->pam_handle);
    if (res != PAM_SUCCESS) {
        WARN ("Failed to start pam for service '%s' with user '%s'",
                service, username);
    }

    return auth_session;
}

