CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');

CREATE USER MAPPING FOR foo SERVER osm 
  OPTIONS (proxy_user 'u', proxy_password 'p');

CREATE USER MAPPING FOR CURRENT_USER SERVER osm 
  OPTIONS (proxy_user 'u', proxy_password 'p');
\deu+

ALTER USER MAPPING FOR CURRENT_USER SERVER osm
  OPTIONS (SET proxy_password 'p2');
\deu+

/* unknown user mapping option */
DROP USER MAPPING FOR CURRENT_USER SERVER osm;
CREATE USER MAPPING FOR CURRENT_USER SERVER osm
  OPTIONS (bogus 'x');

/* clean up */
DROP SERVER osm;