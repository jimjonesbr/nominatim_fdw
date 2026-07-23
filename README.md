
---------------------------------------------
# Nominatim Foreign Data Wrapper for PostgreSQL (nominatim_fdw)

The `nominatim_fdw` is a PostgreSQL Foreign Data Wrapper to access data from [Nominatim](https://nominatim.org/) servers using simple function calls.

![CI](https://github.com/jimjonesbr/nominatim_fdw/actions/workflows/ci.yml/badge.svg)

## Index

- [Requirements](#requirements)
- [Build and Install](#build-and-install)
- [Update](#update)
- [Usage](#usage)
  - [CREATE SERVER](#create-server)
  - [ALTER SERVER](#alter-server)
  - [CREATE USER MAPPING](#create-user-mapping)
  - [Functions](#functions)
    - [Nominatim_Search](#nominatim_search)
    - [Nominatim_Reverse](#nominatim_reverse)
    - [Nominatim_Lookup](#nominatim_lookup)
    - [Version](#nominatim_fdw_version)
    - [Settings](#nominatim_fdw_settings)
- [Deploy with Docker](#deploy-with-docker)
 
## [Requirements](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#requirements)

* [libxml2](http://www.xmlsoft.org/): version 2.5.0 or higher.
* [libcurl](https://curl.se/libcurl/): version 7.74.0 or higher.
* [PostgreSQL](https://www.postgresql.org): version 10 or higher.

## [Build and Install](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#build-and-install)

To compile the source code you need to ensure the [pg_config](https://www.postgresql.org/docs/current/app-pgconfig.html) executable is properly set when you run `make` - this executable is typically in your PostgreSQL installation's bin directory. After that, just run `make` in the root directory:

```bash
$ cd nominatim_fdw
$ make
```

After compilation, just run `make install` to install the Foreign Data Wrapper:

```bash
$ make install
```

After building and installing the extension you're ready to create the extension in a PostgreSQL database with `CREATE EXTENSION`:

```sql
CREATE EXTENSION nominatim_fdw;
```

To install a specific version add the full version number in the `WITH VERSION` clause

```sql
CREATE EXTENSION nominatim_fdw WITH VERSION '2.0';
```

To run the predefined regression tests run `make installcheck` with the user `postgres`:

```bash
$ make PGUSER=postgres installcheck
```

## [Update](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#update)

To update the extension's version you must first build and install the binaries and then run `ALTER EXTENSION`:


```sql
ALTER EXTENSION nominatim_fdw UPDATE;
```

To update to a specific version use `UPDATE TO` and the full version number

```sql
ALTER EXTENSION nominatim_fdw UPDATE TO '2.0';
```

## [Usage](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#usage)

To use the `nominatim_fdw` you must first create a `SERVER` to connect to a Nominatim endpoint. After that, you can retrieve the data using the nominatim_fdw functions.

### [CREATE SERVER](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#create-server)

The SQL command [CREATE SERVER](https://www.postgresql.org/docs/current/sql-createserver.html) defines a new foreign server, which in this case means a Nominatim server. The user who defines the server becomes its owner. A `SERVER` requires an `url`, so that `nominatim_fdw` knows where to send the requests.

The following example creates a `SERVER` that connects to the [OpenStreetMap Nominatim Server](https://nominatim.openstreetmap.org):

```sql
CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');
```


**Server Options**

| Server Option | Type          | Description                                                                                                        |
|---------------|----------------------|--------------------------------------------------------------------------------------------------------------------|
| `url`     | **required**            | URL address of the Nominatim endpoint.
| `http_proxy` | optional            | Proxy for HTTP requests.
| `connect_timeout`         | optional            | Connection timeout for HTTP requests in seconds (default `300` seconds).
| `max_connect_retry`         | optional            | Number of attempts to retry a request in case of failure (default `3` times).
| `max_connect_redirect`         | optional            | Limit of how many times URL redirection may follow (default `1`).
| `accept_language` | optional | language string as in "Accept-Language" HTTP header (default `en-US,en;q=0.9`).

### [ALTER SERVER](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#alter-server)

All options and parameters set to a `SERVER` can be changed, dropped, and new ones can be added using [`ALTER SERVER`](https://www.postgresql.org/docs/current/sql-alterserver.html) statements.


Adding options

```sql
ALTER SERVER osm OPTIONS (ADD max_connect_retry '5');
```

Changing previously configured options

```sql
ALTER SERVER osm OPTIONS (SET url 'https://a.new.url');
```

Dropping options

```sql
ALTER SERVER osm OPTIONS (DROP http_proxy);
```
### [CREATE USER MAPPING](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#create-user-mapping)

Proxy credentials for authenticating with a proxy server are stored in a [`USER MAPPING`](https://www.postgresql.org/docs/current/sql-createusermapping.html) rather than in the `SERVER` itself. This keeps credentials separate per database user.

**User Mapping Options**

| Option | Type | Description |
|---|---|---|
| `proxy_user` | optional | User name for proxy server authentication. |
| `proxy_password` | optional | Password for proxy server authentication. |

The following example creates a user mapping with proxy credentials for the current user:

```sql
CREATE USER MAPPING FOR pguser
SERVER osm
OPTIONS (proxy_user 'myuser', proxy_password 'mysecret');
```

Credentials can be updated with `ALTER USER MAPPING`:

```sql
ALTER USER MAPPING FOR pguser
SERVER osm OPTIONS (SET proxy_password 'newpassword');
```
### [Functions](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#functions)

This section describes the `nominatim_fdw` functions, which are mapped to the Nominatim standard search endpoints [search](https://nominatim.org/release-docs/develop/api/Search/), [reverse](https://nominatim.org/release-docs/develop/api/Reverse/) and [lookup](https://nominatim.org/release-docs/develop/api/Lookup/).

> [!NOTE]  
> All `nominatim_fdw` functions are declared `STRICT`. This means that if any argument is explicitly set to SQL `NULL`, the function short-circuits and returns no rows — the request is never sent to the server, and no error is raised.
>
> Unrecognised values for polygon, layer and featuretype produce a `WARNING` but do not abort the request — the value is forwarded to the Nominatim server as-is. This keeps the wrapper working if the Nominatim API introduces new values in the future.

#### [Nominatim_Search](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#nominatim_search)

The [search](https://nominatim.org/release-docs/develop/api/Search/) API allows you to look up a location from a textual description or address. Just like the Nominatim API, the foreign data wrapper supports [structured](https://nominatim.org/release-docs/develop/api/Search/#structured-query) and [free-form](https://nominatim.org/release-docs/develop/api/Search/#free-form-query) search queries, which are distinguished by either splitting the address components into different parameters, such as `street`, `county`, `state`, or providing a single string in the parameter `q`.

**Parameters**

| Parameter | Type | Description |
|---|---|---|
| `server_name` | **required** | Foreign Data Wrapper server created using the [CREATE SERVER](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#create_server) statement. |
| `q` | optional | Free-form query string to search for (default *unset*) |
| `amenity` | optional | name and/or type of POI (default *unset*) |
| `street` | optional | housenumber and streetname (default *unset*) |
| `city` | optional | city (default *unset*) |
| `county` | optional | county (default *unset*) |
| `state` | optional | state (default *unset*) |
| `country` | optional | country (default *unset*) |
| `postalcode` | optional | postal code (default *unset*) |
| `extratags` | optional | additional information in the result that is available in the database, e.g. wikipedia link, opening hours. (default `false`) |
| `addressdetails` | optional | includes a breakdown of the address into elements (default `false`) |
| `namedetails` | optional | includes a full list of names for the result. (default `false`) |
| `polygon` | optional | one of: `polygon_geojson`, `polygon_kml`, `polygon_svg`, `polygon_text` (default *unset*) |
| `accept_language` | optional | language string as in "Accept-Language" HTTP header (default `en-US,en;q=0.9`). This overrides the `accept_language` set in the `CREATE SERVER` statement |
| `countrycodes` | optional | comma-separated list of [country codes](https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2) (default *unset*) |
| `layer` | optional | comma-separated list of: `address`, `poi`, `railway`, `natural`, `manmade` (default *unset*) |
| `featuretype` | optional | one of: `country`, `state`, `city`, `settlement` (default *unset*) |
| `exclude_place_ids` | optional | comma-separated list of place ids (default *unset*) |
| `viewbox` | optional | bounding box as in `<x1>,<y1>,<x2>,<y2>` (default *unset*) |
| `bounded` | optional | When set to `true`, restrict the results to items within the `viewbox` (requires `viewbox` to be set). (default `false`)"|
| `polygon_threshold` | optional | floating-point number (default `0.0`) |
| `email` | optional | valid email address (default *unset*) |
| `dedupe` | optional | discards duplicated entries (default `true`) |
| `limit_result` | optional | limits the maximum number of returned results (default `0`) |
| `entrances` | optional | when set to `true`, include the tagged entrances in the result. (default `false`) |

As in the Nominatim API, the free-form query string parameter `q` cannot be combined with the parameters `amenity`, `street`, `city`, `county`, `state`, `country` and `postalcode`, as they are used in structured calls.


----------------------


**Usage**

For these examples we assume the following `SERVER`:

```sql
CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');
```

Free-form search

```sql
SELECT osm_id, ref, lon, lat, boundingbox 
FROM nominatim_search(server_name => 'osm', 
                      q => 'Neubrückenstraße 63, münster, germany');

-[ RECORD 1 ]------------------------------------------
osm_id      | 121736959
ref         | Theater Münster
lon         | 7.6293918
lat         | 51.9648163
boundingbox | 51.9644060,51.9652417,7.6286897,7.6304381
-[ RECORD 2 ]------------------------------------------
osm_id      | 2785257564
ref         | 
lon         | 7.6290121
lat         | 51.9651014
boundingbox | 51.9650514,51.9651514,7.6289621,7.6290621
```

Structured search

```sql
SELECT osm_id, ref, lon, lat, boundingbox 
FROM nominatim_search(server_name => 'osm', 
                      street => 'neubrückenstraße 63', 
                      city => 'münster');

-[ RECORD 1 ]------------------------------------------
osm_id      | 121736959
ref         | Theater Münster
lon         | 7.6293918
lat         | 51.9648163
boundingbox | 51.9644060,51.9652417,7.6286897,7.6304381
-[ RECORD 2 ]------------------------------------------
osm_id      | 2785257564
ref         | 
lon         | 7.6290121
lat         | 51.9651014
boundingbox | 51.9650514,51.9651514,7.6289621,7.6290621
```

All columnns:

```sql
SELECT * 
FROM nominatim_search(server_name => 'osm', 
                      street => 'neubrückenstraße 63', 
                      city => 'münster',
                      polygon => 'polygon_text',
                      namedetails => true,
                      extratags => true,
                      addressdetails => true,
                      entrances => true)
FETCH FIRST ROW ONLY;

-[ RECORD 1 ]-
osm_id            | 121736959
osm_type          | way
ref               | Theater Münster
class             | amenity
display_name      | Theater Münster, 63, Neubrückenstraße, Martini, Altstadt, Münster-Mitte, Münster, North Rhine-Westphalia, 48143, Germany
place_id          | 114644984
place_rank        | 30
address_rank      | 30
lon               | 7.6293918
lat               | 51.9648163
boundingbox       | 51.9644060,51.9652417,7.6286897,7.6304381
importance        | 0.38154785896147225
icon              | 
timestamp         | 2026-07-03 12:01:23+00
attribution       | Data © OpenStreetMap contributors, ODbL 1.0. http://osm.org/copyright
querystring       | neubrückenstraße 63, münster
polygon           | POLYGON((7.6286897 51.9647704,7.6287379 51.9647543,7.6287912 51.9647366,7.6288354 51.9647218,7.6288349 51.9646709,7.628834 51.964573,7.6289499 51.9645661,7.6290874 51.9645579,7.6290872 51.9645503,7.6290867 51.9645228,7.6292274 51.9645082,7.6292273 51.9645007,7.6292271 51.964474,7.6293629 51.9644615,7.6293627 51.9644544,7.6293617 51.9644272,7.6295651 51.9644121,7.6296481 51.964406,7.6297295 51.9645083,7.6299845 51.9647498,7.6300692 51.96483,7.6302563 51.9647441,7.6302913 51.9647704,7.6303205 51.9647923,7.630342 51.9648082,7.6303599 51.9648243,7.6303929 51.9648538,7.6304345 51.9648911,7.6304381 51.964895,7.6301466 51.9649831,7.6298991 51.9650575,7.6298803 51.9650638,7.6298581 51.9650704,7.6297948 51.965089,7.6298643 51.9651741,7.6296525 51.9652362,7.6296337 51.9652417,7.6295205 51.9651873,7.6293479 51.9652404,7.629327 51.9652323,7.6293113 51.9652382,7.6292464 51.9652081,7.6291822 51.9652287,7.6291288 51.9652054,7.6291488 51.9651362,7.6291685 51.9651295,7.6291838 51.9650844,7.6291485 51.9650948,7.6291248 51.9650853,7.6291022 51.9650763,7.6290867 51.9650807,7.6290636 51.9650873,7.6290291 51.9650971,7.6289808 51.9651108,7.6286897 51.9647704))
exclude_place_ids | W121736959,N2785257564,N5024387719,N1361849725
more_url          | https://nominatim.openstreetmap.org/search?street=neubr%C3%BCckenstra%C3%9Fe+63&city=m%C3%BCnster&polygon_text=1&addressdetails=1&entrances=1&namedetails=1&extratags=1&limit=20&exclude_place_ids=W121736959%2CN2785257564%2CN5024387719%2CN1361849725&format=xml
extratags         | {"image": "https://upload.wikimedia.org/wikipedia/commons/6/64/Muenster_Stadttheater_%2881%29.JPG", "layer": "-1", "toilets": "customers", "building": "civic", "location": "surface", "wikidata": "Q2415904", "wikipedia": "de:Theater Münster", "roof:shape": "flat", "start_date": "1956", "wheelchair": "yes", "contact:fax": "+49 251 5909202", "roof:colour": "#F5F5DC", "contact:email": "info-theater@stadt-muenster.de", "contact:phone": "+49 251 5909205", "roof:material": "gravel", "building:colour": "silver", "building:levels": "2", "contact:website": "https://www.theater-muenster.com/", "building:material": "concrete", "construction_date": "1956", "wikimedia_commons": "Category:Theater Münster", "toilets:wheelchair": "yes"}
namedetails       | {"name": "Theater Münster", "name:de": "Theater Münster", "alt_name": "Stadttheater", "old_name": "Städtische Bühnen Münster"}
addressdetails    | {"city": "Münster", "road": "Neubrückenstraße", "state": "North Rhine-Westphalia", "suburb": "Altstadt", "amenity": "Theater Münster", "country": "Germany", "postcode": "48143", "country_code": "de", "house_number": "63", "city_district": "Münster-Mitte", "neighbourhood": "Martini", "ISO3166-2-lvl4": "DE-NW"}
entrances         | [{"lat": "51.9650638", "lon": "7.6298803", "type": "service", "osm_id": "2838736213"}, {"lat": "51.9649831", "lon": "7.6301466", "type": "service", "osm_id": "9912525894"}, {"lat": "51.9644121", "lon": "7.6295651", "type": "emergency", "osm_id": "9912525895"}, {"lat": "51.9644544", "lon": "7.6293627", "type": "emergency", "osm_id": "9912525896"}, {"lat": "51.9645007", "lon": "7.6292273", "type": "emergency", "osm_id": "9912525897"}, {"lat": "51.9645503", "lon": "7.6290872", "type": "emergency", "osm_id": "9912525898"}, {"lat": "51.9650853", "lon": "7.6291248", "type": "main", "osm_id": "9912525899"}, {"lat": "51.9646709", "lon": "7.6288349", "type": "main", "osm_id": "9912525900"}]
type              | theatre
```

#### [Nominatim_Reverse](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#nominatim_reverse)

[Reverse](https://nominatim.org/release-docs/develop/api/Reverse/) geocoding generates an address from a coordinate given as latitude and longitude. The reverse geocoding API does not exactly compute the address for the coordinate it receives. It works by finding the closest suitable OSM object and returning its address information. This may occasionally lead to unexpected results.

**Parameters**

| Parameter | Type | Description |
|---|---|---
| `server_name` | **required** | Foreign Data Wrapper server created using the `CREATE SERVER` statement. |
| `lon` | optional | longitude of the location to generate an address for (default `0`) |
| `lat` | optional | latitude of the location to generate an address for (default `0`) |
| `zoom` | optional | level of detail required for the address, `0`–`18`. Roughly corresponds to a map zoom level: `3` country, `5` state, `8` county, `10` city, `12` town/borough, `13` village/suburb, `14` neighbourhood, `15` any settlement, `16` major streets, `17` major and minor streets, `18` building. If unset, the server default (`18`, building level) is applied. (default *unset*)|
| `layer` | optional | comma-separated list of: `address`, `poi`, `railway`, `natural`, `manmade` (default *unset*) |
| `extratags` | optional | additional information in the result that is available in the database, e.g. wikipedia link, opening hours. (default `false`) |
| `addressdetails` | optional | includes a breakdown of the address into elements (default `true`) |
| `namedetails` | optional | includes a full list of names for the result. (default `false`) |
| `polygon` | optional | one of: `polygon_geojson`, `polygon_kml`, `polygon_svg`, `polygon_text` (default *unset*) |
| `accept_language` | optional | language string as in "Accept-Language" HTTP header (default `en-US,en;q=0.9`). This overrides the `accept_language` set in the `CREATE SERVER` statement |
| `entrances` | optional | when set to `true`, include the tagged entrances in the result. (default `false`) |
| `polygon_threshold` | optional | floating-point number (default `0.0`) |
| `email` | optional | valid email address (default *unset*) |

----------------------
**Usage**

For these examples we assume the following `SERVER`:

```sql
CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');
```

Address generation for the coordinates `7.6293` longitude and `51.9648` latitude:        

```sql
SELECT osm_id, display_name, boundingbox
FROM nominatim_reverse(
        server_name => 'osm', 
        lon => 7.6293,
        lat => 51.9648,        
        extratags => true);

-[ RECORD 1 ]-
osm_id       | 2785257564
display_name | 63, Neubrückenstraße, Martini, Altstadt, Münster-Mitte, Münster, North Rhine-Westphalia, 48143, Germany
boundingbox  | 51.9650514,51.9651514,7.6289621,7.6290621

```

All columns:

```sql
SELECT * 
FROM nominatim_reverse(
        server_name => 'osm', 
        lon => 7.6293,
        lat => 51.9648,
        polygon => 'polygon_text',      
        namedetails => true,
        extratags => true,
        addressdetails => true,
        entrances => true);

-[ RECORD 1 ]-
osm_id         | 2785257564
osm_type       | node
display_name   | 63, Neubrückenstraße, Martini, Altstadt, Münster-Mitte, Münster, North Rhine-Westphalia, 48143, Germany
ref            | 
place_id       | 113899235
place_rank     | 30
address_rank   | 30
lon            | 7.6290121
lat            | 51.9651014
boundingbox    | 51.9650514,51.9651514,7.6289621,7.6290621
icon           | 
timestamp      | 2026-07-03 11:58:23+00
attribution    | Data © OpenStreetMap contributors, ODbL 1.0. http://osm.org/copyright
querystring    | lat=51.9648&lon=7.6293&format=xml
polygon        | POINT(7.6290121 51.9651014)
extratags      | {"operator": "Deutsche Post", "collection_times": "Mo-Fr 15:30, Sa 11:30", "operator:wikidata": "Q157645"}
namedetails    | {}
addressdetails | {"city": "Münster", "road": "Neubrückenstraße", "state": "North Rhine-Westphalia", "suburb": "Altstadt", "country": "Germany", "postcode": "48143", "country_code": "de", "house_number": "63", "city_district": "Münster-Mitte", "neighbourhood": "Martini", "ISO3166-2-lvl4": "DE-NW"}
entrances      | []
```

#### [Nominatim_Lookup](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#nominatim_lookup)

The [lookup](https://nominatim.org/release-docs/develop/api/Lookup/) API allows to query the address and other details of one or multiple OSM objects like node, way or relation.

**Parameters**

| Parameter | Type | Description |
|---|---|---
| `server_name` | **required** | Foreign Data Wrapper server created using the [CREATE SERVER](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#create_server) statement. |
| `osm_ids` | **required** | comma-separated list of OSM ids, each prefixed with its type: `N` (node), `W` (way) or `R` (relation), e.g. `N123,W456,R789` |
| `extratags` | optional | additional information in the result that is available in the database, e.g. wikipedia link, opening hours. (default `false`) |
| `addressdetails` | optional | includes a breakdown of the address into elements (default `true`) |
| `namedetails` | optional | includes a full list of names for the result. (default `false`) |
| `polygon` | optional | one of: `polygon_geojson`, `polygon_kml`, `polygon_svg`, `polygon_text` (default *unset*) |
| `entrances` | optional | when set to `true`, include the tagged entrances in the result. (default `false`) |
| `accept_language` | optional | language string as in "Accept-Language" HTTP header (default `en-US,en;q=0.9`). This overrides the `accept_language` set in the `CREATE SERVER` |
| `polygon_threshold` | optional | floating-point number (default `0.0`) |
| `email` | optional | valid email address (default *unset*) |

**Usage**

For these examples we assume the following `SERVER`:

```sql
CREATE SERVER osm 
FOREIGN DATA WRAPPER nominatim_fdw 
OPTIONS (url 'https://nominatim.openstreetmap.org');
```

```sql
SELECT osm_id, display_name 
FROM nominatim_lookup(
      server_name => 'osm',
      osm_ids => 'W121736959');

-[ RECORD 1 ]-
osm_id       | 121736959
display_name | Theater Münster, 63, Neubrückenstraße, Martini, Altstadt, Münster-Mitte, Münster, Nordrhein-Westfalen, 48143, Deutschland
```

All columns:

```sql
SELECT *  
FROM nominatim_lookup(
      server_name => 'osm',
      osm_ids => 'W121736959',
      polygon => 'polygon_text',
      namedetails => true,
      extratags => true,
      addressdetails => true,
      entrances => true);

-[ RECORD 1 ]-
osm_id            | 121736959
osm_type          | way
ref               | Theater Münster
class             | amenity
display_name      | Theater Münster, 63, Neubrückenstraße, Martini, Altstadt, Münster-Mitte, Münster, Nordrhein-Westfalen, 48143, Deutschland
place_id          | 113107324
place_rank        | 30
address_rank      | 30
lon               | 7.6293918
lat               | 51.9648163
boundingbox       | 51.9644060,51.9652417,7.6286897,7.6304381
importance        | 0.38154785896147225
icon              | 
timestamp         | 2026-07-03 11:59:39+00
attribution       | Data © OpenStreetMap contributors, ODbL 1.0. http://osm.org/copyright
querystring       | 
polygon           | POLYGON((7.6286897 51.9647704,7.6287379 51.9647543,7.6287912 51.9647366,7.6288354 51.9647218,7.6288349 51.9646709,7.628834 51.964573,7.6289499 51.9645661,7.6290874 51.9645579,7.6290872 51.9645503,7.6290867 51.9645228,7.6292274 51.9645082,7.6292273 51.9645007,7.6292271 51.964474,7.6293629 51.9644615,7.6293627 51.9644544,7.6293617 51.9644272,7.6295651 51.9644121,7.6296481 51.964406,7.6297295 51.9645083,7.6299845 51.9647498,7.6300692 51.96483,7.6302563 51.9647441,7.6302913 51.9647704,7.6303205 51.9647923,7.630342 51.9648082,7.6303599 51.9648243,7.6303929 51.9648538,7.6304345 51.9648911,7.6304381 51.964895,7.6301466 51.9649831,7.6298991 51.9650575,7.6298803 51.9650638,7.6298581 51.9650704,7.6297948 51.965089,7.6298643 51.9651741,7.6296525 51.9652362,7.6296337 51.9652417,7.6295205 51.9651873,7.6293479 51.9652404,7.629327 51.9652323,7.6293113 51.9652382,7.6292464 51.9652081,7.6291822 51.9652287,7.6291288 51.9652054,7.6291488 51.9651362,7.6291685 51.9651295,7.6291838 51.9650844,7.6291485 51.9650948,7.6291248 51.9650853,7.6291022 51.9650763,7.6290867 51.9650807,7.6290636 51.9650873,7.6290291 51.9650971,7.6289808 51.9651108,7.6286897 51.9647704))
exclude_place_ids | 
more_url          | 
extratags         | {"image": "https://upload.wikimedia.org/wikipedia/commons/6/64/Muenster_Stadttheater_%2881%29.JPG", "layer": "-1", "toilets": "customers", "building": "civic", "location": "surface", "wikidata": "Q2415904", "wikipedia": "de:Theater Münster", "roof:shape": "flat", "start_date": "1956", "wheelchair": "yes", "contact:fax": "+49 251 5909202", "roof:colour": "#F5F5DC", "contact:email": "info-theater@stadt-muenster.de", "contact:phone": "+49 251 5909205", "roof:material": "gravel", "building:colour": "silver", "building:levels": "2", "contact:website": "https://www.theater-muenster.com/", "building:material": "concrete", "construction_date": "1956", "wikimedia_commons": "Category:Theater Münster", "toilets:wheelchair": "yes"}
namedetails       | {"name": "Theater Münster", "name:de": "Theater Münster", "alt_name": "Stadttheater", "old_name": "Städtische Bühnen Münster"}
addressdetails    | {"city": "Münster", "road": "Neubrückenstraße", "state": "Nordrhein-Westfalen", "suburb": "Altstadt", "amenity": "Theater Münster", "country": "Deutschland", "postcode": "48143", "country_code": "de", "house_number": "63", "city_district": "Münster-Mitte", "neighbourhood": "Martini", "ISO3166-2-lvl4": "DE-NW"}
entrances         | [{"lat": "51.9650638", "lon": "7.6298803", "type": "service", "osm_id": "2838736213"}, {"lat": "51.9649831", "lon": "7.6301466", "type": "service", "osm_id": "9912525894"}, {"lat": "51.9644121", "lon": "7.6295651", "type": "emergency", "osm_id": "9912525895"}, {"lat": "51.9644544", "lon": "7.6293627", "type": "emergency", "osm_id": "9912525896"}, {"lat": "51.9645007", "lon": "7.6292273", "type": "emergency", "osm_id": "9912525897"}, {"lat": "51.9645503", "lon": "7.6290872", "type": "emergency", "osm_id": "9912525898"}, {"lat": "51.9650853", "lon": "7.6291248", "type": "main", "osm_id": "9912525899"}, {"lat": "51.9646709", "lon": "7.6288349", "type": "main", "osm_id": "9912525900"}]
type              | theatre
```

#### [nominatim_fdw_version](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#version)

Shows the version of the installed `nominatim_fdw` and its main libraries.

**Usage**

```sql
SELECT nominatim_fdw_version();
                                            nominatim_fdw_version                                             
--------------------------------------------------------------------------------------------------------------
 nominatim_fdw 2.0 (PostgreSQL 18.4 (Debian 18.4-1.pgdg13+1), compiled by gcc, libxml 2.9.14, libcurl 8.14.1)
(1 row)
```
#### [nominatim_fdw_settings](#nominatim_fdw_settings)

A system view that provides detailed version information for `nominatim_fdw` and all its dependencies, including core libraries (PostgreSQL, libxml, libcurl) and optional components (SSL, zlib, libSSH, nghttp2), along with compiler and build information. Returns individual component names and their corresponding versions for convenient programmatic access.

-------

**Usage**

```sql
SELECT * FROM nominatim_fdw_settings; 
   component   |            version            
---------------+-------------------------------
 nominatim_fdw | 2.0
 PostgreSQL    | 18.4 (Debian 18.4-1.pgdg13+1)
 libxml        | 2.9.14
 libcurl       | 8.14.1
 ssl           | GnuTLS/3.8.9
 zlib          | 1.3.1
 libSSH        | libssh2/1.11.1
 nghttp2       | 1.64.0
 compiler      | gcc
 built         | 2026-07-07 13:29:54 UTC
(10 rows)
```

## [Deploy with Docker](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#deploy-with-docker)

To deploy `nominatim_fdw` with docker just pick one of the supported PostgreSQL versions, install the [requirements](#requirements) and [compile](#build-and-install) the [source code](https://github.com/jimjonesbr/nominatim_fdw/releases). For instance, a `nominatim_fdw` `Dockerfile` for PostgreSQL 18 should look like this (minimal example):

```Dockerfile
FROM postgres:18

RUN apt-get update && \
    apt-get install -y make gcc postgresql-server-dev-18 libxml2-dev libcurl4-openssl-dev

RUN tar xvzf nominatim_fdw-[VERSION].tar.gz && \
    cd nominatim_fdw-[VERSION] && \
    make -j && \
    make install
```

To build the image save it in a `Dockerfile` and  run the following command in the root directory - this will create an image called `nominatim_fdw_image`:
 
```bash
 $ docker build -t nominatim_fdw_image .
```

After successfully building the image you're ready to `run` or `create` the container ...
 
```bash
$ docker run --name my_container -e POSTGRES_HOST_AUTH_METHOD=trust nominatim_fdw_image
```

... and then finally you're able to create and use the extension!

```bash
$ docker exec -u postgres my_container psql -d mydatabase -c "CREATE EXTENSION nominatim_fdw;"
```

### [For testers and developers](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#for-testers-and-developers)

Deploying the latest development version straight from the source:

Dockerfile


```dockerfile
FROM postgres:18

RUN apt-get update && \
    apt-get install -y git make gcc postgresql-server-dev-18 libxml2-dev libcurl4-openssl-dev

WORKDIR /

RUN git clone https://github.com/jimjonesbr/nominatim_fdw.git && \
    cd nominatim_fdw && \
    make -j && \
    make install
```
Deployment

```bash
 $ docker build -t nominatim_fdw_image .
 $ docker run --name my_container -e POSTGRES_HOST_AUTH_METHOD=trust nominatim_fdw_image
 $ docker exec -u postgres my_container psql -d mydatabase -c "CREATE EXTENSION nominatim_fdw;"
```