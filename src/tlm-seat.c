#include "tlm-seat.h"
#include "tlm-log.h"

G_DEFINE_TYPE (TlmSeat, tlm_seat, G_TYPE_OBJECT);

#define TLM_SEAT_PRIV(obj) \
    G_TYPE_INSTANCE_GET_PRIVATE ((obj), TLM_TYPE_SEAT, TlmSeatPrivate)

enum {
    PROP_0,
    PROP_ID,
    PROP_PATH,
    N_PROPERTIES
};
static GParamSpec *pspecs[N_PROPERTIES];

struct _TlmSeatPrivate
{
    gchar *id;
    gchar *path;
};

static void
tlm_seat_dispose (GObject *self)
{
    DBG("disposing seat: %s", TLM_SEAT(self)->priv->id);

    G_OBJECT_CLASS (tlm_seat_parent_class)->dispose (self);
}

static void
tlm_seat_finalize (GObject *self)
{
    TlmSeat *seat = TLM_SEAT(self);

    if (seat->priv->id) {
        g_free (seat->priv->id);
        seat->priv->id = NULL;
    }

    if (seat->priv->path) {
        g_free (seat->priv->path);
        seat->priv->path = NULL;
    }

    G_OBJECT_CLASS (tlm_seat_parent_class)->finalize (self);
}

static void
_seat_set_property (GObject *obj,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
    TlmSeat *seat = TLM_SEAT(obj);

    switch (property_id) {
        case PROP_ID: 
        seat->priv->id = g_value_dup_string (value);
        break;

        case PROP_PATH:
        seat->priv->path = g_value_dup_string (value);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
_seat_get_property (GObject *obj,
                    guint property_id,
                    GValue *value,
                    GParamSpec *pspec)
{
    TlmSeat *seat = TLM_SEAT(obj);

    switch (property_id) {
        case PROP_ID: 
        g_value_set_string (value, seat->priv->id);
        break;

        case PROP_PATH:
        g_value_set_string (value, seat->priv->path);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
tlm_seat_class_init (TlmSeatClass *klass)
{
    GObjectClass *g_klass = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof(TlmSeatPrivate));

    g_klass->dispose = tlm_seat_dispose ;
    g_klass->finalize = tlm_seat_finalize;
    g_klass->set_property = _seat_set_property;
    g_klass->get_property = _seat_get_property;

    pspecs[PROP_ID] = g_param_spec_string ("id", "seat id", "Seat Id", NULL, 
                        G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS);
    pspecs[PROP_PATH] = g_param_spec_string ("path", "object path",
             "Seat Object path at logind", NULL, 
             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);
}

static void
tlm_seat_init (TlmSeat *seat)
{
    TlmSeatPrivate *priv = TLM_SEAT_PRIV (seat);
    
    priv->id = priv->path = NULL;

    seat->priv = priv;
}

TlmSeat *
tlm_seat_new (const gchar *id, const gchar *path)
{
    return g_object_new (TLM_TYPE_SEAT, "id", id, "path", path, NULL);
}


