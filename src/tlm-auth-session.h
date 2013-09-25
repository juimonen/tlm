#ifndef _TLM_AUTH_SESSION_H
#define _TLM_AUTH_SESSION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define TLM_TYPE_AUTH_SESSION       (tlm_auth_session_get_type())
#define TLM_AUTH_SESSION(obj)       (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                              TLM_TYPE_AUTH_SESSION, TlmAuthSession))
#define TLM_IS_AUTH_SESSION(obj)    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                             TLM_TYPE_AUTH_SESSION))
#define TLM_AUTH_SESSION_CLASS(kls) (G_TYPE_CHECK_CLASS_CAST((kls), \
                             TLM_TYPE_AUTH_SESSION))
#define TLM_AUTH_SESSION_IS_CLASS(kls)  (G_TYPE_CHECK_CLASS_TYPE((kls), \
                                 TLM_TYPE_AUTH_SESSION))


typedef enum {
    TLM_AUTH_SESSION_PAM_ERROR = 1,
} TlmAuthSessionError;

#define TLM_AUTH_SESSION_ERROR g_quark_from_string("tlm-auth-session-error")

typedef struct _TlmAuthSession TlmAuthSession;
typedef struct _TlmAuthSessionClass TlmAuthSessionClass;
typedef struct _TlmAuthSessionPrivate TlmAuthSessionPrivate;

struct _TlmAuthSession
{
    GObject parent;
    /* Private */
    TlmAuthSessionPrivate *priv;
};

struct _TlmAuthSessionClass
{
    GObjectClass parent_class;
};

GType tlm_auth_session_get_type(void);

TlmAuthSession *
tlm_auth_session_new (const gchar *service, const gchar *username);

gboolean
tlm_auth_session_putenv (TlmAuthSession *auth_session,
                         const gchar *var,
                         const gchar *value);

gboolean
tlm_auth_session_start (TlmAuthSession *auth_session);

gboolean
tlm_auth_session_stop (TlmAuthSession *auth_session);

G_END_DECLS

#endif /* _TLM_AUTH_SESSION_H */
