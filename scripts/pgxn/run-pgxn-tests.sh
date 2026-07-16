#!/bin/bash

CODEPATH="/home/jim/git/nominatim_fdw"
PGVERSIONS="10,11,12,13,14,15,16,17,18,19"
#PGVERSIONS="10"
IMAGENAME="pgxn-image"
NETWORK_NAME="pgnet"
TEST_ENV_PATH=~/git/nominatim_fdw/scripts/postgres-env

# builds a custom pgxn image with extension dependencies (libxml2 and libcurl)
echo -e "\n== Building PGXN podman Image ==\n"
podman build --tag $IMAGENAME . &&

# create a custom podman network for postgres and fuseki containers to communicate
podman network create --driver=bridge --subnet=172.19.42.0/24 $NETWORK_NAME

# keeps things clean
make -C $CODEPATH clean &&
reset &&

IFS=',' read -ra version <<< "$PGVERSIONS" &&
for pgv in "${version[@]}"; 
do
    bash $TEST_ENV_PATH/squid/deploy-proxy-env.sh
    podman run \
        --network $NETWORK_NAME \
        --no-hosts \
        -itw /ext --rm \
        --volume "$CODEPATH:/ext:z" $IMAGENAME sh -c "pg-start $pgv && pg-build-test && make clean" &&
    
    echo -e "\n\n== Tests finished for PostgreSQL $pgv ==\n\n"    
done

#make -C $CODEPATH clean
