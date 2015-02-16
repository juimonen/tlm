/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
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


/**
 * SECTION:tlm-log
 * @short_description: logging utilities
 * @include: tlm-log.h
 *
 * This section describes various logging utilities that TLM plugins can use.
 */

/**
 * CRITICAL:
 * @frmt: log message format
 * @...: arguments
 *
 * Logs a critical message
 */


/**
 * DBG:
 * @frmt: log message format
 * @...: arguments
 *
 * Logs a debugging message
 */

/**
 * INFO:
 * @frmt: log message format
 * @...: arguments
 *
 * Logs an info message
 */

/**
 * ERR:
 * @frmt: log message format
 * @...: arguments
 *
 * Logs an error message
 */

/**
 * WARN:
 * @frmt: log message format
 * @...: arguments
 *
 * Logs a warning message
 */

/**
 * EXPAND_LOG_MSG:
 * @frmt: log message format
 * @...: arguments
 *
 * Internal macro; do not use.
 */


static gboolean _initialized = FALSE;
static int _log_levels_enabled = (G_LOG_LEVEL_ERROR |
                                 G_LOG_LEVEL_CRITICAL |
                                 G_LOG_LEVEL_WARNING |
                                 G_LOG_LEVEL_DEBUG);
GHashTable *_log_handlers = NULL; /* log_domain:handler_id */

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
    int priority ;
    gboolean unint = FALSE;

    if (! (log_level & _log_levels_enabled))
        return; 

    if (!_initialized) {
        tlm_log_init (log_domain);
        unint = TRUE;
    }

    priority = _log_level_to_priority (log_level);

    syslog (priority, "[%s] %s", log_domain, message);

    if (unint) {
        tlm_log_close (log_domain);
    }
}

/**
 * tlm_log_init:
 * @domain: log message domain
 *
 * Call this function before logging any messages to initialize the logging system.
 */
void tlm_log_init (const gchar *domain)
{

    if (!_log_handlers) {
         _log_handlers = g_hash_table_new_full (
                    g_str_hash, g_str_equal, g_free, NULL);
    }

    if (!domain)
        domain = G_LOG_DOMAIN;

    if (g_hash_table_contains (_log_handlers, domain))
        return;

    guint id = g_log_set_handler (
            domain, _log_levels_enabled, _log_handler, NULL);

    g_hash_table_insert (_log_handlers, g_strdup(domain), GUINT_TO_POINTER(id));

    if (_initialized) return ;

    openlog (g_get_prgname(), LOG_PID | LOG_PERROR, LOG_DAEMON);

    _initialized = TRUE;
}

static void _remove_log_handler (gpointer key, gpointer value, gpointer udata)
{
    if (!udata || g_strcmp0(udata, key) == 0) 
        g_log_remove_handler ((const gchar *)key, GPOINTER_TO_UINT(value));
}

/**
 * tlm_log_close:
 * @domain: log message domain
 *
 * Call this function to clean up the logging system (e.g. in an object destructor).
 */
void tlm_log_close (const gchar *domain)
{
    if (_log_handlers) {
        g_hash_table_foreach (_log_handlers, _remove_log_handler, (gpointer)domain);
        if (!domain) {
            g_hash_table_unref (_log_handlers);
            _log_handlers = NULL;
        }
        else {
            g_hash_table_remove (_log_handlers, domain);
        }
    }

    if (_initialized) {
        closelog();
        _initialized = FALSE;
    }
}

