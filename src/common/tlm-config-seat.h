/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
 *
 * Copyright (C) 2014 Intel Corporation.
 *
 * Contact: Jussi Laako <jussi.laako@linux.intel.com>
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

#ifndef __TLM_CONFIG_SEAT_H_
#define __TLM_CONFIG_SEAT_H_

/**
 * SECTION:tlm-config-seat
 * @title: Seat configuration
 * @short_description: Seat specific configuration
 *
 * Configuration items that are specific to a seat are defined here. Items that
 * may be either global or per-seat are defined in #tlm-config-general.
 */

/**
 * TLM_CONFIG_SEAT_ACTIVE:
 *
 * Value specifying whether a seat is to be used or not.
 * Default value: 1
 */
#define TLM_CONFIG_SEAT_ACTIVE          "ACTIVE"

#endif /* __TLM_CONFIG_SEAT_H_ */
