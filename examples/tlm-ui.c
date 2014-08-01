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

#include "common/tlm-log.h"
#include "common/dbus/tlm-dbus.h"
#include "common/dbus/tlm-dbus-login-gen.h"
#include "common/dbus/tlm-dbus-utils.h"

#define BUFLEN 8096
#define UID_MIN "UID_MIN"
#define UID_MAX "UID_MAX"
#define TLM_LOGINDEFS_PATH "/etc/login.defs"

typedef enum {
    TLM_UI_REQUEST_NONE = 0,
    TLM_UI_REQUEST_LOGIN,
    TLM_UI_REQUEST_LOGOUT,
    TLM_UI_REQUEST_SWITCH_USER
} RequestType;

typedef struct {
    Evas_Object *win;
    Evas_Object *user_label;
    gboolean use_nfc_tag;
} MainWindow;

typedef struct {
    Evas_Object *dialog;
    Evas_Object *username_entry;
    Evas_Object *password_entry;
    RequestType req_type;
} MainDialog;

static MainWindow *main_window = NULL;
static MainDialog *main_dialog = NULL;

GDBusConnection *
_get_root_socket_bus_connection (
        GError **error)
{
    gchar address[128];
    g_snprintf (address, 127, TLM_DBUS_ROOT_SOCKET_ADDRESS);
    return g_dbus_connection_new_for_address_sync (address,
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, NULL, NULL, error);
}

GDBusConnection *
_get_bus_connection (
        const gchar *seat_id,
        GError **error)
{
    uid_t ui_user_id = getuid ();

    if (ui_user_id == 0) {
        return _get_root_socket_bus_connection (error);
    }

    /* get dbus connection for specific user only */
    gchar address[128];
    g_snprintf (address, 127, "unix:path=%s/%s-%d", TLM_DBUS_SOCKET_PATH,
            seat_id, ui_user_id);
    return g_dbus_connection_new_for_address_sync (address,
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT, NULL, NULL, error);
}

TlmDbusLogin *
_get_login_object (
        GDBusConnection *connection,
        GError **error)
{
    return tlm_dbus_login_proxy_new_sync (connection, G_DBUS_PROXY_FLAGS_NONE,
            NULL, TLM_LOGIN_OBJECTPATH, NULL, error);
}


static GVariant *
_get_session_property (
        GDBusProxy *proxy,
        const gchar *object_path,
        const gchar *prop_key)
{
    GError *error = NULL;
    GVariant *result = NULL;
    GVariant *prop_value = NULL;

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
            if (g_strcmp0 (key, prop_key) == 0) {
                prop_value = value;
                g_free (key); key = NULL;
                break;
            }
            g_free (key); key = NULL;
            g_variant_unref (value); value = NULL;
        }
    }
    g_variant_unref (result);
    return prop_value;
}

static GVariant *
_get_property (
        const gchar *prop_key)
{
    GError *error = NULL;
    GDBusProxy *proxy = NULL;
    GVariant *res = NULL;
    GVariant *prop_value = NULL;

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
        prop_value = _get_session_property (proxy, obj_path, prop_key);
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

    return prop_value;
}

static void
_trigger_dbus_request (
        RequestType req_type,
        const gchar *username,
        const gchar *password)
{
    GError *error = NULL;
    GDBusConnection *connection = NULL;
    TlmDbusLogin *login_object = NULL;
    GHashTable *environ = NULL;
    GVariant *venv = NULL;
    gchar *seat = NULL;
    GVariant *vseat = NULL;

    const gchar *env_seat = g_getenv ("TLM_SEAT_ID");
    if (env_seat) seat = g_strdup (env_seat);
    if (!seat) {
        WARN ("No seat defined in environment variable, using seat0");
        seat = g_strdup ("seat0");
    }

    environ = g_hash_table_new_full ((GHashFunc)g_str_hash,
            (GEqualFunc)g_str_equal,
            (GDestroyNotify)g_free,
            (GDestroyNotify)g_free);
    venv = tlm_dbus_utils_hash_table_to_variant (environ);
    g_hash_table_unref (environ);

    connection = _get_bus_connection (seat, &error);
    if (error) goto _finished;

    login_object = _get_login_object (connection, &error);
    if (error) goto _finished;

    switch (req_type) {
    case TLM_UI_REQUEST_LOGIN:
        tlm_dbus_login_call_login_user_sync (login_object, seat, username,
                password, venv, NULL, &error);
        break;
    case TLM_UI_REQUEST_LOGOUT:
        tlm_dbus_login_call_logout_user_sync (login_object, seat, NULL, &error);
        break;
   case TLM_UI_REQUEST_SWITCH_USER:
        tlm_dbus_login_call_switch_user_sync (login_object, seat, username,
                password, venv, NULL, &error);
        break;
    }

_finished:
    g_free (seat);

    if (error) {
        DBG("Error %d:%s", error->code, error->message);
        g_error_free (error);
        error = NULL;
    }
    if (login_object) g_object_unref (login_object);
    if (connection) g_object_unref (connection);
}

static Evas_Object*
_add_entry (
        Evas_Object *window,
        Evas_Object *container,
        const gchar *label_text)
{
    Evas_Object *frame = NULL;
    Evas_Object *entry = NULL;

    if (label_text) {
        frame = elm_frame_add(window);

        elm_object_text_set(frame, label_text);
        evas_object_size_hint_weight_set(frame, 0.0, 0.0);
        evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_box_pack_end(container, frame);
        evas_object_show(frame);
    }

    entry = elm_entry_add (window);
    elm_entry_single_line_set (entry, EINA_TRUE);
    elm_entry_scrollable_set (entry, EINA_TRUE);
    evas_object_size_hint_min_set (entry, 150, 80);
    evas_object_size_hint_align_set (entry,  EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0.0);
    frame ? elm_object_content_set (frame, entry)
            : elm_box_pack_end (container, entry);

    return entry;
}

static void
_close_dialog ()
{
    if (main_dialog && main_dialog->dialog) {
        evas_object_hide (main_dialog->dialog);
    }
    g_free (main_dialog);
    main_dialog = NULL;
}

static void
_on_close_dialog_clicked (
        void *data,
        Evas_Object *obj,
        void *event_info)
{
    _close_dialog ();
}

static void
_on_ok_dialog_clicked (
        void *data,
        Evas_Object *obj,
        void *event_info)
{
    const gchar *username = NULL;
    const gchar *password = NULL;

    if (main_dialog) {
        RequestType req_type = main_dialog->req_type;
        if (main_dialog->username_entry)
            username = elm_entry_entry_get (main_dialog->username_entry);
        if (main_dialog->password_entry)
            password = elm_entry_entry_get (main_dialog->password_entry);

        _close_dialog ();

        if (main_window && main_window->use_nfc_tag)
            return;

        if (!username || !password) {
            WARN ("Invalid username/password");
            return;
        }

        _trigger_dbus_request (req_type, username, password);
    }
}

static Evas_Object *
_create_nfc_dialog (
        const gchar *username)
{
    Evas_Object *dialog, *bg, *box, *frame, *content_box, *label;
    Evas_Object *button_frame, *pad_frame, *button_box;
    Evas_Object *ok_button;

    main_dialog = g_malloc0 (sizeof (MainDialog));

    /* main window */
    dialog = elm_win_add (NULL, "dialog", ELM_WIN_BASIC);
    elm_win_title_set (dialog, "Show NFC tag");
    elm_win_center (dialog, EINA_TRUE, EINA_TRUE);
    evas_object_smart_callback_add (dialog, "delete,request",
            _on_close_dialog_clicked, dialog);
    main_dialog->dialog = dialog;

    /* window background */
    bg = elm_bg_add (dialog);
    evas_object_size_hint_weight_set (bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (bg);
    elm_win_resize_object_add (dialog, bg);

    box = elm_box_add (dialog);
    evas_object_size_hint_min_set (box, 200, 200);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (dialog, box);

    frame = elm_frame_add (dialog);
    elm_object_style_set (frame, "pad_small");
    evas_object_size_hint_weight_set (frame, EVAS_HINT_EXPAND,
            EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show (frame);
    elm_box_pack_start (box, frame);

    content_box = elm_box_add (dialog);
    elm_box_padding_set (content_box, 0, 3);
    evas_object_size_hint_weight_set (content_box, EVAS_HINT_EXPAND,
            EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (content_box, 0.0, 0.0);
    evas_object_show (content_box);
    elm_object_part_content_set (frame, NULL, content_box);

    /* NFC label */
    label = elm_label_add(dialog);
    elm_object_text_set(label, "Show your NFC tag");
    elm_object_style_set(label, "marker");
    evas_object_color_set(label, 255, 0, 0, 255);
    elm_box_pack_end (content_box, label);
    evas_object_show(label);

    button_frame = elm_frame_add (dialog);
    elm_object_style_set (button_frame, "outdent_bottom");
    evas_object_size_hint_weight_set (button_frame, 0.0, 0.0);
    evas_object_size_hint_align_set (button_frame, EVAS_HINT_FILL,
            EVAS_HINT_FILL);
    evas_object_show (button_frame);
    elm_box_pack_end (box, button_frame);

    pad_frame = elm_frame_add (dialog);
    elm_object_style_set (pad_frame, "pad_medium");
    evas_object_show (pad_frame);
    elm_object_part_content_set (button_frame, NULL, pad_frame);

    button_box = elm_box_add (dialog);
    elm_box_horizontal_set (button_box, 1);
    elm_box_homogeneous_set (button_box, 1);
    evas_object_show (button_box);
    elm_object_part_content_set (pad_frame, NULL, button_box);

    /* OK button */
    ok_button = elm_button_add (dialog);
    elm_object_text_set (ok_button, "OK");
    evas_object_size_hint_weight_set (ok_button,
            EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (ok_button,
            EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_smart_callback_add (ok_button, "clicked", _on_ok_dialog_clicked,
            dialog);
    evas_object_show (ok_button);
    elm_box_pack_end (button_box, ok_button);

    return dialog;
}

static Evas_Object *
_create_dialog (
        const gchar *username)
{
    Evas_Object *dialog, *bg, *box, *frame, *content_box;
    Evas_Object *button_frame, *pad_frame, *button_box;
    Evas_Object *cancel_button, *ok_button;

    g_free (main_dialog);
    main_dialog = g_malloc0 (sizeof (MainDialog));

    /* main window */
    dialog = elm_win_add (NULL, "dialog", ELM_WIN_BASIC);
    elm_win_title_set (dialog, "Enter user password");
    elm_win_center (dialog, EINA_TRUE, EINA_TRUE);
    evas_object_smart_callback_add (dialog, "delete,request",
            _on_close_dialog_clicked, dialog);
    main_dialog->dialog = dialog;

    /* window background */
    bg = elm_bg_add (dialog);
    evas_object_size_hint_weight_set (bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (bg);
    elm_win_resize_object_add (dialog, bg);

    box = elm_box_add (dialog);
    evas_object_size_hint_min_set (box, 200, 200);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (dialog, box);

    frame = elm_frame_add (dialog);
    elm_object_style_set (frame, "pad_small");
    evas_object_size_hint_weight_set (frame, EVAS_HINT_EXPAND,
            EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show (frame);
    elm_box_pack_start (box, frame);

    content_box = elm_box_add (dialog);
    elm_box_padding_set (content_box, 0, 3);
    evas_object_size_hint_weight_set (content_box, EVAS_HINT_EXPAND,
            EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (content_box, 0.0, 0.0);
    evas_object_show (content_box);
    elm_object_part_content_set (frame, NULL, content_box);

    main_dialog->username_entry = _add_entry (dialog, content_box, "Username:");
    if (username) elm_entry_entry_set (main_dialog->username_entry, username);
    elm_entry_editable_set (main_dialog->username_entry,
            username? EINA_FALSE : EINA_TRUE);

    main_dialog->password_entry = _add_entry (dialog, content_box, "Password:");
    elm_entry_password_set (main_dialog->password_entry, EINA_TRUE);
    elm_entry_editable_set (main_dialog->password_entry, EINA_TRUE);

    button_frame = elm_frame_add (dialog);
    elm_object_style_set (button_frame, "outdent_bottom");
    evas_object_size_hint_weight_set (button_frame, 0.0, 0.0);
    evas_object_size_hint_align_set (button_frame, EVAS_HINT_FILL,
            EVAS_HINT_FILL);
    evas_object_show (button_frame);
    elm_box_pack_end (box, button_frame);

    pad_frame = elm_frame_add (dialog);
    elm_object_style_set (pad_frame, "pad_medium");
    evas_object_show (pad_frame);
    elm_object_part_content_set (button_frame, NULL, pad_frame);

    button_box = elm_box_add (dialog);
    elm_box_horizontal_set (button_box, 1);
    elm_box_homogeneous_set (button_box, 1);
    evas_object_show (button_box);
    elm_object_part_content_set (pad_frame, NULL, button_box);

    /* Cancel button */
    cancel_button = elm_button_add (dialog);
    elm_object_text_set (cancel_button, "Cancel");
    evas_object_size_hint_weight_set (cancel_button,
            EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (cancel_button,
            EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_smart_callback_add (cancel_button, "clicked",
            _on_close_dialog_clicked, dialog);
    evas_object_show (cancel_button);
    elm_box_pack_end (button_box, cancel_button);

    /* OK button */
    ok_button = elm_button_add (dialog);
    elm_object_text_set (ok_button, "OK");
    evas_object_size_hint_weight_set (ok_button,
            EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (ok_button,
            EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_smart_callback_add (ok_button, "clicked", _on_ok_dialog_clicked,
            dialog);
    evas_object_show (ok_button);
    elm_box_pack_end (button_box, ok_button);

    return dialog;
}

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
_on_dialog_request (
        void *data,
        Evas_Object *obj,
        void *event_info,
        RequestType req_type)
{
    Elm_Object_Item *item = event_info;
    Evas_Object *dialog = NULL;
    const gchar *username = NULL;

    _close_dialog ();
    if (item) {
        DBG("%s", elm_object_item_text_get(item));
        _set_list_title (obj, item);
        username = elm_object_item_text_get(item);
    }
    if (main_window->use_nfc_tag)
        dialog = _create_nfc_dialog (username);
    else
        dialog = _create_dialog (username);
    if (dialog) {
        evas_object_show (dialog);
        main_dialog->req_type = req_type;
    }
}

static void
_on_cbox_changed (
        void *data,
        Evas_Object *obj,
        void *event_info)
{
    main_window->use_nfc_tag = *((Eina_Bool*)data);
    DBG("Use NFC tag : %d", main_window->use_nfc_tag);
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
    Eina_Bool value = main_window->use_nfc_tag;

    box = elm_box_add (win);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (win, box);
    evas_object_size_hint_align_set (box, 0.0, 0.1);

    checkbox = elm_check_add(win);
    elm_object_text_set(checkbox, "NFC Authentication");
    elm_check_state_pointer_set(checkbox, &value);
    evas_object_smart_callback_add(checkbox, "changed", _on_cbox_changed,
            &value);
    elm_box_pack_end (box, checkbox);
    evas_object_show(checkbox);

    return box;
}

static void
_on_switch_user_clicked (
        void *data,
        Evas_Object *obj,
        void *event_info)
{
    _on_dialog_request (data, obj, event_info, TLM_UI_REQUEST_SWITCH_USER);
}

static Evas_Object *
_add_user_list (
        Evas_Object *win)
{
    Evas_Object *box = NULL, *main_box, *label, *users_list, *cb;
    box = elm_box_add (win);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (win, box);
    evas_object_size_hint_align_set (box, 0.1, 0.1);

    label = elm_label_add(win);
    elm_object_text_set(label, "Switch User");
    elm_box_pack_end (box, label);
    evas_object_show(label);

    users_list = elm_hoversel_add(win);
    elm_hoversel_horizontal_set(users_list, EINA_FALSE);
    elm_object_text_set(users_list, "Select User From the List");

    _populate_users (users_list);

    evas_object_smart_callback_add(users_list, "selected",
            _on_switch_user_clicked, NULL);
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

static void
_on_login_user_clicked (
        void *data,
        Evas_Object *obj,
        void *event_info)
{
    _on_dialog_request (data, obj, event_info, TLM_UI_REQUEST_LOGIN);
}

static Evas_Object *
_add_login_box (
        Evas_Object *win)
{
    Evas_Object *box = NULL, *label, *login_button;
    Eina_Bool value;

    box = elm_box_add (win);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (win, box);
    elm_box_horizontal_set (box, EINA_TRUE);
    evas_object_size_hint_align_set (box, 0.1, 0.0);

    /* login button */
    login_button = elm_button_add (win);
    elm_object_text_set (login_button, "Login");
    evas_object_size_hint_weight_set (login_button,
            EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (login_button,
            EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_smart_callback_add (login_button, "clicked",
            _on_login_user_clicked, win);
    elm_box_pack_end (box, login_button);
    evas_object_show (login_button);

    return box;
}

static void
_on_logout_user_clicked (
        void *data,
        Evas_Object *obj,
        void *event_info)
{
    _trigger_dbus_request (TLM_UI_REQUEST_LOGOUT, NULL, NULL);
}

static Evas_Object *
_add_logout_box (
        Evas_Object *win)
{
    Evas_Object *box = NULL, *label, *logout_button;
    Eina_Bool value;

    box = elm_box_add (win);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (win, box);
    elm_box_horizontal_set (box, EINA_TRUE);
    evas_object_size_hint_align_set (box, 0.1, 0.0);

    /* logout button */
    logout_button = elm_button_add (win);
    elm_object_text_set (logout_button, "Logout");
    evas_object_size_hint_weight_set (logout_button,
            EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (logout_button,
            EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_smart_callback_add (logout_button, "clicked",
            _on_logout_user_clicked, win);
    elm_box_pack_end (box, logout_button);
    evas_object_show (logout_button);

    label = elm_label_add(win);
    elm_object_text_set(label, "Current logged-in user :: ");
    elm_box_pack_end (box, label);
    evas_object_show(label);

    main_window->user_label = elm_label_add(win);
    elm_box_pack_end (box, main_window->user_label);
    evas_object_show(main_window->user_label);

    return box;
}

static void
_set_loggedin_username ()
{
    GVariant *vusername = _get_property ("Name");
    if (vusername && main_window->user_label) {
        elm_object_text_set(main_window->user_label,
                g_variant_get_string (vusername, NULL));
    }
    if (vusername) g_variant_unref (vusername);
}

EAPI_MAIN int
elm_main (
        int argc,
        char **argv)
{
    Evas_Object *win, *bg, *box, *logout, *ulist, *login;

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    win = elm_win_add(NULL, "tlmui", ELM_WIN_BASIC);
    elm_win_title_set(win, "Demo tlm-ui");
    elm_win_autodel_set(win, EINA_TRUE);
    elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
    evas_object_resize(win, 500, 400);
    evas_object_show(win);

    main_window = g_malloc0 (sizeof(MainWindow));
    main_window->win = win;
    main_window->use_nfc_tag = FALSE;
    main_window->user_label = NULL;

    //background
    bg = elm_bg_add(win);
    evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(win, bg);
    evas_object_show(bg);

    box = elm_box_add (win);
    evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show (box);
    elm_win_resize_object_add (win, box);

    login = _add_login_box (win);
    elm_box_pack_end (box, login);

    ulist = _add_user_list (win);
    elm_box_pack_end (box, ulist);

    logout = _add_logout_box (win);
    elm_box_pack_end (box, logout);

    _set_loggedin_username ();

    // run the mainloop and process events and callbacks
    elm_run();
    elm_shutdown();

    g_free (main_window);

    return 0;
}
ELM_MAIN()
