DROP EXTENSION IF EXISTS nominatim_fdw CASCADE; 
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
         http_user '');

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
         connect_retry '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         connect_retry '-1');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         connect_retry '73',
         request_redirect '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         connect_retry '73',
         request_redirect 'foo');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         connect_retry '73',
         request_redirect 'true',
         request_max_redirect '');

CREATE SERVER foo 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'http://server.im',
         http_proxy 'http://server.im',
         proxy_user 'jim',
         proxy_user_password 'pw',
         connect_timeout '42',
         connect_retry '73',
         request_redirect 'true',
         request_max_redirect '-1');