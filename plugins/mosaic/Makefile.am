PLUGIN = ccm-mosaic
PLUGIN_UI = ccm-mosaic.ui mosaic.svg
PLUGIN_SCHEMA = ccm-mosaic.schema-key.in

PLUGIN_SOURCES = ccm-mosaic.vala

include $(top_srcdir)/build/plugin.am

plugin_LTLIBRARIES = libccm_mosaic.la 

libccm_mosaic_la_SOURCES = $(PLUGIN_SOURCES)

libccm_mosaic_la_LDFLAGS =  $(PLUGIN_LDFLAGS)

libccm_mosaic_la_LIBADD =  $(PLUGIN_LIBADD)