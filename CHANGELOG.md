# 1.4

Release date: **YYYY-MM-DD**

## Bug fixes

* Fixed memory leaks in XML parsing: `xmlGetProp()` and `xmlNodeGetContent()` return libxml2-heap-allocated strings that were never freed with `xmlFree()`. Introduced `xml_get_prop()` and `xml_node_content()` helper functions that copy the result into palloc'd memory and immediately free the libxml2 string, making ownership clear at a glance.
* Fixed JSON injection in `extratags`, `namedetails`, `addressdetails`, and `addressparts` fields: XML values from the Nominatim response were embedded into hand-crafted JSON strings without escaping, so values containing `"`, `\`, or control characters produced malformed `jsonb` or allowed content injection from a malicious server. PostgreSQL's own `escape_json()` (from `utils/json.h`) is now used to escape all keys and values before they are appended.
* Fixed `nominatim_search`, `nominatim_lookup`, and `nominatim_reverse` incorrectly declared as `IMMUTABLE`, which allowed PostgreSQL to cache or optimize away repeated calls and return stale results. Functions are now correctly declared `VOLATILE`.
* Fixed build failure when specifying a custom `PG_CONFIG` pointing to a PostgreSQL installation built without `--with-libxml`. The Makefile now uses `PG_CPPFLAGS` (instead of `CFLAGS`) and explicitly includes `xml2-config --cflags`, so libxml2 include paths are always passed to the compiler regardless of which `pg_config` is used.

# 1.3

Release date: **2026-04-12**

## Breaking Changes

Proxy authentication credentials moved to `USER MAPPING`: For improved security, proxy authentication credentials (proxy_user and proxy_password) must now be specified in `USER MAPPING` instead of `SERVER` options. This change prevents proxy passwords from being visible to all users with `USAGE` privilege on the foreign server, as PostgreSQL automatically hides `USER MAPPING` passwords from non-owners.

# 1.2.0

Release date: **2026-04-05**

## Bug fixes

* Fixed `lon`/`lat` values of `0.0` being omitted from reverse geocoding requests.
* Fixed memory leaks in `curl_easy_escape` calls.
* Fixed undefined behaviour from `xmlFreeNode` on document-owned nodes; replaced with `xmlFreeDoc`.
* Fixed `IsLayerValid` rejecting valid comma-separated layer lists.
* Fixed duplicate `state->amenity` assignment in `nominatim_fdw_search`.
* Fixed redundant `palloc0` for `state` in `nominatim_fdw_search` and `nominatim_fdw_lookup`.
* Fixed early (incomplete) assignment of `place->addressparts` in `ParseNominatimReverseData`.

## Security

* Enabled TLS peer verification (`CURLOPT_SSL_VERIFYPEER`).

## Improvements

* Moved `curl_global_init`/`curl_global_cleanup` to `_PG_init`/`_PG_fini` — called once per backend instead of once per request.
* Made attribute name lookup in `GetAttributeValue` consistently use `NameStr`.

# 1.1.0

Release date: **2024-11-01**

## Enhancements

* Add support to PostgreSQL 17 and 18.
