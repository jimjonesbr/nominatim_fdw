
SET client_min_messages TO DEBUG1;

CREATE SERVER osm_http_auth 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://172.19.42.102:3128');

CREATE USER MAPPING FOR postgres
SERVER osm_http_auth OPTIONS (user 'nominatimuser', password 'nominatimpass');

/* must return HTTP 200 */
SELECT osm_id, display_name
FROM nominatim_search(
      server_name => 'osm_http_auth',
      q => 'einsteinstraße 60, münster, germany');

ALTER USER MAPPING FOR postgres
SERVER osm_http_auth OPTIONS (SET user 'foo', SET password 'bar');

/* invalid credentials, must fail (HTTP 401) */
SELECT osm_id, display_name
FROM nominatim_search(
      server_name => 'osm_http_auth',
      q => 'einsteinstraße 60, münster, germany');

DROP SERVER osm_http_auth CASCADE;