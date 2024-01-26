CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');

SELECT * FROM nominatim_fdw_version();

SELECT * 
FROM nominatim_query(
      server_name => 'osm',
      query => 'einsteinstraße 60,münster,germany',
      extra_params => '&extratags=1&namedetails=1&addressdetails=1&polygon_text=1&polygon_threshold=0.0');


SELECT * 
FROM nominatim_query_structured(
      server_name => 'osm',
      query => 'wwu it',
      street => 'einsteinstraße 60',
      city => 'münster',
      state => 'nordrhein westfalen',
      country => 'germany',
      postalcode => '48149',
      extra_params => '&polygon_text=1&extratags=1&addressdetails=1');


SELECT * 
FROM nominatim_query_reverse(
        server_name => 'osm', 
        lon => 7.6038115,
        lat => 51.9660873,        
        extra_params => '&polygon_text=1&extratags=1&addressdetails=1&namedetails=1');


SELECT * 
FROM nominatim_query_lookup(
      server_name => 'osm',
      osm_ids => 'R146656,W50637691,N240109189',
      extra_params => '&polygon_text=1&extratags=1&addressdetails=1&namedetails=1');