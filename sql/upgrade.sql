DROP EXTENSION IF EXISTS nominatim_fdw;
CREATE EXTENSION nominatim_fdw WITH VERSION '1.0';

SELECT extversion
FROM pg_extension
WHERE extname = 'nominatim_fdw';

CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');

ALTER EXTENSION nominatim_fdw UPDATE TO '1.1';
SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';

ALTER EXTENSION nominatim_fdw UPDATE TO '1.2';
SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';

ALTER EXTENSION nominatim_fdw UPDATE TO '1.3';
SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';

ALTER EXTENSION nominatim_fdw UPDATE TO '2.0';
SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';

ALTER EXTENSION nominatim_fdw UPDATE TO '2.1';
SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';

ALTER EXTENSION nominatim_fdw UPDATE TO '2.2';
SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';

ALTER EXTENSION nominatim_fdw UPDATE TO '2.3';
SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';

/* verify functions are still callable after upgrade */
SELECT nominatim_fdw_version() IS NOT NULL;

/* the upgraded objects must match those of a fresh install */
SELECT provolatile, proisstrict
FROM pg_proc
WHERE proname = 'nominatim_fdw_version';

SELECT string_agg(attname, ',' ORDER BY attnum)
FROM pg_attribute
WHERE attrelid = 'nominatimrecord'::regclass AND attnum > 0 AND NOT attisdropped;

DROP SERVER osm CASCADE;

/*
 * An installation that reached 2.2 through the upgrade scripts released
 * with 2.2 kept nominatim_fdw_version() IMMUTABLE STRICT and 'type' ahead
 * of 'entrances'. Recreate that state and check that the update to 2.3
 * repairs it.
 */
DROP EXTENSION nominatim_fdw;
CREATE EXTENSION nominatim_fdw VERSION '2.2';
ALTER FUNCTION nominatim_fdw_version() IMMUTABLE STRICT;
ALTER TYPE nominatimrecord DROP ATTRIBUTE entrances, ADD ATTRIBUTE entrances jsonb;
ALTER EXTENSION nominatim_fdw UPDATE TO '2.3';

SELECT provolatile, proisstrict
FROM pg_proc
WHERE proname = 'nominatim_fdw_version';

SELECT string_agg(attname, ',' ORDER BY attnum)
FROM pg_attribute
WHERE attrelid = 'nominatimrecord'::regclass AND attnum > 0 AND NOT attisdropped;

ALTER EXTENSION nominatim_fdw UPDATE TO '2.4';
SELECT extversion FROM pg_extension WHERE extname = 'nominatim_fdw';