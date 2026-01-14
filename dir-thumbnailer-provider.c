/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2011 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <tumbler/tumbler.h>

#include <dir-thumbnailer-provider.h>
#include <dir-thumbnailer.h>



static void   dir_thumbnailer_provider_thumbnailer_provider_init (TumblerThumbnailerProviderIface *iface);
static GList *dir_thumbnailer_provider_get_thumbnailers          (TumblerThumbnailerProvider      *provider);



struct _DirThumbnailerProviderClass
{
  GObjectClass __parent__;
};

struct _DirThumbnailerProvider
{
  GObject __parent__;
};



G_DEFINE_DYNAMIC_TYPE_EXTENDED (DirThumbnailerProvider,
                                dir_thumbnailer_provider,
                                G_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (TUMBLER_TYPE_THUMBNAILER_PROVIDER,
                                                       dir_thumbnailer_provider_thumbnailer_provider_init));



void
dir_thumbnailer_provider_register (TumblerProviderPlugin *plugin)
{
  dir_thumbnailer_provider_register_type (G_TYPE_MODULE (plugin));
}



static void
dir_thumbnailer_provider_class_init (DirThumbnailerProviderClass *klass)
{
}



static void
dir_thumbnailer_provider_class_finalize (DirThumbnailerProviderClass *klass)
{
}



static void
dir_thumbnailer_provider_thumbnailer_provider_init (TumblerThumbnailerProviderIface *iface)
{
  iface->get_thumbnailers = dir_thumbnailer_provider_get_thumbnailers;
}



static void
dir_thumbnailer_provider_init (DirThumbnailerProvider *provider)
{
}



static GList *
dir_thumbnailer_provider_get_thumbnailers (TumblerThumbnailerProvider *provider)
{
  static const gchar *mime_types[] =
  {
    "inode/directory",
    NULL
  };
  DirThumbnailer    *thumbnailer;
  GList             *thumbnailers = NULL;
  GStrv              uri_schemes;

  /* determine the URI schemes supported by GIO */
  uri_schemes = tumbler_util_get_supported_uri_schemes ();

  /* create the directory thumbnailer */
  thumbnailer = g_object_new (TYPE_DIR_THUMBNAILER,
                              "uri-schemes", uri_schemes,
                              "mime-types", mime_types,
                              NULL);

  /* add the thumbnailer to the list */
  thumbnailers = g_list_append (thumbnailers, thumbnailer);

  /* free URI schemes */
  g_strfreev (uri_schemes);

  return thumbnailers;
}
