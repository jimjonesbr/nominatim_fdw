/*
 * Installations upgraded from 1.x to 2.2 with the scripts released with 2.2
 * kept two differences from a fresh install: nominatim_fdw--1.0.sql created
 * nominatim_fdw_version() IMMUTABLE STRICT, and nominatim_fdw--1.3--2.0.sql
 * added NominatimRecord's 'type' attribute ahead of 'entrances'. Those
 * scripts have been corrected since, but an installation that is already at
 * 2.2 never runs them again, so the repair has to happen here as well. Both
 * statements are no-ops on installations that are already correct.
 */
ALTER FUNCTION nominatim_fdw_version() STABLE CALLED ON NULL INPUT;

DO $$
BEGIN
    IF (SELECT attnum FROM pg_attribute
        WHERE attrelid = 'NominatimRecord'::regclass AND attname = 'type' AND NOT attisdropped) <
       (SELECT attnum FROM pg_attribute
        WHERE attrelid = 'NominatimRecord'::regclass AND attname = 'entrances' AND NOT attisdropped)
    THEN
        ALTER TYPE NominatimRecord DROP ATTRIBUTE type, ADD ATTRIBUTE type text;
    END IF;
END
$$;
