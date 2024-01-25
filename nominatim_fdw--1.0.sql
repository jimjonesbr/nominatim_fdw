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

CREATE FUNCTION nominatim_query(
    server_name text, 
    query text, 
    extra_params text DEFAULT '')
RETURNS SETOF NominatimRecord AS 'MODULE_PATHNAME', 'nominatim_fdw_query'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FOREIGN DATA WRAPPER nominatim_fdw
HANDLER nominatim_fdw_handler
VALIDATOR nominatim_fdw_validator;

