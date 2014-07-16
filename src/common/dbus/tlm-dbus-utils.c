/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tizen Login Manager)
 *
 * Copyright (C) 2014 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@intel.com>
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

#include "tlm-dbus-utils.h"

TlmDbusRequest *
tlm_dbus_utils_create_request (
        GObject *object,
        GDBusMethodInvocation *invocation,
        TlmDbusRequestType type,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        GVariant *environment)
{
    TlmDbusRequest *request = g_malloc0 (sizeof (TlmDbusRequest));
    if (!request) return NULL;

    request->type = type;
    if (invocation) request->invocation = g_object_ref (invocation);
    if (object) request->dbus_adapter = g_object_ref (object);
    if (seat_id) request->seat_id = g_strdup (seat_id);
    if (username) request->username = g_strdup (username);
    if (password) request->password = g_strdup (password);
    if (environment)
        request->environment = tlm_dbus_utils_hash_table_from_variant (
                environment);
    return request;
}

void
tlm_dbus_utils_dispose_request (
        TlmDbusRequest *request)
{
    if (!request) return;

    g_object_unref (request->dbus_adapter);
    g_object_unref (request->invocation);
    g_free (request->seat_id);
    g_free (request->username);
    g_free (request->password);

    if (request->environment) {
        g_hash_table_unref (request->environment);
    }

    g_free (request);
}

GVariantBuilder *
_tlm_utils_hash_table_to_variant_builder (GHashTable *dict)
{
    GVariantBuilder *builder;
    GHashTableIter iter;
    const gchar *key = NULL;
    const gchar *value = NULL;

    g_return_val_if_fail (dict != NULL, NULL);

    builder = g_variant_builder_new (((const GVariantType *) "a{ss}"));

    g_hash_table_iter_init (&iter, dict);
    while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value))
    {
        g_variant_builder_add (builder, "{ss}", key, value);
    }

    return builder;
}

GVariant *
tlm_dbus_utils_hash_table_to_variant (GHashTable *dict)
{
    GVariantBuilder *builder = NULL;
    GVariant *vdict = NULL;

    g_return_val_if_fail (dict != NULL, NULL);

    builder = _tlm_utils_hash_table_to_variant_builder (dict);
    if (!builder) return NULL;

    vdict = g_variant_builder_end (builder);

    g_variant_builder_unref (builder);

    return vdict;
}

GHashTable *
tlm_dbus_utils_hash_table_from_variant (GVariant *variant)
{
    GHashTable *dict = NULL;
    GVariantIter iter;
    gchar *key = NULL;
    GVariant *value = NULL;

    g_return_val_if_fail (variant != NULL, NULL);

    dict = g_hash_table_new_full ((GHashFunc)g_str_hash,
            (GEqualFunc)g_str_equal,
            (GDestroyNotify)g_free,
            (GDestroyNotify)g_free);
    g_variant_iter_init (&iter, variant);
    while (g_variant_iter_next (&iter, "{ss}", &key, &value))
    {
        g_hash_table_insert (dict, key, value);
    }

    return dict;
}
