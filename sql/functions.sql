CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');

SELECT * FROM nominatim_fdw_version();

SELECT 
    osm_id, osm_type, ref, class, display_name, display_rank, place_id IS NULL, place_rank,
    lon, lat, boundingbox, importance, icon, timestamp IS NOT NULL AS ts, attribution,
    querystring, length(polygon) AS polygon_length, exclude_place_ids, more_url,
    jsonb_pretty(extratags) AS extratags, jsonb_pretty(namedetails) AS namedetails,
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
      featuretype => 'office',
      dedupe => true,
      exclude_place_ids => '42,73',
      viewbox => '51.9659397,51.9661584,7.6036345,7.6039893',
      polygon_threshold => 0.1,
      layer => 'address',
      limit_result => 1,
      accept_language => 'de_DE,de,q=0.9');

SELECT pg_sleep(2);

SELECT 
    osm_id, osm_type, ref, class, display_name, display_rank, place_id IS NULL, place_rank,
    lon, lat, boundingbox, importance, icon, timestamp IS NOT NULL AS ts, attribution,
    querystring, length(polygon) AS polygon_length, exclude_place_ids, more_url,
    jsonb_pretty(extratags) AS extratags, jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails
FROM nominatim_search(
      server_name => 'osm',
      amenity => 'wwu it',
      polygon => 'polygon_text',
      extratags => true,
      addressdetails => true,
      namedetails => true,      
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

SELECT pg_sleep(2);

SELECT 
    osm_id, osm_type, result, ref, place_id IS NULL, place_rank, lon, lat, boundingbox, 
    icon, timestamp IS NOT NULL AS ts, attribution, querystring, 
    length(polygon) AS polygon_length, jsonb_pretty(extratags) AS extratags, 
    jsonb_pretty(namedetails) AS namedetails, jsonb_pretty(addressparts) AS addressparts
FROM nominatim_reverse(
        server_name => 'osm', 
        lon => 7.6038115,
        lat => 51.9660873,        
        polygon => 'polygon_text',
        extratags => true,
        addressdetails => true,
        namedetails => true,
        accept_language => 'de_DE,de,q=0.9');

SELECT pg_sleep(2);

SELECT 
    osm_id, osm_type, ref, class, display_name, display_rank, place_id IS NULL, place_rank,
    lon, lat, boundingbox, importance, icon, timestamp IS NOT NULL AS ts, attribution,
    querystring, length(polygon) AS polygon_length, exclude_place_ids, more_url,
    jsonb_pretty(extratags) AS extratags, jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails 
FROM nominatim_lookup(
      server_name => 'osm',
      osm_ids => 'W88291927',
      extratags => true,
      addressdetails => true,
      namedetails => true,
      polygon => 'polygon_text',
      email => 'jim.jones@uni-muenster.de',
      countrycodes => 'DE,BR,US',
      featuretype => 'office',
      dedupe => true,
      exclude_place_ids => '42,73',
      viewbox => '51.9659397,51.9661584,7.6036345,7.6039893',
      polygon_threshold => 0.1,
      layer => 'address',
      accept_language => 'de_DE,de,q=0.9');