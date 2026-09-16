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
