CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');

SELECT * FROM nominatim_fdw_version();

SELECT 
    osm_id, osm_type, ref, class, display_name, display_rank, place_id, place_rank,
    lon, lat, boundingbox, importance, icon, timestamp IS NOT NULL AS ts, attribution,
    querystring, length(polygon) AS polygon_length, exclude_place_ids, more_url,
    jsonb_pretty(extratags) AS extratags, jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails
FROM nominatim_query(
      server_name => 'osm',
      query => 'einsteinstraße 60,münster,germany',
      extratags => true,
      addressdetails => true,
      namedetails => true,
      polygon => 'polygon_text');

SELECT pg_sleep(2);

SELECT 
    osm_id, osm_type, ref, class, display_name, display_rank, place_id, place_rank,
    lon, lat, boundingbox, importance, icon, timestamp IS NOT NULL AS ts, attribution,
    querystring, length(polygon) AS polygon_length, exclude_place_ids, more_url,
    jsonb_pretty(extratags) AS extratags, jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails
FROM nominatim_query_structured(
      server_name => 'osm',
      query => 'wwu it',
      street => 'einsteinstraße 60',
      city => 'münster',
      state => 'nordrhein westfalen',
      country => 'germany',
      postalcode => '48149',
      polygon => 'polygon_text',
      extratags => true,
      addressdetails => true,
      namedetails => true);

SELECT pg_sleep(2);

SELECT 
    osm_id, osm_type, result, ref, place_id, place_rank, lon, lat, boundingbox, 
    icon, timestamp IS NOT NULL AS ts, attribution, querystring, 
    length(polygon) AS polygon_length, jsonb_pretty(extratags) AS extratags, 
    jsonb_pretty(namedetails) AS namedetails, jsonb_pretty(addressparts) AS addressparts
FROM nominatim_query_reverse(
        server_name => 'osm', 
        lon => 7.6038115,
        lat => 51.9660873,        
        polygon => 'polygon_text',
        extratags => true,
        addressdetails => true,
        namedetails => true);

SELECT pg_sleep(2);

SELECT 
    osm_id, osm_type, ref, class, display_name, display_rank, place_id, place_rank,
    lon, lat, boundingbox, importance, icon, timestamp IS NOT NULL AS ts, attribution,
    querystring, length(polygon) AS polygon_length, exclude_place_ids, more_url,
    jsonb_pretty(extratags) AS extratags, jsonb_pretty(namedetails) AS namedetails,
    jsonb_pretty(addressdetails) AS addressdetails 
FROM nominatim_query_lookup(
      server_name => 'osm',
      osm_ids => 'W88291927',
      polygon => 'polygon_text',
      extratags => true,
      addressdetails => true,
      namedetails => true);