/*
 * Copyright (C) 2014 Collabora Ltd.
 *     Author: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
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

#include "v4l2-utils.h"
#include "v4l2_calls.h"
#include "gstv4l2videodec.h"
#include "gstv4l2videoenc.h"
#include "gstv4l2h264enc.h"
#include "string.h"

/**************************/
/* Common device iterator */
/**************************/

#if HAVE_GUDEV
#include <gudev/gudev.h>

struct _GstV4l2GUdevIterator
{
  GstV4l2Iterator parent;
  GList *devices;
  GUdevDevice *device;
  GUdevClient *client;
};

GstV4l2Iterator *
gst_v4l2_iterator_new (void)
{
  static const gchar *subsystems[] = { "video4linux", NULL };
  struct _GstV4l2GUdevIterator *it;

  it = g_slice_new0 (struct _GstV4l2GUdevIterator);

  it->client = g_udev_client_new (subsystems);
  it->devices = g_udev_client_query_by_subsystem (it->client, "video4linux");

  return (GstV4l2Iterator *) it;
}

gboolean
gst_v4l2_iterator_next (GstV4l2Iterator * _it)
{
  struct _GstV4l2GUdevIterator *it = (struct _GstV4l2GUdevIterator *) _it;
  const gchar *device_name;

  if (it->device)
    g_object_unref (it->device);

  it->device = NULL;
  it->parent.device_path = NULL;
  it->parent.device_name = NULL;

  if (it->devices == NULL)
    return FALSE;

  it->device = it->devices->data;
  it->devices = g_list_delete_link (it->devices, it->devices);

  device_name = g_udev_device_get_property (it->device, "ID_V4L_PRODUCT");
  if (!device_name)
    device_name = g_udev_device_get_property (it->device, "ID_MODEL_ENC");
  if (!device_name)
    device_name = g_udev_device_get_property (it->device, "ID_MODEL");

  it->parent.device_path = g_udev_device_get_device_file (it->device);
  it->parent.device_name = device_name;
  it->parent.sys_path = g_udev_device_get_sysfs_path (it->device);

  return TRUE;
}

void
gst_v4l2_iterator_free (GstV4l2Iterator * _it)
{
  struct _GstV4l2GUdevIterator *it = (struct _GstV4l2GUdevIterator *) _it;
  g_list_free_full (it->devices, g_object_unref);
  gst_object_unref (it->client);
  g_slice_free (struct _GstV4l2GUdevIterator, it);
}

#else /* No GUDEV */

struct _GstV4l2FsIterator
{
  GstV4l2Iterator parent;
  gint base_idx;
  gint video_idx;
  gchar *device;
};

GstV4l2Iterator *
gst_v4l2_iterator_new (void)
{
  struct _GstV4l2FsIterator *it;

  it = g_slice_new0 (struct _GstV4l2FsIterator);
  it->base_idx = 0;
  it->video_idx = -1;
  it->device = NULL;

  return (GstV4l2Iterator *) it;
}

gboolean
gst_v4l2_iterator_next (GstV4l2Iterator * _it)
{
  struct _GstV4l2FsIterator *it = (struct _GstV4l2FsIterator *) _it;
  static const gchar *dev_base[] = { "/dev/video", "/dev/v4l2/video", NULL };
  gchar *device = NULL;

  g_free ((gchar *) it->parent.device_path);
  it->parent.device_path = NULL;

  while (device == NULL) {
    it->video_idx++;

    if (it->video_idx >= 64) {
      it->video_idx = 0;
      it->base_idx++;
    }

    if (dev_base[it->base_idx] == NULL) {
      it->video_idx = 0;
      break;
    }

    device = g_strdup_printf ("%s%d", dev_base[it->base_idx], it->video_idx);

    if (g_file_test (device, G_FILE_TEST_EXISTS)) {
      it->parent.device_path = device;
      break;
    }

    g_free (device);
    device = NULL;
  }

  return it->parent.device_path != NULL;
}

void
gst_v4l2_iterator_free (GstV4l2Iterator * _it)
{
  struct _GstV4l2FsIterator *it = (struct _GstV4l2FsIterator *) _it;
  g_free ((gchar *) it->parent.device_path);
  g_slice_free (struct _GstV4l2FsIterator, it);
}

#endif
/* the part used to detect and register v4l2 encoder/decoder */
struct v4l2_elements
{
  GType (*get_type) (void);
  gboolean (*is_element) (GstCaps * sink_caps, GstCaps * src_caps);
  gboolean is_encoder;
  const gchar *element_name;
};

static const struct v4l2_elements elements[] = {
/*  {
  gst_v4l2_video_enc_get_type, NULL, TRUE, "videoenc"},*/
  {gst_v4l2_video_dec_get_type, gst_v4l2_is_video_dec, FALSE, "videodec"},
  {gst_v4l2_h264_enc_get_type, gst_v4l2_is_h264_enc, TRUE, "h264enc"},
};

static void
gst_v4l2_encoder_subinstance_init (GTypeInstance * instance, gpointer g_class)
{
  GstV4l2VideoEnc *self = GST_V4L2_VIDEO_ENC (instance);

  self->v4l2output =
      gst_v4l2_object_new (GST_ELEMENT (self),
      V4L2_BUF_TYPE_VIDEO_OUTPUT,
      GST_V4L2_VIDEO_ENC_CLASS (g_class)->default_device, gst_v4l2_get_output,
      gst_v4l2_set_output, NULL);
  self->v4l2output->no_initial_format = TRUE;
  self->v4l2output->keep_aspect = FALSE;

  self->v4l2capture =
      gst_v4l2_object_new (GST_ELEMENT (self),
      V4L2_BUF_TYPE_VIDEO_CAPTURE,
      GST_V4L2_VIDEO_ENC_CLASS (g_class)->default_device, gst_v4l2_get_input,
      gst_v4l2_set_input, NULL);
  self->v4l2capture->no_initial_format = TRUE;
  self->v4l2capture->keep_aspect = FALSE;
}


static void
gst_v4l2_encoder_subclass_init (gpointer g_class, gpointer data)
{
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

static void
gst_v4l2_decoder_subinstance_init (GTypeInstance * instance, gpointer g_class)
{
  GstV4l2VideoDec *self = GST_V4L2_VIDEO_DEC (instance);

  self->v4l2output =
      gst_v4l2_object_new (GST_ELEMENT (self),
      V4L2_BUF_TYPE_VIDEO_OUTPUT,
      GST_V4L2_VIDEO_DEC_CLASS (g_class)->default_device, gst_v4l2_get_output,
      gst_v4l2_set_output, NULL);
  self->v4l2output->no_initial_format = TRUE;
  self->v4l2output->keep_aspect = FALSE;

  self->v4l2capture =
      gst_v4l2_object_new (GST_ELEMENT (self),
      V4L2_BUF_TYPE_VIDEO_CAPTURE,
      GST_V4L2_VIDEO_DEC_CLASS (g_class)->default_device, gst_v4l2_get_input,
      gst_v4l2_set_input, NULL);
  self->v4l2capture->no_initial_format = TRUE;
  self->v4l2capture->keep_aspect = FALSE;
}


static void
gst_v4l2_decoder_subclass_init (gpointer g_class, gpointer data)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstV4l2VideoCData *cdata = data;

  GST_V4L2_VIDEO_DEC_CLASS (g_class)->default_device = cdata->device;

  /* Note: gst_pad_template_new() take the floating ref from the caps */
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink",
          GST_PAD_SINK, GST_PAD_ALWAYS, cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src",
          GST_PAD_SRC, GST_PAD_ALWAYS, cdata->src_caps));

  g_free (cdata);
}


gboolean
gst_v4l2_element_register (GstPlugin * plugin, const gchar * basename,
    const gchar * device_path, GstCaps * sink_caps, GstCaps * src_caps)
{
  GTypeQuery type_query;
  GTypeInfo type_info = { 0, };
  GType type, subtype;
  gchar *type_name;
  GstV4l2VideoCData *cdata;

  for (gint i = 0; i < (G_N_ELEMENTS (elements)); i++) {
    /*  if (NULL == elements[i].is_element) {
       elements[i].get_type ();
       continue;
       } else */
    if (!(elements[i].is_element (sink_caps, src_caps))) {
      continue;
    }

    cdata = g_new0 (GstV4l2VideoCData, 1);
    cdata->device = g_strdup (device_path);
    cdata->sink_caps = gst_caps_ref (sink_caps);
    cdata->src_caps = gst_caps_ref (src_caps);

    type = elements[i].get_type ();
    g_type_query (type, &type_query);
    memset (&type_info, 0, sizeof (type_info));
    type_info.class_size = type_query.class_size;
    type_info.instance_size = type_query.instance_size;
    type_info.class_data = cdata;
    if (elements[i].is_encoder) {
      type_info.class_init = gst_v4l2_encoder_subclass_init;
      type_info.instance_init = gst_v4l2_encoder_subinstance_init;
    } else {
      type_info.class_init = gst_v4l2_decoder_subclass_init;
      type_info.instance_init = gst_v4l2_decoder_subinstance_init;
    }

    type_name =
        g_strdup_printf ("v4l2%s%s", basename, elements[i].element_name);
    subtype = g_type_register_static (type, type_name, &type_info, 0);

    gst_element_register (plugin, type_name, GST_RANK_PRIMARY + 1, subtype);

    g_free (type_name);
    break;
  }
  return TRUE;
}
