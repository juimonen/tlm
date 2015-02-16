/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm (Tiny Login Manager)
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

#include <string.h>
#include <gio/gio.h>

#include "tlm-error.h"

/**
 * SECTION:tlm-error
 * @short_description: error definitions and utilities
 * @title: Errors
 *
 * This file provides Tlm error definitions and utilities.
 * When creating an error, use #TLM_ERROR for the error domain and errors
 * from #TlmError for the error code.
 *
 * |[
 *     GError* err = g_error_new(TLM_ERROR, TLM_ERROR_INVALID_INPUT,
 *     "Invalid input");
 * ]|
 */

/**
 * TLM_ERROR:
 *
 * This macro should be used when creating a #GError (for example with
 * g_error_new()).
 */

/**
 * TlmError:
 * @TLM_ERROR_NONE: No error
 * @TLM_ERROR_UNKNOWN: Catch-all for errors not distinguished by another error
 * code
 * @TLM_ERROR_INTERNAL_SERVER: Server internal error
 * @TLM_ERROR_PERMISSION_DENIED: Permission denied
 * @TLM_ERROR_INVALID_INPUT: Invalid input
 * @TLM_ERROR_SEAT_NOT_FOUND: Seat not found
 * @TLM_ERROR_SESSION_CREATION_FAILURE: Session creation failed
 * @TLM_ERROR_SESSION_ALREADY_EXISTS: Session already exists
 * @TLM_ERROR_SESSION_NOT_VALID: session is not valid anymore
 * @TLM_ERROR_SESSION_TERMINATION_FAILURE: Session termination failed
 * @TLM_ERROR_DBUS_SERVER_START_FAILURE: dbus-server startup failed
 * @TLM_ERROR_PAM_AUTH_FAILURE: PAM authentication failed
 * @TLM_ERROR_DBUS_REQ_ABORTED: Dbus request aborted
 * @TLM_ERROR_DBUS_REQ_NOT_SUPPORTED: Dbus request not supported
 * @TLM_ERROR_DBUS_REQ_UNKNOWN: Dbus request failed with unknown error
 * @TLM_ERROR_LAST_ERR: Placeholder to rearrange enumeration
 *
 * This enumeration provides a list of errors
 */

/**
 * TLM_ERROR_DOMAIN:
 *
 * This macro defines the error domain for tlm.
 */
#define TLM_ERROR_DOMAIN "tlm"

/**
 * TLM_GET_ERROR_FOR_ID:
 * @code: A #TlmError specifying the error
 * @message: Format string for the error message
 * @...: parameters for the error string
 *
 * A helper macro that creates a #GError with the proper tlm domain
 */

#define _ERROR_PREFIX "org.O1.Tlm.Error"

GDBusErrorEntry _tlm_errors[] =
{
    {TLM_ERROR_UNKNOWN, _ERROR_PREFIX".Unknown"},
    {TLM_ERROR_INTERNAL_SERVER, _ERROR_PREFIX".InternalServerError"},
    {TLM_ERROR_PERMISSION_DENIED, _ERROR_PREFIX".PermissionDenied"},

    {TLM_ERROR_INVALID_INPUT, _ERROR_PREFIX".InvalidInput"},
    {TLM_ERROR_SEAT_NOT_FOUND, _ERROR_PREFIX".SeatNotFound"},
    {TLM_ERROR_SESSION_CREATION_FAILURE,
            _ERROR_PREFIX".SessionCreationFailure"},
            {TLM_ERROR_SESSION_TERMINATION_FAILURE,
                    _ERROR_PREFIX".SessionTerminationFailure"},
    {TLM_ERROR_SESSION_ALREADY_EXISTS, _ERROR_PREFIX".SessionAlreadyExists"},
    {TLM_ERROR_SESSION_NOT_VALID, _ERROR_PREFIX".SessionNotValid"},
    {TLM_ERROR_DBUS_SERVER_START_FAILURE,
            _ERROR_PREFIX".DBusServerStartFailure"},
    {TLM_ERROR_PAM_AUTH_FAILURE,
            _ERROR_PREFIX".PamAuthFailure"},
    {TLM_ERROR_DBUS_REQ_ABORTED, _ERROR_PREFIX".DBusRequestAborted"},
    {TLM_ERROR_DBUS_REQ_NOT_SUPPORTED, _ERROR_PREFIX".DBusRequestNotSupported"},
    {TLM_ERROR_DBUS_REQ_UNKNOWN, _ERROR_PREFIX".DBusRequestUknown"},
} ;

 /**
  * tlm_error_quark:
  *
  * Creates and returns a domain for Tlm errors.
  *
  * Returns: #GQuark for Tlm errors
  */
GQuark
tlm_error_quark (void)
{
    static volatile gsize quark_volatile = 0;

    g_dbus_error_register_error_domain (TLM_ERROR_DOMAIN,
                                        &quark_volatile,
                                        _tlm_errors,
                                        G_N_ELEMENTS (_tlm_errors));

    return (GQuark) quark_volatile;
}

/**
 * tlm_error_new_from_variant:
 * @var: (transfer none): instance of #GVariant
 *
 * Converts the GVariant to GError.
 *
 * Returns: (transfer full): #GError object if successful, NULL otherwise.
 */
GError *
tlm_error_new_from_variant (
        GVariant *var)
{
    GError *error = NULL;
    gchar *message;
    GQuark domain;
    gint code;

    if (!var) {
        return NULL;
    }

    g_variant_get (var, "(uis)", &domain, &code, &message);
    error = g_error_new_literal (domain, code, message);
    g_free (message);
    return error;
}

/**
 * tlm_error_to_variant:
 * @error: (transfer none): instance of #GError
 *
 * Converts the GError to GVariant.
 *
 * Returns: (transfer full) #GVariant object if successful, NULL otherwise.
 */
GVariant *
tlm_error_to_variant (
        GError *error)
{
    if (!error) {
        return NULL;
    }

    return g_variant_new ("(uis)", error->domain, error->code, error->message);
}
