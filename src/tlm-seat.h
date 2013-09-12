#ifndef _TLM_SEAT_H
#define _TLM_SEAT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define TLM_TYPE_SEAT       (tlm_seat_get_type())
#define TLM_SEAT(obj)       (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                              TLM_TYPE_SEAT, TlmSeat))
#define TLM_IS_SEAT(obj)    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                             TLM_TYPE_SEAT))
#define TLM_SEAT_CLASS(kls) (G_TYPE_CHECK_CLASS_CAST((kls), \
                             TLM_TYPE_SEAT))
#define TLM_SEAT_IS_CLASS(kls)  (G_TYPE_CHECK_CLASS_TYPE((kls), \
                                 TLM_TYPE_SEAT))

typedef struct _TlmSeat TlmSeat;
typedef struct _TlmSeatClass TlmSeatClass;
typedef struct _TlmSeatPrivate TlmSeatPrivate;

struct _TlmSeat
{
    GObject parent;
    /* Private */
    TlmSeatPrivate *priv;
};

struct _TlmSeatClass
{
    GObjectClass parent_class;
};

GType tlm_seat_get_type(void);

TlmSeat *
tlm_seat_new (const gchar *id, const gchar *path);

G_END_DECLS

#endif /* _TLM_SEAT_H */
