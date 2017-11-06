#include "eric_window.h"

double gdk_rgba_get_luminance( GdkRGBA *color )
{
	return  ( color->red ) * 0.2126 +
		( color->green ) * 0.7152 +
		( color->blue ) * 0.0722;
}

void gdk_color_lerp( GdkRGBA* c1, GdkRGBA* c2, double s, GdkRGBA* out )
{
	out->red = c1->red + ( c2->red - c1->red ) * s;	
	out->green = c1->green + ( c2->green - c1->green ) * s;	
	out->blue = c1->blue + ( c2->blue - c1->blue ) * s;	
	out->alpha = c1->alpha + ( c2->alpha - c1->alpha ) * s;	
}

gboolean eric_window_animation_timer( eric_window* w )
{
	w->background_change_percentage += 0.05;
	gdk_color_lerp( &w->background_color_old, &w->background_color_new, 
                        w->background_change_percentage, &w->background_color);
	gdk_color_lerp( &w->text_color_old, &w->text_color_new, 
                        w->background_change_percentage, &w->text_color);
	gtk_widget_queue_draw( w->window );

	if( w->background_change_percentage >= 1.0 ) return FALSE;

	return TRUE;
}

gboolean eric_window_draw( GtkWidget* widget, cairo_t* cr, eric_window* w )
{
	cairo_set_operator(cr,CAIRO_OPERATOR_SOURCE);
	w->background_color.alpha = 0.75;
	gdk_cairo_set_source_rgba( cr, &w->background_color );
	cairo_paint( cr );	
	
    if( w->draw_callback == NULL )
        return FALSE;
    else
        return w->draw_callback( widget, cr, w );
}


void eric_window_screen_changed( GtkWidget *widget, GdkScreen *old_screen, gpointer userdata )
{
    GdkVisual *visual;
	
	GdkScreen* screen=gtk_widget_get_screen(widget);
	if(!screen) return;

	visual = gdk_screen_get_rgba_visual(screen);
	if(visual==NULL) visual=gdk_screen_get_system_visual(screen);

	gtk_widget_set_visual(widget,visual);
}

void eric_window_gsettings_value_changed( GSettings *settings, const gchar *key, eric_window* w )
{
    if( strcmp( key, "primary-color" ) == 0 )
    {
        w->background_color_old = w->background_color;
        gdk_rgba_parse( &w->background_color_new, g_settings_get_string( settings, "primary-color" ) );
        w->text_color_old = w->text_color;
        if( gdk_rgba_get_luminance( &w->background_color_new ) > 0.5 )
            gdk_rgba_parse( &w->text_color_new, "#000000" );
        else
            gdk_rgba_parse( &w->text_color_new, "#FFFFFF" );
        
        w->background_change_percentage = 0.0;
        g_timeout_add( 32, (gpointer)eric_window_animation_timer, w );
    }
}

eric_window* eric_window_create( int width, int height, char* title )
{
    eric_window* w = malloc( sizeof( eric_window ) );
    w->draw_callback = NULL;

    if( title == NULL )
    {
        title = "eric window";
    }

	w->window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    gtk_window_set_title( GTK_WINDOW( w->window ), title );
	gtk_window_resize( GTK_WINDOW( w->window ), width, height );
	gtk_widget_add_events( w->window, GDK_STRUCTURE_MASK );

    gtk_widget_set_app_paintable( w->window, TRUE );

    g_signal_connect( G_OBJECT( w->window ), "draw", G_CALLBACK(eric_window_draw), (gpointer)w );
    g_signal_connect( G_OBJECT( w->window ), "screen-changed", G_CALLBACK(eric_window_screen_changed), (gpointer)w );
	g_signal_connect( G_OBJECT( w->window ), "delete-event", gtk_main_quit, NULL );

    eric_window_screen_changed( w->window, NULL, NULL );

    /* GSettings Stuff */
    GSettings* gsettings;
    GSettingsSchema* gsettings_schema;

    gsettings_schema = g_settings_schema_source_lookup( g_settings_schema_source_get_default(),
                                                "org.gnome.desktop.background",
                                                TRUE );
    if( gsettings_schema )
    {
        g_settings_schema_unref (gsettings_schema);
        gsettings_schema = NULL;
        gsettings = g_settings_new ( "org.gnome.desktop.background" );
    }

    g_signal_connect_data( gsettings, "changed", G_CALLBACK( eric_window_gsettings_value_changed ), (gpointer)w, 0, 0 );
    gdk_rgba_parse( &w->background_color, g_settings_get_string( gsettings, "primary-color" ) );
    if( gdk_rgba_get_luminance( &w->background_color ) > 0.5 )
        gdk_rgba_parse( &w->text_color, "#000000" );
    else
        gdk_rgba_parse( &w->text_color, "#FFFFFF" );

    gsettings = g_settings_new ( "org.gnome.desktop.interface" );
    w->interface_scale = (double)g_settings_get_uint( gsettings, "scaling-factor" );
    if( w->interface_scale < 1.0 )
        w->interface_scale = 1.0;

    return w;
}
