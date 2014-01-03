/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Amarnath Valluri <amarnath.valluri@linux.intel.com>
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

#ifndef _TLM_ACCOUNT_PLUGIN_DEFAULT_H
#define _TLM_ACCOUNT_PLUGIN_DEFAULT_H

#include <glib.h>
#include "tlm-account-plugin.h"

G_BEGIN_DECLS

#define TLM_TYPE_ACCOUNT_PLUGIN_DEFAULT (tlm_account_plugin_default_get_type())
#define TLM_ACCOUNT_PLUGIN_DEFAULT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST( \
        (obj), TLM_TYPE_ACCOUNT_PLUGIN_DEFAULT, TlmAccountPluginDefault))
#define TLM_IS_ACCOUNT_PLUGIN_DEFAULT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TLM_TYPE_ACCOUNT_PLUGIN_DEFAULT))
#define TLM_ACCOUNT_PLUGIN_DEFAULT_CLASS(kls) \
    (G_TYPE_CHECK_CLASS_CAST( \
        (klass), TLM_TYPE_ACCOUNT_PLUGIN_DEFAULT, TlmPluginDefaultClass))
#define TLM_IS_ACCOUNT_PLUGIN_DEFAULT_CLASS(kls) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), TLM_TYPE_ACCOUNT_PLUGIN_DEFAULT))

typedef struct _TlmAccountPluginDefault TlmAccountPluginDefault;
typedef struct _TlmAccountPluginDefaultClass TlmAccountPluginDefaultClass;

struct _TlmAccountPluginDefaultClass
{
    GObjectClass parent_class;
};

GType tlm_account_plugin_default_get_type ();

G_END_DECLS

#endif /* _TLM_ACCOUNT_PLUGIN_DEFAULT_H */

