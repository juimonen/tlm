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

/* inclusion guard */
#ifndef __TLM_ERROR_H__
#define __TLM_ERROR_H__

#include <glib.h>

G_BEGIN_DECLS

#define TLM_ERROR   (tlm_error_quark())

typedef enum {
    TLM_ERROR_NONE,

    TLM_ERROR_UNKNOWN = 1,
    TLM_ERROR_INTERNAL_SERVER,
    TLM_ERROR_PERMISSION_DENIED,

    TLM_ERROR_INVALID_INPUT = 32,
    TLM_ERROR_SEAT_NOT_FOUND,
    TLM_ERROR_SESSION_CREATION_FAILURE,
    TLM_ERROR_SESSION_ALREADY_EXISTS,
    TLM_ERROR_SESSION_NOT_VALID,
    TLM_ERROR_SESSION_TERMINATION_FAILURE,
    TLM_ERROR_DBUS_SERVER_START_FAILURE,
    TLM_ERROR_PAM_AUTH_FAILURE,

    TLM_ERROR_DBUS_REQ_ABORTED = 50,
    TLM_ERROR_DBUS_REQ_NOT_SUPPORTED,
    TLM_ERROR_DBUS_REQ_UNKNOWN,

    TLM_ERROR_LAST_ERR = 400

} TlmError;

#define TLM_GET_ERROR_FOR_ID(code, message, args...) \
    g_error_new (TLM_ERROR, code, message, ##args);

GQuark
tlm_error_quark (void);

GError *
tlm_error_new_from_variant (
        GVariant *var);

GVariant *
tlm_error_to_variant (
        GError *error);


G_END_DECLS

#endif /* __TLM_ERROR_H__ */
