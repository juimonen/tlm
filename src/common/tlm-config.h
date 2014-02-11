/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@intel.com>
 *          Amarnath Valluri <amarnath.valluri@linux.intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __TLM_CONFIG_H_
#define __TLM_CONFIG_H_

#include <glib.h>
#include <glib-object.h>


G_BEGIN_DECLS

#define TLM_TYPE_CONFIG            (tlm_config_get_type())
#define TLM_CONFIG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        TLM_TYPE_CONFIG, TlmConfig))
#define TLM_CONFIG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), \
        TLM_TYPE_CONFIG, TlmConfigClass))
#define TLM_IS_CONFIG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
        TLM_TYPE_CONFIG))
#define TLM_IS_CONFIG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
        TLM_TYPE_CONFIG))
#define TLM_CONFIG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), \
        TLM_TYPE_CONFIG, TlmConfigClass))

typedef struct _TlmConfig TlmConfig;
typedef struct _TlmConfigClass TlmConfigClass;
typedef struct _TlmConfigPrivate TlmConfigPrivate;

struct _TlmConfig
{
    GObject parent;

    /* priv */
    TlmConfigPrivate *priv;
};

struct _TlmConfigClass
{
    GObjectClass parent_class;
};

GType
tlm_config_get_type (void) G_GNUC_CONST;

TlmConfig *
tlm_config_new ();

gint
tlm_config_get_int (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        gint retval);

void
tlm_config_set_int (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        gint value);

guint
tlm_config_get_uint (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        guint retval);

void
tlm_config_set_uint (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        guint value);

const gchar*
tlm_config_get_string (
        TlmConfig *self,
        const gchar *group,
        const gchar *key);

void
tlm_config_set_string (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        const gchar *value);

gboolean
tlm_config_get_boolean (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        gboolean retval);

void
tlm_config_set_boolean (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        gboolean value);

gboolean
tlm_config_has_group (
        TlmConfig *self,
        const gchar *group);

gboolean
tlm_config_has_key (
        TlmConfig *self,
        const gchar *group,
        const gchar *key);

GHashTable *
tlm_config_get_group (
        TlmConfig *self,
        const gchar *group);

void
tlm_config_reload (
        TlmConfig *self);

G_END_DECLS

#endif /* __TLM_CONFIG_H_ */
