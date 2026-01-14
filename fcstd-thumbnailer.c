/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2012 Yorik van Havre <yorik@uncreated.net>
 * Copyright (c) 2011 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2011 Nick Schermer <nick@xfce.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <gsf/gsf.h>
#include <gsf/gsf-input-gio.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-zip.h>

#include <tumbler/tumbler.h>
#include <fcstd-thumbnailer.h>

static void fcstd_thumbnailer_create (TumblerAbstractThumbnailer *thumbnailer,
                                      GCancellable               *cancellable,
                                      TumblerFileInfo            *info);

struct _FcstdThumbnailerClass {
  TumblerAbstractThumbnailerClass __parent__;
};

struct _FcstdThumbnailer {
  TumblerAbstractThumbnailer __parent__;
};

G_DEFINE_DYNAMIC_TYPE (FcstdThumbnailer, fcstd_thumbnailer, TUMBLER_TYPE_ABSTRACT_THUMBNAILER);

void fcstd_thumbnailer_register (TumblerProviderPlugin *plugin) {
  fcstd_thumbnailer_register_type (G_TYPE_MODULE (plugin));
}

static void fcstd_thumbnailer_class_init (FcstdThumbnailerClass *klass) {
  TumblerAbstractThumbnailerClass *abstractthumbnailer_class;
  abstractthumbnailer_class = TUMBLER_ABSTRACT_THUMBNAILER_CLASS (klass);
  abstractthumbnailer_class->create = fcstd_thumbnailer_create;
}

static void fcstd_thumbnailer_class_finalize (FcstdThumbnailerClass *klass) {}

static void fcstd_thumbnailer_init (FcstdThumbnailer *thumbnailer) {}

static void fcstd_thumbnailer_size_prepared (GdkPixbufLoader *loader,
                                             gint source_width, gint source_height,
                                             TumblerThumbnail *thumbnail) {
  TumblerThumbnailFlavor *flavor;
  gint dest_width, dest_height;
  gdouble hratio, wratio;

  if (!TUMBLER_IS_THUMBNAIL(thumbnail)) return;

  flavor = tumbler_thumbnail_get_flavor (thumbnail);
  tumbler_thumbnail_flavor_get_size (flavor, &dest_width, &dest_height);
  g_object_unref (flavor);

  if (source_width <= dest_width && source_height <= dest_height) {
    dest_width = source_width;
    dest_height = source_height;
  } else {
    wratio = (gdouble) source_width / (gdouble) dest_width;
    hratio = (gdouble) source_height / (gdouble) dest_height;
    if (hratio > wratio)
      dest_width = rint (source_width / hratio);
    else
      dest_height = rint (source_height / wratio);
  }
  gdk_pixbuf_loader_set_size (loader, MAX (dest_width, 1), MAX (dest_height, 1));
}

static GdkPixbuf *
fcstd_thumbnailer_create_from_data (const guchar *data, gsize bytes,
                                    TumblerThumbnail *thumbnail, GError **error) {
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf = NULL;
  GError *err = NULL;

  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (loader, "size-prepared", G_CALLBACK (fcstd_thumbnailer_size_prepared), thumbnail);

  if (gdk_pixbuf_loader_write (loader, data, bytes, &err)) {
    if (gdk_pixbuf_loader_close (loader, &err)) {
      pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
      if (pixbuf != NULL)
        g_object_ref (pixbuf);
    }
  } else {
    gdk_pixbuf_loader_close (loader, NULL);
  }
  g_object_unref (loader);

  if (err != NULL) g_propagate_error (error, err);
  return pixbuf;
}

static GdkPixbuf *
fcstd_thumbnailer_create_zip (GsfInfile *infile, TumblerThumbnail *thumbnail, GError **error) {
  GsfInput *thumb_file = NULL;
  const guint8 *data;
  gsize bytes;
  GdkPixbuf *pixbuf = NULL;

  /* Try multiple paths for the thumbnail image */
  thumb_file = gsf_infile_child_by_vname (infile, "Thumbnails", "Thumbnail.png", NULL);
  if (!thumb_file)
    thumb_file = gsf_infile_child_by_vname (infile, "thumbnails", "Thumbnail.png", NULL);
  if (!thumb_file)
    thumb_file = gsf_infile_child_by_name (infile, "Thumbnail.png");

  if (!thumb_file) return NULL;

  bytes = gsf_input_remaining (thumb_file);
  data = gsf_input_read (thumb_file, bytes, NULL);

  if (data)
    pixbuf = fcstd_thumbnailer_create_from_data (data, bytes, thumbnail, error);

  g_object_unref (thumb_file);
  return pixbuf;
}

static void
fcstd_thumbnailer_create (TumblerAbstractThumbnailer *thumbnailer,
                          GCancellable *cancellable, TumblerFileInfo *info) {
  GsfInput *input = NULL;
  GsfInfile *infile = NULL;
  TumblerThumbnail *thumbnail;
  GFile *file;
  GError *error = NULL;
  GdkPixbuf *pixbuf = NULL;
  TumblerImageData data;

  if (g_cancellable_is_cancelled (cancellable)) return;

  file = g_file_new_for_uri (tumbler_file_info_get_uri (info));

  /* ALWAYS use GIO stream, skip MMAP for stability */
  input = gsf_input_gio_new (file, &error);

  if (!input) {
    g_signal_emit_by_name (thumbnailer, "error", tumbler_file_info_get_uri (info),
                           error->code, error->message);
    g_error_free (error);
    g_object_unref (file);
    return;
  }

  /* CRITICAL: Do NOT uncompress. FCStd is a ZIP file. */

  thumbnail = tumbler_file_info_get_thumbnail (info);

  /* Open as ZIP archive */
  infile = gsf_infile_zip_new (input, &error);

  if (infile) {
    pixbuf = fcstd_thumbnailer_create_zip (infile, thumbnail, &error);
    g_object_unref (infile);
  } else {
    /* If gsf_infile_zip_new fails, error is set */
    if (!error) {
       g_set_error (&error, TUMBLER_ERROR, TUMBLER_ERROR_NO_CONTENT,
                    "Failed to open as ZIP archive");
    }
  }

  if (pixbuf) {
    data.data = gdk_pixbuf_get_pixels (pixbuf);
    data.has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
    data.bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
    data.width = gdk_pixbuf_get_width (pixbuf);
    data.height = gdk_pixbuf_get_height (pixbuf);
    data.rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    data.colorspace = (TumblerColorspace) gdk_pixbuf_get_colorspace (pixbuf);

    tumbler_thumbnail_save_image_data (thumbnail, &data,
                                       tumbler_file_info_get_mtime (info),
                                       NULL, &error);
    g_object_unref (pixbuf);
  }

  if (error) {
    g_signal_emit_by_name (thumbnailer, "error", tumbler_file_info_get_uri (info),
                           error->code, error->message);
    g_error_free (error);
  } else {
    g_signal_emit_by_name (thumbnailer, "ready", tumbler_file_info_get_uri (info));
  }

  if (input) g_object_unref (input);
  if (thumbnail) g_object_unref (thumbnail);
  if (file) g_object_unref (file);
}