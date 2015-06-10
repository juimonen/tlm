/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
 *
 * Copyright (C) 2015 Intel Corporation.
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
#include <gio/gio.h>

#include "tlm-dbus-launcher-observer.h"
#include "common/tlm-log.h"
#include "common/tlm-utils.h"
#include "common/dbus/tlm-dbus-server-interface.h"
#include "common/dbus/tlm-dbus-server-p2p.h"
#include "common/dbus/tlm-dbus-utils.h"
#include "common/dbus/tlm-dbus-launcher-gen.h"
#include "common/tlm-config-general.h"
#include "common/tlm-error.h"
#include "dbus/tlm-dbus-launcher-adapter.h"

G_DEFINE_TYPE (TlmDbusLauncherObserver, tlm_dbus_launcher_observer, G_TYPE_OBJECT);

#define TLM_DBUS_LAUNCHER_OBSERVER_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            TLM_TYPE_DBUS_LAUNCHER_OBSERVER, TlmDbusLauncherObserverPrivate)

struct ProcessObject
{
	pid_t pid;
	gchar *path;
	gchar *args;
    int last_sig;
    guint timer_id;
    guint watch_id;
};

struct _TlmDbusLauncherObserverPrivate
{
	TlmConfig *config;
    TlmDbusServer *dbus_server;
    GHashTable *launched_processes;
};

enum {
    PROP_0,
    PROP_CONFIG,

    N_PROPERTIES
};

static GParamSpec *pspecs[N_PROPERTIES];

enum {
	SIG_PROCESS_STOPPED,

    SIG_MAX
};
static guint signals[SIG_MAX];

static void
_handle_dbus_server_new_connection (
        TlmDbusLauncherObserver *self,
        GDBusConnection *dbus_connection,
        GObject *dbus_server);

static void
_handle_dbus_server_client_added (
        TlmDbusLauncherObserver *self,
        GObject *dbus_adapter,
        GObject *dbus_server);

static void
_handle_dbus_server_client_removed (
        TlmDbusLauncherObserver *self,
        GObject *dbus_adapter,
        GObject *dbus_server);

static void
_destroy_process_obj (
		struct ProcessObject *obj)
{
	if (obj) {
		g_free (obj->path);
		g_free (obj->args);
		if (obj->watch_id) {
			g_source_remove (obj->watch_id);
			obj->watch_id = 0;
		}
		if (obj->timer_id > 0) {
			g_source_remove (obj->timer_id);
			obj->timer_id = 0;
		}
		g_free (obj);
	}
}

static gboolean
_stop_process_timeout (gpointer user_data)
{
    struct ProcessObject *obj = user_data;

    switch (obj->last_sig)
    {
        case SIGHUP:
            DBG ("process %u didn't respond to SIGHUP, sending SIGTERM",
                 obj->pid);
            if (kill (obj->pid, SIGTERM))
                WARN ("kill(%u, SIGTERM): %s", obj->pid, strerror(errno));
            obj->last_sig = SIGTERM;
            return G_SOURCE_CONTINUE;
        case SIGTERM:
            DBG ("process %u didn't respond to SIGTERM, sending SIGKILL",
                 obj->pid);
            if (kill (obj->pid, SIGKILL))
                WARN ("kill(%u, SIGKILL): %s", obj->pid, strerror(errno));
            obj->last_sig = SIGKILL;
            return G_SOURCE_CONTINUE;
        case SIGKILL:
            WARN ("process %u didn't respond to SIGKILL, it is stuck in kernel",
            	 obj->pid);
            obj->timer_id = 0;
            return G_SOURCE_REMOVE;
        default:
            WARN ("%d has unknown signaling state %d", obj->pid, obj->last_sig);
    }
    return G_SOURCE_REMOVE;
}

static void
_stop_process (
		pid_t pid,
		struct ProcessObject *obj,
        TlmDbusLauncherObserver *self)
{
    DBG ("Stop process with pid %d", pid);

    if (kill (pid, 0) != 0) {
        DBG ("no launcher process is running");
        obj->timer_id = 0;
        return;
    }

    if (kill (pid, SIGHUP) < 0)
        WARN ("kill(%u, SIGHUP): %s", pid, strerror(errno));
    obj->last_sig = SIGHUP;
    obj->timer_id = g_timeout_add_seconds (
            tlm_config_get_uint (self->priv->config, TLM_CONFIG_GENERAL,
            		TLM_CONFIG_GENERAL_TERMINATE_TIMEOUT, 1),
            		_stop_process_timeout, obj);
}

static void
_stop_process_blocking (
        pid_t pid,
        gpointer proc_obj,
        TlmDbusLauncherObserver *self)
{
	struct ProcessObject *obj = proc_obj;
	_stop_process (pid, obj, self);
	while (obj->timer_id != 0)
	      g_main_context_iteration(NULL, TRUE);
}

static void
_stop_dbus_server (TlmDbusLauncherObserver *self)
{
    DBG("self %p", self);

    if (self->priv->dbus_server) {
        tlm_dbus_server_stop (self->priv->dbus_server);
        g_object_unref (self->priv->dbus_server);
        self->priv->dbus_server = NULL;
    }
}

static gboolean
_start_dbus_server (
        TlmDbusLauncherObserver *self,
        const gchar *address,
        uid_t uid)
{
    DBG("self %p address %s uid %d", self, address, uid);
    self->priv->dbus_server = TLM_DBUS_SERVER (tlm_dbus_server_p2p_new (address,
            uid));
    return tlm_dbus_server_start (self->priv->dbus_server);
}

static void
tlm_dbus_launcher_observer_dispose (GObject *object)
{
    TlmDbusLauncherObserver *self = TLM_DBUS_LAUNCHER_OBSERVER(object);

    if (self->priv->launched_processes) {
        GHashTableIter iter;
        pid_t key;
        struct ProcessObject *value = NULL;
    	g_hash_table_iter_init (&iter, self->priv->launched_processes);
    	while (g_hash_table_iter_next (&iter,
    			(gpointer)&key,
    			(gpointer)&value)) {
    		_stop_process_blocking (key, value, self);
    	}
    	g_hash_table_unref (self->priv->launched_processes);
        self->priv->launched_processes = NULL;
    }

    _stop_dbus_server (self);

    g_clear_object (&self->priv->config);
    DBG("disposing launcher dbus_observer DONE: %p", self);

    G_OBJECT_CLASS (tlm_dbus_launcher_observer_parent_class)->dispose (object);
}

static void
_on_dbus_launcher_adapter_dispose (
        TlmDbusLauncherObserver *self,
        GObject *dead)
{
    GList *head=NULL, *elem=NULL, *next;

    g_return_if_fail (self && TLM_IS_DBUS_LAUNCHER_OBSERVER(self) && dead &&
                TLM_IS_DBUS_LAUNCHER_ADAPTER(dead));
}

static void
_handle_dbus_server_new_connection (
        TlmDbusLauncherObserver *self,
        GDBusConnection *dbus_connection,
        GObject *dbus_server)
{
	TlmDbusLauncherAdapter *launcher_object = NULL;
	g_return_if_fail (self && TLM_IS_DBUS_LAUNCHER_OBSERVER(self) && dbus_connection);

	launcher_object = tlm_dbus_launcher_adapter_new_with_connection (self, dbus_connection);
	tlm_dbus_server_p2p_add_adaptor_object (
			TLM_DBUS_SERVER_P2P (self->priv->dbus_server),
			dbus_connection, G_OBJECT(launcher_object));
}

static void
_handle_dbus_server_client_added (
        TlmDbusLauncherObserver *self,
        GObject *dbus_adapter,
        GObject *dbus_server)
{
    g_return_if_fail (self && TLM_IS_DBUS_LAUNCHER_OBSERVER(self) && dbus_adapter &&
            TLM_IS_DBUS_LAUNCHER_ADAPTER(dbus_adapter));
    g_object_weak_ref (G_OBJECT (dbus_adapter),
            (GWeakNotify)_on_dbus_launcher_adapter_dispose, self);
}

static void
_handle_dbus_server_client_removed (
        TlmDbusLauncherObserver *self,
        GObject *dbus_adapter,
        GObject *dbus_server)
{
    g_return_if_fail (self && TLM_IS_DBUS_LAUNCHER_OBSERVER(self) && dbus_adapter &&
            TLM_IS_DBUS_LAUNCHER_ADAPTER(dbus_adapter));
    g_object_weak_unref (G_OBJECT (dbus_adapter),
            (GWeakNotify)_on_dbus_launcher_adapter_dispose, self);
}

static void
_on_process_down_cb (
        GPid  pid,
        gint  status,
        gpointer data)
{
    g_spawn_close_pid (pid);

    TlmDbusLauncherObserver *self = TLM_DBUS_LAUNCHER_OBSERVER (data);

    DBG ("process with pid (%d) closed with status %d", pid, status);
    g_hash_table_remove (self->priv->launched_processes,
    		GUINT_TO_POINTER (pid));
    g_signal_emit (self, signals[SIG_PROCESS_STOPPED], 0, pid);
}

gboolean
tlm_dbus_launcher_launch_process (
        TlmDbusLauncherObserver *self,
        const gchar *command,
        const gchar *args,
        guint *procid,
        GError **error)
{
	gchar **split_args = NULL;
    gchar **args_iter = NULL;
    gint i;

    DBG ("start process with path %s and args %s", command, args);
    g_return_if_fail (self && TLM_IS_DBUS_LAUNCHER_OBSERVER(self));

    pid_t child_pid = fork ();
    if (child_pid) {
    	DBG ("setup watch for the new process with pid %u", child_pid);
    	struct ProcessObject *obj = g_malloc0 (sizeof (struct ProcessObject));
    	obj->pid = child_pid;
    	g_hash_table_insert (self->priv->launched_processes,
    			GUINT_TO_POINTER (child_pid), obj);
        *procid = obj->pid;
    	obj->watch_id = g_child_watch_add (child_pid,
    			(GChildWatchFunc)_on_process_down_cb, self);
    	return TRUE;
    }

    DBG ("start new process: ");
    split_args = tlm_utils_split_command_line (args);
    args_iter = split_args; i = 0;
    while (args_iter && *args_iter) {
        DBG ("\targv[%d]: %s", i, *args_iter);
        args_iter++; i++;
    }
    execvp (command, split_args);
    /* we reach here only in case of error */
    g_strfreev (split_args);
    DBG ("execl(): %s", strerror(errno));
    exit (0);
}

gboolean
tlm_dbus_launcher_stop_process (
        TlmDbusLauncherObserver *self,
        guint procid,
        GError **error)
{
    DBG ("stop process with id %d", procid);
    g_return_if_fail (self && TLM_IS_DBUS_LAUNCHER_OBSERVER(self));
    struct ProcessObject *obj = g_hash_table_lookup (
            self->priv->launched_processes, GUINT_TO_POINTER (procid));

    if (obj) {
        _stop_process (procid, obj, self);
    } else {
        *error = TLM_GET_ERROR_FOR_ID (TLM_ERROR_INVALID_INPUT,
                "No process with app id exists");
    	return FALSE;
    }
    return TRUE;
}

gboolean
tlm_dbus_launcher_list_processes (
        TlmDbusLauncherObserver *self)
{
    g_return_if_fail (self && TLM_IS_DBUS_LAUNCHER_OBSERVER(self));

    //TODO
}

static void
tlm_dbus_launcher_observer_finalize (GObject *self)
{
    //TlmDbusLauncherObserver *dbus_observer = TLM_DBUS_LAUNCHER_OBSERVER(self);

    G_OBJECT_CLASS (tlm_dbus_launcher_observer_parent_class)->finalize (self);
}

static void
_set_property (GObject *obj,
               guint property_id,
               const GValue *value,
               GParamSpec *pspec)
{
	TlmDbusLauncherObserver *dbus_observer = TLM_DBUS_LAUNCHER_OBSERVER(obj);
	TlmDbusLauncherObserverPrivate *priv =
			TLM_DBUS_LAUNCHER_OBSERVER_PRIV (dbus_observer);

    switch (property_id) {
        case PROP_CONFIG:
            priv->config = g_value_dup_object (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
_get_property (GObject *obj,
               guint property_id,
               GValue *value,
               GParamSpec *pspec)
{
	TlmDbusLauncherObserver *dbus_observer = TLM_DBUS_LAUNCHER_OBSERVER(obj);
	TlmDbusLauncherObserverPrivate *priv =
			TLM_DBUS_LAUNCHER_OBSERVER_PRIV (dbus_observer);

    switch (property_id) {
        case PROP_CONFIG:
            g_value_set_object (value, priv->config);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, property_id, pspec);
    }
}

static void
tlm_dbus_launcher_observer_class_init (TlmDbusLauncherObserverClass *klass)
{
    GObjectClass *g_klass = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (TlmDbusLauncherObserverPrivate));

    g_klass->dispose = tlm_dbus_launcher_observer_dispose;
    g_klass->finalize = tlm_dbus_launcher_observer_finalize;
    g_klass->set_property = _set_property;
    g_klass->get_property = _get_property;

    pspecs[PROP_CONFIG] =
        g_param_spec_object ("config",
                             "config object",
                             "Configuration object",
                             TLM_TYPE_CONFIG,
                             G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY|
                             G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (g_klass, N_PROPERTIES, pspecs);

    signals[SIG_PROCESS_STOPPED] = g_signal_new ("process-stopped",
    							TLM_TYPE_DBUS_LAUNCHER_OBSERVER,
    							G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE,
                                1, G_TYPE_UINT);

}

static void
tlm_dbus_launcher_observer_init (TlmDbusLauncherObserver *dbus_observer)
{
    TlmDbusLauncherObserverPrivate *priv =
    		TLM_DBUS_LAUNCHER_OBSERVER_PRIV (dbus_observer);
    priv->launched_processes = g_hash_table_new_full (g_direct_hash, g_direct_equal,
            NULL, (GDestroyNotify)_destroy_process_obj);
    dbus_observer->priv = priv;
    priv->config = NULL;
}

TlmDbusLauncherObserver *
tlm_dbus_launcher_observer_new (
		TlmConfig *config,
        const gchar *address,
        uid_t uid)
{
    TlmDbusLauncherObserver *dbus_observer =
        g_object_new (TLM_TYPE_DBUS_LAUNCHER_OBSERVER, "config", config, NULL);
    DBG ("%p", dbus_observer);

    if (!_start_dbus_server (dbus_observer, address, uid)) {
        WARN ("Launcher DbusObserver startup failed");
        g_object_unref (dbus_observer);
        return NULL;
    }

    g_signal_connect_swapped (dbus_observer->priv->dbus_server,
            "new-connection", G_CALLBACK(_handle_dbus_server_new_connection),
            dbus_observer);
    g_signal_connect_swapped (dbus_observer->priv->dbus_server,
            "client-added", G_CALLBACK (_handle_dbus_server_client_added),
            dbus_observer);
    g_signal_connect_swapped (dbus_observer->priv->dbus_server,
            "client-removed", G_CALLBACK(_handle_dbus_server_client_removed),
            dbus_observer);
    return dbus_observer;
}
