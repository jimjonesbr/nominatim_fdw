POSTGRES18_IMAGE=pg18-nominatim_fdw
NOMINATIM_FDW_PATH=~/git/nominatim_fdw
PG_CONTAINER=nominatim_pg18
NETWORK_NAME=pgnet
IP_ADDRESS="172.19.42.218"

docker build -t $POSTGRES18_IMAGE .

docker stop $PG_CONTAINER
docker rm $PG_CONTAINER
docker network create --driver=bridge --subnet=172.19.42.0/24 $NETWORK_NAME
docker run -d \
  --name   $PG_CONTAINER \
  --network $NETWORK_NAME \
  --env POSTGRES_HOST_AUTH_METHOD=trust \
  --ip $IP_ADDRESS \
  --volume  $NOMINATIM_FDW_PATH:/nominatim_fdw:Z \
  $POSTGRES18_IMAGE -c logging_collector=on &&

docker exec -itw /nominatim_fdw/ $PG_CONTAINER make clean &&
docker exec -itw /nominatim_fdw/ $PG_CONTAINER make PG_CONFIG=/usr/lib/postgresql/18/bin/pg_config &&
docker exec -itw /nominatim_fdw/ $PG_CONTAINER make install