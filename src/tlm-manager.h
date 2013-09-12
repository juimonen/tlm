#ifndef _TLM_MANAGER_H
#define _TLM_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define TLM_TYPE_MANAGER            (tlm_manager_get_type())
#define TLM_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                     TLM_TYPE_MANAGER, TlmManager))
#define TLM_IS_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                                     TLM_TYPE_MANAGER))
#define TLM_MANAGER_CLASS(cls)      (G_TYPE_CHECK_CLASS_CAST((cls), \
                                     TLM_TYPE_MANAGER))
#define TLM_IS_MANAGER_CLASS(cls)   (G_TYPE_CHECK_CLASS_TYPE((cls), \
                                     TLM_TYPE_MANAGER))

typedef struct _TlmManager TlmManager;
typedef struct _TlmManagerClass TlmManagerClass;
typedef struct _TlmManagerPrivate TlmManagerPrivate;

struct _TlmManager
{
    GObject parent;
    TlmManagerPrivate *priv;
};

struct _TlmManagerClass
{
    GObjectClass parent_class;
};

GType tlm_manager_get_type (void);

TlmManager * tlm_manager_new ();

G_END_DECLS

#endif /* _TLM_MANAGER_H */
