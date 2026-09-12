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

/*
 * The statement above is the only one in this block that succeeds. Drop the
 * server again, otherwise every CREATE SERVER below fails with 'server "foo"
 * already exists' before the option validator is ever reached - which would
 * silently turn all of the option checks that follow into no-ops.
 */
DROP SERVER foo;

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
         connect_timeout 'abc');

CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.im',
         request_timeout '');

CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.im',
         request_timeout '-1');

CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.im',
         request_timeout 'abc');

CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.im',
         request_timeout '42 ');

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

/* valid timeout values - '0' disables the request timeout */
CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.im',
         connect_timeout '42',
         request_timeout '0');
SELECT srvoptions FROM pg_foreign_server WHERE srvname = 'foo';

ALTER SERVER foo OPTIONS (SET request_timeout '30');
SELECT srvoptions FROM pg_foreign_server WHERE srvname = 'foo';

/* invalid values must be rejected by ALTER SERVER as well */
ALTER SERVER foo OPTIONS (SET request_timeout '-1');
ALTER SERVER foo OPTIONS (SET request_timeout '');

ALTER SERVER foo OPTIONS (DROP request_timeout);
SELECT srvoptions FROM pg_foreign_server WHERE srvname = 'foo';

DROP SERVER foo;

/* invalid URL - retrying as set in 'max_connect_retry' */
CREATE SERVER srv
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         max_connect_retry '2',
         connect_timeout '15');
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
         connect_timeout '15', 
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
