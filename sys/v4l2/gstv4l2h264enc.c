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
G_DEFINE_ABSTRACT_TYPE (GstV4l2H264Enc, gst_v4l2_h264_enc,
    GST_TYPE_V4L2_VIDEO_ENC);

static void
gst_v4l2_h264_enc_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstV4l2H264Enc *self = GST_V4L2_H264_ENC (object);

  switch (prop_id) {
      /* By default, only set on output */
    default:
      break;
  }
}

static void
gst_v4l2_h264_enc_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstV4l2H264Enc *self = GST_V4L2_H264_ENC (object);

  switch (prop_id) {
      /* By default read from output */
    default:
      break;
  }
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
gst_v4l2_h264_enc_subinstance_init (GTypeInstance * instance, gpointer g_class)
{
  GstV4l2H264EncClass *klass = GST_V4L2_H264_ENC_CLASS (g_class);
  GstV4l2H264Enc *self = GST_V4L2_H264_ENC (instance);
  GstV4l2VideoEnc *parent = GST_V4L2_VIDEO_ENC (instance);

  parent->v4l2output =
      gst_v4l2_object_new (GST_ELEMENT (self),
      V4L2_BUF_TYPE_VIDEO_OUTPUT,
      GST_V4L2_VIDEO_ENC_CLASS (g_class)->default_device, gst_v4l2_get_output,
      gst_v4l2_set_output, NULL);
  parent->v4l2output->no_initial_format = TRUE;
  parent->v4l2output->keep_aspect = FALSE;

  parent->v4l2capture =
      gst_v4l2_object_new (GST_ELEMENT (self),
      V4L2_BUF_TYPE_VIDEO_CAPTURE,
      GST_V4L2_VIDEO_ENC_CLASS (g_class)->default_device, gst_v4l2_get_input,
      gst_v4l2_set_input, NULL);
  parent->v4l2capture->no_initial_format = TRUE;
  parent->v4l2capture->keep_aspect = FALSE;
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

static void
gst_v4l2_h264_enc_subclass_init (gpointer g_class, gpointer data)
{
  GstV4l2H264EncClass *klass = GST_V4L2_H264_ENC_CLASS (g_class);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstV4l2VideoCData *cdata = data;

  GST_V4L2_VIDEO_ENC_CLASS (g_class)->default_device = cdata->device;

  /* Note: gst_pad_template_new() take the floating ref from the caps */
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink",
          GST_PAD_SINK, GST_PAD_ALWAYS, cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src",
          GST_PAD_SRC, GST_PAD_ALWAYS, cdata->src_caps));

  g_free (cdata);
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

gboolean
gst_v4l2_h264_enc_register (GstPlugin * plugin, const gchar * basename,
    const gchar * device_path, GstCaps * sink_caps, GstCaps * src_caps)
{
  GTypeQuery type_query;
  GTypeInfo type_info = { 0, };
  GType type, subtype;
  gchar *type_name;
  GstV4l2VideoCData *cdata;

  cdata = g_new0 (GstV4l2VideoCData, 1);
  cdata->device = g_strdup (device_path);
  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = gst_caps_ref (src_caps);

  type = gst_v4l2_h264_enc_get_type ();
  g_type_query (type, &type_query);
  memset (&type_info, 0, sizeof (type_info));
  type_info.class_size = type_query.class_size;
  type_info.instance_size = type_query.instance_size;
  type_info.class_init = gst_v4l2_h264_enc_subclass_init;
  type_info.class_data = cdata;
  type_info.instance_init = gst_v4l2_h264_enc_subinstance_init;

  type_name = g_strdup_printf ("v4l2%sh264enc", basename);
  subtype = g_type_register_static (type, type_name, &type_info, 0);

  gst_element_register (plugin, type_name, GST_RANK_PRIMARY + 1, subtype);

  g_free (type_name);

  return TRUE;
}
