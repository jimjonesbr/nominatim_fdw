CREATE EXTENSION nominatim_fdw;

CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (http_proxy 'http://proxy.im');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'bar');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://proxy.im',
         http_proxy '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'bar');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '-1');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         max_connect_retry '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         max_connect_retry '-1');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         max_connect_retry '73',
         max_connect_redirect '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         max_connect_retry '73',
         max_connect_redirect '-1');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         max_connect_retry '73',
         max_connect_redirect '');

CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         max_connect_retry '73',
         max_connect_redirect '-1');

/* invalid URL - retrying as set in 'max_connect_retry' */ 
CREATE SERVER srv
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         max_connect_retry '5',
         connect_timeout '10');
SELECT * FROM nominatim_search(server_name => 'srv', q => 'foo');

/* no retry! */
ALTER SERVER srv OPTIONS (SET max_connect_retry '0');
SELECT * FROM nominatim_search(server_name => 'srv', q => 'foo');

DROP SERVER srv;

/* server does not exist */
SELECT * FROM nominatim_search(server_name => 'foo', q => 'bar');
SELECT * FROM nominatim_search(server_name => 'foo', city => 'bar');
SELECT * FROM nominatim_reverse(server_name => 'foo',lon => '1', lat => '2');
SELECT * FROM nominatim_lookup(server_name => 'foo', osm_ids => 'W1');

CREATE SERVER srv
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im');

/* FOREIGN TABLE not supported */
CREATE FOREIGN TABLE t (osm_id bigint OPTIONS (foo 'bar'))
SERVER srv OPTIONS (foo 'bar');