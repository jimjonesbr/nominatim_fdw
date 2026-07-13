CREATE SERVER osm_proxy 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (
    url 'https://nominatim.openstreetmap.org',
    http_proxy 'http://172.19.42.100:3128');

\x

SELECT
    osm_id, osm_type, 
	class, 
    type,
	display_name,
	place_id IS NOT NULL AND place_id > 0 AS valid_place_id, 
	place_rank,
    lon, lat, boundingbox, 
	importance, 
	icon, 
	timestamp IS NOT NULL AS valid_timestamp, 
	attribution,
    querystring, 
	polygon AS geom,
	exclude_place_ids, 
	more_url,
    jsonb_pretty(entrances) AS entrances,
    jsonb_pretty(extratags) AS extratags, 
	jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails
FROM nominatim_search(
      server_name => 'osm_proxy',
      q => 'einsteinstraße 60, münster, germany',
      extratags => true,
      addressdetails => true,
      namedetails => true,
      polygon => 'polygon_text',
      email => 'jim.jones@uni-muenster.de',
      countrycodes => 'DE,BR,US',
      dedupe => true,
      exclude_place_ids => '42,73',
      viewbox => '7.6036345,51.9659397,7.6039893,51.9661584',
      polygon_threshold => 0.1,
      layer => 'address,poi',
      limit_result => 1,
      bounded => false,
      accept_language => 'de_DE,de,q=0.9',
      entrances => true);

SELECT pg_sleep(2);

DROP SERVER osm_proxy;

CREATE SERVER osm_proxy 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (
    url 'https://nominatim.openstreetmap.org',
    http_proxy 'http://172.19.42.101:3128');

CREATE USER MAPPING FOR postgres
SERVER osm_proxy OPTIONS (proxy_user 'proxyuser', proxy_password 'proxypass');

SELECT
    osm_id, osm_type, 
	class, 
    type,
	display_name,
	place_id IS NOT NULL AND place_id > 0 AS valid_place_id, 
	place_rank,
    lon, lat, boundingbox, 
	importance, 
	icon, 
	timestamp IS NOT NULL AS valid_timestamp, 
	attribution,
    querystring, 
	polygon AS geom,
	exclude_place_ids, 
	more_url,
    jsonb_pretty(entrances) AS entrances,
    jsonb_pretty(extratags) AS extratags, 
	jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails
FROM nominatim_search(
      server_name => 'osm_proxy',
      q => 'einsteinstraße 60, münster, germany',
      extratags => true,
      addressdetails => true,
      namedetails => true,
      polygon => 'polygon_text',
      email => 'jim.jones@uni-muenster.de',
      countrycodes => 'DE,BR,US',
      dedupe => true,
      exclude_place_ids => '42,73',
      viewbox => '7.6036345,51.9659397,7.6039893,51.9661584',
      polygon_threshold => 0.1,
      layer => 'address,poi',
      limit_result => 1,
      bounded => false,
      accept_language => 'de_DE,de,q=0.9',
      entrances => true);

ALTER USER MAPPING FOR postgres
SERVER osm_proxy OPTIONS (SET proxy_password 'WRONGPASS!!!');

-- This has to fail since the proxy password is wrong.
SELECT
    osm_id, osm_type, 
	class, 
    type,
	display_name,
	place_id IS NOT NULL AND place_id > 0 AS valid_place_id, 
	place_rank,
    lon, lat, boundingbox, 
	importance, 
	icon, 
	timestamp IS NOT NULL AS valid_timestamp, 
	attribution,
    querystring, 
	polygon AS geom,
	exclude_place_ids, 
	more_url,
    jsonb_pretty(entrances) AS entrances,
    jsonb_pretty(extratags) AS extratags, 
	jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails
FROM nominatim_search(
      server_name => 'osm_proxy',
      q => 'einsteinstraße 60, münster, germany',
      extratags => true,
      addressdetails => true,
      namedetails => true,
      polygon => 'polygon_text',
      email => 'jim.jones@uni-muenster.de',
      countrycodes => 'DE,BR,US',
      dedupe => true,
      exclude_place_ids => '42,73',
      viewbox => '7.6036345,51.9659397,7.6039893,51.9661584',
      polygon_threshold => 0.1,
      layer => 'address,poi',
      limit_result => 1,
      bounded => false,
      accept_language => 'de_DE,de,q=0.9',
      entrances => true);