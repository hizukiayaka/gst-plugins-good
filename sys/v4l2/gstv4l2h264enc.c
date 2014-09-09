/*
 * Copyright (C) 2014 SUMOMO Computer Association
 *     Author: ayaka <ayaka@soulik.info>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "gstv4l2h264enc.h"
#include "v4l2_calls.h"

#include <string.h>
#include <gst/gst-i18n-plugin.h>

GST_DEBUG_CATEGORY_STATIC (gst_v4l2_h264_enc_debug);
#define GST_CAT_DEFAULT gst_v4l2_h264_enc_debug

enum
{
  PROP_0,
  V4L2_STD_OBJECT_PROPS,
};

#define gst_v4l2_h264_enc_parent_class parent_class
G_DEFINE_TYPE (GstV4l2H264Enc, gst_v4l2_h264_enc, GST_TYPE_V4L2_VIDEO_ENC);

static void
gst_v4l2_h264_enc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
    GST_V4L2_VIDEO_ENC_CLASS(parent_class)->set_property
      (object, prop_id, value, pspec);
}

static void
gst_v4l2_h264_enc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
   GST_V4L2_VIDEO_ENC_CLASS(parent_class)->get_property
     (object, prop_id, value, pspec);
}

static GstFlowReturn
gst_v4l2_h264_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstV4l2H264Enc *self = GST_V4L2_H264_ENC (encoder);
  GstV4l2VideoEnc *parent = GST_V4L2_VIDEO_ENC (encoder);
  GstFlowReturn ret = GST_FLOW_OK;
  GstStructure *structure;
  GstCaps *outcaps;

  if (G_UNLIKELY (!GST_V4L2_IS_ACTIVE (parent->v4l2capture))) {
    outcaps = gst_caps_new_empty_simple ("video/x-h264");
    structure = gst_caps_get_structure (outcaps, 0);
    gst_structure_set (structure, "stream-format",
        G_TYPE_STRING, "byte-stream", NULL);
    gst_structure_set (structure, "alignment", G_TYPE_STRING, "au", NULL);
    return GST_V4L2_VIDEO_ENC_CLASS (parent_class)->handle_frame
        (encoder, frame, outcaps);
  }

  return GST_V4L2_VIDEO_ENC_CLASS (parent_class)->handle_frame
      (encoder, frame, NULL);
}

static void
gst_v4l2_h264_enc_init (GstV4l2H264Enc * self)
{

}

static void
gst_v4l2_h264_enc_class_init (GstV4l2H264EncClass * klass)
{
  GstElementClass *element_class;
  GObjectClass *gobject_class;
  GstV4l2VideoEncClass *v4l2_encoder_class;
  GstVideoEncoderClass *baseclass;

  parent_class = g_type_class_peek_parent (klass);

  element_class = (GstElementClass *) klass;
  gobject_class = (GObjectClass *) klass;
  v4l2_encoder_class = GST_V4L2_VIDEO_ENC_CLASS (klass);
  baseclass = GST_VIDEO_ENCODER_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_v4l2_h264_enc_debug, "v4l2h264enc", 0,
      "V4L2 H.264 Encoder");

  gst_element_class_set_static_metadata (element_class,
      "V4L2 H.264 Encoder",
      "Codec/Encoder/Video",
      "Encode H.264 video streams via V4L2 API", "ayaka <ayaka@soulik.info>");

  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_v4l2_h264_enc_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_v4l2_h264_enc_get_property);
  /* FIXME propose_allocation or not ? */
  baseclass->handle_frame = GST_DEBUG_FUNCPTR (gst_v4l2_h264_enc_handle_frame);

  gst_v4l2_object_install_m2m_properties_helper (gobject_class);

}

/* Probing functions */
gboolean
gst_v4l2_is_h264_enc (GstCaps * sink_caps, GstCaps * src_caps)
{
  gboolean ret = FALSE;

  if (gst_caps_is_subset (sink_caps, gst_v4l2_object_get_raw_caps ())
      && gst_caps_can_intersect (src_caps,
          gst_caps_from_string ("video/x-h264")))
    ret = TRUE;

  return ret;
}
