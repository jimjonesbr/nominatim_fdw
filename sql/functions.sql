CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');

SELECT nominatim_fdw_version() IS NOT NULL, 
       nominatim_fdw_version() <> '';

\x

SELECT
    osm_id, osm_type, 
	class, 
    type,
	display_name IS NOT NULL AND display_name <> '' valid_display_name,
	place_id IS NOT NULL AND place_id > 0 AS valid_place_id, 
	place_rank,
    lon, lat, boundingbox, 
	importance, 
	icon, 
	timestamp IS NOT NULL AS valid_timestamp, 
	attribution,
    querystring, 
	polygon AS geom,
	exclude_place_ids IS NOT NULL AS valid_exclude_place_ids, 
	more_url IS NOT NULL AND more_url <> '' AS valid_more_url,
    jsonb_pretty(entrances) AS entrances,
    jsonb_pretty(extratags) AS extratags, 
	jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails
FROM nominatim_search(
      server_name => 'osm',
      q => 'einsteinstraße 60, münster, germany',
      extratags => true,
      addressdetails => true,
      namedetails => true,
      polygon => 'polygon_text',
      email => 'jim.jones@uni-muenster.de',
      countrycodes => 'DE,BR,US',
      featuretype => 'poi',
      dedupe => true,
      exclude_place_ids => '42,73',
      viewbox => '51.9659397,51.9661584,7.6036345,7.6039893',
      polygon_threshold => 0.1,
      layer => 'address,poi',
      limit_result => 1,
      bounded => false,
      accept_language => 'de_DE,de,q=0.9',
      entrances => true);

SELECT pg_sleep(2);

/* unknown address */
SELECT *
FROM nominatim_search(
      server_name => 'osm',
      q => 'foo 42, bar');

SELECT 
    osm_id, osm_type, 
	class,
    type,
	display_name IS NOT NULL AND display_name <> '' valid_display_name,
	place_id IS NOT NULL AND place_id > 0 AS valid_place_id, 
	place_rank,
    lon, lat, boundingbox, 
	importance, 
	icon, 
	timestamp IS NOT NULL AS valid_timestamp, 
	attribution,
    querystring, 
	polygon AS geom,
	exclude_place_ids IS NOT NULL AS valid_exclude_place_ids, 
	more_url IS NOT NULL AND more_url <> '' AS valid_more_url,
    jsonb_pretty(entrances) AS entrances, 
    jsonb_pretty(extratags) AS extratags, 
	jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails
FROM nominatim_search(
      server_name => 'osm',
      amenity => 'cit',
      polygon => 'polygon_text',
      street => 'einsteinstraße 60',
      city => 'münster',
      state => 'nordrhein westfalen',
      country => 'germany',
      postalcode => '48149',
      email => 'jim.jones@uni-muenster.de',
      countrycodes => 'DE,BR,US',
      featuretype => 'office',
      dedupe => true,
      exclude_place_ids => '42, 73',
      viewbox => '51.9659397,51.9661584,7.6036345,7.6039893',
      polygon_threshold => 0.1,
      layer => 'address',
      limit_result => 1,
      accept_language => 'de_DE,de,q=0.9');

/* q combined with structured params => should error */
SELECT * FROM nominatim_search(server_name => 'osm', q => 'foo', city => 'münster');

/* nothing to search for => should error */
SELECT * FROM nominatim_search(server_name => 'osm');

/* invalid polygon type */
SELECT * FROM nominatim_search(server_name => 'osm', q => 'x', polygon => 'polygon_foo');

/* invalid layer */
SELECT * FROM nominatim_search(server_name => 'osm', q => 'x', layer => 'address,bogus');

/* nonexistent server */
SELECT * FROM nominatim_reverse(server_name => 'does_not_exist', lon => 0, lat => 0);

SELECT pg_sleep(2);

SELECT count(*) <= 3 AS respects_limit
FROM nominatim_search(server_name => 'osm', q => 'münster', limit_result => 3);

SELECT pg_sleep(2);

/* nominatim reverse */

SELECT 
    osm_id > 0 AS valid_osm_id, 
	osm_type IS NOT NULL AND osm_type <> '' AS valid_osm_type, 
	result IS NOT NULL AND result <> '' AS valid_result,
	place_id IS NOT NULL AND place_id > 0 AS valid_place_id, 
	place_rank, 
	lon > 0 AS valid_lon, lat > 0 AS valid_lat, 
	array_length(string_to_array(boundingbox,','),1) = 4 AS valid_bbox, 
    icon, 
	timestamp IS NOT NULL AS valid_timestamp, 
	attribution IS NOT NULL AND attribution <> '' AS valid_attribution,  
	querystring, 
    polygon AS geom,
    jsonb_pretty(entrances) AS entrances, 
	jsonb_pretty(extratags) AS extratags, 
    jsonb_pretty(namedetails) AS namedetails, 
	jsonb_pretty(addressparts) AS addressparts
FROM nominatim_reverse(
        server_name => 'osm', 
        lon => 7.6038115,
        lat => 51.9660873,        
        polygon => 'polygon_text',
        extratags => true,
        addressdetails => true,
        namedetails => true,
        accept_language => 'de_DE,de,q=0.9',
        zoom => 18,
        entrances => true);

SELECT pg_sleep(2);

/* invalid coordinates */
SELECT osm_id, result, boundingbox
FROM nominatim_reverse(
        server_name => 'osm', 
        lon => 4200.37,
        lat => 9999.99,
        extratags => true);

SELECT pg_sleep(2);

/* longitude out of range */
SELECT *
FROM nominatim_reverse(
        server_name => 'osm',
        lon => 200,
        lat => 50);

SELECT pg_sleep(2);

/* boundary values must be ACCEPTED (inclusive endpoints) */
SELECT osm_id IS NOT NULL
FROM nominatim_reverse(
    server_name => 'osm',
    lon => 180,
    lat => 90);

SELECT pg_sleep(2);

SELECT osm_id IS NOT NULL
FROM nominatim_reverse(
        server_name => 'osm',
        lon => -180,
        lat => -90);

SELECT pg_sleep(2);

/* valid coordinates but no location */
SELECT osm_id, result
FROM nominatim_reverse(
        server_name => 'osm', lon => 0, lat => -60);

SELECT pg_sleep(2);

SELECT osm_id, result
FROM nominatim_reverse(
        server_name => 'osm', lon => 0, lat => 0);

SELECT pg_sleep(2);

SELECT count(*) 
FROM nominatim_lookup(
        server_name => 'osm',
        osm_ids => 'W88291927,R62591');

SELECT pg_sleep(2);

/* nominatim lookup */

SELECT 
    osm_id, osm_type, 
	class,
    type,
	display_name IS NOT NULL AND display_name <> '' valid_display_name,
	place_id IS NOT NULL AND place_id > 0 AS valid_place_id, 
	place_rank,
    lon, lat, boundingbox, 
	importance, 
	icon, 
	timestamp IS NOT NULL AS valid_timestamp, 
	attribution,
    querystring, 
	polygon AS geom, 
    jsonb_pretty(entrances) AS entrances,
    jsonb_pretty(extratags) AS extratags, 
	jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails
FROM nominatim_lookup(
      server_name => 'osm',
      osm_ids => 'W88291927',
      extratags => true,
      addressdetails => true,
      namedetails => true,
      entrances => true,
      polygon => 'polygon_text',
      email => 'jim.jones@uni-muenster.de',
      polygon_threshold => 0.1,
      accept_language => 'de_DE,de,q=0.9');

/* must error, since osm_ids is required */
SELECT * FROM nominatim_lookup(server_name => 'osm', osm_ids => '');

/* returns 0 rows (STRICT function) */
SELECT * FROM nominatim_lookup(server_name => 'osm', osm_ids => NULL);