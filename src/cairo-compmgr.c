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
#include <gnome.h>

#include "ccm.h"
#include "ccm-debug.h"
#include "ccm-tray-icon.h"
#include "ccm-extension-loader.h"

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
session_die (GnomeClient *client, gpointer client_data)
{
	gtk_main_quit ();
}

static void
log_func(const gchar *log_domain, GLogLevelFlags log_level,
		 const gchar *message, gpointer user_data)
{
	ccm_log(message);
	ccm_log_print_backtrace();
}

int
main(gint argc, gchar **argv)
{
	CCMTrayIcon* trayicon;
	GnomeClient* client;
	
#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	signal(SIGSEGV, crash);
	gnome_program_init (PACKAGE_NAME, VERSION,
						LIBGNOMEUI_MODULE, argc, argv,
                        GNOME_PARAM_NONE);
	
	g_log_set_default_handler(log_func, NULL);
	client = gnome_master_client();
	gnome_client_set_restart_style(client, GNOME_RESTART_IF_RUNNING);
	g_signal_connect (client, "save_yourself", G_CALLBACK (gtk_true), NULL);
	g_signal_connect (client, "die", G_CALLBACK (session_die), NULL);
	
	ccm_extension_loader_add_plugin_path(PACKAGE_PLUGIN_DIR);
	
	trayicon = ccm_tray_icon_new ();
	gtk_main();
	g_object_unref(trayicon);
	return 0;
}
