/*
 * The three query functions issue HTTP requests. Marking them PARALLEL SAFE
 * allowed PostgreSQL to run them inside parallel workers, so a single query
 * could fire several concurrent requests at the endpoint - something public
 * Nominatim instances explicitly ask clients not to do.
 */
ALTER FUNCTION nominatim_search(text, text, text, text, text, text, text, text, text,
                                boolean, boolean, boolean, text, text, text, text, text,
                                text, text, boolean, double precision, text, boolean,
                                int, boolean) PARALLEL RESTRICTED;

ALTER FUNCTION nominatim_lookup(text, text, boolean, boolean, boolean, text, boolean,
                                text, double precision, text) PARALLEL RESTRICTED;

ALTER FUNCTION nominatim_reverse(text, double precision, double precision, int, text,
                                 boolean, boolean, boolean, text, text, boolean,
                                 double precision, text) PARALLEL RESTRICTED;

/* purely informational, and symmetric with nominatim_fdw_version() */
ALTER FUNCTION nominatim_fdw_settings() PARALLEL SAFE;

/*
 * nominatim_fdw_version() was created IMMUTABLE STRICT in 1.0 and never
 * altered, while the fresh-install script declares it STABLE: its result
 * changes with every upgrade of the shared library, so it must not be
 * constant-folded into views or cached plans.
 */
ALTER FUNCTION nominatim_fdw_version() STABLE CALLED ON NULL INPUT;

/*
 * Before this release, nominatim_fdw--1.3--2.0.sql added the 'type'
 * attribute ahead of 'entrances', the other way round from the
 * fresh-install script. Move 'type' to the end on installations that went
 * through that path, so that all of them return the same columns.
 */
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
