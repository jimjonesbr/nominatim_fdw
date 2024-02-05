/*
 * nominatim_search() example - Geolocates addresses of Christmas Markets in Germany.
 * Requires PostGIS.
 */
 
DROP SERVER IF EXISTS osm;
DROP TABLE IF EXISTS xmas_market;

CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');

CREATE TABLE xmas_market(
  id int GENERATED ALWAYS AS IDENTITY,
  name text,
  address text,
  geom geometry(point,4326)
);

INSERT INTO xmas_market (name, address)
VALUES 
  ('Stuttgarter Weihnachtsmarkt','Marktplatz 1, 70173 Stuttgart'),
  ('Esslinger Mittelalter- und Weihnachtsmarkt','Rathausplatz, 73728 Esslingen am Neckar'),
  ('Kätchen Weihnachtsmarkt Heilbronn','74072 Heilbronn-Innenstadt'),
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

DO $$
DECLARE 
 rec xmas_market%rowtype;
 g geometry(point,4326);
BEGIN
  FOR rec IN
    SELECT * FROM public.xmas_market
  LOOP
   RAISE NOTICE 'Resolving "%" ...', rec.address;
   SELECT ST_MakePoint(lon,lat) INTO g 
   FROM nominatim_search(
          server_name => 'osm', 
          q => rec.address);
   IF g IS NOT NULL THEN
     UPDATE xmas_market SET geom = g WHERE id = rec.id;
	 EXECUTE pg_sleep(2); -- waits 2 seconds between requests to avoid any trouble with OSM.
   END IF;    
  END LOOP;
END; $$;

SELECT * FROM xmas_market;

