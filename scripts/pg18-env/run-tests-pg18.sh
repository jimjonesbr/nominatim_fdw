#!/bin/bash

CONTAINER_NAME=nominatim_pg18
NETWORK_NAME=pgnet
TEST_ENV_PATH=~/git/nominatim_fdw/scripts

bash $TEST_ENV_PATH/squid/deploy-proxy-env.sh

# Build and install nominatim_fdw
echo -e "\n== Building and Installing nominatim_fdw on PostgreSQL 18 ==\n"

podman exec -itw /nominatim_fdw/ $CONTAINER_NAME make uninstall 2>/dev/null || true
podman exec -itw /nominatim_fdw/ $CONTAINER_NAME make clean
podman exec -itw /nominatim_fdw/ $CONTAINER_NAME make
podman exec -itw /nominatim_fdw/ $CONTAINER_NAME make install
podman restart $CONTAINER_NAME
podman exec -itw /nominatim_fdw/ -u postgres $CONTAINER_NAME psql -d postgres \
  -c "DROP EXTENSION IF EXISTS nominatim_fdw CASCADE; CREATE EXTENSION nominatim_fdw"

# Tests that need a Nominatim server are opt-in (see the Makefile):
# INCLUDE_EXTERNAL_TESTS=1 - tests against nominatim.openstreetmap.org
# INCLUDE_LOCAL_TESTS=1    - tests through the Squid proxies deployed above
# INCLUDE_ALL_TESTS=1      - all of the above

podman exec -itw /nominatim_fdw/ $CONTAINER_NAME make PGUSER=postgres INCLUDE_ALL_TESTS=1 installcheck 
echo -e "\n== Tests completed ==\n"