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

#include "tlm-log.h"

#include <syslog.h>

static gboolean initialized = FALSE;
static int log_levels_enabled = (G_LOG_LEVEL_ERROR |
                                 G_LOG_LEVEL_CRITICAL |
                                 G_LOG_LEVEL_WARNING |
                                 G_LOG_LEVEL_DEBUG);
guint log_handler_id = 0;

static int
_log_level_to_priority (GLogLevelFlags log_level)
{
    switch (log_level & G_LOG_LEVEL_MASK) {
        case G_LOG_LEVEL_ERROR: return LOG_ERR;
        case G_LOG_LEVEL_CRITICAL: return LOG_CRIT;
        case G_LOG_LEVEL_WARNING: return LOG_WARNING;
        case G_LOG_LEVEL_MESSAGE: return LOG_NOTICE;
        case G_LOG_LEVEL_DEBUG: return LOG_DEBUG;
        case G_LOG_LEVEL_INFO: return LOG_INFO;
        default: return LOG_DEBUG;
    }
}

static void
_log_handler (const gchar *log_domain,
              GLogLevelFlags log_level,
              const gchar *message,
              gpointer userdata)
{
    GString *gstring ;
    gchar *msg = 0;
    int priority ;
    gboolean unint = FALSE;

    if (! (log_level & log_levels_enabled))
        return; 

    if (!initialized) {
        tlm_log_init ();
        unint = TRUE;
    }

    priority = _log_level_to_priority (log_level);

    gstring = g_string_new (NULL);
    g_string_printf (gstring, "[%s] %s", log_domain, message);
    msg = g_string_free (gstring, FALSE);

    syslog (priority, msg);

    g_free (msg);

    if (unint) {
        tlm_log_close ();
    }
}

void tlm_log_init (void)
{
    log_handler_id = g_log_set_handler (
                G_LOG_DOMAIN, log_levels_enabled, _log_handler, NULL);
    
    openlog (g_get_prgname(), LOG_PID | LOG_PERROR, LOG_DAEMON);

    initialized = TRUE;
}

void tlm_log_close (void)
{
    g_log_remove_handler (G_LOG_DOMAIN, log_handler_id);

    closelog();

    initialized = FALSE;
}

