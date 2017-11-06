#pragma once
/*
 * eric_window.h
 * creates a transluscent window using the current theme color
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>

#include <stdlib.h>
#include <string.h>

typedef struct eric_window eric_window;
struct eric_window
{
    GtkWidget* window;
    GdkRGBA background_color;
    GdkRGBA background_color_old;
    GdkRGBA background_color_new;
    GdkRGBA text_color;
    GdkRGBA text_color_old;
    GdkRGBA text_color_new;
    double background_change_percentage;
    gboolean (*draw_callback)( GtkWidget* widget, cairo_t* cr, eric_window* w );
    double interface_scale;
};

double gdk_rgba_get_luminance( GdkRGBA *color );
void gdk_color_lerp( GdkRGBA* c1, GdkRGBA* c2, double s, GdkRGBA* out );
gboolean eric_window_animation_timer( eric_window* w );
gboolean eric_window_draw( GtkWidget* widget, cairo_t* cr, eric_window* w );
void eric_window_screen_changed( GtkWidget *widget, GdkScreen *old_screen, gpointer userdata );
void eric_window_gsettings_value_changed( GSettings *settings, const gchar *key, eric_window* w );
eric_window* eric_window_create( int width, int height, char* title );
