/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
 *
 * Copyright (C) 2013-2014 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@intel.com>
 *          Amarnath Valluri <amarnath.valluri@linux.intel.com>
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

#ifndef __TLM_CONFIG_GENERAL_H_
#define __TLM_CONFIG_GENERAL_H_

/**
 * SECTION:tlm-config-general
 * @title: General configuration
 * @short_description: tlm general configuration keys
 *
 * General configuration keys are defined below. See #TlmConfig for how to use
 * them.
 */

/**
 * TLM_CONFIG_GENERAL:
 *
 * A prefix for general keys. Should be used only when defining new keys.
 */
#define TLM_CONFIG_GENERAL                  "General"

/**
 * TLM_CONFIG_GENERAL_ACCOUNTS_PLUGIN:
 *
 * Accounts plugin (implementation of #TlmAccountPlugin) to use.
 * Default value: "default". If Tlm has been configured with --enable-debug, the
 * value can be overriden with TLM_ACCOUNT_PLUGIN environment variable.
 */
#define TLM_CONFIG_GENERAL_ACCOUNTS_PLUGIN  "ACCOUNTS_PLUGIN"

/**
 * TLM_CONFIG_GENERAL_SESSION_CMD:
 *
 * Session command line: the command run after successfull login. If the value
 * is not defined in the config file, user's shell from /etc/passwd is used,
 * and if that is not defined, the fallback is "systemd --user"
 */
#define TLM_CONFIG_GENERAL_SESSION_CMD      "SESSION_CMD"

/**
 * TLM_CONFIG_GENERAL_SESSION_PATH:
 *
 * Default value for PATH environment variable in user's session. If not set,
 * "/usr/local/bin:/usr/bin:/bin" is used.
 */
#define TLM_CONFIG_GENERAL_SESSION_PATH     "SESSION_PATH"

/**
 * TLM_CONFIG_GENERAL_DATA_DIRS:
 *
 * Default value for XDG_DATA_DIRS environment variable. If not set,
 * "/usr/share:/usr/local/share" is used.
 */
#define TLM_CONFIG_GENERAL_DATA_DIRS        "XDG_DATA_DIRS"

/**
 * TLM_CONFIG_GENERAL_AUTO_LOGIN
 *
 * Autologin to default user : TRUE/FALSE. TRUE if value is not set
 *
 * Whether to automatically log in the default user on startup and when another
 * user session has been terminated.
 *
 */
#define TLM_CONFIG_GENERAL_AUTO_LOGIN       "AUTO_LOGIN"

/**
 * TLM_CONFIG_GENERAL_PREPARE_DEFAULT
 *
 * Prepare default user before auto-login: TRUE/FALSE (FALSE if value not set).
 *
 * If set to TRUE, methods of #TlmAccountPlugin are used to set up the default
 * user's account before auto-login.
 */
#define TLM_CONFIG_GENERAL_PREPARE_DEFAULT  "PREPARE_DEFAULT"

/**
 * TLM_CONFIG_GENERAL_PAM_SERVICE:
 *
 * PAM service file to use for authentication and session setup. Default value: "tlm-login".
 */ 
#define TLM_CONFIG_GENERAL_PAM_SERVICE      "PAM_SERVICE"

/**
 * TLM_CONFIG_GENERAL_DEFAULT_USER:
 *
 * Default username for autologin. Default value: "guest".
 *
 * The value can include: \%S - seat number, \%I - seat id string.
 */
#define TLM_CONFIG_GENERAL_DEFAULT_USER     "DEFAULT_USER"

/**
 * TLM_CONFIG_GENERAL_SETUP_TERMINAL
 *
 * Setup terminal while creating session : TRUE/FALSE. (TRUE if not set).
 *
 * Whether to connect the standard input, output and error streams for a newly
 * created session to the terminal device.
 */
#define TLM_CONFIG_GENERAL_SETUP_TERMINAL   "SETUP_TERMINAL"

/**
 * TLM_CONFIG_GENERAL_TERMINATE_TIMEOUT
 *
 * Timeout for session termination in seconds. Default value: 10
 *
 * Specifies timeout between sending different termination signals in case
 * the previous signal wasn't obeyed.
 */
#define TLM_CONFIG_GENERAL_TERMINATE_TIMEOUT "TERMINATE_TIMEOUT" 

#endif /* __TLM_GENERAL_CONFIG_H_ */
