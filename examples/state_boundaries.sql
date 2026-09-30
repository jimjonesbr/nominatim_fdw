/*
 * nominatim_search() example - Retrieves the boundaries of a few German states as
 * polygons and calculates their area. Requires PostGIS.
 *
 * A structured search (state, country) restricted to featuretype 'state' finds the
 * state itself rather than anything named after it. polygon_threshold simplifies
 * the outlines (in degrees), which keeps the responses small.
 */

CREATE EXTENSION IF NOT EXISTS nominatim_fdw;
CREATE EXTENSION IF NOT EXISTS postgis;

CREATE SERVER IF NOT EXISTS osm
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'https://nominatim.openstreetmap.org');

DROP TABLE IF EXISTS state_boundary;

CREATE TABLE state_boundary (
  name text,
  osm_id bigint,
  geom geometry(multipolygon,4326)
);

DO $$
DECLARE
  state_name text;
BEGIN
  FOREACH state_name IN ARRAY ARRAY['Bavaria', 'Baden-Württemberg', 'Saarland', 'Bremen']
  LOOP
    RAISE NOTICE 'Retrieving the boundary of "%" ...', state_name;
    INSERT INTO state_boundary (name, osm_id, geom)
    SELECT state_name, osm_id, ST_Multi(ST_GeomFromText(polygon, 4326))
    FROM nominatim_search(
           server_name => 'osm',
           state => state_name,
           country => 'Germany',
           featuretype => 'state',
           polygon => 'polygon_text',
           polygon_threshold => 0.01,
           limit_result => 1);
    PERFORM pg_sleep(2); -- waits 2 seconds between requests to avoid any trouble with OSM.
  END LOOP;
END; $$;

/*
 * Official areas, for comparison: Bavaria 70,542 km², Baden-Württemberg 35,748 km²,
 * Saarland 2,571 km², Bremen 420 km². The simplified outlines, and boundaries that
 * run through lakes, e.g. Lake Constance, account for the small differences.
 */
SELECT name, osm_id,
       round(ST_Area(geom::geography) / 1e6) AS area_km2,
       ST_NumGeometries(geom) AS parts,
       ST_NPoints(geom) AS points
FROM state_boundary
ORDER BY area_km2 DESC;
