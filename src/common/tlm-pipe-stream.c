/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@linux.intel.com>
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
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include "tlm-pipe-stream.h"
#include "tlm-log.h"

#define TLM_PIPE_STREAM_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),\
    TLM_TYPE_PIPE_STREAM, TlmPipeStreamPrivate))

struct _TlmPipeStreamPrivate
{
    GInputStream  *input_stream;
    GOutputStream *output_stream;
};

G_DEFINE_TYPE (TlmPipeStream, tlm_pipe_stream, G_TYPE_IO_STREAM);

static GInputStream *
_tlm_pipe_stream_get_input_stream (GIOStream *io_stream)
{
    return TLM_PIPE_STREAM (io_stream)->priv->input_stream;
}

static GOutputStream *
_tlm_pipe_stream_get_output_stream (GIOStream *io_stream)
{
    return TLM_PIPE_STREAM (io_stream)->priv->output_stream;
}

static void
_tlm_pipe_stream_dispose (GObject *gobject)
{
    g_return_if_fail (TLM_IS_PIPE_STREAM (gobject));

    DBG ("%p", gobject);
    /* Chain up to the parent class */
    G_OBJECT_CLASS (tlm_pipe_stream_parent_class)->dispose (gobject);

}

static void
_tlm_pipe_stream_finalize (GObject *gobject)
{
    TlmPipeStream *stream = TLM_PIPE_STREAM (gobject);

    /* g_io_stream needs streams to be valid in its dispose still
     */
    if (stream->priv->input_stream) {
        g_clear_object (&stream->priv->input_stream);
    }

    if (stream->priv->output_stream) {
        g_clear_object (&stream->priv->output_stream);
    }

    G_OBJECT_CLASS (tlm_pipe_stream_parent_class)->finalize (gobject);
}

static void
tlm_pipe_stream_class_init (TlmPipeStreamClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GIOStreamClass *stream_class = G_IO_STREAM_CLASS (klass);

    gobject_class->finalize = _tlm_pipe_stream_finalize;
    gobject_class->dispose = _tlm_pipe_stream_dispose;

    /* virtual methods */
    stream_class->get_input_stream = _tlm_pipe_stream_get_input_stream;
    stream_class->get_output_stream = _tlm_pipe_stream_get_output_stream;

    g_type_class_add_private (klass, sizeof (TlmPipeStreamPrivate));
}

static void
tlm_pipe_stream_init (TlmPipeStream *self)
{
    self->priv = TLM_PIPE_STREAM_GET_PRIVATE (self);
    self->priv->input_stream = NULL;
    self->priv->output_stream = NULL;
}

TlmPipeStream *
tlm_pipe_stream_new (
        gint in_fd,
        gint out_fd,
        gboolean close_fds)
{
    TlmPipeStream *stream = TLM_PIPE_STREAM (g_object_new (
            TLM_TYPE_PIPE_STREAM, NULL));
    if (stream) {
        stream->priv->input_stream = G_INPUT_STREAM (
                g_unix_input_stream_new (in_fd, close_fds));
        stream->priv->output_stream = G_OUTPUT_STREAM (
                g_unix_output_stream_new (out_fd, close_fds));
    }
    DBG ("%p", stream);
    return stream;
}


