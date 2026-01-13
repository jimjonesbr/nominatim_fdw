MODULE_big = nominatim_fdw
OBJS = nominatim_fdw.o
EXTENSION = nominatim_fdw
DATA = nominatim_fdw--1.1.sql nominatim_fdw--1.0.sql nominatim_fdw--1.0--1.1.sql
REGRESS = create-extension upgrade exceptions functions 

CURL_CONFIG = curl-config
PG_CONFIG = pg_config

CFLAGS += $(shell $(CURL_CONFIG) --cflags)
LIBS += $(shell $(CURL_CONFIG) --libs)

SHLIB_LINK := $(LIBS)

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)