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

#include <sys/types.h>
#include <pwd.h>

#include "tlm-utils.h"

void
g_clear_string (gchar **str)
{
    if (str && *str) {
        g_free (*str);
        str = NULL;
    }
}

const gchar *
tlm_user_get_name (uid_t user_id)
{
    struct passwd *pwent;

    pwent = getpwuid (user_id);
    if (!pwent)
        return NULL;

    return pwent->pw_name;
}

uid_t
tlm_user_get_uid (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return -1;

    return pwent->pw_uid;
}

gid_t
tlm_user_get_gid (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return -1;

    return pwent->pw_gid;
}

const gchar *
tlm_user_get_home_dir (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return NULL;

    return pwent->pw_dir;
}

const gchar *
tlm_user_get_shell (const gchar *username)
{
    struct passwd *pwent;

    pwent = getpwnam (username);
    if (!pwent)
        return NULL;

    return pwent->pw_shell;
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
tlm_utils_hash_table_to_variant (GHashTable *dict)
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
tlm_utils_hash_table_from_variant (GVariant *variant)
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

