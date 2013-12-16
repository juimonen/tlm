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

#include <pwd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <glib.h>

#include "tlm-auth-plugin-default.h"
#include "tlm-log.h"

struct _TlmAuthPluginDefault
{
    GObject parent;
    /* priv */
};

GWeakRef __self ;

static void _plugin_interface_init (TlmAuthPluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TlmAuthPluginDefault, tlm_auth_plugin_default,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TLM_TYPE_AUTH_PLUGIN,
                                                _plugin_interface_init));


static void
_plugin_interface_init (TlmAuthPluginInterface *iface)
{
    (void) iface;
}

static void
_plugin_finalize (GObject *self)
{
    TlmAuthPluginDefault *plugin = TLM_AUTH_PLUGIN_DEFAULT(self);

    g_weak_ref_clear (&__self);

    G_OBJECT_CLASS (tlm_auth_plugin_default_parent_class)->finalize(self);
}

static void
_on_signal_cb (int sig_no)
{
    DBG("Signal '%d'", sig_no);
    TlmAuthPluginDefault *self = g_weak_ref_get (&__self);

    if (!self) return ;

    /* re-login guest on seat0 */
    if (!tlm_auth_plugin_start_authentication (
                TLM_AUTH_PLUGIN(self),
                "seat0",
                "tlm-login",
                NULL,
                NULL)) {
        WARN ("Failed to create session");
    }
}

static void
tlm_auth_plugin_default_class_init (TlmAuthPluginDefaultClass *kls)
{
    G_OBJECT_CLASS (kls)->finalize  = _plugin_finalize;
}

static void
tlm_auth_plugin_default_init (TlmAuthPluginDefault *self)
{
    struct sigaction sa;

    g_weak_ref_init (&__self, self);

    sa.sa_handler = _on_signal_cb;
    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        WARN ("assert(sigaction()) : %s", strerror(errno));
    }

    tlm_log_init("TLM_AUTH_PLIGIN");
}

