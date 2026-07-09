/* drop unused attribute */
ALTER TYPE NominatimRecord DROP ATTRIBUTE display_rank;

/* add new attributes */
ALTER TYPE NominatimRecord ADD ATTRIBUTE type text;
ALTER TYPE NominatimRecord ADD ATTRIBUTE entrances jsonb;
ALTER TYPE NominatimReverseGeocode ADD ATTRIBUTE entrances jsonb;

/* rename attribute to make it consistant with NominatimRecord */
ALTER TYPE NominatimReverseGeocode RENAME ATTRIBUTE result TO display_name;

/* removed unsed parameters and added entrances */
DROP FUNCTION nominatim_lookup;
CREATE FUNCTION nominatim_lookup(
    server_name text, 
    osm_ids text,
    extratags boolean DEFAULT false,
    addressdetails boolean DEFAULT true,
    namedetails boolean DEFAULT false,
    polygon text DEFAULT '',
    entrances boolean DEFAULT false,    
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
    addressdetails boolean DEFAULT true,
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
    entrances boolean DEFAULT false)
RETURNS SETOF NominatimRecord AS 'MODULE_PATHNAME', 'nominatim_fdw_search'
LANGUAGE C VOLATILE STRICT PARALLEL SAFE;

DROP FUNCTION nominatim_reverse;
CREATE FUNCTION nominatim_reverse(
    server_name text, 
    lon double precision DEFAULT 0,
    lat double precision DEFAULT 0,
    zoom int DEFAULT -1,
    layer text DEFAULT '',
    extratags boolean DEFAULT false,
    addressdetails boolean DEFAULT true,
    namedetails boolean DEFAULT false,
    polygon text DEFAULT '',
    accept_language text DEFAULT '',
    entrances boolean DEFAULT false)
RETURNS SETOF NominatimReverseGeocode AS 'MODULE_PATHNAME', 'nominatim_fdw_reverse'
LANGUAGE C VOLATILE STRICT PARALLEL SAFE;