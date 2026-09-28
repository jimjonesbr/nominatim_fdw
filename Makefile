MODULE_big = nominatim_fdw
OBJS = nominatim_fdw.o
EXTENSION = nominatim_fdw
DATA = nominatim_fdw--2.4.sql \
       nominatim_fdw--2.3--2.4.sql \
       nominatim_fdw--2.2--2.3.sql \
	   nominatim_fdw--2.1--2.2.sql \
	   nominatim_fdw--2.0--2.1.sql \
	   nominatim_fdw--1.3--2.0.sql \
	   nominatim_fdw--1.2--1.3.sql \
	   nominatim_fdw--1.1--1.2.sql \
	   nominatim_fdw--1.0--1.1.sql \
	   nominatim_fdw--1.0.sql

REGRESS = create-extension version upgrade create-user-mapping create-server permissions exceptions

#
# The tests above need nothing but a PostgreSQL server, and are the ones that
# run by default - package builds, for instance, have no network access. The
# groups below need a Nominatim server to talk to, so they are opt-in:
#
#   make installcheck INCLUDE_EXTERNAL_TESTS=1  the public Nominatim instance,
#                                               nominatim.openstreetmap.org
#   make installcheck INCLUDE_LOCAL_TESTS=1     the Squid proxies deployed by
#                                               scripts/squid, which forward to
#                                               the public instance as well
#   make installcheck INCLUDE_ALL_TESTS=1       all of the above
#
ifdef INCLUDE_ALL_TESTS
  INCLUDE_EXTERNAL_TESTS = 1
  INCLUDE_LOCAL_TESTS = 1
endif

ifdef INCLUDE_EXTERNAL_TESTS
  REGRESS += functions
endif

ifdef INCLUDE_LOCAL_TESTS
  REGRESS += proxy http-auth
endif

CURL_CONFIG = curl-config
XML2_CONFIG = xml2-config
PG_CONFIG = pg_config

# Build timestamp reported by nominatim_fdw_settings(). Package builds set
# SOURCE_DATE_EPOCH, and honouring it in place of the wall clock is what keeps
# the resulting binary reproducible.
ifdef SOURCE_DATE_EPOCH
  BUILD_DATE := $(shell date -u -d "@$(SOURCE_DATE_EPOCH)" +'%Y-%m-%d %H:%M:%S UTC' 2>/dev/null || \
                        date -u -r "$(SOURCE_DATE_EPOCH)" +'%Y-%m-%d %H:%M:%S UTC' 2>/dev/null || \
                        echo "$(SOURCE_DATE_EPOCH)")
else
  BUILD_DATE := $(shell date -u +'%Y-%m-%d %H:%M:%S UTC')
endif

PG_CPPFLAGS += $(shell $(CURL_CONFIG) --cflags) \
			   $(shell $(XML2_CONFIG) --cflags) \
			   -DNOMINATIM_FDW_CC="\"$(CC)\"" \
			   -DNOMINATIM_FDW_BUILD_DATE="\"$(BUILD_DATE)\""

LIBS += $(shell $(CURL_CONFIG) --libs) \
        $(shell $(XML2_CONFIG) --libs)

SHLIB_LINK := $(LIBS)

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)