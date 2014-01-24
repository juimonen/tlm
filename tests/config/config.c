/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
 *
 * Copyright (C) 2014 Intel Corporation.
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

#include <check.h>
#include <stdlib.h>
#include "tlm-config.h"

#define TLM_GROUP   "tlm-test"
#define STR_KEY     "str_key"
#define STR_VALUE   "str_value"
#define INT_KEY     "int_key"
#define INT_VALUE   -32768
#define UINT_KEY    "uint_key"
#define UINT_VALUE  65535
#define BOOL_KEY    "bool_key"
#define BOOL_VALUE  TRUE

#define _(arg) #arg
#define STRINGIFY(arg) _(arg)
#define STR_INT_VALUE  STRINGIFY(INT_VALUE)
#define STR_UINT_VALUE STRINGIFY(UINT_VALUE)
#define STR_BOOL_VALUE "True"


START_TEST(test_config)
{
    const gchar *tmp_str = NULL;
    gint tmp_int;
    guint tmp_uint;
    gboolean tmp_bool;
    GHashTable *tmp_group = NULL;
    struct _Group {
       const gchar *key;
       const gchar *value;
    } tmp_group_table[] = {
        { STR_KEY, STR_VALUE },
        { INT_KEY, STR_INT_VALUE },
        { UINT_KEY, STR_UINT_VALUE },
        { BOOL_KEY, STR_BOOL_VALUE }
    };
    size_t len;
    int i;

    TlmConfig *config = NULL;

    config = tlm_config_new ();
    fail_if (config == NULL, "Failed to create config object");

    /* string */
    tmp_str = tlm_config_get_string (config, TLM_GROUP, STR_KEY);
    fail_if (tmp_str == NULL, "Failed to read key '%s/%s'", TLM_GROUP, STR_KEY);
    fail_if (strcmp(tmp_str, STR_VALUE) != 0, "Wrong value : %s", tmp_str);

    /* int */
    tmp_int = tlm_config_get_int (config, TLM_GROUP, INT_KEY, -1);
    fail_if (tmp_int != INT_VALUE, "Failed to read key '%s/%s' : %d",
                                    TLM_GROUP, INT_KEY, tmp_int);


    /* uint */
    tmp_uint = tlm_config_get_uint (config, TLM_GROUP, UINT_KEY, 0);
    fail_if (tmp_uint != UINT_VALUE, "Failed to read key '%s/%s': %u",
                                     TLM_GROUP, UINT_KEY, tmp_uint);

    /* boolean */
    tmp_bool = tlm_config_get_boolean (config, TLM_GROUP, BOOL_KEY, FALSE);
    fail_if (tmp_bool == FALSE, "Failed to read key : '%s/%s' : %d",
                                TLM_GROUP, BOOL_KEY, tmp_bool);

    /* group */
    len = sizeof(tmp_group_table)/sizeof(struct _Group);
    tmp_group = tlm_config_get_group (config, TLM_GROUP);
    fail_if (tmp_group == NULL);
    fail_if (g_hash_table_size (tmp_group) != len);

    for (i=0; i<len; i++) {
        tmp_str = g_hash_table_lookup (tmp_group, tmp_group_table[i].key);
        fail_if (tmp_str == NULL);
        fail_if (strcmp(tmp_str, tmp_group_table[i].value) != 0,
                 "Wrong value '%s' for key '%s' where expected '%s'",
                 tmp_str, tmp_group_table[i].key, tmp_group_table[i].value);
    }

    /* unknwon key */
    tmp_str = tlm_config_get_string (config, TLM_GROUP, "unknown");
    fail_if (tmp_str != NULL);

    /* unknown group */
    tmp_group = tlm_config_get_group (config, "unknown");
    fail_if (tmp_group != NULL);

    g_object_unref (config);

}
END_TEST

int main (void)
{
    int number_failed;
#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif
    SRunner *sr = NULL;
    Suite *s = suite_create ("tlm config tests");
    TCase *tc = tcase_create ("Config");

    tcase_add_test (tc, test_config);
    suite_add_tcase (s, tc);

    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : -1;
}
