/* missing required url */
CREATE SERVER bad1 FOREIGN DATA WRAPPER nominatim_fdw;

/* invalid url */
CREATE SERVER bad2 FOREIGN DATA WRAPPER nominatim_fdw OPTIONS (url 'not a url');

/* empty option value */
CREATE SERVER bad3 FOREIGN DATA WRAPPER nominatim_fdw OPTIONS (url '');

/* invalid connect_timeout (non-integer / negative) */
CREATE SERVER bad4 FOREIGN DATA WRAPPER nominatim_fdw 
  OPTIONS (url 'https://x.org', connect_timeout 'abc');

CREATE SERVER bad5 FOREIGN DATA WRAPPER nominatim_fdw 
  OPTIONS (url 'https://x.org', connect_timeout '-5');

/* unknown option */
CREATE SERVER bad6 FOREIGN DATA WRAPPER nominatim_fdw 
  OPTIONS (url 'https://x.org', bogus_option 'x');

/* FOREIGN TABLE explicitly unsupported */
CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');

CREATE FOREIGN TABLE ft (x int) SERVER osm;

ALTER SERVER osm OPTIONS (ADD connect_timeout '60');
\des+
ALTER SERVER osm OPTIONS (SET connect_timeout '120');
\des+
ALTER SERVER osm OPTIONS (DROP connect_timeout);
\des+

/* clean up */
DROP SERVER osm;