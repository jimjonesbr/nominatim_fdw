/*
 * nominatim_search() example - Geolocates the addresses of Christmas markets in
 * Baden-Württemberg. Requires PostGIS.
 *
 * Sends 14 requests to nominatim.openstreetmap.org, one every 2 seconds (about
 * 30 seconds in total).
 *
 * Each address is sent as a free-form query (q), and Nominatim ranks its matches
 * by importance, not by how well they fit the address. "Marktplatz 1, 70173
 * Stuttgart", for instance, finds a church on the market square of Bad Cannstatt
 * (70372) before the town hall at the actual address. The match whose postcode
 * agrees with the address is therefore preferred; when none agrees, the most
 * important match is used.
 *
 * Clean up: DROP TABLE xmas_market; DROP SERVER osm;
 */

CREATE EXTENSION IF NOT EXISTS nominatim_fdw;
CREATE EXTENSION IF NOT EXISTS postgis;

CREATE SERVER IF NOT EXISTS osm
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'https://nominatim.openstreetmap.org');

DROP TABLE IF EXISTS xmas_market;

CREATE TABLE xmas_market (
  id int GENERATED ALWAYS AS IDENTITY,
  name text,
  address text,
  geom geometry(point,4326)
);

INSERT INTO xmas_market (name, address)
VALUES
  ('Stuttgarter Weihnachtsmarkt','Marktplatz 1, 70173 Stuttgart'),
  ('Esslinger Mittelalter- und Weihnachtsmarkt','Rathausplatz, 73728 Esslingen am Neckar'),
  ('Käthchen Weihnachtsmarkt Heilbronn','74072 Heilbronn-Innenstadt'),
  ('Goldener Pforzheimer Weihnachtsmarkt','75172 Pforzheim, Innenstadt'),
  ('chokoMARKT Tübingen','Am Markt 1, 72070 Tübingen'),
  ('Tübinger Weihnachtsmarkt','Am Markt 1, 72070 Tübingen'),
  ('Ludwigsburger Barock-Weihnachtsmarkt','Marktplatz, 71634 Ludwigsburg'),
  ('Weihnachtsmarkt Freiburg','Rathausplatz, 79098 Freiburg im Breisgau'),
  ('Karlsruher Christkindlesmarkt','Marktplatz, 76133 Karlsruhe'),
  ('Ulmer Weihnachtsmarkt','Münsterplatz, 89073 Ulm'),
  ('Reutlinger Weihnachtsmarkt','Weibermarkt, 72764 Reutlingen'),
  ('Göppinger Advent','Marktplatz, 73033 Göppingen'),
  ('Weihnachtsmarkt Rottweil','Friedrichsplatz, 78628 Rottweil'),
  ('Gmünder Weihnachtsmarkt','Marktplatz, 73525 Schwäbisch Gmünd');

/*
 * One request per address. addressdetails => true returns the postcode of each
 * match, which is compared with the five-digit postcode in the address. Matches
 * without a postcode, e.g. a square or a district, rank below those that agree.
 */
DO $$
DECLARE
  rec xmas_market%rowtype;
  g geometry(point,4326);
BEGIN
  FOR rec IN
    SELECT * FROM xmas_market
  LOOP
    RAISE NOTICE 'Resolving "%" ...', rec.address;
    SELECT ST_MakePoint(lon, lat) INTO g
    FROM nominatim_search(
           server_name => 'osm',
           q => rec.address,
           addressdetails => true)
    ORDER BY (addressdetails->>'postcode' = substring(rec.address from '\d{5}')) IS TRUE DESC,
             importance DESC
    LIMIT 1;
    IF g IS NOT NULL THEN
      UPDATE xmas_market SET geom = g WHERE id = rec.id;
    END IF;
    PERFORM pg_sleep(2); -- the public server allows at most 1 request per second
  END LOOP;
END; $$;

SELECT name, address, ST_AsText(geom) AS geom FROM xmas_market;
