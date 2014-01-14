/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
 *
 * Copyright (C) 2013 Intel Corporation.
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
 * TLM_CONFIG_GENERAL_PLUGINS_DIR:
 *
 * Plugins directory path
 */
#define TLM_CONFIG_GENERAL_PLUGINS_DIR      "PLUGINS_DIR"

/**
 * TLM_CONFIG_GENERAL_ACCOUNTS_PLUGIN:
 *
 * Accounts plugin to use
 */
#define TLM_CONFIG_GENERAL_ACCOUNTS_PLUGIN  "ACCOUNTS_PLUGIN"

/**
 * TLM_CONFIG_GENERAL_SESSION_CMD:
 *
 * Session command line
 */
#define TLM_CONFIG_GENERAL_SESSION_CMD      "SESSION_CMD"

/**
 * TLM_CONFIG_GENERAL_SESSION_PATH:
 *
 * Default session search PATH
 */
#define TLM_CONFIG_GENERAL_SESSION_PATH     "SESSION_PATH"

/**
 * TLM_CONFIG_GENERAL_AUTO_LOGIN_GUEST
 *
 * Autologin to default user : TRUE/FALSE
 */
#define TLM_CONFIG_GENERAL_AUTO_LOGIN       "AUTO_LOGIN"

/**
 * TLM_CONFIG_GENERAL_PREPARE_DEFAULT
 *
 * Prepare default user before auto-login: TRUE/FALSE
 */
#define TLM_CONFIG_GENERAL_PREPARE_DEFAULT  "PREPARE_DEFAULT"

/**
 * TLM_CONFIG_GENERAL_PAM_SERVICE:
 *
 * PAM service file to use
 */ 
#define TLM_CONFIG_GENERAL_PAM_SERVICE      "PAM_SERVICE"

/**
 * TLM_CONFIG_GENERAL_DEFAULT_USER:
 *
 * Default username for autologin
 */
#define TLM_CONFIG_GENERAL_DEFAULT_USER     "DEFAULT_USER"

#endif /* __TLM_GENERAL_CONFIG_H_ */
