CREATE FUNCTION nominatim_fdw_handler()
RETURNS fdw_handler AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION nominatim_fdw_validator(text[], oid)
RETURNS void AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION nominatim_fdw_version()
RETURNS text AS 'MODULE_PATHNAME', 'nominatim_fdw_version'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE TYPE NominatimRecord AS ( 
  osm_id bigint,  
  osm_type text, 
  ref text,
  class text,
  display_name text,
  display_rank text,
  place_id bigint,
  place_rank int,
  address_rank int,
  lon numeric,
  lat numeric,
  boundingbox text,  
  importance double precision,
  icon text,
  timestamp timestamptz,
  attribution text,
  querystring text,
  polygon text,
  exclude_place_ids text,
  more_url text,
  extratags jsonb,
  namedetails jsonb,
  addressdetails jsonb
);

CREATE TYPE NominatimReverseGeocode AS ( 
  osm_id bigint,  
  osm_type text, 
  result text,
  ref text,
  place_id bigint,
  place_rank int,
  address_rank int,
  lon numeric,
  lat numeric,
  boundingbox text,  
  icon text,
  timestamp timestamptz,
  attribution text,
  querystring text,
  polygon text,
  extratags jsonb,
  namedetails jsonb,
  addressparts jsonb
);

CREATE FUNCTION nominatim_query(
    server_name text, 
    query text,
    extratags boolean DEFAULT false,
    addressdetails boolean DEFAULT false,
    namedetails boolean DEFAULT false,
    polygon text DEFAULT '')
RETURNS SETOF NominatimRecord AS 'MODULE_PATHNAME', 'nominatim_fdw_query'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION nominatim_query_lookup(
    server_name text, 
    osm_ids text,
    extratags boolean DEFAULT false,
    addressdetails boolean DEFAULT false,
    namedetails boolean DEFAULT false,
    polygon text DEFAULT '')
RETURNS SETOF NominatimRecord AS 'MODULE_PATHNAME', 'nominatim_fdw_query_lookup'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION nominatim_query_structured(
    server_name text, 
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
    polygon text DEFAULT '')
RETURNS SETOF NominatimRecord AS 'MODULE_PATHNAME', 'nominatim_fdw_query_structured'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION nominatim_query_reverse(
    server_name text, 
    lon double precision DEFAULT 0,
    lat double precision DEFAULT 0,
    zoom int DEFAULT 0,
    layer text DEFAULT '',
    extratags boolean DEFAULT false,
    addressdetails boolean DEFAULT false,
    namedetails boolean DEFAULT false,
    polygon text DEFAULT '')
RETURNS SETOF NominatimReverseGeocode AS 'MODULE_PATHNAME', 'nominatim_fdw_query_reverse'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FOREIGN DATA WRAPPER nominatim_fdw
HANDLER nominatim_fdw_handler
VALIDATOR nominatim_fdw_validator;

