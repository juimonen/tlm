/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
 *
 * Copyright (C) 2014 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <glib.h>
#include <gio/gio.h>
#include <Elementary.h>
#include <pwd.h>
#include <shadow.h>

#include "tlm-log.h"

#define BUFLEN 8096
#define UID_MIN "UID_MIN"
#define UID_MAX "UID_MAX"

static Evas_Object *user_label = NULL;

static void
_set_list_title (
        Evas_Object *obj,
        Elm_Object_Item *item)
{
    if (item && obj) {
        elm_object_text_set(obj, elm_object_item_text_get(item));
    }
}

static void
_on_selected (
        void *data,
        Evas_Object *obj,
        void *event_info)
{
    Elm_Object_Item *item = event_info;
    if (item) {
        DBG("%s", elm_object_item_text_get(item));
        _set_list_title (obj, item);
    }
}

static void
_on_cbox_changed (
        void *data,
        Evas_Object *obj,
        void *event_info)
{
    gboolean marked = *((Eina_Bool*)data);
    DBG("check %s marked", marked ? "" : "un");
}

static void
_get_uids (
        gint *uid_min,
        gint *uid_max)
{
    FILE *fp = NULL;
    gchar buf[BUFLEN];

    fp = fopen (TLM_LOGINDEFS_PATH, "r");
    if (!fp) return;

    while (!feof (fp)) {
        gchar *key, *val, *line;
        ssize_t n;

        buf[0] = '\0';
        if (!fgets (buf, BUFLEN-1, fp) || strlen (buf) < 1)
            break;

        g_strdelimit ((gchar *)buf, "#\n", '\0');
        if (strlen (buf) < 1)
            continue;

        val = buf;
        while (isspace ((int)*val))
            ++val;

        key = strsep (&val, " \t=");
        if (val != NULL) {
            while (isspace ((int)*val) || *val == '=')
                ++val;
        }
        if (g_strcmp0 (key, UID_MIN) == 0 && val) {
            *uid_min = atoi (val);
        }
        else if (g_strcmp0 (key, UID_MAX) == 0 && val) {
            *uid_max = atoi (val);
        }
    }
    fclose (fp);
}

static gboolean
_is_valid_user (struct passwd *pent)
{
    gint uid_min = 1000, uid_max = 60000;
    _get_uids (&uid_min, &uid_max);
    return (pent->pw_uid >= uid_min) && (pent->pw_uid <= uid_max);
}

static void
_populate_users (Evas_Object *users_list)
{
    struct passwd *pent = NULL;

    setpwent ();
    while ((pent = getpwent ()) != NULL) {
        if (g_strcmp0 ("x", pent->pw_passwd) == 0 &&
            _is_valid_user (pent)) {
            elm_hoversel_item_add(users_list, pent->pw_name, NULL,
                    ELM_ICON_NONE, NULL, NULL);
        }
        pent = NULL;
    }
    endpwent ();
}

static Evas_Object *
_add_checkbox (
        Evas_Object *win)
{
    Evas_Object *box = NULL, *checkbox;
    Eina_Bool value = EINA_FALSE;

    box = elm_box_add (win);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (win, box);
    evas_object_size_hint_align_set (box, 0.0, 0.1);

    checkbox = elm_check_add(win);
    elm_object_text_set(checkbox, "NFC Authentication");
    elm_check_state_pointer_set(checkbox, &value);
    evas_object_smart_callback_add(checkbox, "changed", _on_cbox_changed, &value);
    elm_box_pack_end (box, checkbox);
    evas_object_show(checkbox);

    return box;
}

static Evas_Object *
_add_user_list (
        Evas_Object *win)
{
    Evas_Object *box = NULL, *main_box, *label, *users_list, *cb;
    box = elm_box_add (win);
    //evas_object_size_hint_max_set (box, 50, 50);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (win, box);
    evas_object_size_hint_align_set (box, 0.1, 0.1);

    label = elm_label_add(win);
    elm_object_text_set(label, "Switch User");
    //evas_object_move(label, 0, 10);
    elm_box_pack_end (box, label);
    evas_object_show(label);

    users_list = elm_hoversel_add(win);
    //elm_hoversel_hover_parent_set(users_list, box);
    elm_hoversel_horizontal_set(users_list, EINA_FALSE);
    elm_object_text_set(users_list, "Select User From the List");

    _populate_users (users_list);

    evas_object_smart_callback_add(users_list, "selected", _on_selected, NULL);
    elm_box_pack_end (box, users_list);

    evas_object_show(users_list);

    main_box = elm_box_add (win);
    evas_object_size_hint_weight_set (main_box, EVAS_HINT_EXPAND,
            EVAS_HINT_EXPAND);
    evas_object_show (main_box);
    elm_box_horizontal_set (main_box, EINA_TRUE);
    elm_win_resize_object_add (win, main_box);

    elm_box_pack_end (main_box, box);

    cb = _add_checkbox (win);
    elm_box_pack_end (main_box, cb);

    return box;
}

static Evas_Object *
_add_loggedin_box (
        Evas_Object *win)
{
    Evas_Object *box = NULL, *label;
    Eina_Bool value;

    box = elm_box_add (win);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (win, box);
    elm_box_horizontal_set (box, EINA_TRUE);
    evas_object_size_hint_align_set (box, 0.1, 0.0);

    label = elm_label_add(win);
    elm_object_text_set(label, "Current logged-in user :: ");
    elm_box_pack_end (box, label);
    evas_object_show(label);

    user_label = elm_label_add(win);
    elm_box_pack_end (box, user_label);
    evas_object_show(user_label);

    return box;
}

static gchar *
_get_loggedin_username (
        GDBusProxy *proxy,
        const gchar *object_path)
{
    GError *error = NULL;
    GVariant *result = NULL;
    gchar *username = NULL;

    result = g_dbus_connection_call_sync (
            g_dbus_proxy_get_connection (proxy),
            g_dbus_proxy_get_name (proxy),
            object_path,
            "org.freedesktop.DBus.Properties",
            "GetAll",
            g_variant_new ("(s)",  "org.freedesktop.login1.Session"),
            G_VARIANT_TYPE ("(a{sv})"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &error);
    if (error) {
        printf ("Failed with error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
        return NULL;
    }

    if (g_variant_is_of_type (result, G_VARIANT_TYPE ("(a{sv})"))) {
        GVariantIter *iter = NULL;
        GVariant *item = NULL;
        g_variant_get (result, "(a{sv})",  &iter);
        while ((item = g_variant_iter_next_value (iter)))  {
            gchar *key;
            GVariant *value;
            g_variant_get (item, "{sv}", &key, &value);
            if (g_strcmp0 (key, "Name") == 0) {
                username = g_strdup (g_variant_get_string(value, NULL));
                g_free (key); key = NULL;
                g_variant_unref (value); value = NULL;
                break;
            }
            g_free (key); key = NULL;
            g_variant_unref (value); value = NULL;
        }
    }
    g_variant_unref (result);
    return username;
}

static void
_set_loggedin_username ()
{
    GError *error = NULL;
    GDBusProxy *proxy = NULL;
    GVariant *res = NULL;

    GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL,
            &error);
    if (error) goto _finished;

    proxy = g_dbus_proxy_new_sync (connection,
            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL,
            "org.freedesktop.login1", //destintation
            "/org/freedesktop/login1", //path
            "org.freedesktop.login1.Manager", //interface
            NULL, &error);
    if (error) goto _finished;

    res = g_dbus_proxy_call_sync (proxy, "GetSessionByPID",
            g_variant_new ("(u)", getpid()), G_DBUS_CALL_FLAGS_NONE, -1,
            NULL, &error);
    if (res) {
        gchar *obj_path = NULL;
        g_variant_get (res, "(o)", &obj_path);
        gchar *name = _get_loggedin_username (proxy, obj_path);
        if (name && user_label) {
            elm_object_text_set(user_label, name);
        }
        g_free (name);
        g_variant_unref (res); res = NULL;
        g_free (obj_path);
    }

_finished:
    if (error) {
        DBG ("failed to listen for login events: %s", error->message);
        g_error_free (error);
    }
    if (proxy) g_object_unref (proxy);
    if (connection) g_object_unref (connection);
}

EAPI_MAIN int
elm_main (
        int argc,
        char **argv)
{
    Evas_Object *win, *bg, *box, *loggedin, *ulist;

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    win = elm_win_add(NULL, "tlmui", ELM_WIN_BASIC);
    elm_win_title_set(win, "Demo tlm-ui");
    elm_win_autodel_set(win, EINA_TRUE);
    elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
    evas_object_resize(win, 500, 400);
    evas_object_show(win);

    //background
    bg = elm_bg_add(win);
    evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(win, bg);
    evas_object_show(bg);

    box = elm_box_add (win);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (win, box);

    ulist = _add_user_list (win);
    elm_box_pack_end (box, ulist);

    loggedin = _add_loggedin_box (win);
    elm_box_pack_end (box, loggedin);

    _set_loggedin_username ();

    // run the mainloop and process events and callbacks
    elm_run();
    elm_shutdown();
    return 0;
}
ELM_MAIN()
