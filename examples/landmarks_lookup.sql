/*
 * nominatim_lookup() example - Retrieves the details of places whose OpenStreetMap
 * ids are already known, e.g. to refresh them, with a single request.
 *
 * Each id is prefixed with its type: N (node), W (way) or R (relation). A lookup
 * takes up to 50 ids at once, which is far kinder to the public server than one
 * search per place.
 */

CREATE EXTENSION IF NOT EXISTS nominatim_fdw;

CREATE SERVER IF NOT EXISTS osm
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'https://nominatim.openstreetmap.org');

DROP TABLE IF EXISTS landmark;

CREATE TABLE landmark (
  osm_id text PRIMARY KEY,
  name text,
  city text,
  wikidata text,
  lon numeric,
  lat numeric
);

INSERT INTO landmark (osm_id)
VALUES
  ('W4532022'),   -- Cologne Cathedral
  ('W518071791'), -- Brandenburg Gate
  ('W40967526');  -- St.-Paulus-Dom, Münster

/*
 * The subquery hands all ids over at once, so nominatim_lookup() is called - and
 * the server asked - only once. Its results carry the type spelled out ('way'),
 * so the prefix is rebuilt to match them with the table.
 */
UPDATE landmark l
SET name = r.namedetails->>'name',
    city = r.addressdetails->>'city',
    wikidata = r.extratags->>'wikidata',
    lon = r.lon,
    lat = r.lat
FROM nominatim_lookup(
       server_name => 'osm',
       osm_ids => (SELECT string_agg(osm_id, ',') FROM landmark),
       extratags => true,
       namedetails => true) r
WHERE l.osm_id = upper(left(r.osm_type, 1)) || r.osm_id;

SELECT * FROM landmark;
