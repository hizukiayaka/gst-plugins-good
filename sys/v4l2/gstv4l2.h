#ifndef GST_V4L2_H
#define GST_V4L2_H

typedef struct
{
  gchar *device;
  GstCaps *sink_caps;
  GstCaps *src_caps;
} GstV4l2VideoCData;

#endif
