#!/bin/bash

CONTAINER_NAME=nominatim_pg18
NETWORK_NAME=pgnet

# Build and install nominatim_fdw
echo -e "\n== Building and Installing nominatim_fdw on PostgreSQL 18 ==\n"

docker exec -itw /nominatim_fdw/ $CONTAINER_NAME make uninstall 2>/dev/null || true
docker exec -itw /nominatim_fdw/ $CONTAINER_NAME make clean
docker exec -itw /nominatim_fdw/ $CONTAINER_NAME make
docker exec -itw /nominatim_fdw/ $CONTAINER_NAME make install
docker restart $CONTAINER_NAME
docker exec -itw /nominatim_fdw/ -u postgres $CONTAINER_NAME psql -d postgres \
  -c "DROP EXTENSION IF EXISTS nominatim_fdw CASCADE; CREATE EXTENSION nominatim_fdw"

# SKIP_PROXY_TESTS=1 - skip proxy tests since we don't have a proxy set up in this environment

docker exec -itw /nominatim_fdw/ $CONTAINER_NAME make PGUSER=postgres installcheck 
echo -e "\n== Tests completed ==\n"