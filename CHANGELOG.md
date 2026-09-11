# 2.2
Release date: **unreleased**

## Bug Fixes

* **Fixed queries not being cancellable during an HTTP request**: `CURLOPT_XFERINFOFUNCTION` was registered without clearing `CURLOPT_NOPROGRESS`, which defaults to `1` and disables libcurl's progress machinery entirely - the callback was therefore never invoked and the `CHECK_FOR_INTERRUPTS()` inside it never ran. A `nominatim_search`, `nominatim_lookup` or `nominatim_reverse` call could not be interrupted with `pg_cancel_backend()` or `Ctrl+C` and blocked the backend until the server replied or `connect_timeout` (default `300` seconds) expired. `CURLOPT_NOPROGRESS` is now explicitly set to `0`.
* **Fixed leak of the libcurl handle on interrupted requests**: the progress callback raised the interrupt itself via `CHECK_FOR_INTERRUPTS()`, so `ereport(ERROR)` would `longjmp` out of libcurl's own call stack, skipping `curl_easy_cleanup()` and leaking the easy handle and its socket for the lifetime of the backend. The callback now returns a non-zero value instead, which aborts the transfer with `CURLE_ABORTED_BY_CALLBACK`, and the pending interrupt is processed after the handle has been released. The request/retry loop is additionally wrapped in `PG_TRY()`/`PG_CATCH()` so that the handle is also released when an error is raised from a write callback (e.g. on out-of-memory). Aborted transfers and interrupts arriving during the inter-retry sleep no longer consume the `max_connect_retry` attempts.

# 2.1
Release date: **2026-07-24**

## Enhancements

* **Add HTTP basic authentication in `USER MAPPING`**: This feature defines a mapping of a PostgreSQL user to an user in the target Nominatim server - `user` and `password`, so that the user can be authenticated.

## Bug fixes

* **Fixed invalid libcurl lifecycle**: Initialize libcurl's global state once per backend via `_PG_init()` (`curl_global_init`). Previously the wrapper relied on the implicit initialization performed by `curl_easy_init()`, which libcurl documents as **not thread-safe** and unsafe when the address space is shared with other libcurl-using extensions (e.g. `rdf_fdw`).

# 2.0
Release date: **2026-07-07**

## Enhancements

* Add error message for invalid coordinate pairs: this adds a check on the reverse call to reject invalid coordinate pairs before sending the request to the server, therefore avoiding a HTTP request that is doomed to fail.
* Add `email` and `polygon_threshold` parameters to reverse function.
* Add support to PostgreSQL 10 and 11 (EOL'd versions).
* Add system view `nominatim_fdw_settings` to list all library dependencies.

## Bug fixes

* Fixed memory leaks in XML parsing: `xmlGetProp()` and `xmlNodeGetContent()` return libxml2-heap-allocated strings that were never freed with `xmlFree()`. Introduced `xml_get_prop()` and `xml_node_content()` helper functions that copy the result into palloc'd memory and immediately free the libxml2 string, making ownership clear at a glance.
* Fixed JSON injection in `extratags`, `namedetails`, `addressdetails`, and `addressparts` fields: XML values from the Nominatim response were embedded into hand-crafted JSON strings without escaping, so values containing `"`, `\`, or control characters produced malformed `jsonb` or allowed content injection from a malicious server. PostgreSQL's own `escape_json()` (from `utils/json.h`) is now used to escape all keys and values before they are appended.
* Fixed `nominatim_search`, `nominatim_lookup`, and `nominatim_reverse` incorrectly declared as `IMMUTABLE`, which allowed PostgreSQL to cache or optimize away repeated calls and return stale results. Functions are now correctly declared `VOLATILE`.
* Fixed build failure when specifying a custom `PG_CONFIG` pointing to a PostgreSQL installation built without `--with-libxml`. The Makefile now uses `PG_CPPFLAGS` (instead of `CFLAGS`) and explicitly includes `xml2-config --cflags`, so libxml2 include paths are always passed to the compiler regardless of which `pg_config` is used.
* Add missing `type` attribute: the custom data type `NominatimRecord` was missing the attribute `type`. Thid has been now fixed.
* Fix `DEFAULT` value for `addressdetails`: it now defaults to `true`, as defined in the API spec.
* Set `DEFAULT` value of reverse's `zoom` to `-1` (disabled): the previous value was 0, which is a valid zoom level.
* Fix parsing of `KML` geometries iun reverse calls: the parser was ignoring this format and returning `NULL` for `polygon_kml` requests. This is now fixed.

## Breaking changes
* Add `entrances` column to lookup, search, and reverse calls.
* Rename reverse's column `result` to `display_name`: the previous name was mimicing the xml node retrieved from the API, which was inconsistent with the lookup and search functions.
* For simplicity, `nominatim_fdw_version()` now omits ssl, zblib, libSSH, and ngt http2 versions.
* Rename `addressparts` column from reverse function to `addressdetails`, so that it aligns with search and lookup.

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
