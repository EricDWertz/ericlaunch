#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <pango/pango-context.h>
#include <pango/pangocairo.h>

PangoContext* pango_context;

#include "eric_window.h"

#define SCALE_VALUE(x) (x)*window->interface_scale

int ICON_SIZE = 128;

double WINDOW_WIDTH=640;
double WINDOW_HEIGHT=480;

typedef struct
{ 
    PangoLayout* layout;
    int layout_width;
    char name[64];
	double x;
	double y;
	GdkPixbuf* pixbuf;
	char launch_command[128];
	gboolean hover;
} LAUNCHER_APP_ICON;

eric_window* window;
GtkWidget* entry;

GList* icon_list;

gboolean windowed;

GdkPixbuf* get_app_icon(const char* name,int size)
{
	GtkIconTheme* theme=gtk_icon_theme_get_default();
	return gtk_icon_theme_load_icon(theme,name,size,0,NULL);
}

void draw_rounded_rect(cairo_t* cr,double x,double y,double w,double h,double r)
{
	cairo_move_to(cr,x+r,y);
	cairo_line_to(cr,x+w-r*2,y);
	cairo_arc(cr,x+w-r,y+r,r,-M_PI/2.0,0);
	cairo_line_to(cr,x+w,y+h-r*2);
	cairo_arc(cr,x+w-r,y+h-r,r,0,M_PI/2.0);
	cairo_line_to(cr, x+r,y+h);
	cairo_arc(cr,x+r,y+h-r,r,M_PI/2.0,M_PI);
	cairo_line_to(cr, x, y+r);
	cairo_arc(cr,x+r,y+r,r,M_PI,-M_PI/2.0);
	cairo_close_path(cr);
}

void draw_app_icon(cairo_t* cr,LAUNCHER_APP_ICON* icon)
{
	double icon_center=(double)ICON_SIZE/2.0;
	double x=icon->x;
	double y=icon->y;
    PangoRectangle rect;

	//Hover rounded rect highlight
	if(icon->hover)
	{
		cairo_set_source_rgba(cr,( window->text_color.red ), 
                ( window->text_color.green ),
                ( window->text_color.blue ),
                0.25);
		draw_rounded_rect(cr,icon->x-ICON_SIZE/8,icon->y-ICON_SIZE/8,ICON_SIZE*1.25,ICON_SIZE*1.25,8);
		cairo_fill(cr);
	}
	
    //Draw Icon
	gdk_cairo_set_source_pixbuf(cr,icon->pixbuf,x+ICON_SIZE/4,y+ICON_SIZE/8);
	cairo_paint(cr);
	
    pango_layout_get_pixel_extents( icon->layout, NULL, &rect );

    double text_x= x + icon_center - ((double)rect.width / 2.0);
    double text_y=y+ICON_SIZE*0.75;

    //Draw a shadow
    cairo_set_source_rgba(cr,( 1.0 - window->text_color.red ), 
            ( 1.0 - window->text_color.green ),
            ( 1.0 - window->text_color.blue ),
            0.25);
    cairo_move_to(cr,text_x - rect.x + SCALE_VALUE(1.0),text_y + SCALE_VALUE(1.0));
    pango_cairo_layout_path( cr, icon->layout );
    cairo_fill(cr);

	gdk_cairo_set_source_rgba( cr, &window->text_color );	
    cairo_move_to(cr,text_x - rect.x,text_y);
    pango_cairo_layout_path( cr, icon->layout );

    cairo_fill(cr);
}   

GFunc draw_icon_list_item( gpointer data, gpointer user )
{
	draw_app_icon((cairo_t*)user,(LAUNCHER_APP_ICON*)data);
}

static gboolean draw( GtkWidget* widget, cairo_t* cr, eric_window* w )
{
   	cairo_set_operator(cr,CAIRO_OPERATOR_OVER);	

	g_list_foreach(icon_list,(GFunc)draw_icon_list_item,cr);
	
    return FALSE;
}

static gboolean draw_entry( GtkWidget* widget, cairo_t* cr, gpointer user_data )
{
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.75);

	cairo_paint(cr);

    return FALSE;
}

double icon_layout_x;
double icon_layout_y;
double icon_layout_max_x;
double icon_layout_margin;
double icon_layout_stride_x;
double icon_layout_stride_y;
double icon_layout_width;
double icon_width;

GFunc layout_app_icon(gpointer data,gpointer user)
{
	LAUNCHER_APP_ICON* icon=(LAUNCHER_APP_ICON*)data;
	icon->x=icon_layout_x;
	icon->y=icon_layout_y;
    
    pango_layout_set_alignment( icon->layout, PANGO_ALIGN_CENTER );
    pango_layout_set_width( icon->layout, (int)(ICON_SIZE*0.99) * PANGO_SCALE );
	pango_layout_set_text( icon->layout, icon->name, strlen(icon->name) );
    pango_layout_get_pixel_size( icon->layout, &icon->layout_width, NULL );
	
	icon_layout_x+=icon_layout_stride_x;
	if(icon_layout_x>icon_layout_max_x) 
	{
		icon_layout_x=icon_layout_margin;
		icon_layout_y+=icon_layout_stride_y;
	}
    
    return 0;
}

//This is a really horrible naming convention change it
void do_icon_layout()
{
	icon_layout_x=ICON_SIZE/2;
	icon_layout_y=ICON_SIZE*1.5;

	icon_layout_margin=ICON_SIZE/2;
	icon_layout_width = WINDOW_WIDTH - ICON_SIZE;
	icon_layout_max_x=icon_layout_margin+(icon_layout_width-ICON_SIZE*1.25);
	
	double icon_count=(icon_layout_width-ICON_SIZE*1.25)/(ICON_SIZE*1.25);
	icon_width = icon_layout_width / floor(icon_count);
	icon_layout_stride_x=(icon_layout_width-ICON_SIZE*1.25)/floor(icon_count);
	icon_layout_stride_y=ICON_SIZE*1.5;
	
	icon_layout_x=icon_layout_margin;
	icon_layout_y=ICON_SIZE*1.5;
	
	g_list_foreach(icon_list,(GFunc)layout_app_icon,NULL);
}

void add_app_icon(const char* name,const char* icon_name,const char* cmd)
{
	LAUNCHER_APP_ICON* icon=malloc(sizeof(LAUNCHER_APP_ICON));
	strcpy(icon->name,name);
	icon->pixbuf=get_app_icon(icon_name,ICON_SIZE*0.5);
	icon->x=icon_layout_x;
	icon->y=icon_layout_y;
	strcpy(icon->launch_command,cmd);

    icon->layout = pango_layout_new( pango_context );
	pango_layout_set_wrap (icon->layout, PANGO_WRAP_WORD );
    pango_layout_set_auto_dir (icon->layout, FALSE);
    //pango_layout_set_width (icon->layout, -1);
    pango_layout_set_text (icon->layout, name, strlen (name));
    pango_layout_get_pixel_size (icon->layout, &icon->layout_width, NULL);
	
	icon_list=g_list_append(icon_list,icon);
	
	icon_layout_x+=ICON_SIZE*2;
	if(icon_layout_x>WINDOW_WIDTH) 
	{
		icon_layout_x=ICON_SIZE/2;
		icon_layout_y+=ICON_SIZE*2;
	}
	icon->hover=FALSE;
}

gboolean changedstate;

GFunc icon_check_hover(gpointer data,gpointer user)
{
	LAUNCHER_APP_ICON* icon=(LAUNCHER_APP_ICON*)data;
	GdkEventMotion* ev=(GdkEventMotion*)user;

	if(ev->x > icon->x && ev->x < icon->x+ICON_SIZE &&
		ev->y > icon->y && ev->y < icon->y+ICON_SIZE)
	{
		if(icon->hover==FALSE) changedstate=TRUE;
		icon->hover=TRUE;
	}
	else
	{
		if(icon->hover==TRUE) changedstate=TRUE;
		icon->hover=FALSE;
	}

    return 0;
}

gboolean window_mouse_move(GtkWidget* widget,GdkEvent* event,gpointer user)
{
	changedstate=FALSE;

	g_list_foreach(icon_list,(GFunc)icon_check_hover,(gpointer)&event->motion);

	if( changedstate ) 
        gtk_widget_queue_draw( window->window );

    return FALSE;
}

gboolean window_key_press(GtkWidget* widget,GdkEvent* event,gpointer user)
{
	GdkEventKey* keyevent=&event->key;
	if(keyevent->keyval==GDK_KEY_Escape) gtk_main_quit();

	if(keyevent->keyval==GDK_KEY_Return)
	{
		char buffer[512];
        GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();

        if( (keyevent->state & modifiers) == GDK_SHIFT_MASK )
            sprintf(buffer,"xfce4-terminal -e \"%s\" &",gtk_entry_get_text(GTK_ENTRY(entry)));
        else
            sprintf(buffer,"%s &",gtk_entry_get_text(GTK_ENTRY(entry)));


		if(system(buffer)!=-1) gtk_main_quit();
	}

	return FALSE;
}

GFunc icon_test_launch(gpointer data,gpointer user)
{
	LAUNCHER_APP_ICON* icon=(LAUNCHER_APP_ICON*)data;
	GdkEventButton* ev=(GdkEventButton*)user;

	if(ev->x > icon->x && ev->x < icon->x+ICON_SIZE &&
		ev->y > icon->y && ev->y < icon->y+ICON_SIZE)
	{
		char buffer[129];
		sprintf(buffer,"%s &",icon->launch_command);
		system(buffer);
		gtk_main_quit();
	}
}


gboolean window_button_release(GtkWidget* widget,GdkEvent* event,gpointer user)
{
	GdkEventButton* buttonevent=&event->button;
	
	g_list_foreach(icon_list,(GFunc)icon_test_launch,(gpointer)buttonevent);

	return TRUE;
}

void parse_desktop_entry(const char* name)
{
    char buffer[256];
    GKeyFile* key_file = g_key_file_new();

    sprintf( buffer, "/usr/share/applications/%s.desktop", name );
    printf( "%s\n", buffer );
    if( !g_key_file_load_from_file( key_file, buffer, G_KEY_FILE_NONE, NULL ) )
    {
        return;
    }

    gchar* app_name = g_key_file_get_string( key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME, NULL );
    gchar* icon_name = g_key_file_get_string( key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, NULL );
    gchar* exec = g_key_file_get_string( key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL );


    printf( "%s, %s, %s\n", app_name, icon_name, exec );
    add_app_icon(app_name,icon_name,exec);
}
	
void do_layout_fullscreen()
{
	GdkScreen* screen=gdk_screen_get_default();

    GdkRectangle mon_geom;
    gdk_screen_get_monitor_geometry( screen, gdk_screen_get_primary_monitor(screen), &mon_geom );

    WINDOW_WIDTH=mon_geom.width;
    WINDOW_HEIGHT=mon_geom.height;
}

void do_layout_windowed(int width,int height)
{
	WINDOW_WIDTH=width;
	WINDOW_HEIGHT=height;
}

gboolean entry_lose_focus(GtkWidget* widget,GdkEvent* event, gpointer user)
{
	gtk_main_quit();
	return TRUE;	
}

//Command line arguments
// -w is windowed mode
// -p X Y is windowed mode position
// -d W H is windowed mode width/height
// -s X is icon size

int main(int argc, char **argv)
{
    /* boilerplate initialization code */
    gtk_init(&argc, &argv);

    windowed=FALSE;
    int x=0;
    int y=0;
    int w=640;
    int h=480;
    char fontdesc[256];

    //read CLI arguments
    int i;
    for(i=0;i<argc;i++)
    {
        if(strcmp(argv[i],"-w")==0) 
            windowed=TRUE;

        if(strcmp(argv[i],"-p")==0)
        {
            x=atoi(argv[i+1]);
            y=atoi(argv[i+2]);
        }

        if(strcmp(argv[i],"-d")==0)
        {
            w=atoi(argv[i+1]);
            h=atoi(argv[i+2]);
        }

        if(strcmp(argv[i],"-s")==0) 
            ICON_SIZE=atoi(argv[i+1]);
    }
		
    if(windowed)
       do_layout_windowed(w,h);
    else
       do_layout_fullscreen();

    printf( "%f, %f\n", WINDOW_WIDTH, WINDOW_HEIGHT );
    
    window = eric_window_create( WINDOW_WIDTH, WINDOW_HEIGHT, "" );;
    gtk_widget_add_events( window->window, GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK );
    g_signal_connect( G_OBJECT( window->window ), "key-press-event", G_CALLBACK(window_key_press), NULL);
    g_signal_connect( G_OBJECT( window->window ), "motion-notify-event", G_CALLBACK(window_mouse_move), NULL);
    g_signal_connect( G_OBJECT( window->window ), "button-release-event", G_CALLBACK(window_button_release), NULL);
    gtk_widget_set_size_request(window->window,WINDOW_WIDTH,WINDOW_HEIGHT);

    ICON_SIZE = SCALE_VALUE(ICON_SIZE);

    window->draw_callback = draw;

    pango_context = gtk_widget_create_pango_context( window->window );
    sprintf( fontdesc, "Source Sans Pro Regular %ipx", MAX( 12, ICON_SIZE / 8 ) );
    PangoFontDescription* font=pango_font_description_from_string( fontdesc );
    pango_context_set_font_description( pango_context, font );

    icon_list=NULL;
    //TODO: load custom file here...
    parse_desktop_entry( "chromium" );
    parse_desktop_entry( "xfce4-terminal" );
    parse_desktop_entry( "libreoffice-writer" );
    parse_desktop_entry( "libreoffice-calc" );
    parse_desktop_entry( "Thunar" );
    parse_desktop_entry( "steam" );
    parse_desktop_entry( "xfce-settings-manager" );
    parse_desktop_entry( "gnome-system-monitor" );
    
    do_icon_layout();

    gtk_window_set_icon( GTK_WINDOW( window->window ), NULL );
    gtk_window_set_type_hint(GTK_WINDOW( window->window ), GDK_WINDOW_TYPE_HINT_DIALOG );
    gtk_window_set_decorated(GTK_WINDOW( window->window ), FALSE );
    gtk_window_set_resizable(GTK_WINDOW( window->window ), FALSE );


    //Layout widgets
    GtkWidget* fixed=gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(window->window),fixed);
    entry=gtk_entry_new();
    gtk_fixed_put(GTK_FIXED(fixed),entry,ICON_SIZE/4,ICON_SIZE/4);
    gtk_widget_set_size_request(entry,WINDOW_WIDTH-ICON_SIZE/2,ICON_SIZE);
    //gtk_widget_set_app_paintable(entry, TRUE);
    g_signal_connect(G_OBJECT(entry), "draw", G_CALLBACK(draw_entry), NULL);
    g_signal_connect(G_OBJECT(entry), "focus-out-event", G_CALLBACK(entry_lose_focus), NULL);

    //Set entry font here
    font=pango_font_description_from_string("Source Sans Pro Regular 24");
    gtk_widget_override_font(entry,font);
    pango_font_description_free(font);

    gtk_widget_show_all( window->window );
    
    gtk_window_move( GTK_WINDOW( window->window ), x, y );

    gtk_main();

    return 0;
}
