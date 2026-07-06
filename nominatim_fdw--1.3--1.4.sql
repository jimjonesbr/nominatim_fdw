/* Fix incorrect IMMUTABLE volatility on functions that make HTTP requests */
ALTER FUNCTION nominatim_search(text, text, text, text, text, text, text, text, text, boolean, boolean, boolean, text, text, text, text, text, text, text, boolean, double precision, text, boolean, int, int) VOLATILE;
--ALTER FUNCTION nominatim_lookup(text, text, boolean, boolean, boolean, text, text, text, text, text, text, text, boolean, double precision, text, boolean) VOLATILE;
ALTER FUNCTION nominatim_reverse(text, double precision, double precision, int, text, boolean, boolean, boolean, text, text) VOLATILE;

/* drop unused attribute */
ALTER TYPE NominatimRecord DROP ATTRIBUTE display_rank;

ALTER TYPE NominatimRecord ADD ATTRIBUTE type text;
ALTER TYPE NominatimRecord ADD ATTRIBUTE entrances jsonb;
ALTER TYPE NominatimReverseGeocode ADD ATTRIBUTE entrances jsonb;

/* removed unsed parameters and added entrances */
DROP FUNCTION nominatim_lookup;
CREATE FUNCTION nominatim_lookup(
    server_name text, 
    osm_ids text,
    extratags boolean DEFAULT false,
    addressdetails boolean DEFAULT false,
    namedetails boolean DEFAULT false,
    polygon text DEFAULT '',
    entrances int DEFAULT 0,    
    accept_language text DEFAULT '',    
    polygon_threshold double precision DEFAULT 0.0,
    email text DEFAULT '')
RETURNS SETOF NominatimRecord AS 'MODULE_PATHNAME', 'nominatim_fdw_lookup'
LANGUAGE C VOLATILE STRICT PARALLEL SAFE;

DROP FUNCTION nominatim_search;
CREATE FUNCTION nominatim_search(
    server_name text, 
    q text DEFAULT '',
    amenity text DEFAULT '',
    street text DEFAULT '', 
    city text DEFAULT '',
    county text DEFAULT '',
    state text DEFAULT '',
    country text DEFAULT '',
    postalcode text DEFAULT '',
    extratags boolean DEFAULT false,
    addressdetails boolean DEFAULT false,
    namedetails boolean DEFAULT false,
    polygon text DEFAULT '',
    accept_language text DEFAULT '',    
    countrycodes text DEFAULT '',
    layer text DEFAULT '',
    featuretype text DEFAULT '',
    exclude_place_ids text DEFAULT '',
    viewbox text DEFAULT '',
    bounded boolean DEFAULT false,
    polygon_threshold double precision DEFAULT 0.0,
    email text DEFAULT '',
    dedupe boolean DEFAULT true,
    limit_result int DEFAULT 0,
    offset_result int DEFAULT 0,
    entrances int DEFAULT 0)
RETURNS SETOF NominatimRecord AS 'MODULE_PATHNAME', 'nominatim_fdw_search'
LANGUAGE C VOLATILE STRICT PARALLEL SAFE;

DROP FUNCTION nominatim_reverse;
CREATE FUNCTION nominatim_reverse(
    server_name text, 
    lon double precision DEFAULT 0,
    lat double precision DEFAULT 0,
    zoom int DEFAULT 0,
    layer text DEFAULT '',
    extratags boolean DEFAULT false,
    addressdetails boolean DEFAULT false,
    namedetails boolean DEFAULT false,
    polygon text DEFAULT '',
    accept_language text DEFAULT '',
    entrances int DEFAULT 0)
RETURNS SETOF NominatimReverseGeocode AS 'MODULE_PATHNAME', 'nominatim_fdw_reverse'
LANGUAGE C VOLATILE STRICT PARALLEL SAFE;