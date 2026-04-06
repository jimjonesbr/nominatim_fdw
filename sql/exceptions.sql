CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw;

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'bar');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://proxy.im');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         connect_timeout '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         connect_timeout '-1');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         connect_timeout '42',
         max_connect_retry '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         connect_timeout '42',
         max_connect_retry '-1');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         connect_timeout '42',
         max_connect_retry '73',
         max_connect_redirect '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         connect_timeout '42',
         max_connect_retry '73',
         max_connect_redirect '-1');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         connect_timeout '42',
         max_connect_retry '73',
         max_connect_redirect '');

CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         connect_timeout '42',
         max_connect_retry '73',
         max_connect_redirect '-1');

/* invalid URL - retrying as set in 'max_connect_retry' */ 
CREATE SERVER srv
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         max_connect_retry '2',
         connect_timeout '1');
SELECT * FROM nominatim_search(server_name => 'srv', q => 'foo');

/* no retry! */
ALTER SERVER srv OPTIONS (SET max_connect_retry '0');
SELECT * FROM nominatim_search(server_name => 'srv', q => 'foo');

DROP SERVER srv;

/* server does not exist */
SELECT * FROM nominatim_search(server_name => 'srv', q => 'bar');
SELECT * FROM nominatim_search(server_name => 'srv', city => 'bar');
SELECT * FROM nominatim_reverse(server_name => 'srv',lon => '1', lat => '2');
SELECT * FROM nominatim_lookup(server_name => 'srv', osm_ids => 'W1');

CREATE SERVER srv
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im', 
         connect_timeout '1', 
         max_connect_retry '0');

/* bad request: 'q' and 'amenity' cannot be combined */
SELECT * FROM nominatim_search(server_name => 'srv',  q => 'foo', amenity => 'bar');

/* bad request: nothing to search for */
SELECT * FROM nominatim_search(server_name => 'srv');

/* bad request: invalid layer */
SELECT * FROM nominatim_search(server_name => 'srv',  q => 'foo', layer => 'bar');

/* FOREIGN TABLE not supported */
CREATE FOREIGN TABLE t (osm_id bigint OPTIONS (foo 'bar'))
SERVER srv OPTIONS (foo 'bar');

/* invalid user mapping options */
CREATE USER MAPPING FOR postgres SERVER srv OPTIONS (foo 'bar');
CREATE USER MAPPING FOR postgres SERVER srv OPTIONS (proxy_user 'u1', proxy_password '');
CREATE USER MAPPING FOR postgres SERVER srv OPTIONS (proxy_user '', proxy_password 'pw1');
CREATE USER MAPPING FOR postgres SERVER srv OPTIONS (proxy_user '', proxy_password '');
