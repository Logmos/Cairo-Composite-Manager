/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2007 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with cairo-compmgr.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "ccm-drawable.h"
#include "ccm-window.h"
#include "ccm-display.h"
#include "ccm-screen.h"
#include "ccm-extension-loader.h"
#include "ccm-mosaic.h"
#include "ccm-keybind.h"
#include "ccm.h"

enum
{
	CCM_MOSAIC_SPACING,
	CCM_MOSAIC_SHORTCUT,
	CCM_MOSAIC_OPTION_N
};

static gchar* CCMMosaicOptions[CCM_MOSAIC_OPTION_N] = {
	"spacing",
	"shortcut"
};

static void ccm_mosaic_screen_iface_init(CCMScreenPluginClass* iface);
static void ccm_mosaic_window_iface_init(CCMWindowPluginClass* iface);

CCM_DEFINE_PLUGIN (CCMMosaic, ccm_mosaic, CCM_TYPE_PLUGIN, 
				   CCM_IMPLEMENT_INTERFACE(ccm_mosaic,
										   CCM_TYPE_SCREEN_PLUGIN,
										   ccm_mosaic_screen_iface_init);
				   CCM_IMPLEMENT_INTERFACE(ccm_mosaic,
										   CCM_TYPE_WINDOW_PLUGIN,
										   ccm_mosaic_window_iface_init))

typedef struct {
	cairo_rectangle_t   geometry;
	CCMWindow*			window;
} CCMMosaicArea;

struct _CCMMosaicPrivate
{	
	CCMScreen*			 screen;
	gboolean 			 enabled;
	Window				 window;
	
	int					 x_mouse;
	int 				 y_mouse;
	CCMMosaicArea*     	 areas;
	gint				 nb_areas;
	cairo_surface_t* 	 surface;
	CCMKeybind*			 keybind;
	
	CCMConfig*           options[CCM_MOSAIC_OPTION_N];
};

#define CCM_MOSAIC_GET_PRIVATE(o)  \
   ((CCMMosaicPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_MOSAIC, CCMMosaicClass))

static void
cairo_rectangle_round (cairo_t *cr,
                       double x0,    double y0,
                       double width, double height,
                       double radius)
{
  double x1,y1;

  x1=x0+width;
  y1=y0+height;

  if (!width || !height)
    return;
  if (width/2<radius)
    {
      if (height/2<radius)
        {
          cairo_move_to  (cr, x0, (y0 + y1)/2);
          cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1)/2, y0);
          cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1)/2);
          cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0)/2, y1);
          cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1)/2);
        }
      else
        {
          cairo_move_to  (cr, x0, y0 + radius);
          cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1)/2, y0);
          cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
          cairo_line_to (cr, x1 , y1 - radius);
          cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0)/2, y1);
          cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);
        }
    }
  else
    {
      if (height/2<radius)
        {
          cairo_move_to  (cr, x0, (y0 + y1)/2);
          cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
          cairo_line_to (cr, x1 - radius, y0);
          cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1)/2);
          cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
          cairo_line_to (cr, x0 + radius, y1);
          cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1)/2);
        }
      else
        {
          cairo_move_to  (cr, x0, y0 + radius);
          cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
          cairo_line_to (cr, x1 - radius, y0);
          cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
          cairo_line_to (cr, x1 , y1 - radius);
          cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
          cairo_line_to (cr, x0 + radius, y1);
          cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);
        }
    }

  cairo_close_path (cr);
}

static void
ccm_mosaic_init (CCMMosaic *self)
{
	self->priv = CCM_MOSAIC_GET_PRIVATE(self);
	self->priv->screen = NULL;
	self->priv->window = 0;
	self->priv->enabled = FALSE;
	self->priv->areas = NULL;
	self->priv->surface = NULL;
	self->priv->keybind = NULL;
}

static void
ccm_mosaic_finalize (GObject *object)
{
	CCMMosaic* self = CCM_MOSAIC(object);
	
	if (self->priv->keybind) g_object_unref(self->priv->keybind);
	if (self->priv->surface) cairo_surface_destroy (self->priv->surface);
	if (self->priv->areas) g_free(self->priv->areas);
	if (self->priv->window)
	{
		CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
		XDestroyWindow (CCM_DISPLAY_XDISPLAY(display), self->priv->window);
	}
	
	G_OBJECT_CLASS (ccm_mosaic_parent_class)->finalize (object);
}

static void
ccm_mosaic_class_init (CCMMosaicClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMMosaicPrivate));

	object_class->finalize = ccm_mosaic_finalize;
}

static void
ccm_mosaic_create_window(CCMMosaic* self)
{
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	CCMWindow* root = ccm_screen_get_root_window (self->priv->screen);
	XSetWindowAttributes attr;
	
	attr.override_redirect = True;
	self->priv->window = XCreateWindow (CCM_DISPLAY_XDISPLAY(display), 
										CCM_WINDOW_XWINDOW(root), 0, 0,
										self->priv->screen->xscreen->width,
										self->priv->screen->xscreen->height,
										0, CopyFromParent, InputOnly, 
										CopyFromParent, CWOverrideRedirect, 
										&attr);
	XMapWindow (CCM_DISPLAY_XDISPLAY(display), self->priv->window);
	XRaiseWindow (CCM_DISPLAY_XDISPLAY(display), self->priv->window);
}

static void
ccm_mosaic_on_key_press(CCMMosaic* self)
{
	self->priv->enabled = ~self->priv->enabled;
	ccm_screen_set_filtered_damage (self->priv->screen, !self->priv->enabled);
	_ccm_screen_set_buffered(self->priv->screen, !self->priv->enabled);
	if (self->priv->enabled)
	{
		self->priv->surface = cairo_image_surface_create (
										CAIRO_FORMAT_ARGB32, 
										self->priv->screen->xscreen->width, 
										self->priv->screen->xscreen->height);
		ccm_mosaic_create_window(self);
	}
	else
	{
		CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	
		cairo_surface_destroy (self->priv->surface);
		self->priv->surface = NULL;
		XDestroyWindow (CCM_DISPLAY_XDISPLAY(display), self->priv->window);
		self->priv->window = None;
	}
	ccm_screen_damage (self->priv->screen);
}

static void
ccm_mosaic_screen_load_options(CCMScreenPlugin* plugin, CCMScreen* screen)
{
	CCMMosaic* self = CCM_MOSAIC(plugin);
	gint cpt;
	
	for (cpt = 0; cpt < CCM_MOSAIC_OPTION_N; cpt++)
	{
		self->priv->options[cpt] = ccm_config_new(screen->number, "mosaic", 
												  CCMMosaicOptions[cpt]);
	}
	ccm_screen_plugin_load_options(CCM_SCREEN_PLUGIN_PARENT(plugin), screen);
	
	self->priv->screen = screen;
	self->priv->keybind = ccm_keybind_new(self->priv->screen, 
		ccm_config_get_string(self->priv->options [CCM_MOSAIC_SHORTCUT]));
	g_signal_connect_swapped(self->priv->keybind, "key_press", 
							 G_CALLBACK(ccm_mosaic_on_key_press), self);
}

static void
ccm_mosaic_cursor_get_position(CCMMosaic*self)
{
	CCMDisplay* display = ccm_screen_get_display (self->priv->screen);
	CCMWindow* root = ccm_screen_get_root_window (self->priv->screen);
	int x, y, cursor_size;
	unsigned int m;
	Window r, w;
			
	XQueryPointer (CCM_DISPLAY_XDISPLAY(display), CCM_WINDOW_XWINDOW(root),
				   &r, &w, &self->priv->x_mouse, &self->priv->y_mouse,
				   &x, &y, &m);
}

static void
ccm_mosaic_create_areas(CCMMosaic* self, gint nb_windows)
{
	g_return_if_fail(self != NULL);
	g_return_if_fail(nb_windows != 0);
	
	int i, j, lines, n;
	int x, y, width, height;
    int spacing = ccm_config_get_integer (self->priv->options [CCM_MOSAIC_SPACING]);
    
	if (self->priv->areas) g_free(self->priv->areas);
	self->priv->areas = g_new0(CCMMosaicArea, nb_windows);
    lines = sqrt (nb_windows + 1);
    self->priv->nb_areas = 0;
    
	y = spacing;
    height = (self->priv->screen->xscreen->height - (lines + 1) * spacing) / lines;

    for (i = 0; i < lines; i++)
    {
	    n = MIN (nb_windows - self->priv->nb_areas, ceilf ((float)nb_windows / lines));
    	x = spacing;
	    width = (self->priv->screen->xscreen->width - (n + 1) * spacing) / n;
        for (j = 0; j < n; j++)
	    {
	        self->priv->areas[self->priv->nb_areas].geometry.x = x;
	        self->priv->areas[self->priv->nb_areas].geometry.y = y;
            self->priv->areas[self->priv->nb_areas].geometry.width = width;
	        self->priv->areas[self->priv->nb_areas].geometry.height = height;

	        x += width + spacing;
            
	        self->priv->nb_areas++;
	    }
	    y += height + spacing;
    }
}

static CCMMosaicArea*
ccm_mosaic_find_area(CCMMosaic* self, CCMWindow* window)
{
	g_return_val_if_fail(self != NULL, NULL);
	g_return_val_if_fail(window != NULL, NULL);
	
	CCMMosaicArea* area = NULL;
	gfloat x_scale, y_scale;
	gint width_scale;
	gint cpt;
	cairo_rectangle_t geometry;
	
	if (ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(window), &geometry))
	{
		width_scale = geometry.width;
		
		for (cpt = 0; cpt < self->priv->nb_areas; cpt++)
		{
			if (!self->priv->areas[cpt].window)
			{
				y_scale = self->priv->areas[cpt].geometry.height / geometry.height;
				x_scale = self->priv->areas[cpt].geometry.width / geometry.width;
				if (abs((geometry.width * y_scale) - (geometry.width * x_scale)) < width_scale)
				{
					if (area) area->window = NULL;
					area = &self->priv->areas[cpt];
					area->window = window;
					width_scale = abs((geometry.width * y_scale) - (geometry.width * x_scale));
				}
			}
			else if (self->priv->areas[cpt].window == window)
			{
				area = &self->priv->areas[cpt];
				break;
			}
		}
	}
	
	return area;
}

static gboolean
ccm_mosaic_screen_paint(CCMScreenPlugin* plugin, CCMScreen* screen,
                        cairo_t* context)
{
	CCMMosaic* self = CCM_MOSAIC (plugin);
	gboolean ret = FALSE;
	gint nb_windows = 0;
		
	if (self->priv->enabled) 
	{
		GList* item = ccm_screen_get_windows(screen);
		
		for (;item; item = item->next)
		{
			CCMWindowType type = ccm_window_get_hint_type (item->data);
			if (CCM_WINDOW_XWINDOW(item->data) != self->priv->window &&
				((CCMWindow*)item->data)->is_viewable && 
				!((CCMWindow*)item->data)->is_input_only &&
				type != CCM_WINDOW_TYPE_DESKTOP && type != CCM_WINDOW_TYPE_DOCK) 
				nb_windows++;
		}
		if (nb_windows) ccm_mosaic_create_areas(self, nb_windows);
	}
	
	ret = ccm_screen_plugin_paint(CCM_SCREEN_PLUGIN_PARENT (plugin), screen, 
								  context);
	
	if (ret && self->priv->enabled)
	{
		cairo_rectangle_t* rects;
		gint nb_rects, cpt;
		
		cairo_save(context);
		ccm_region_get_rectangles (ccm_screen_get_damaged (screen), &rects, &nb_rects);
		for (cpt = 0; cpt < nb_rects; cpt++)
			cairo_rectangle (context, rects[cpt].x, rects[cpt].y,
							 rects[cpt].width, rects[cpt].height);
		cairo_clip (context);
		cairo_set_source_rgba (context, 0, 0, 0, 0.6);
		cairo_paint(context);
		cairo_set_source_surface (context, self->priv->surface, 0, 0);
		cairo_paint(context);
		cairo_restore (context);
	}
	
	return ret;
}

static gboolean
ccm_mosaic_window_paint(CCMWindowPlugin* plugin, CCMWindow* window,
						cairo_t* context, cairo_surface_t* surface)
{
	CCMScreen* screen = ccm_drawable_get_screen(CCM_DRAWABLE(window));
	CCMMosaic* self = CCM_MOSAIC(_ccm_screen_get_plugin (screen, CCM_TYPE_MOSAIC));
	CCMWindowType type = ccm_window_get_hint_type (window);
	gboolean ret = FALSE;
	
	ret = ccm_window_plugin_paint(CCM_WINDOW_PLUGIN_PARENT(plugin), window,
								  context, surface);
	
	if (ccm_drawable_is_damaged (CCM_DRAWABLE(window)) && self->priv->enabled)
	{
		if (CCM_WINDOW_XWINDOW(window) != self->priv->window &&
			window->is_viewable && !window->is_input_only && 
			type != CCM_WINDOW_TYPE_DESKTOP && type != CCM_WINDOW_TYPE_DOCK) 
		{
			CCMMosaicArea* area = ccm_mosaic_find_area(self, window);
			
			if (area)
			{
				gfloat scale;
				cairo_rectangle_t geometry;
				cairo_t* ctx = cairo_create (self->priv->surface);
				CCMRegion* damaged = ccm_region_rectangle (&area->geometry);
				
				scale = MIN(area->geometry.height / geometry.height,
							area->geometry.width / geometry.width);
				
				ccm_region_offset(damaged, area->geometry.width / 2,
								  area->geometry.height / 2);
				ccm_region_resize (damaged, geometry.width * scale, 
								   geometry.height * scale);
				ccm_region_offset(damaged, -(geometry.width / 2) * scale,
								  -(geometry.height / 2) * scale);
				ccm_drawable_get_geometry_clipbox (CCM_DRAWABLE(window), &geometry);
				
				cairo_translate (ctx, area->geometry.x + area->geometry.width / 2, 
								 area->geometry.y + area->geometry.height / 2);
				cairo_scale(ctx, scale, scale);
				
				cairo_set_source_rgba (ctx, 0, 0, 0, 0);
				cairo_paint (ctx);
				cairo_set_source_surface (ctx, surface, -(geometry.width / 2), 
										  -(geometry.height / 2));
				
				cairo_paint (ctx);
				
				ccm_screen_add_damaged_region (self->priv->screen, damaged);
				ccm_region_destroy (damaged);
				cairo_destroy (ctx);
			}
		}
	}
	
	return ret;
}

static void
ccm_mosaic_screen_iface_init(CCMScreenPluginClass* iface)
{
	iface->load_options 	= ccm_mosaic_screen_load_options;
	iface->paint 			= ccm_mosaic_screen_paint;
	iface->add_window 		= NULL;
	iface->remove_window 	= NULL;
}

static void
ccm_mosaic_window_iface_init(CCMWindowPluginClass* iface)
{
	iface->load_options 	= NULL;
	iface->query_geometry 	= NULL;
	iface->paint 			= ccm_mosaic_window_paint;
	iface->map				= NULL;
	iface->unmap			= NULL;
	iface->query_opacity  	= NULL;
	iface->set_opaque		= NULL;
	iface->move				= NULL;
	iface->resize			= NULL;
}
