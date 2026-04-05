SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';

CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');

ALTER EXTENSION nominatim_fdw UPDATE TO '1.1';
SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';

ALTER EXTENSION nominatim_fdw UPDATE TO '1.2';
SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';

/* verify functions are still callable after upgrade */
SELECT nominatim_fdw_version() IS NOT NULL;

DROP SERVER osm CASCADE;