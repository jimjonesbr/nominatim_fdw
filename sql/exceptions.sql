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
         max_response_size '');

CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.im',
         max_response_size '-1');

CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.im',
         max_response_size 'abc');

CREATE SERVER foo
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.im',
         max_response_size '1.5');

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
OPTIONS (url 'http://server.invalid',
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
OPTIONS (url 'http://server.invalid', 
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

/*
 * nominatim_reverse() coordinate validation, including values every
 * comparison is false for (NaN). The helper reports the SQLSTATE and the
 * message up to the offending value, which older releases print as 'nan'
 * or 'inf' rather than 'NaN' or 'Infinity'. Accepted coordinates reach the
 * request, which fails on a host that never resolves (RFC 2606) - proof
 * that validation let them through.
 */
CREATE SERVER regress_coord_srv
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.invalid', max_connect_retry '0');

CREATE FUNCTION regress_reverse_check(p_lon double precision, p_lat double precision)
RETURNS text LANGUAGE plpgsql AS $$
BEGIN
    PERFORM * FROM nominatim_reverse(server_name => 'regress_coord_srv',
                                     lon => p_lon, lat => p_lat);
    RETURN 'accepted';
EXCEPTION WHEN OTHERS THEN
    RETURN SQLSTATE || ': ' || split_part(SQLERRM, ':', 1);
END
$$;

SELECT lon, lat, regress_reverse_check(lon, lat) AS result
FROM (VALUES
        /* not a number */
        (7.6::float8,       'NaN'::float8),
        ('NaN',             51.9),
        ('NaN',             'NaN'),
        /* infinite */
        (7.6,               'Infinity'),
        (7.6,               '-Infinity'),
        ('Infinity',        51.9),
        ('-Infinity',       51.9),
        /* just out of range */
        (7.6,               90.0000001),
        (7.6,               -90.0000001),
        (180.0000001,       51.9),
        (-180.0000001,      51.9),
        /* valid, including the boundaries */
        (0,                 0),
        (180,               90),
        (-180,              -90)
     ) AS t(lon, lat);

DROP FUNCTION regress_reverse_check(double precision, double precision);
DROP SERVER regress_coord_srv;

/*
 * The 'url' option only accepts http and https, since requests are
 * restricted to those two protocols anyway. Terse verbosity: depending on
 * the libcurl build, an unsupported scheme is refused either by the scheme
 * check or already by the URL parser, with a different DETAIL.
 */
\set VERBOSITY terse
/* one server name per case, so that a regression fails each case on its own */
CREATE SERVER regress_url_file FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'file:///etc/passwd');
CREATE SERVER regress_url_ftp FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'ftp://server.invalid');
CREATE SERVER regress_url_gopher FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'gopher://server.invalid');
CREATE SERVER regress_url_noscheme FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'server.invalid');

/* ALTER SERVER is checked the same way */
CREATE SERVER regress_url_alter FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.invalid');
ALTER SERVER regress_url_alter OPTIONS (SET url 'ftp://server.invalid');
SELECT srvoptions FROM pg_foreign_server WHERE srvname = 'regress_url_alter';
DROP SERVER regress_url_alter;
\set VERBOSITY default

/* http and https are accepted, whatever the case of the scheme */
CREATE SERVER regress_url_http FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.invalid');
CREATE SERVER regress_url_https FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'https://server.invalid');
CREATE SERVER regress_url_upper FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'HTTPS://server.invalid');
CREATE SERVER regress_url_port_path FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://server.invalid:8080/nominatim/');
CREATE SERVER regress_url_query FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'https://server.invalid/?key=abc');
SELECT srvname, srvoptions FROM pg_foreign_server
WHERE srvname LIKE 'regress\_url\_%' ORDER BY srvname;
DROP SERVER regress_url_http;
DROP SERVER regress_url_https;
DROP SERVER regress_url_upper;
DROP SERVER regress_url_port_path;
DROP SERVER regress_url_query;
