/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2011 Nick Schermer <nick@xfce.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <tumbler/tumbler.h>
#include <dir-thumbnailer.h>

static void dir_thumbnailer_create (TumblerAbstractThumbnailer *thumbnailer,
                                    GCancellable               *cancellable,
                                    TumblerFileInfo            *info);

struct _DirThumbnailerClass {
  TumblerAbstractThumbnailerClass __parent__;
};

struct _DirThumbnailer {
  TumblerAbstractThumbnailer __parent__;
};

G_DEFINE_DYNAMIC_TYPE (DirThumbnailer, dir_thumbnailer, TUMBLER_TYPE_ABSTRACT_THUMBNAILER);

void dir_thumbnailer_register (TumblerProviderPlugin *plugin) {
  dir_thumbnailer_register_type (G_TYPE_MODULE (plugin));
}

static void dir_thumbnailer_class_init (DirThumbnailerClass *klass) {
  TumblerAbstractThumbnailerClass *abstractthumbnailer_class;
  abstractthumbnailer_class = TUMBLER_ABSTRACT_THUMBNAILER_CLASS (klass);
  abstractthumbnailer_class->create = dir_thumbnailer_create;
}

static void dir_thumbnailer_class_finalize (DirThumbnailerClass *klass) {}

static void dir_thumbnailer_init (DirThumbnailer *thumbnailer) {}

static void dir_thumbnailer_size_prepared (GdkPixbufLoader *loader,
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

/* Find the best matching image file in .thumbnails directory based on requested size */
static gchar *
find_best_thumbnail_image (const gchar *thumbnails_dir, gint requested_size) {
  GDir *dir;
  const gchar *filename;
  gchar *best_match = NULL;
  gint best_size = -1;
  GError *error = NULL;

  dir = g_dir_open (thumbnails_dir, 0, &error);
  if (!dir) {
    if (error) g_error_free (error);
    return NULL;
  }

  /* Scan all files in .thumbnails directory */
  while ((filename = g_dir_read_name (dir)) != NULL) {
    gchar *basename = g_strdup (filename);
    gchar *dot = strrchr (basename, '.');
    
    if (dot) {
      /* Check if it's an image file */
      const gchar *ext = dot + 1;
      if (g_ascii_strcasecmp (ext, "png") == 0 ||
          g_ascii_strcasecmp (ext, "jpg") == 0 ||
          g_ascii_strcasecmp (ext, "jpeg") == 0 ||
          g_ascii_strcasecmp (ext, "svg") == 0 ||
          g_ascii_strcasecmp (ext, "svgz") == 0) {
        
        /* Extract the numeric part (filename without extension) */
        *dot = '\0';
        gchar *endptr;
        glong size = strtol (basename, &endptr, 10);
        
        /* Check if the entire basename is a valid number */
        if (*endptr == '\0' && size > 0) {
          /* Select the best matching size:
           * - For sizes <= requested: use the largest one that fits
           * - For sizes > requested: use the smallest one that's larger
           */
          if (best_size == -1) {
            /* First valid image found */
            best_size = size;
            g_free (best_match);
            best_match = g_build_filename (thumbnails_dir, filename, NULL);
          } else if (size <= requested_size && size > best_size) {
            /* Better match: larger but still <= requested */
            best_size = size;
            g_free (best_match);
            best_match = g_build_filename (thumbnails_dir, filename, NULL);
          } else if (best_size > requested_size && size < best_size && size > requested_size) {
            /* Better match: smaller but still > requested */
            best_size = size;
            g_free (best_match);
            best_match = g_build_filename (thumbnails_dir, filename, NULL);
          } else if (best_size < requested_size && size > requested_size) {
            /* Switch from smaller-than to larger-than match */
            best_size = size;
            g_free (best_match);
            best_match = g_build_filename (thumbnails_dir, filename, NULL);
          }
        }
      }
    }
    g_free (basename);
  }

  g_dir_close (dir);
  return best_match;
}

static GdkPixbuf *
load_image_file (const gchar *image_path, TumblerThumbnail *thumbnail, GError **error) {
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf = NULL;
  GError *err = NULL;
  gchar *contents = NULL;
  gsize length = 0;

  /* Read the image file */
  if (!g_file_get_contents (image_path, &contents, &length, &err)) {
    g_propagate_error (error, err);
    return NULL;
  }

  /* Load with size-prepared signal for automatic scaling */
  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (loader, "size-prepared", 
                    G_CALLBACK (dir_thumbnailer_size_prepared), thumbnail);

  if (gdk_pixbuf_loader_write (loader, (const guchar *)contents, length, &err)) {
    if (gdk_pixbuf_loader_close (loader, &err)) {
      pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
      if (pixbuf != NULL)
        g_object_ref (pixbuf);
    }
  } else {
    gdk_pixbuf_loader_close (loader, NULL);
  }
  
  g_object_unref (loader);
  g_free (contents);

  if (err != NULL) g_propagate_error (error, err);
  return pixbuf;
}

static void
dir_thumbnailer_create (TumblerAbstractThumbnailer *thumbnailer,
                        GCancellable *cancellable, TumblerFileInfo *info) {
  TumblerThumbnail *thumbnail;
  TumblerThumbnailFlavor *flavor;
  GFile *file;
  GFile *thumbnails_dir_file;
  GError *error = NULL;
  GdkPixbuf *pixbuf = NULL;
  TumblerImageData data;
  gchar *dir_path = NULL;
  gchar *thumbnails_dir = NULL;
  gchar *best_image = NULL;
  gint requested_width, requested_height, requested_size;

  if (g_cancellable_is_cancelled (cancellable)) return;

  file = g_file_new_for_uri (tumbler_file_info_get_uri (info));
  
  /* Get the directory path */
  dir_path = g_file_get_path (file);
  if (!dir_path) {
    g_set_error (&error, TUMBLER_ERROR, TUMBLER_ERROR_INVALID_FORMAT,
                 "Failed to get directory path");
    goto cleanup;
  }

  /* Check if it's a directory */
  if (!g_file_test (dir_path, G_FILE_TEST_IS_DIR)) {
    g_set_error (&error, TUMBLER_ERROR, TUMBLER_ERROR_INVALID_FORMAT,
                 "Not a directory");
    goto cleanup;
  }

  /* Build path to .thumbnails subdirectory */
  thumbnails_dir = g_build_filename (dir_path, ".thumbnails", NULL);
  thumbnails_dir_file = g_file_new_for_path (thumbnails_dir);

  thumbnail = tumbler_file_info_get_thumbnail (info);

  /* Check if .thumbnails directory exists */
  if (!g_file_test (thumbnails_dir, G_FILE_TEST_IS_DIR)) {
    /* No .thumbnails directory - remove any cached thumbnail */
    tumbler_thumbnail_delete (thumbnail);
    g_set_error (&error, TUMBLER_ERROR, TUMBLER_ERROR_NO_CONTENT,
                 "Directory does not have .thumbnails subdirectory");
    goto cleanup;
  }

  /* Get the requested thumbnail size */
  flavor = tumbler_thumbnail_get_flavor (thumbnail);
  tumbler_thumbnail_flavor_get_size (flavor, &requested_width, &requested_height);
  g_object_unref (flavor);
  
  /* Use the maximum dimension as the size for selection */
  requested_size = MAX (requested_width, requested_height);

  /* Find the best matching image */
  best_image = find_best_thumbnail_image (thumbnails_dir, requested_size);
  
  if (!best_image) {
    g_set_error (&error, TUMBLER_ERROR, TUMBLER_ERROR_NO_CONTENT,
                 "No suitable thumbnail image found in .thumbnails directory");
    goto cleanup;
  }

  /* Load the image */
  pixbuf = load_image_file (best_image, thumbnail, &error);

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

cleanup:
  if (error) {
    g_signal_emit_by_name (thumbnailer, "error", tumbler_file_info_get_uri (info),
                           error->code, error->message);
    g_error_free (error);
  } else {
    g_signal_emit_by_name (thumbnailer, "ready", tumbler_file_info_get_uri (info));
  }

  g_free (best_image);
  g_free (thumbnails_dir);
  g_free (dir_path);
  if (thumbnails_dir_file) g_object_unref (thumbnails_dir_file);
  if (thumbnail) g_object_unref (thumbnail);
  if (file) g_object_unref (file);
}
