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
  place_id text,
  place_rank text,
  address_rank text,
  lon text,
  lat text,
  boundingbox text,  
  importance numeric,
  icon text,
  extratags text,

  timestamp text,
  attribution text,
  querystring text,
  polygon text,
  exclude_place_ids text,
  more_url text
);



CREATE FUNCTION nominatim_simple_query(server_name text, query text)
RETURNS SETOF NominatimRecord AS 'MODULE_PATHNAME', 'nominatim_fdw_freeform_query'
LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;


CREATE FOREIGN DATA WRAPPER nominatim_fdw
HANDLER nominatim_fdw_handler
VALIDATOR nominatim_fdw_validator;

