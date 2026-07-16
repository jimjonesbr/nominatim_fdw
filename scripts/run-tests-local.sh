#!/bin/bash

CODE_PATH=~/git/nominatim_fdw
PSQL_PATH=/usr/local/postgres-dev/bin/psql
PG_CONFIG_PATH=/usr/local/postgres-dev/bin/pg_config
TEST_ENV_PATH=~/git/nominatim_fdw/scripts/postgres-env
#####################################################

# Build and install nominatim_fdw
echo -e "\n== Building and Installing nominatim_fdw on PostgreSQL (local) ==\n"

cd $CODE_PATH

make clean && \
 make PG_CONFIG=$PG_CONFIG_PATH CFLAGS="-DUSE_ASSERT_CHECKING -O0 -g" && \
 make install PG_CONFIG=$PG_CONFIG_PATH

$PSQL_PATH postgres -c "DROP EXTENSION IF EXISTS nominatim_fdw CASCADE; CREATE EXTENSION nominatim_fdw"
$PSQL_PATH postgres -c "SELECT nominatim_fdw_version();"
$PSQL_PATH postgres -c "SELECT version();"
$PSQL_PATH postgres -c "CREATE USER postgres SUPERUSER;"

make PG_CONFIG=$PG_CONFIG_PATH PGUSER=postgres SKIP_PROXY_TESTS=1 installcheck 

echo -e "\n== local deployment complete ==\n"
