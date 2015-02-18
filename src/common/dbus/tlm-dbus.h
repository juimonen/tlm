/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
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
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __TLM_DBUS_H_
#define __TLM_DBUS_H_

#include "config.h"

/*
 * Common DBUS definitions
 */
#define TLM_SERVICE_PREFIX       "org.O1.Tlm"
#define TLM_SERVICE              TLM_SERVICE_PREFIX
#define TLM_LOGIN_OBJECTPATH     "/org/O1/Tlm/Login"
#define TLM_SESSION_OBJECTPATH   "/org/O1/Tlm/Session"

#define TLM_DBUS_FREEDESKTOP_SERVICE    "org.freedesktop.DBus"
#define TLM_DBUS_FREEDESKTOP_PATH       "/org/freedesktop/DBus"
#define TLM_DBUS_FREEDESKTOP_INTERFACE  "org.freedesktop.DBus"

#ifndef TLM_BUS_TYPE
    #define TLM_BUS_TYPE G_BUS_TYPE_NONE
#endif

#endif /* __TLM_DBUS_H_ */
