/***************************************************************************
 *            cairo-compmgr.c
 *
 *  Mon Jul 23 22:35:30 2007
 *  Copyright  2007  Nicolas Bruguier
 *  <gandalfn@club-internet.fr>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <config.h>
#include <signal.h>
#include <stdlib.h>

#include "ccm.h"
#include "ccm-debug.h"
#include "ccm-config.h"
#include "ccm-tray-icon.h"
#include "ccm-extension-loader.h"

#ifdef ENABLE_GOBJECT_INTROSPECTION
#include <girepository.h>
#endif

/*
 * Standard gettext macros.
 */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

gboolean crashed = FALSE;

static void
crash(int signum)
{
	if (!crashed)
	{
		crashed = TRUE;
		ccm_log_print_backtrace();
		exit(0);
	}
}

static void
log_func(const gchar *log_domain, GLogLevelFlags log_level,
		 const gchar *message, gpointer user_data)
{
	ccm_log("");
	ccm_log(message);
	ccm_log_print_backtrace();
}

int
main(gint argc, gchar **argv)
{
	CCMTrayIcon* trayicon;
    GError* error = NULL;
    gchar* user_plugin_path = NULL;

#ifdef ENABLE_GCONF
    static gboolean use_gconf = FALSE;
#endif
#ifdef ENABLE_GOBJECT_INTROSPECTION
    static gchar* gir_output = NULL;
#endif
    
    GOptionEntry options[] = 
    {
#ifdef ENABLE_GCONF
        { "use-gconf", 'g', 0, G_OPTION_ARG_NONE, &use_gconf,
 		  N_("Force use gconf for configuration files"),
 		  NULL },
#endif
#ifdef ENABLE_GOBJECT_INTROSPECTION
		{ "introspect-dump", 'i', 0, G_OPTION_ARG_STRING, &gir_output,
 		  N_("Dump gobject introspection file"),
 		  N_("types.txt,out.xml") },
#endif
		{ NULL, '\0', 0, 0, NULL, NULL, NULL }
 	};

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif
    
	signal(SIGSEGV, crash);
    
    if (!gtk_init_with_args (&argc, &argv, NULL, options, NULL, &error)) 
    {
		g_print ("%s\n", error->message);
		return 1;
	}

#ifdef ENABLE_GOBJECT_INTROSPECTION
    if (gir_output)
    {
        if (!g_irepository_dump (gir_output, &error))
        {
            g_print ("%s\n", error->message);
            return 1;
        }
        return 0;
    }
#endif
    
	g_log_set_default_handler(log_func, NULL);

#ifdef ENABLE_GCONF
    if (use_gconf)
        ccm_config_set_backend ("gconf");
    else
#endif
        ccm_config_set_backend ("key");
    
    ccm_extension_loader_add_plugin_path(PACKAGE_PLUGIN_DIR);
    
    user_plugin_path = g_strdup_printf("%s/%s/plugins", g_get_user_data_dir(),
                                       PACKAGE);
    ccm_extension_loader_add_plugin_path (user_plugin_path);
    g_free(user_plugin_path);
    
	trayicon = ccm_tray_icon_new ();
	gtk_main();
	g_object_unref(trayicon);
	return 0;
}
