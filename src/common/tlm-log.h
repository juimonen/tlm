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

#ifndef _TLM_LOG_H
#define _TLM_LOG_H

#include <glib.h>

G_BEGIN_DECLS

void tlm_log_init (const gchar *domain);
void tlm_log_close (const gchar *domain);

#define EXPAND_LOG_MSG(frmt, args...) "%f %s +%d %s :" frmt, \
    g_get_monotonic_time()*1.0e-6, __FILE__, __LINE__, __PRETTY_FUNCTION__, \
    ##args

#define INFO(frmt, args...)     g_print(EXPAND_LOG_MSG(frmt, ##args))
#define DBG(frmt, args...)      g_debug("debug:"EXPAND_LOG_MSG(frmt, ##args))
#define WARN(frmt, args...)     g_warning("warning:"EXPAND_LOG_MSG(frmt, ##args))
#define CRITICAL(frmt, args...) g_critical(EXPAND_LOG_MSG(frmt, ##args))
#define ERR(frmt, args...)      g_error(EXPAND_LOG_MSG(frmt, ##args))

G_END_DECLS

#endif /* _TLM_LOG_H */
