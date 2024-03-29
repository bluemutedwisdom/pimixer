/* vi:set expandtab sw=2 sts=2: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <gst/audio/mixerutils.h>
#include <gst/interfaces/mixer.h>

#include <libxfce4util/libxfce4util.h>

#include "libxfce4mixer.h"



static gboolean _xfce_mixer_filter_mixer  (GstMixer *mixer,
                                           gpointer  user_data);
static void     _xfce_mixer_destroy_mixer (GstMixer *mixer);



static guint       refcount = 0;
static GList      *mixers = NULL;
#ifdef HAVE_GST_MIXER_NOTIFICATION
static GstBus     *bus = NULL;
static GstElement *selected_card = NULL;
#endif



void
xfce_mixer_init (void)
{
  GtkIconTheme *icon_theme;
  gint          counter = 0;

  if (G_LIKELY (refcount++ == 0))
    {
      /* Append application icons to the search path */
      icon_theme = gtk_icon_theme_get_default ();
      gtk_icon_theme_append_search_path (icon_theme, MIXER_DATADIR G_DIR_SEPARATOR_S "icons");

      /* Get list of all available mixer devices */
      mixers = gst_audio_default_registry_mixer_filter (_xfce_mixer_filter_mixer, FALSE, &counter);

#ifdef HAVE_GST_MIXER_NOTIFICATION
      /* Create a GstBus for notifications */
      bus = gst_bus_new ();
      gst_bus_add_signal_watch (bus);
#endif
    }
}



void
xfce_mixer_shutdown (void)
{
  if (G_LIKELY (--refcount <= 0))
    {
      g_list_foreach (mixers, (GFunc) _xfce_mixer_destroy_mixer, NULL);
      g_list_free (mixers);

#ifdef HAVE_GST_MIXER_NOTIFICATION
      gst_bus_remove_signal_watch (bus);
      gst_object_unref (bus);
#endif
    }
}



GList *
xfce_mixer_get_cards (void)
{
  g_return_val_if_fail (refcount > 0, NULL);
  return mixers;
}



GstElement *
xfce_mixer_get_card (const gchar *name)
{
  GstElement *element = NULL;
  GList      *iter;
  gchar      *card_name;

  g_return_val_if_fail (refcount > 0, NULL);

  if (G_UNLIKELY (name == NULL))
    return NULL;

  for (iter = g_list_first (mixers); iter != NULL; iter = g_list_next (iter))
    {
      card_name = g_object_get_data (G_OBJECT (iter->data), "xfce-mixer-internal-name");

      if (G_UNLIKELY (g_utf8_collate (name, card_name) == 0))
        {
          element = iter->data;
          break;
        }
    }

  return element;
}



const gchar *
xfce_mixer_get_card_display_name (GstElement *card)
{
  g_return_val_if_fail (GST_IS_MIXER (card), NULL);
  if (xfce_mixer_is_default_card (card))
    return g_strconcat (g_object_get_data (G_OBJECT (card), "xfce-mixer-name"), " (Default)", NULL);
  else
    return g_object_get_data (G_OBJECT (card), "xfce-mixer-name");
}



const gchar *
xfce_mixer_get_card_internal_name (GstElement *card)
{
  g_return_val_if_fail (GST_IS_MIXER (card), NULL);
  return g_object_get_data (G_OBJECT (card), "xfce-mixer-internal-name");
}


const gchar *
xfce_mixer_get_card_id (GstElement *card)
{
  g_return_val_if_fail (GST_IS_MIXER (card), NULL);
  return g_object_get_data (G_OBJECT (card), "xfce-mixer-id");
}



void
xfce_mixer_select_card (GstElement *card)
{
  g_return_if_fail (GST_IS_MIXER (card));

#ifdef HAVE_GST_MIXER_NOTIFICATION
  gst_element_set_bus (card, bus);
  selected_card = card;
#endif
}



GstMixerTrack *
xfce_mixer_get_track (GstElement  *card,
                      const gchar *track_name)
{
  GstMixerTrack *track = NULL;
  const GList   *iter;
  gchar         *label;

  g_return_val_if_fail (GST_IS_MIXER (card), NULL);
  g_return_val_if_fail (track_name != NULL, NULL);

  for (iter = gst_mixer_list_tracks (GST_MIXER (card)); iter != NULL; iter = g_list_next (iter))
    {
      g_object_get (GST_MIXER_TRACK (iter->data), "label", &label, NULL);

      if (g_utf8_collate (label, track_name) == 0)
        {
          track = iter->data;
          g_free (label);
          break;
        }

      g_free (label);
    }

  return track;
}



#ifdef HAVE_GST_MIXER_NOTIFICATION
guint
xfce_mixer_bus_connect (GCallback callback,
                        gpointer  user_data)
{
  g_return_val_if_fail (refcount > 0, 0);
  return g_signal_connect (bus, "message::element", callback, user_data);
}



void
xfce_mixer_bus_disconnect (guint signal_handler_id)
{
  g_return_if_fail (refcount > 0);
  if (signal_handler_id != 0)
    g_signal_handler_disconnect (bus, signal_handler_id);
}
#endif



gint
xfce_mixer_get_max_volume (gint *volumes,
                           gint  num_channels)
{
  gint max = 0;

  g_return_val_if_fail (volumes != NULL, 0);

  for (--num_channels; num_channels >= 0; --num_channels)
    if (volumes[num_channels] > max)
      max = volumes[num_channels];

  return max;
}



static gboolean
_xfce_mixer_filter_mixer (GstMixer *mixer,
                          gpointer  user_data)
{
  GstElementFactory *factory;
  const gchar       *long_name;
  gchar             *device_name = NULL;
  gchar             *internal_name;
  gchar             *name;
  gchar             *p;
  gint               length;
  gint              *counter = user_data;
  gchar *device;

  /* Get long name of the mixer element */
  factory = gst_element_get_factory (GST_ELEMENT (mixer));
  long_name = gst_element_factory_get_longname (factory);

  /* Get the device name of the mixer element */
  if (g_object_class_find_property (G_OBJECT_GET_CLASS (G_OBJECT (mixer)), "device-name"))
    g_object_get (mixer, "device-name", &device_name, NULL);

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (G_OBJECT (mixer)), "device"))
    g_object_get (mixer, "device", &device, NULL);

  /* Fall back to default name if neccessary */
  if (G_UNLIKELY (device_name == NULL))
    device_name = g_strdup_printf (_("Unknown Volume Control %d"), (*counter)++);

  /* Build display name */
  name = g_strdup_printf ("%s (%s)", device_name, long_name);

  /* Free device name */
  g_free (device_name);

  /* Set name to be used by xfce4-mixer */
  g_object_set_data_full (G_OBJECT (mixer), "xfce-mixer-name", name, (GDestroyNotify) g_free);
  g_object_set_data_full (G_OBJECT (mixer), "xfce-mixer-id", device, (GDestroyNotify) g_free);

  /* Count alpha-numeric characters in the name */
  for (length = 0, p = name; *p != '\0'; ++p)
    if (g_ascii_isalnum (*p))
      ++length;

  /* Generate internal name */
  internal_name = g_new0 (gchar, length+1);
  for (length = 0, p = name; *p != '\0'; ++p)
    if (g_ascii_isalnum (*p))
      internal_name[length++] = *p;
  internal_name[length] = '\0';

  /* Remember name for use by xfce4-mixer */
  g_object_set_data_full (G_OBJECT (mixer), "xfce-mixer-internal-name", internal_name, (GDestroyNotify) g_free);

  /* Keep the mixer (we want all devices to be visible) */
  return TRUE;
}



static void
_xfce_mixer_destroy_mixer (GstMixer *mixer)
{
  gst_element_set_state (GST_ELEMENT (mixer), GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (mixer));
}


void xfce_mixer_set_default_card (char *id)
{
  char cmdbuf[256], idbuf[16], type[16], cid[16], *card, *bufptr = cmdbuf, state = 0, indef = 0;
  int inchar, count;
  char *user_config_file = g_build_filename (g_get_home_dir (), "/.asoundrc", NULL);

  // kill pulseaudio if it is running to disconnect Bluetooth devices
  system ("pulseaudio --kill");

  // Break the id string into the type (before the colon) and the card number (after the colon)
  strcpy (idbuf, id);
  card = strchr (idbuf, ':') + 1;
  *(strchr (idbuf, ':')) = 0;

  FILE *fp = fopen (user_config_file, "rb");
  if (!fp)
  {
    // File does not exist - create it from scratch
    fp = fopen (user_config_file, "wb");
    fprintf (fp, "pcm.!default {\n\ttype %s\n\tcard %s\n}\n\nctl.!default {\n\ttype %s\n\tcard %s\n}\n", idbuf, card, idbuf, card);
    fclose (fp);
  }
  else
  {
  // File exists - check to see whether it contains a default card
  type[0] = 0;
    cid[0] = 0;
    count = 0;
    while ((inchar = fgetc (fp)) != EOF)
    {
      if (inchar == ' ' || inchar == '\t' || inchar == '\n' || inchar == '\r')
      {
        if (bufptr != cmdbuf)
        {
          *bufptr = 0;
          switch (state)
          {
            case 1 :  strcpy (type, cmdbuf);
                      state = 0;
                      break;
            case 2 :  strcpy (cid, cmdbuf);
                      state = 0;
                      break;
            default : if (!strcmp (cmdbuf, "type") && indef) state = 1;
                      else if (!strcmp (cmdbuf, "card") && indef) state = 2;
                      else if (!strcmp (cmdbuf, "pcm.!default")) indef = 1;
                      else if (!strcmp (cmdbuf, "}")) indef = 0;
                      break;
          }
          bufptr = cmdbuf;
          count = 0;
          if (cid[0] && type[0]) break;
        }
        else
        {
          bufptr = cmdbuf;
          count = 0;
        }
      }
      else
      {
        if (count < 255)
        {
          *bufptr++ = inchar;
          count++;
        }
        else cmdbuf[255] = 0;
      }
    }
    fclose (fp);
    if (cid[0] && type[0])
    {
      // This piece of sed is surely self-explanatory...
      sprintf (cmdbuf, "sed -i '/pcm.!default\\|ctl.!default/,/}/ { s/type .*/type %s/g; s/card .*/card %s/g; }' %s", idbuf, card, user_config_file);
      system (cmdbuf);
      // Oh, OK then - it looks for type * and card * within the delimiters pcm.!default or ctl.!default and } and replaces the parameters
    }
    else
    {
      // No default card; append to end of file
      fp = fopen (user_config_file, "ab");
      fprintf (fp, "\n\npcm.!default {\n\ttype %s\n\tcard %s\n}\n\nctl.!default {\n\ttype %s\n\tcard %s\n}\n", idbuf, card, idbuf, card);
      fclose (fp);
    }
  }
  g_free (user_config_file);
}


guint
xfce_mixer_is_default_card (GstElement *card)
{
  g_return_val_if_fail (GST_IS_MIXER (card), 0);

  char tokenbuf[256], type[16], cid[16], state = 0, indef = 0;
  char *bufptr = tokenbuf;
  int inchar, count;

  // if pulseaudio is running, no ALSA devices are default
  FILE *fp = popen ("pulseaudio --check ; echo $?", "r");
  if (fp && fgets (tokenbuf, sizeof (tokenbuf) - 1, fp))
  {
    inchar = -1;
    sscanf (tokenbuf, "%d", &inchar);
    if (inchar == 0) return 0;
  }
  if (fp) fclose (fp);

  char *user_config_file = g_build_filename (g_get_home_dir (), "/.asoundrc", NULL);
  fp = fopen (user_config_file, "rb");
  if (fp)
  {
    type[0] = 0;
    cid[0] = 0;
    count = 0;
    while ((inchar = fgetc (fp)) != EOF)
    {
      if (inchar == ' ' || inchar == '\t' || inchar == '\n' || inchar == '\r')
      {
        if (bufptr != tokenbuf)
        {
          *bufptr = 0;
          switch (state)
          {
            case 1 :  strcpy (type, tokenbuf);
                    state = 0;
                    break;
            case 2 :    strcpy (cid, tokenbuf);
                    state = 0;
                    break;
            default :   if (!strcmp (tokenbuf, "type") && indef) state = 1;
                    else if (!strcmp (tokenbuf, "card") && indef) state = 2;
                    else if (!strcmp (tokenbuf, "pcm.!default")) indef = 1;
                    else if (!strcmp (tokenbuf, "}")) indef = 0;
                    break;
          }
          bufptr = tokenbuf;
          count = 0;
          if (cid[0] && type[0]) break;
        }
        else
        {
          bufptr = tokenbuf;
          count = 0;
        }
      }
      else
      {
        if (count < 255)
        {
          *bufptr++ = inchar;
          count++;
        }
        else tokenbuf[255] = 0;
      }
    }
    fclose (fp);
  }
  if (cid[0] && type[0]) sprintf (tokenbuf, "%s:%s", type, cid);
  else sprintf (tokenbuf, "hw:0");
  if (!strcmp (tokenbuf, xfce_mixer_get_card_id (card))) return 1;
  return 0;
}


