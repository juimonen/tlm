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

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "tlm-dbus-observer.h"
#include "tlm-log.h"
#include "tlm-utils.h"
#include "dbus/tlm-dbus-server-interface.h"
#include "dbus/tlm-dbus-server-p2p.h"
#include "dbus/tlm-dbus-login-adapter.h"
#include "dbus/tlm-dbus-utils.h"
#include "tlm-seat.h"
#include "tlm-manager.h"
#include "common/tlm-error.h"

G_DEFINE_TYPE (TlmDbusObserver, tlm_dbus_observer, G_TYPE_OBJECT);

#define TLM_DBUS_OBSERVER_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            TLM_TYPE_DBUS_OBSERVER, TlmDbusObserverPrivate)

struct _TlmDbusObserverPrivate
{
    TlmManager *manager;
    TlmSeat *seat;
    TlmDbusServer *dbus_server;
    GQueue *request_queue;
    guint request_id;
    TlmDbusRequest *active_request;
    DbusObserverEnableFlags enable_flags;
};

static void
_handle_dbus_client_added (
        TlmDbusObserver *self,
        GObject *dbus_adapter,
        GObject *dbus_server);

static void
_handle_dbus_client_removed (
        TlmDbusObserver *self,
        GObject *dbus_adapter,
        GObject *dbus_server);

static void
_handle_dbus_login_user (
        TlmDbusObserver *self,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        GVariant *environment,
        GDBusMethodInvocation *invocation,
        GObject *dbus_adapter);

static void
_handle_dbus_logout_user (
        TlmDbusObserver *self,
        const gchar *seat_id,
        GDBusMethodInvocation *invocation,
        GObject *dbus_adapter);

static void
_handle_dbus_switch_user (
        TlmDbusObserver *self,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        GVariant *environment,
        GDBusMethodInvocation *invocation,
        GObject *dbus_adapter);

static void
_disconnect_dbus_adapter (
        TlmDbusObserver *self,
        TlmDbusLoginAdapter *adapter);

static void
_handle_seat_session_created (
        TlmDbusObserver *self,
        const gchar *seat_id,
        GObject *seat);

static gboolean
_handle_seat_session_terminated (
        TlmDbusObserver *self,
        const gchar *seat_id,
        GObject *seat);

static void
_handle_seat_session_error (
        TlmDbusObserver *self,
        TlmError error,
        GObject *seat);

static void
_disconnect_seat (
        TlmDbusObserver *self,
        TlmSeat *seat);

static void
_process_next_request_in_idle (
        TlmDbusObserver *self);

static void
_on_seat_dispose (
        TlmDbusObserver *self,
        GObject *dead)
{
    g_return_if_fail (self && TLM_IS_DBUS_OBSERVER(self) && dead &&
                TLM_IS_SEAT(dead));
    _disconnect_seat (self, TLM_SEAT(dead));
    g_object_weak_unref (dead, (GWeakNotify)_on_seat_dispose, self);
}

static void
_disconnect_seat (
        TlmDbusObserver *self,
        TlmSeat *seat)
{
    DBG ("self %p seat %p", self, seat);
    g_signal_handlers_disconnect_by_func (G_OBJECT (seat),
            _handle_seat_session_created, self);
    g_signal_handlers_disconnect_by_func (G_OBJECT (seat),
            _handle_seat_session_terminated, self);
    g_signal_handlers_disconnect_by_func (G_OBJECT (seat),
            _handle_seat_session_error, self);
}

static void
_connect_seat (
        TlmDbusObserver *self,
        TlmSeat *seat)
{
    DBG ("self %p seat %p", self, seat);
    g_signal_connect_swapped (G_OBJECT (seat), "session-created",
            G_CALLBACK(_handle_seat_session_created), self);
    g_signal_connect_swapped (G_OBJECT (seat), "session-terminated",
            G_CALLBACK(_handle_seat_session_terminated), self);
    g_signal_connect_swapped (G_OBJECT (seat), "session-error",
            G_CALLBACK(_handle_seat_session_error), self);
}

static void
_on_dbus_adapter_dispose (
        TlmDbusObserver *self,
        GObject *dead)
{
    GList *head=NULL, *elem=NULL, *next;

    g_return_if_fail (self && TLM_IS_DBUS_OBSERVER(self) && dead &&
                TLM_IS_DBUS_LOGIN_ADAPTER(dead));
    _disconnect_dbus_adapter (self, TLM_DBUS_LOGIN_ADAPTER(dead));

    if (self->priv->request_queue)
        head = elem = g_queue_peek_head_link (self->priv->request_queue);
    while (elem) {
        TlmDbusRequest *request = elem->data;
        next = g_list_next (elem);
        if (request && G_OBJECT (request->dbus_adapter) == dead) {
            DBG ("removing the request for dead dbus adapter");
            head = g_list_delete_link (head, elem);
            tlm_dbus_utils_dispose_request (request);
        }
        elem = next;
    }

    /* check for active request */
    if (self->priv->active_request &&
        G_OBJECT (self->priv->active_request->dbus_adapter) == dead) {
        DBG ("removing the request for dead dbus adapter");
        tlm_dbus_utils_dispose_request (self->priv->active_request);
        self->priv->active_request = NULL;
        if (self->priv->request_id) {
            g_source_remove (self->priv->request_id);
            self->priv->request_id = 0;
        }
        _process_next_request_in_idle (self);
    }
}

static void
_connect_dbus_adapter (
        TlmDbusObserver *self,
        TlmDbusLoginAdapter *adapter)
{
    DBG ("");
    if (self->priv->enable_flags & DBUS_OBSERVER_ENABLE_LOGIN_USER)
        g_signal_connect_swapped (G_OBJECT (adapter),
                "login-user", G_CALLBACK(_handle_dbus_login_user), self);
    if (self->priv->enable_flags & DBUS_OBSERVER_ENABLE_LOGOUT_USER)
        g_signal_connect_swapped (G_OBJECT (adapter),
                "logout-user", G_CALLBACK(_handle_dbus_logout_user), self);
    if (self->priv->enable_flags & DBUS_OBSERVER_ENABLE_SWITCH_USER)
        g_signal_connect_swapped (G_OBJECT (adapter),
                "switch-user", G_CALLBACK(_handle_dbus_switch_user), self);
}

static void
_disconnect_dbus_adapter (
        TlmDbusObserver *self,
        TlmDbusLoginAdapter *adapter)
{
    DBG ("");

    if (self->priv->enable_flags & DBUS_OBSERVER_ENABLE_LOGIN_USER)
        g_signal_handlers_disconnect_by_func (G_OBJECT(adapter),
                _handle_dbus_login_user, self);
    if (self->priv->enable_flags & DBUS_OBSERVER_ENABLE_LOGOUT_USER)
        g_signal_handlers_disconnect_by_func (G_OBJECT(adapter),
                _handle_dbus_logout_user, self);
    if (self->priv->enable_flags & DBUS_OBSERVER_ENABLE_SWITCH_USER)
        g_signal_handlers_disconnect_by_func (G_OBJECT(adapter),
                _handle_dbus_switch_user, self);
}

static void
_handle_dbus_client_added (
        TlmDbusObserver *self,
        GObject *dbus_adapter,
        GObject *dbus_server)
{
    DBG ("");
    g_return_if_fail (self && TLM_IS_DBUS_OBSERVER(self) && dbus_adapter &&
            TLM_IS_DBUS_LOGIN_ADAPTER(dbus_adapter));
    _connect_dbus_adapter (self, TLM_DBUS_LOGIN_ADAPTER(dbus_adapter));
    g_object_weak_ref (G_OBJECT (dbus_adapter),
            (GWeakNotify)_on_dbus_adapter_dispose, self);
}

static void
_handle_dbus_client_removed (
        TlmDbusObserver *self,
        GObject *dbus_adapter,
        GObject *dbus_server)
{
    DBG ("");
    g_return_if_fail (self && TLM_IS_DBUS_OBSERVER(self) && dbus_adapter &&
            TLM_IS_DBUS_LOGIN_ADAPTER(dbus_adapter));
    _disconnect_dbus_adapter (self, TLM_DBUS_LOGIN_ADAPTER(dbus_adapter));
    g_object_weak_unref (G_OBJECT (dbus_adapter),
            (GWeakNotify)_on_dbus_adapter_dispose, self);
}

static void
_complete_dbus_request (
        TlmDbusRequest *request,
        GError *error)
{
    if (request) {
        tlm_dbus_login_adapter_request_completed (request, error);
        tlm_dbus_utils_dispose_request (request);
    }
    if (error) g_error_free (error);
}

static void
_abort_dbus_request (
        TlmDbusRequest *request)
{
    GError *error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_DBUS_REQ_ABORTED,
            "Dbus request aborted");
    _complete_dbus_request (request, error);
}

static gboolean
_is_request_supported (
        TlmDbusObserver *self,
        TlmDbusRequestType req_type)
{
    switch (req_type) {
    case TLM_DBUS_REQUEST_TYPE_LOGIN_USER:
        return (self->priv->enable_flags & DBUS_OBSERVER_ENABLE_LOGIN_USER);
    case TLM_DBUS_REQUEST_TYPE_LOGOUT_USER:
        return (self->priv->enable_flags & DBUS_OBSERVER_ENABLE_LOGOUT_USER);
    case TLM_DBUS_REQUEST_TYPE_SWITCH_USER:
        return (self->priv->enable_flags & DBUS_OBSERVER_ENABLE_SWITCH_USER);
    }
    return FALSE;
}

static gboolean
_process_request (
        TlmDbusObserver *self)
{
    g_return_val_if_fail (self && TLM_IS_DBUS_OBSERVER(self), FALSE);
    GError *err = NULL;
    TlmDbusRequest* req = NULL;
    TlmSeat *seat = NULL;

    self->priv->request_id = 0;

    if (!self->priv->active_request) {
        gboolean ret = FALSE;

        req = g_queue_pop_head (self->priv->request_queue);
        if (!req) {
            DBG ("request queue is empty");
            goto _finished;
        }

        if (!_is_request_supported (self, req->type)) {
            WARN ("Request not supported -- req-type %d flags %d", req->type,
                    self->priv->enable_flags);
            err = TLM_GET_ERROR_FOR_ID (TLM_ERROR_DBUS_REQ_NOT_SUPPORTED,
                    "Dbus request not supported");
            goto _finished;
        }

        seat = self->priv->seat;
        if (!seat) {
            seat = tlm_manager_get_seat (self->priv->manager, req->seat_id);
        }

        if (!seat) {
            WARN ("Cannot find the seat");
            err = TLM_GET_ERROR_FOR_ID (TLM_ERROR_SEAT_NOT_FOUND,
                    "Seat not found");
            goto _finished;
        }

        self->priv->active_request = req;
        _connect_seat (self, seat);
        g_object_weak_ref (G_OBJECT (seat), (GWeakNotify)_on_seat_dispose,
                self);

        switch(req->type) {
        case TLM_DBUS_REQUEST_TYPE_LOGIN_USER:
            //request is completed when the function returns
            ret = tlm_seat_create_session (seat, NULL, req->username,
                    req->password, req->environment);
            break;
        case TLM_DBUS_REQUEST_TYPE_LOGOUT_USER:
            ret = tlm_seat_terminate_session (seat);
            break;
        case TLM_DBUS_REQUEST_TYPE_SWITCH_USER:
            ret = tlm_seat_switch_user (seat, NULL, req->username,
                    req->password, req->environment);
            break;
        }
        if (!ret) {
            g_object_weak_unref (G_OBJECT (seat), (GWeakNotify)_on_seat_dispose,
                    self);
            _disconnect_seat (self, seat);
            self->priv->active_request = NULL;
        }
    }

_finished:
    if (err) {
        if (req) {
            _complete_dbus_request (req, err);
        } else {
            g_error_free (err);
        }
        _process_next_request_in_idle (self);
    }

    return FALSE;
}

static void
_process_next_request_in_idle (
        TlmDbusObserver *self)
{
    if (!self->priv->request_id &&
        !g_queue_is_empty (self->priv->request_queue)) {
        DBG ("request queue has request(s) to be processed");
        self->priv->request_id = g_idle_add ((GSourceFunc)_process_request,
                self);
    }
}

static void
_add_dbus_request (
        TlmDbusObserver *self,
        TlmDbusRequest *request)
{
    g_queue_push_tail (self->priv->request_queue, request);
    _process_next_request_in_idle (self);
}

static void
_handle_seat_session_created (
        TlmDbusObserver *self,
        const gchar *seat_id,
        GObject *seat)
{
    DBG ("self %p seat %p", self, seat);

    g_return_if_fail (self && TLM_IS_DBUS_OBSERVER(self));
    g_return_if_fail (seat && TLM_IS_SEAT(seat));

    /* Login/switch request should only be completed on session terminated
     * signal from seat */
    if (!self->priv->active_request ||
        self->priv->active_request->type == TLM_DBUS_REQUEST_TYPE_LOGOUT_USER)
        return;

    _complete_dbus_request (self->priv->active_request, NULL);

    self->priv->active_request = NULL;

    _disconnect_seat (self, TLM_SEAT (seat));

    _process_next_request_in_idle (self);
}

static gboolean
_handle_seat_session_terminated (
        TlmDbusObserver *self,
        const gchar *seat_id,
        GObject *seat)
{
    DBG ("self %p seat %p", self, seat);

    g_return_val_if_fail (self && TLM_IS_DBUS_OBSERVER(self), FALSE);
    g_return_val_if_fail (seat && TLM_IS_SEAT(seat), FALSE);

    /* Logout request should only be completed on session terminated signal
     * from seat */
    if (!self->priv->active_request ||
        self->priv->active_request->type != TLM_DBUS_REQUEST_TYPE_LOGOUT_USER)
        return FALSE;

    _disconnect_dbus_adapter (self,
            TLM_DBUS_LOGIN_ADAPTER (self->priv->active_request->dbus_adapter));

    _complete_dbus_request (self->priv->active_request, NULL);

    self->priv->active_request = NULL;

    _disconnect_seat (self, TLM_SEAT (seat));

    _process_next_request_in_idle (self);

    return FALSE;
}

static void
_handle_seat_session_error (
        TlmDbusObserver *self,
        TlmError error_code,
        GObject *seat)
{
    DBG ("self %p seat %p", self, seat);
    GError *error = NULL;

    g_return_if_fail (self && TLM_IS_DBUS_OBSERVER(self));
    g_return_if_fail (seat && TLM_IS_SEAT(seat));

    if (!self->priv->active_request)
        return;

    error = TLM_GET_ERROR_FOR_ID (error_code, "Dbus request failed");
    _complete_dbus_request (self->priv->active_request, error);
    self->priv->active_request = NULL;

    _disconnect_seat (self, TLM_SEAT (seat));

    _process_next_request_in_idle (self);
}

static void
_handle_dbus_login_user (
        TlmDbusObserver *self,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        GVariant *environment,
        GDBusMethodInvocation *invocation,
        GObject *dbus_adapter)
{
    TlmDbusRequest *request = NULL;

    DBG ("seat id %s, username %s", seat_id, username);
    g_return_if_fail (self && TLM_IS_DBUS_OBSERVER(self));

    request = tlm_dbus_utils_create_request (dbus_adapter, invocation,
            TLM_DBUS_REQUEST_TYPE_LOGIN_USER, seat_id, username, password,
            environment);
    _add_dbus_request (self, request);
}

static void
_handle_dbus_logout_user (
        TlmDbusObserver *self,
        const gchar *seat_id,
        GDBusMethodInvocation *invocation,
        GObject *dbus_adapter)
{
    TlmDbusRequest *request = NULL;

    DBG ("seat id %s", seat_id);
    g_return_if_fail (self && TLM_IS_DBUS_OBSERVER(self));

    request = tlm_dbus_utils_create_request (dbus_adapter, invocation,
            TLM_DBUS_REQUEST_TYPE_LOGOUT_USER, seat_id, NULL, NULL, NULL);
    _add_dbus_request (self, request);
}

static void
_handle_dbus_switch_user (
        TlmDbusObserver *self,
        const gchar *seat_id,
        const gchar *username,
        const gchar *password,
        GVariant *environment,
        GDBusMethodInvocation *invocation,
        GObject *dbus_adapter)
{
    TlmDbusRequest *request = NULL;

    DBG ("seat id %s, username %s", seat_id, username);
    g_return_if_fail (self && TLM_IS_DBUS_OBSERVER(self));

    request = tlm_dbus_utils_create_request (dbus_adapter, invocation,
            TLM_DBUS_REQUEST_TYPE_SWITCH_USER, seat_id, username, password,
            environment);
    _add_dbus_request (self, request);
}

static void
_stop_dbus_server (TlmDbusObserver *self)
{
    _abort_dbus_request (self->priv->active_request);
    self->priv->active_request = NULL;

    if (self->priv->request_queue) {
        g_queue_free_full (self->priv->request_queue,
                           (GDestroyNotify) _abort_dbus_request);
        self->priv->request_queue = NULL;
    }

    if (self->priv->dbus_server) {
        tlm_dbus_server_stop (self->priv->dbus_server);
        g_object_unref (self->priv->dbus_server);
        self->priv->dbus_server = NULL;
    }
}

static gboolean
_start_dbus_server (
        TlmDbusObserver *self,
        const gchar *address,
        uid_t uid)
{
    self->priv->dbus_server = TLM_DBUS_SERVER (tlm_dbus_server_p2p_new (address,
            uid));

    return tlm_dbus_server_start (self->priv->dbus_server);
}

static void
tlm_dbus_observer_dispose (GObject *object)
{
    TlmDbusObserver *self = TLM_DBUS_OBSERVER(object);
    DBG("disposing dbus_observer: %p", self);

    if (self->priv->request_id) {
        g_source_remove (self->priv->request_id);
        self->priv->request_id = 0;
    }

    _stop_dbus_server (self);

    if (self->priv->manager) {
        g_object_unref (self->priv->manager);
        self->priv->manager = NULL;
    }

    if (self->priv->seat) {
        g_object_unref (self->priv->seat);
        self->priv->seat = NULL;
    }
    DBG("disposing dbus_observer DONE: %p", self);

    G_OBJECT_CLASS (tlm_dbus_observer_parent_class)->dispose (object);
}

static void
tlm_dbus_observer_finalize (GObject *self)
{
    //TlmDbusObserver *dbus_observer = TLM_DBUS_OBSERVER(self);

    G_OBJECT_CLASS (tlm_dbus_observer_parent_class)->finalize (self);
}

static void
tlm_dbus_observer_class_init (TlmDbusObserverClass *klass)
{
    GObjectClass *g_klass = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (TlmDbusObserverPrivate));

    g_klass->dispose = tlm_dbus_observer_dispose ;
    g_klass->finalize = tlm_dbus_observer_finalize;
}

static void
tlm_dbus_observer_init (TlmDbusObserver *dbus_observer)
{
    TlmDbusObserverPrivate *priv = TLM_DBUS_OBSERVER_PRIV (dbus_observer);
    
    priv->manager = NULL;
    priv->seat = NULL;
    priv->enable_flags = DBUS_OBSERVER_ENABLE_ALL;
    priv->request_queue = g_queue_new ();
    priv->request_id = 0;
    priv->active_request = NULL;
    dbus_observer->priv = priv;
}

TlmDbusObserver *
tlm_dbus_observer_new (
        TlmManager *manager,
        TlmSeat *seat,
        const gchar *address,
        uid_t uid,
        DbusObserverEnableFlags enable_flags)
{
    DBG ("");

    TlmDbusObserver *dbus_observer =
        g_object_new (TLM_TYPE_DBUS_OBSERVER,  NULL);
    dbus_observer->priv->manager = g_object_ref (manager);
    if (seat) dbus_observer->priv->seat = g_object_ref (seat);
    dbus_observer->priv->enable_flags = enable_flags;

    if (!_start_dbus_server (dbus_observer, address, uid)) {
        WARN ("DbusObserver startup failed");
        g_object_unref (dbus_observer);
        return NULL;
    }

    g_signal_connect_swapped (dbus_observer->priv->dbus_server,
            "client-added", G_CALLBACK (_handle_dbus_client_added),
            dbus_observer);
    g_signal_connect_swapped (dbus_observer->priv->dbus_server,
            "client-removed", G_CALLBACK(_handle_dbus_client_removed),
            dbus_observer);
    return dbus_observer;
}
