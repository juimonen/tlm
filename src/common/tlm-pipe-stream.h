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

#ifndef __TLM_PIPE_STREAM_H__
#define __TLM_PIPE_STREAM_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/*
 * Type macros.
 */
#define TLM_TYPE_PIPE_STREAM   (tlm_pipe_stream_get_type ())
#define TLM_PIPE_STREAM(obj)   (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                           TLM_TYPE_PIPE_STREAM, \
                                           TlmPipeStream))
#define TLM_IS_PIPE_STREAM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                           TLM_TYPE_PIPE_STREAM))
#define TLM_PIPE_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                             TLM_TYPE_PIPE_STREAM, \
                                             TlmPipeStreamClass))
#define TLM_IS_PIPE_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
                                             TLM_TYPE_PIPE_STREAM))
#define TLM_PIPE_STREAM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                             TLM_TYPE_PIPE_STREAM, \
                                             TlmPipeStreamClass))

typedef struct _TlmPipeStreamPrivate TlmPipeStreamPrivate;

typedef struct {
    GIOStream parent_instance;

    /*< private >*/
    TlmPipeStreamPrivate *priv;
} TlmPipeStream;

typedef struct {
    GIOStreamClass parent_class;

} TlmPipeStreamClass;

/* used by TLM_TYPE_PIPE_STREAM */
GType
tlm_pipe_stream_get_type (void);

TlmPipeStream *
tlm_pipe_stream_new (
        gint in_fd,
        gint out_fd,
        gboolean close_fds);

G_END_DECLS

#endif /* __TLM_PIPE_STREAM_H__ */
