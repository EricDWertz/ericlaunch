#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>

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
    GtkWidget* area;
} LAUNCHER_APP_ICON;

eric_window* window;
GtkWidget* entry;
GtkWidget* icon_grid;

GList* icon_list;

gboolean windowed;

GdkPixbuf* get_app_icon(const char* name,int size)
{
    GdkPixbuf* out;
	GtkIconTheme* theme=gtk_icon_theme_get_default();
	out = gtk_icon_theme_load_icon(theme,name,size,0,NULL);
    
    //TODO: Support file loading?

    //Attempt to load a generic icon
    if( !GDK_IS_PIXBUF( out ) )
    {
        out = gtk_icon_theme_load_icon( theme, "application-x-executable", size, 0, NULL );
    }

    return out;
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

static gboolean draw_app_icon( GtkWidget* widget, cairo_t* cr, gpointer user )
{
	LAUNCHER_APP_ICON* icon=(LAUNCHER_APP_ICON*)user;

	double icon_center=(double)ICON_SIZE/2.0;
	double x=ICON_SIZE/8; //icon->x;
	double y=ICON_SIZE/8; //icon->y;
    PangoRectangle rect;

	//cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	//cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
	//cairo_paint(cr);

	//Hover rounded rect highlight
	if(icon->hover)
	{
		cairo_set_source_rgba(cr,( window->text_color.red ), 
                ( window->text_color.green ),
                ( window->text_color.blue ),
                0.25);
		draw_rounded_rect(cr,0,0,ICON_SIZE*1.25,ICON_SIZE*1.25,8);
		cairo_fill(cr);
	}
	
    //Draw Icon
    if( GDK_IS_PIXBUF( icon->pixbuf ) )
    {
        gdk_cairo_set_source_pixbuf(cr,icon->pixbuf,x+ICON_SIZE/4,y+ICON_SIZE/8);
        cairo_paint(cr);
    }
    else
    {
        printf( "No icon for: %s\n", icon->name );
    }
	
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

    return FALSE;
}   

static gboolean draw_entry( GtkWidget* widget, cairo_t* cr, gpointer user_data )
{
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.75);

	cairo_paint(cr);

    return FALSE;
}

gboolean icon_mouse_enter( GtkWidget* widget, GdkEvent* event, gpointer user )
{
	LAUNCHER_APP_ICON* icon=(LAUNCHER_APP_ICON*)user;
    icon->hover = TRUE;
    gtk_widget_queue_draw( icon->area );
    
    return FALSE;
}

gboolean icon_mouse_leave( GtkWidget* widget, GdkEvent* event, gpointer user )
{
	LAUNCHER_APP_ICON* icon=(LAUNCHER_APP_ICON*)user;
    icon->hover = FALSE;
    gtk_widget_queue_draw( icon->area );
    
    return FALSE;
}

gboolean icon_button_release( GtkWidget* widget,GdkEvent* event,gpointer user )
{
	LAUNCHER_APP_ICON* icon=(LAUNCHER_APP_ICON*)user;

    char buffer[250];
    sprintf(buffer,"%s &",icon->launch_command);
    system(buffer);
    gtk_main_quit();

    return FALSE;
}

double icon_layout_x;
double icon_layout_y;
double icon_rows;
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

    icon->area = gtk_drawing_area_new();
    gtk_widget_set_size_request( icon->area, icon_layout_stride_x, icon_layout_stride_y );
    gtk_widget_add_events( icon->area, GDK_POINTER_MOTION_MASK 
            | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK 
            | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK );
    g_signal_connect( G_OBJECT( icon->area ), "draw", G_CALLBACK( draw_app_icon ), (void*)icon );
    g_signal_connect( G_OBJECT( icon->area ), "enter-notify-event", G_CALLBACK(icon_mouse_enter), (void*)icon );
    g_signal_connect( G_OBJECT( icon->area ), "leave-notify-event", G_CALLBACK(icon_mouse_leave), (void*)icon );
    g_signal_connect( G_OBJECT( icon->area ), "button-release-event", G_CALLBACK(icon_button_release), (void*)icon );

    pango_layout_set_alignment( icon->layout, PANGO_ALIGN_CENTER );
    pango_layout_set_width( icon->layout, (int)(ICON_SIZE*0.99) * PANGO_SCALE );
	pango_layout_set_text( icon->layout, icon->name, strlen(icon->name) );
    pango_layout_get_pixel_size( icon->layout, &icon->layout_width, NULL );
	
	icon_layout_x+=icon_layout_stride_x;
	if(icon_layout_x>icon_layout_max_x) 
	{
		icon_layout_x=icon_layout_margin;
		icon_layout_y+=icon_layout_stride_y;
        icon_rows++;
	}
    
    return 0;
}

GFunc layout_container_attach( gpointer data, gpointer user )
{
	LAUNCHER_APP_ICON* icon=(LAUNCHER_APP_ICON*)data;
    GtkWidget* container = (GtkWidget*)user;

    gtk_fixed_put( GTK_FIXED( container ), icon->area, icon->x - ICON_SIZE/8, icon->y - ICON_SIZE/8 );

    printf( "Attached" );

    return 0;
}

//This is a really horrible naming convention change it
void do_icon_layout()
{
    icon_rows = 1;

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
	icon_layout_y=icon_layout_margin;
	//icon_layout_y=ICON_SIZE*1.5;
	
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


//CLEANUP
//gboolean changedstate;
//
//gboolean icon_mouse_move( GtkWidget* widget, GdkEvent* event, gpointer user )
//GFunc icon_check_hover(gpointer data,gpointer user)
//{
//	GdkEventMotion* ev=(GdkEventMotion*)event;
//
//	if(ev->x > icon->x && ev->x < icon->x+ICON_SIZE &&
//		ev->y > icon->y && ev->y < icon->y+ICON_SIZE)
//	{
//		if(icon->hover==FALSE) changedstate=TRUE;
//		icon->hover=TRUE;
//	}
//	else
//	{
//		if(icon->hover==TRUE) changedstate=TRUE;
//		icon->hover=FALSE;
//	}
//
//    return 0;
//}

//gboolean icon_mouse_move(GtkWidget* widget,GdkEvent* event,gpointer user)
//{
//	changedstate=FALSE;
//
//	g_list_foreach(icon_list,(GFunc)icon_check_hover,(gpointer)&event->motion);
//
//	if( changedstate ) 
//        gtk_widget_queue_draw( icon_grid );
//
//    return FALSE;
//}

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

//Strip out arguments from exec strings
void exec_strip_chars( gchar* exec )
{
    int i, j;
    int strip = 0;
    gchar* out = exec;
    for( i = 0; i < strlen( exec ); i++ )
    {
        if( exec[i] == '%' )
            strip = 1;

        if( exec[i] == ' ' )
            strip = 0;

        if( !strip )
        {
            out[j] = exec[i];
            j++;
        }
    }
    out[j] = '\0';
}

void parse_desktop_entry(const char* name)
{
    //TODO: Make sure to strip out arguments from Exec strings
    char buffer[256];
    GKeyFile* key_file = g_key_file_new();

    sprintf( buffer, "/usr/share/applications/%s", name );
    printf( "%s\n", buffer );
    if( !g_key_file_load_from_file( key_file, buffer, G_KEY_FILE_NONE, NULL ) )
    {
        return;
    }

    gchar* app_name = g_key_file_get_string( key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME, NULL );
    gchar* icon_name = g_key_file_get_string( key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, NULL );
    gchar* exec = g_key_file_get_string( key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL );
    exec_strip_chars( exec );


    printf( "%s, %s, %s\n", app_name, icon_name, exec );
    add_app_icon(app_name,icon_name,exec);
}
	
//Load all the desktop entries in the system
void load_desktop_entries()
{
    DIR* dir=opendir( "/usr/share/applications" );
	if(dir==NULL) return;
	
	struct dirent* entry=readdir(dir);
	while(entry!=NULL)
	{
		if( entry->d_type == 0x8 ) //only deal with files
		{			
			int len=strlen(entry->d_name);
			if( strcmp(entry->d_name+len-8,".desktop") == 0 )
			{
                parse_desktop_entry( entry->d_name );
			}
		}
		
		entry=readdir(dir);
	}

}

void do_layout_fullscreen()
{
    GdkMonitor* mon = gdk_display_get_primary_monitor( gdk_display_get_default() );

    GdkRectangle mon_geom;
    gdk_monitor_get_geometry( mon, &mon_geom );

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

    window = eric_window_create( WINDOW_WIDTH, WINDOW_HEIGHT, "" );;

    if(windowed)
       do_layout_windowed(w*window->interface_scale,h*window->interface_scale);
    else
       do_layout_fullscreen();

    printf( "%f, %f\n", WINDOW_WIDTH, WINDOW_HEIGHT );
    
    gtk_widget_add_events( window->window, GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK );
    g_signal_connect( G_OBJECT( window->window ), "key-press-event", G_CALLBACK(window_key_press), NULL);
    gtk_widget_set_size_request(window->window,WINDOW_WIDTH,WINDOW_HEIGHT);

    ICON_SIZE = SCALE_VALUE(ICON_SIZE);

    pango_context = gtk_widget_create_pango_context( window->window );
    sprintf( fontdesc, "Source Sans Pro Regular %ipx", MAX( 12, (int)((double)ICON_SIZE * 0.140625) ) );
    PangoFontDescription* font=pango_font_description_from_string( fontdesc );
    pango_context_set_font_description( pango_context, font );

    icon_list=NULL;
    //TODO: load custom file here...
    parse_desktop_entry( "chromium.desktop" );
    parse_desktop_entry( "xfce4-terminal.desktop" );
    parse_desktop_entry( "libreoffice-writer.desktop" );
    parse_desktop_entry( "libreoffice-calc.desktop" );
    parse_desktop_entry( "Thunar.desktop" );
    parse_desktop_entry( "steam.desktop" );
    parse_desktop_entry( "xfce-settings-manager.desktop" );
    parse_desktop_entry( "gnome-system-monitor.desktop" );
    
    //TODO: load all desktop entries
    load_desktop_entries();

    add_app_icon("WhatsApp","chromium","chromium --app=\"https://web.whatsapp.com\"");
    add_app_icon("Notes","accessories-text-editor-symbolic","chromium --app=\"http://24.3.110.119/editor\"");
    add_app_icon("Outlook","outlook","chromium --app=\"https://owa.linkcorp.com/owa\"");
    
    do_icon_layout();

    gtk_window_set_icon( GTK_WINDOW( window->window ), NULL );
    gtk_window_set_type_hint(GTK_WINDOW( window->window ), GDK_WINDOW_TYPE_HINT_DIALOG );
    gtk_window_set_decorated(GTK_WINDOW( window->window ), FALSE );
    gtk_window_set_resizable(GTK_WINDOW( window->window ), FALSE );


    //Layout widgets
    entry=gtk_entry_new();
    gtk_widget_set_size_request(entry,WINDOW_WIDTH-ICON_SIZE/2,ICON_SIZE);
    //gtk_widget_set_app_paintable(entry, TRUE);
    g_signal_connect(G_OBJECT(entry), "draw", G_CALLBACK(draw_entry), NULL);
    g_signal_connect(G_OBJECT(entry), "focus-out-event", G_CALLBACK(entry_lose_focus), NULL);

    GtkWidget* scrollable = gtk_scrolled_window_new( NULL, NULL );
    gtk_widget_add_events( scrollable, GDK_BUTTON_RELEASE_MASK );
    gtk_widget_set_hexpand( scrollable, TRUE );
    gtk_widget_set_vexpand( scrollable, TRUE );

    //Attach icons to fixed container
    icon_grid = gtk_fixed_new();
    gtk_widget_set_size_request( icon_grid, WINDOW_WIDTH, icon_layout_y + icon_layout_stride_y );
	g_list_foreach(icon_list,(GFunc)layout_container_attach,(void*)icon_grid);
    gtk_container_add( GTK_CONTAINER( scrollable ), icon_grid );
    
    //Layout grid
	GtkWidget* layoutgrid = gtk_grid_new();
	gtk_grid_insert_row( GTK_GRID( layoutgrid ), 0 );
	gtk_grid_insert_row( GTK_GRID( layoutgrid ), 1 );
	gtk_grid_attach( GTK_GRID( layoutgrid ), scrollable, 0, 1, 1, 1 );
	gtk_grid_attach( GTK_GRID( layoutgrid ), entry, 0, 0, 1, 1 );

    gtk_container_add( GTK_CONTAINER( window->window ), layoutgrid );

    //Set entry font here
    //font=pango_font_description_from_string("Source Sans Pro Regular 24");
    //gtk_widget_override_font(entry,font);
    //pango_font_description_free(font);

    gtk_widget_show_all( window->window );
    
    gtk_window_move( GTK_WINDOW( window->window ), x, y );

    gtk_main();

    return 0;
}
