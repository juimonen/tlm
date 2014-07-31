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

typedef struct
{
    TlmDbusRequest *dbus_request;
    TlmSeat *seat;
} TlmRequest;

struct _TlmDbusObserverPrivate
{
    TlmManager *manager;
    TlmSeat *seat;
    TlmDbusServer *dbus_server;
    GQueue *request_queue;
    guint request_id;
    TlmRequest *active_request;
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
_connect_seat (
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
    g_object_weak_unref (dead, (GWeakNotify)_on_seat_dispose, self);
    _disconnect_seat (self, TLM_SEAT (dead));
    if (self->priv->active_request &&
        G_OBJECT(self->priv->active_request->seat) == dead) {
    	self->priv->active_request->seat = NULL;
    }
    if (G_OBJECT(self->priv->seat) == dead)
    	self->priv->seat = NULL;
}

static void
_on_manager_dispose (
        TlmDbusObserver *self,
        GObject *dead)
{
    g_return_if_fail (self && TLM_IS_DBUS_OBSERVER(self) && dead &&
                TLM_IS_MANAGER(dead));
    g_object_weak_unref (dead, (GWeakNotify)_on_manager_dispose, self);
    self->priv->manager = NULL;
}

static TlmRequest *
_create_request (
        TlmDbusObserver *self,
        TlmDbusRequest *dbus_req,
        TlmSeat *seat)
{
    TlmRequest *request = g_malloc0 (sizeof (TlmRequest));
    if (!request) return NULL;

    request->dbus_request = dbus_req;
    request->seat = seat;
    if (request->seat) {
    	_connect_seat (self, request->seat);
    	g_object_weak_ref (G_OBJECT (request->seat),
    			(GWeakNotify)_on_seat_dispose, self);
    }
    return request;
}

static void
_dispose_request (
        TlmDbusObserver *self,
        TlmRequest *request)
{
    if (!request) return;

    if (request->dbus_request) {
        tlm_dbus_login_adapter_request_completed (request->dbus_request, NULL);
        tlm_dbus_utils_dispose_request (request->dbus_request);
    	request->dbus_request = NULL;
    }
    if (request->seat) {
    	_disconnect_seat (self, request->seat);
    	g_object_weak_unref (G_OBJECT (request->seat),
    			(GWeakNotify)_on_seat_dispose, self);
    	request->seat = NULL;
    }
    g_free (request);
}

static void
_disconnect_seat (
        TlmDbusObserver *self,
        TlmSeat *seat)
{
    DBG ("self %p seat %p", self, seat);
    if (!seat) return;

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
    if (!seat) return;

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
        TlmRequest *request = elem->data;
        TlmDbusRequest *dbus_req = request->dbus_request;
        next = g_list_next (elem);
        if (dbus_req && G_OBJECT (dbus_req->dbus_adapter) == dead) {
            DBG ("removing the request for dead dbus adapter");
            head = g_list_delete_link (head, elem);
            _dispose_request (self, request);
        }
        elem = next;
    }

    /* check for active request */
    if (self->priv->active_request &&
        G_OBJECT (self->priv->active_request->dbus_request->dbus_adapter) ==
        		dead) {
        DBG ("removing the request for dead dbus adapter");
        _dispose_request (self, self->priv->active_request);
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
	if (!request) return;

	GError *error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_DBUS_REQ_ABORTED,
            "Dbus request aborted");
    _complete_dbus_request (request, error);
}

static void
_complete_request (
		TlmDbusObserver *self,
        TlmRequest *request,
        GError *error)
{
    _complete_dbus_request (request->dbus_request, error);
	request->dbus_request = NULL;
    _dispose_request (self, request);
}

static void
_clear_request (
        TlmRequest *request,
        TlmDbusObserver *self)
{
	if (!request) return;

	_abort_dbus_request (request->dbus_request);
	request->dbus_request = NULL;
	_dispose_request (self, request);
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
    TlmRequest* req = NULL;
    TlmDbusRequest* dbus_req = NULL;
    TlmSeat *seat = NULL;

    self->priv->request_id = 0;

    if (!self->priv->active_request) {
        gboolean ret = FALSE;

        req = g_queue_pop_head (self->priv->request_queue);
        if (!req) {
            DBG ("request queue is empty");
            goto _finished;
        }
        dbus_req = req->dbus_request;
        if (!_is_request_supported (self, dbus_req->type)) {
            WARN ("Request not supported -- req-type %d flags %d",
            		dbus_req->type, self->priv->enable_flags);
            err = TLM_GET_ERROR_FOR_ID (TLM_ERROR_DBUS_REQ_NOT_SUPPORTED,
                    "Dbus request not supported");
            goto _finished;
        }

        seat = self->priv->seat;
        if (!seat && self->priv->manager) {
            seat = tlm_manager_get_seat (self->priv->manager,
            		dbus_req->seat_id);
            req->seat = seat;
            /* NOTE: When no seat is set at dbus object creation time,
             * seat is connected on per dbus request basis and then
             * disconnected when the dbus request is completed or aborted */
            if (seat) {
            	_connect_seat (self, seat);
            	g_object_weak_ref (G_OBJECT (seat),
            			(GWeakNotify)_on_seat_dispose, self);
            }
        }

        if (!seat) {
            WARN ("Cannot find the seat");
            err = TLM_GET_ERROR_FOR_ID (TLM_ERROR_SEAT_NOT_FOUND,
                    "Seat not found");
            goto _finished;
        }

        self->priv->active_request = req;
        switch(dbus_req->type) {
        case TLM_DBUS_REQUEST_TYPE_LOGIN_USER:
            ret = tlm_seat_create_session (seat, NULL, dbus_req->username,
            		dbus_req->password, dbus_req->environment);
            break;
        case TLM_DBUS_REQUEST_TYPE_LOGOUT_USER:
            ret = tlm_seat_terminate_session (seat);
            break;
        case TLM_DBUS_REQUEST_TYPE_SWITCH_USER:
            ret = tlm_seat_switch_user (seat, NULL, dbus_req->username,
            		dbus_req->password, dbus_req->environment);
            break;
        }
        if (!ret) {
            _dispose_request (self, self->priv->active_request);
            self->priv->active_request = NULL;
        }
    }

_finished:
    if (err) {
        if (req) {
            _complete_request (self, req, err);
        } else {
            g_error_free (err);
        }
    }
    if (!self->priv->active_request)
    	_process_next_request_in_idle (self);

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
_add_request (
        TlmDbusObserver *self,
        TlmRequest *request)
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

    /* Login/switch request should only be completed on session created
     * signal from seat */
    if (!self->priv->active_request ||
    	!self->priv->active_request->dbus_request ||
        self->priv->active_request->dbus_request->type ==
        		TLM_DBUS_REQUEST_TYPE_LOGOUT_USER)
        return;

    _complete_request (self, self->priv->active_request, NULL);
    self->priv->active_request = NULL;

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
        !self->priv->active_request->dbus_request ||
        self->priv->active_request->dbus_request->type !=
        		TLM_DBUS_REQUEST_TYPE_LOGOUT_USER)
        return FALSE;

    _disconnect_dbus_adapter (self, TLM_DBUS_LOGIN_ADAPTER (
    		self->priv->active_request->dbus_request->dbus_adapter));

    _complete_request (self, self->priv->active_request, NULL);
    self->priv->active_request = NULL;

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
    _complete_request (self, self->priv->active_request, error);
    self->priv->active_request = NULL;

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
    _add_request (self, _create_request (self, request, NULL));
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
    _add_request (self, _create_request (self, request, NULL));
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
    _add_request (self, _create_request (self, request, NULL));
}

static void
_stop_dbus_server (TlmDbusObserver *self)
{
    DBG("self %p", self);
    _clear_request (self->priv->active_request, self);
    self->priv->active_request = NULL;

    if (self->priv->request_queue) {
    	g_queue_foreach (self->priv->request_queue,
                           (GFunc) _clear_request, self);
    	g_queue_free (self->priv->request_queue);
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
    DBG("self %p address %s uid %d", self, address, uid);
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
    	g_object_weak_unref (G_OBJECT (self->priv->manager),
    			(GWeakNotify)_on_manager_dispose, self);
        self->priv->manager = NULL;
    }

    if (self->priv->seat) {
    	g_object_weak_unref (G_OBJECT (self->priv->seat),
    	    			(GWeakNotify)_on_seat_dispose, self);
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
    TlmDbusObserver *dbus_observer =
        g_object_new (TLM_TYPE_DBUS_OBSERVER,  NULL);
    DBG ("%p", dbus_observer);

    if (manager) {
    	dbus_observer->priv->manager = manager;
    	g_object_weak_ref (G_OBJECT (manager), (GWeakNotify)_on_manager_dispose,
    			dbus_observer);
    }
    /* NOTE: When no seat is set at dbus object creation time,
     * seat is connected on per dbus request basis and then
     * disconnected when the dbus request is completed or aborted */
    if (seat) {
        dbus_observer->priv->seat = seat;
        g_object_weak_ref (G_OBJECT (seat), (GWeakNotify)_on_seat_dispose,
        		dbus_observer);
    }
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
