#ifndef _TLM_SESSION_H
#define _TLM_SESSION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define TLM_TYPE_SESSION       (tlm_session_get_type())
#define TLM_SESSION(obj)       (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                              TLM_TYPE_SESSION, TlmSession))
#define TLM_IS_SESSION(obj)    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                             TLM_TYPE_SESSION))
#define TLM_SESSION_CLASS(kls) (G_TYPE_CHECK_CLASS_CAST((kls), \
                             TLM_TYPE_SESSION))
#define TLM_SESSION_IS_CLASS(kls)  (G_TYPE_CHECK_CLASS_TYPE((kls), \
                                 TLM_TYPE_SESSION))

typedef struct _TlmSession TlmSession;
typedef struct _TlmSessionClass TlmSessionClass;
typedef struct _TlmSessionPrivate TlmSessionPrivate;

struct _TlmSession
{
    GObject parent;
    /* Private */
    TlmSessionPrivate *priv;
};

struct _TlmSessionClass
{
    GObjectClass parent_class;
};

GType tlm_session_get_type(void);

TlmSession *
tlm_session_new (const gchar *service);

gboolean
tlm_session_putenv (TlmSession *session,
                    const gchar *var,
                    const gchar *val);

gboolean
tlm_session_start (TlmSession *session, const gchar *username);

gboolean
tlm_session_stop (TlmSession *session);

G_END_DECLS

#endif /* _TLM_SESSION_H */
