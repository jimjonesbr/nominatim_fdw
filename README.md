
---------------------------------------------
# PostgreSQL Foreign Data Wrapper for Nominatim (nominatim_fdw)

The `nominatim_fdw` is a PostgreSQL Foreign Data Wrapper to access data from [Nominatim](https://nominatim.org/) servers using simple function calls.

![CI](https://github.com/jimjonesbr/nominatim_fdw/actions/workflows/ci.yml/badge.svg)

## Index

- [Requirements](#requirements)
- [Build and Install](#build-and-install)
- [Update](#update)
- [Usage](#usage)
  - [CREATE SERVER](#create-server)
  - [ALTER SERVER](#alter-server)  
  - [Functions](#functions)
    - [Nominatim_Search](#nominatim_search)
    - [Nominatim_Reverse](#nominatim_reverse)
    - [Nominatim_Lookup](#nominatim_lookup)
    - [Version](#nominatim_fdw_version)
- [Examples](#examples)
- [Deploy with Docker](#deploy-with-docker)
 
## [Requirements](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#requirements)

* [libxml2](http://www.xmlsoft.org/): version 2.5.0 or higher.
* [libcurl](https://curl.se/libcurl/): version 7.74.0 or higher.
* [PostgreSQL](https://www.postgresql.org): version 11 or higher.

## [Build and Install](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#build_and_install)

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

To install an specific version add the full version number in the `WITH VERSION` clause

```sql
CREATE EXTENSION nominatim_fdw WITH VERSION '1.0';
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

To update to an specific version use `UPDATE TO` and the full version number

```sql
ALTER EXTENSION nominatim_fdw UPDATE TO '1.1';
```

## [Usage](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#usage)

To use the `nominatim_fdw` you must first create a `SERVER` to connect to a Nominatim endpoint. After that, you can retrieve the data using the nominatim_fdw functions.

### [CREATE SERVER](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#create_server)

The SQL command [CREATE SERVER](https://www.postgresql.org/docs/current/sql-createserver.html) defines a new foreign server, which in this case means a Nominatim server. The user who defines the server becomes its owner. A `SERVER` requires an `url`, so that `nominatim_fdw` knows where to sent the requests.

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
| `proxy_user` | optional            | User for proxy server authentication.
| `proxy_user_password` | optional            | Password for proxy server authentication.
| `connect_timeout`         | optional            | Connection timeout for HTTP requests in seconds (default `300` seconds).
| `max_connect_retry`         | optional            | Number of attempts to retry a request in case of failure (default `3` times).
| `max_request_redirect`         | optional            | Limit of how many times the URL redirection may occur. If that many redirections have been followed, the next redirect will cause an error. Not setting this parameter or setting it to `0` will allow an infinite number of redirects.


### [ALTER SERVER](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#alter-foreign-table-and-alter-server)

All options and parameters set to a `SERVER` can be changed, dropped, and new ones can be added using [`ALTER SERVER`](https://www.postgresql.org/docs/current/sql-alterserver.html) statements.


Adding options

```sql
ALTER SERVER osm OPTIONS (ADD max_connect_rety '5');
```

Changing previously configured options

```sql
ALTER SERVER osm OPTIONS (SET url 'https://a.new.url');
```

Dropping options

```sql
ALTER SERVER osm OPTIONS (DROP http_proxy);
```
### [Functions](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#functions)

This section describes the `nominatim_fdw` functions, which are mapped to the Nominatim standard search endpoints [search](https://nominatim.org/release-docs/develop/api/Search/), [reverse](https://nominatim.org/release-docs/develop/api/Reverse/) and [lookup](https://nominatim.org/release-docs/develop/api/Lookup/).

#### [Nominatim_Search](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#nominatim_search)

**Description**

The [search](https://nominatim.org/release-docs/develop/api/Search/) API allows you to look up a location from a textual description or address. Just like the Nominatim API, the foreign data wrapper supports [structured](https://nominatim.org/release-docs/develop/api/Search/#structured-query) and [free-form](https://nominatim.org/release-docs/develop/api/Search/#free-form-query) search queries, which are distinguished by either spliting the address components into different paramenteres, such as `street`, `county`, `state`, or  providing a single string in the parameter `q`.

**Availability**: 1.0.0

**Synopsis**

*SETOF Record* nominatim_search(*parameters*)

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
| `limit` | optional | limits the maximum number of returned results (default `10`) |
| `addressdetails` | optional | includes a breakdown of the address into elements (default `false`) |
| `extratags` | optional | additional information in the result that is available in the database, e.g. wikipedia link, opening hours. (default `false`) |
| `namedetails` | optional | include a full list of names for the result. (default `false`) |
| `accept_language` | optional | language string as in "Accept-Language" HTTP header (default `en_US`). This overrides the `accept_language` set in the `CREATE SERVER` statement |
| `countrycodes` | optional | comma-separated list of [country codes](https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2) (default *unset*) |
| `layer` | optional | comma-separated list of: `address`, `poi`, `railway`, `natural`, `manmade` (default *unset*) |
| `featureType` | optional | one of: `country`, `state`, `city`, `settlement` (default *unset*) |
| `exclude_place_ids` | optional | comma-separeted list of place ids (default *unset*) |
| `viewbox` | optional | bounding box as in `<x1>,<y1>,<x2>,<y2>` (default *unset*) |
| `bounded` | optional | When `bounded` is set to `true` and the `viewbox` is small enough, then an amenity-only search is allowed. Give the special keyword for the amenity in square brackets, e.g. [pub] and a selection of objects of this type is returned. There is no guarantee that the result returns all objects in the area. (default `false`) |
| `polygon_type` | optional | one of: `polygon_geojson`, `polygon_kml`, `polygon_svg`, `polygon_text (default *unset*) |
| `polygon_treshold` | optional | floating-point number (default `0.0`) |
| `email` | optional | valid email address (default *unset*) |
| `dedupe` | optional | discards duplicated entries (default `true`) |

As in the Nominatim API, the free-form query string parameter `q` cannot be combined with the parameters `amenity`, `street`, `city`, `county`, `state`, `country` and `postalcode, as they are used in structured calls.


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

  osm_id   |       ref       |    lon    |    lat     |                boundingbox                
-----------+-----------------+-----------+------------+-------------------------------------------
 121736959 | Theater Münster | 7.6293918 | 51.9648162 | 51.9644060,51.9652417,7.6286897,7.6304381
(1 row)
```

Structured search

```sql
SELECT osm_id, ref, lon, lat, boundingbox 
FROM nominatim_search(server_name => 'osm', 
                      street => 'neubrückenstraße 63', 
                      city => 'münster');

  osm_id   |       ref       |    lon    |    lat     |                boundingbox                
-----------+-----------------+-----------+------------+-------------------------------------------
 121736959 | Theater Münster | 7.6293918 | 51.9648162 | 51.9644060,51.9652417,7.6286897,7.6304381
(1 row)
```
 Strcutured search with `extratags`

```sql
SELECT osm_id, ref, lon, lat, jsonb_pretty(extratags) AS extratags 
FROM nominatim_search(server_name => 'osm', 
                      street => 'neubrückenstraße 63', 
                      city => 'münster',
                      extratags => true);
  osm_id   |       ref       |    lon    |    lat     |                                               extratags                                                
-----------+-----------------+-----------+------------+--------------------------------------------------------------------------------------------------------
 121736959 | Theater Münster | 7.6293918 | 51.9648162 | {                                                                                                     +
           |                 |           |            |     "image": "https://upload.wikimedia.org/wikipedia/commons/6/64/Muenster_Stadttheater_%2881%29.JPG",+
           |                 |           |            |     "layer": "-1",                                                                                    +
           |                 |           |            |     "toilets": "customers",                                                                           +
           |                 |           |            |     "building": "civic",                                                                              +
           |                 |           |            |     "location": "surface",                                                                            +
           |                 |           |            |     "wikidata": "Q2415904",                                                                           +
           |                 |           |            |     "wikipedia": "de:Theater Münster",                                                                +
           |                 |           |            |     "roof:shape": "flat",                                                                             +
           |                 |           |            |     "start_date": "1956",                                                                             +
           |                 |           |            |     "wheelchair": "yes",                                                                              +
           |                 |           |            |     "contact:fax": "+49 251 5909202",                                                                 +
           |                 |           |            |     "roof:colour": "#F5F5DC",                                                                         +
           |                 |           |            |     "contact:email": "info-theater@stadt-muenster.de",                                                +
           |                 |           |            |     "contact:phone": "+49 251 5909205",                                                               +
           |                 |           |            |     "roof:material": "gravel",                                                                        +
           |                 |           |            |     "building:colour": "silver",                                                                      +
           |                 |           |            |     "building:levels": "2",                                                                           +
           |                 |           |            |     "contact:website": "https://www.theater-muenster.com/start/index.html",                           +
           |                 |           |            |     "building:material": "concrete",                                                                  +
           |                 |           |            |     "construction_date": "1956",                                                                      +
           |                 |           |            |     "wikimedia_commons": "Category:Theater Münster",                                                  +
           |                 |           |            |     "toilets:wheelchair": "yes"                                                                       +
           |                 |           |            | }
(1 row)

```

#### [Nominatim_Reverse](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#nominatim_reverse)

**Description**

[Reverse](https://nominatim.org/release-docs/develop/api/Reverse/) geocoding generates an address from a coordinate given as latitude and longitude. The reverse geocoding API does not exactly compute the address for the coordinate it receives. It works by finding the closest suitable OSM object and returning its address information. This may occasionally lead to unexpected results.

**Availability**: 1.0.0

**Synopsis**

*SETOF Record* nominatim_reverse(*parameters*)

**Parameters**

| Parameter | Type | Description |
|---|---|---
| `server_name` | **required** | Foreign Data Wrapper server created using the `CREATE SERVER` statement. |
| `addressdetails` | optional | includes a breakdown of the address into elements (default `false`) |
| `extratags` | optional | additional information in the result that is available in the database, e.g. wikipedia link, opening hours. (default `false`) |
| `namedetails` | optional | includes a full list of names for the result. (default `false`) |
| `accept_language` | optional | language string as in "Accept-Language" HTTP header (default `en_US`). This overrides the `accept_language` set in the `CREATE SERVER` statement |
| `zoom` | optional | Level of detail required for the address. This is a number that corresponds roughly to the zoom level used in XYZ tile sources in frameworks like Leaflet.js, Openlayers etc. In terms of address details the zoom levels are as follows: `3` country, `5` state, `8` county, `10` city, `12` town / borough, `13` village / suburb, `14` neighbourhood, `15` any settlement, `16` major streets, `17` major and minor streets, `18` building (default `18`) |
| `layer` | optional | comma-separated list of: `address`, `poi`, `railway`, `natural`, `manmade` (default *unset*) |
| `polygon_type` | optional | one of: `polygon_geojson`, `polygon_kml`, `polygon_svg`, `polygon_text` (default *unset*) |
| `polygon_treshold` | optional | floating-point number. When one of the `polygon_*` outputs is chosen, return a simplified version of the output geometry. The parameter describes the tolerance in degrees with which the geometry may differ from the original geometry. Topology is preserved in the geometry. (default `0.0`) |
| `email` | optional | In case you're using the public Nominatim service: If you are making large numbers of requests, please include an appropriate email address to identify your requests. See Nominatim's [Usage Policy](https://operations.osmfoundation.org/policies/nominatim/) for more details. You may ignore this parameter if you're hosting your server (default *unset*) |

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
SELECT osm_id, result, boundingbox
FROM nominatim_reverse(
        server_name => 'osm', 
        lon => 7.6293,
        lat => 51.9648,        
        extratags => true);

  osm_id   |                                                          result                                                          |                boundingbox                
-----------+--------------------------------------------------------------------------------------------------------------------------+-------------------------------------------
 121736959 | Theater Münster, 63, Neubrückenstraße, Martini, Altstadt, Münster-Mitte, Münster, North Rhine-Westphalia, 48143, Germany | 51.9644060,51.9652417,7.6286897,7.6304381
(1 row)
```

#### [Nominatim_Lookup](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#nominatim_lookup)

**Description**

The [lookup](https://nominatim.org/release-docs/develop/api/Lookup/) API allows to query the address and other details of one or multiple OSM objects like node, way or relation.

**Availability**: 1.0.0

**Synopsis**

*SETOF Record* nominatim_lookup(*parameters*)

**Parameters**

| Parameter | Type | Description |
|---|---|---
| `server_name` | **required** | Foreign Data Wrapper server created using the [CREATE SERVER](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#create_server) statement. |
| `addressdetails` | optional | includes a breakdown of the address into elements (default `false`) |
| `extratags` | optional | additional information in the result that is available in the database, e.g. wikipedia link, opening hours. (default `false`) |
| `namedetails` | optional | include a full list of names for the result. (default `false`) |
| `accept_language` | optional | language string as in "Accept-Language" HTTP header (default `en_US`). This overrides the `accept_language` set in the `CREATE SERVER` |
| `polygon_type` | optional | one of: `polygon_geojson`, `polygon_kml`, `polygon_svg`, `polygon_text (default *unset*) |
| `polygon_treshold` | optional | floating-point number (default `0.0`) |
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

  osm_id   |                                                       display_name                                                       
-----------+--------------------------------------------------------------------------------------------------------------------------
 121736959 | Theater Münster, 63, Neubrückenstraße, Martini, Altstadt, Münster-Mitte, Münster, North Rhine-Westphalia, 48143, Germany
(1 row)
```

#### [nominatim_fdw_version](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#version)

**Description**

Shows the version of the installed `nominatim_fdw` and its main libraries.

**Availability**: 1.0.0

**Synopsis**

*text* **nominatim_fdw_version**();

**Usage**

```sql
SELECT nominatim_fdw_version();
                                                                               nominatim_fdw_version                                                                               
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 nominatim_fdw = 1.0.0, libxml/2.9.10 libcurl/7.74.0 OpenSSL/1.1.1w zlib/1.2.11 brotli/1.0.9 libidn2/2.3.0 libpsl/0.21.0 (+libidn2/2.3.0) libssh2/1.9.0 nghttp2/1.43.0 librtmp/2.3
(1 row)
```

## [Deploy with Docker](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#deploy-with-docker)

To deploy `nominatim_fdw` with docker just pick one of the supported PostgreSQL versions, install the [requirements](#requirements) and [compile](#build-and-install) the [source code](https://github.com/jimjonesbr/nominatim_fdw/releases). For instance, a `nominatim_fdw` `Dockerfile` for PostgreSQL 15 should look like this (minimal example):

```dockerfile
FROM postgres:15

RUN apt-get update && \
    apt-get install -y make gcc postgresql-server-dev-15 libxml2-dev libcurl4-openssl-dev

RUN tar xvzf nominatim_fdw-[VERSION].tar.gz && \
    cd nominatim_fdw-[VERSION] && \
    make -j && \
    make install
```

To build the image save it in a `Dockerfile` and  run the following command in the root directory - this will create an image called `nominatim_fdw_image`.:
 
```bash
 $ docker build -t nominatim_fdw_image .
```

After successfully building the image you're ready to `run` or `create` the container ..
 
```bash
$ docker run --name my_container -e POSTGRES_HOST_AUTH_METHOD=trust nominatim_fdw_image
```

.. and then finally you're able to create and use the extension!

```bash
$ docker exec -u postgres my_container psql -d mydatabase -c "CREATE EXTENSION nominatim_fdw;"
```

### [For testers and developers](https://github.com/jimjonesbr/nominatim_fdw/blob/master/README.md#for-testers-and-developers)

Deploying the latest development version straight from the source:

Dockerfile


```dockerfile
FROM postgres:15

RUN apt-get update && \
    apt-get install -y git make gcc postgresql-server-dev-15 libxml2-dev libcurl4-openssl-dev

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