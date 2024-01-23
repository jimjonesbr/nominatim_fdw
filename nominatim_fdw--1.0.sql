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
  timestamp timestamp,
  attribution text,
  querystring text,
  polygon text,
  exclude_place_ids text,
  more_url text,
  place_id text,
  osm_type text,
  osm_id text,
  ref text,
  lat numeric,
  lon numeric,
  boundingbox text,
  place_rank text,
  address_tank text,
  display_name text,
  class text,
  type text,
  importance text,
  icon text,
  extratags text  
);

CREATE FUNCTION nominatim_simple_query(server_name text, query text)
RETURNS SETOF NominatimRecord AS 'MODULE_PATHNAME', 'nominatim_fdw_freeform_query'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;


CREATE FOREIGN DATA WRAPPER nominatim_fdw
HANDLER nominatim_fdw_handler
VALIDATOR nominatim_fdw_validator;

