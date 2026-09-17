# 2.2
Release date: **unreleased**

## Bug Fixes

* **Fixed queries not being cancellable during an HTTP request**: `CURLOPT_XFERINFOFUNCTION` was registered without clearing `CURLOPT_NOPROGRESS`, which defaults to `1` and disables libcurl's progress machinery entirely - the callback was therefore never invoked and the `CHECK_FOR_INTERRUPTS()` inside it never ran. A `nominatim_search`, `nominatim_lookup` or `nominatim_reverse` call could not be interrupted with `pg_cancel_backend()` or `Ctrl+C` and blocked the backend until the server replied or `connect_timeout` (default `300` seconds) expired. `CURLOPT_NOPROGRESS` is now explicitly set to `0`.
* **Fixed leak of the libcurl handle on interrupted requests**: the progress callback raised the interrupt itself via `CHECK_FOR_INTERRUPTS()`, so `ereport(ERROR)` would `longjmp` out of libcurl's own call stack, skipping `curl_easy_cleanup()` and leaking the easy handle and its socket for the lifetime of the backend. The callback now returns a non-zero value instead, which aborts the transfer with `CURLE_ABORTED_BY_CALLBACK`, and the pending interrupt is processed after the handle has been released. The request/retry loop is additionally wrapped in `PG_TRY()`/`PG_CATCH()` so that the handle is also released when an error is raised from a write callback (e.g. on out-of-memory). Aborted transfers and interrupts arriving during the inter-retry sleep no longer consume the `max_connect_retry` attempts.
* **Fixed missing libxml2 linkage**: the Makefile passed `xml2-config --cflags` to the compiler but never `xml2-config --libs` to the linker, so `nominatim_fdw.so` was built with eleven unresolved libxml2 symbols and no `libxml2` entry in its dynamic dependencies. This went unnoticed because the symbols happen to be provided by the backend itself whenever PostgreSQL was built with `--with-libxml`; against a PostgreSQL built without it, `CREATE EXTENSION` failed with an undefined-symbol error while loading the module. `libxml2` is now linked explicitly.
* **Fixed `max_connect_redirect` of `0` allowing unlimited redirects**: the redirect limit was only handed to libcurl when the configured value was non-zero, so `0` - the one value that unmistakably means "do not follow any redirect" - was the only value that left `CURLOPT_MAXREDIRS` at its default of `-1`. A server redirecting in a loop was followed for 30 hops instead of none. The limit is now always applied, and `0` makes the request fail on the first redirect.
* **Fixed the server's error message being discarded on failed requests**: `CURLOPT_FAILONERROR` made libcurl throw away the response body of a `4xx`/`5xx` answer, which is precisely where Nominatim explains what it objected to. A rejected request surfaced as `The requested URL returned error: 400` and nothing else. HTTP status handling is now done by the wrapper, and the response body is reported as part of the error detail, truncated to 512 bytes so that a large HTML error page cannot flood the logs.
* **Fixed `<error>` responses being silently reported as an empty result**: Nominatim answers some requests with HTTP 200 and an `<error>` element - an un-geocodable coordinate, for instance, yields `Unable to geocode`. The parsers ignored that element, so the caller saw zero rows and no explanation. Such responses now raise a `WARNING` carrying the server's message.
* **Fixed `Retry-After` being ignored on rate-limited requests**: response headers were collected on every request and then discarded without being read. A `429 Too Many Requests` answer is now retried after the delay the server asks for, capped at 30 seconds, instead of after a fixed one-second pause. Retries are also no longer attempted for client errors other than `429`, since an identical request would only be rejected again.
* **Fixed the inter-retry pause not reacting to query cancellation**: the one-second `pg_usleep()` between attempts neither processes nor notices interrupts, so a cancellation could sit unnoticed for up to one second per retry. The pause is now taken in 100 ms slices and abandoned as soon as an interrupt arrives.
* **Fixed `nominatim_fdw_handler()` returning an `FdwRoutine` with no callbacks**: every planner callback was left `NULL`, so a foreign table reaching the planner would have dereferenced a NULL function pointer. The handler now raises the same "FOREIGN TABLE not supported" error the validator does. No released version allowed such a table to be created, so this is a hardening fix.
* **Fixed use of `strtok()` in `IsLayerValid()`**: `strtok()` keeps its parsing state in a process-wide static buffer, which is not safe in backend code. Replaced with `strtok_r()`; the working copy of the string is also freed on the rejection path.
* **Fixed server options being parsed differently by the validator and at request time**: `connect_timeout`, `max_connect_retry` and `max_connect_redirect` were validated with `strtol()` base `0` in `nominatim_fdw_validator()` but read back with base `10` in `InitSession()`, so the two disagreed on any value that is not plain decimal. A `connect_timeout` of `'0x10'` was accepted as 16 by `CREATE SERVER` and then silently used as `0`, and `'010'` was validated as 8 but used as 10. Both readings now go through a single `ParseNonNegativeLong()` helper, which also rejects values that overflow `long` - previously accepted and clamped to `LONG_MAX`. Hexadecimal and octal notation are no longer accepted; values are always interpreted as decimal.
* **Fixed the endpoint URL being joined naively**: a `url` written with a trailing slash - a natural way to write it - produced request URLs such as `https://host//search?...`. Trailing slashes are now trimmed before the request path is appended.
* **Fixed a negative `limit_result` being silently ignored**: `nominatim_search()` dropped the parameter instead of complaining, so a caller passing a negative limit got the server default with no indication. Negative values are now rejected, consistent with how out-of-range coordinates are handled.
* **Fixed the libxml2 document leaking when parsing fails**: the response document lives in libxml2's heap rather than in a palloc context, so an error raised part-way through parsing - on a node that cannot be dumped, or on out-of-memory - abandoned it for the lifetime of the backend. Parsing is now wrapped so the document is released on the error path as well.
* **Fixed libxml2 parse diagnostics going to stderr**: an unparsable response body made libxml2 write directly to stderr, producing unstructured noise in the server log. The parser is now called with `XML_PARSE_NOERROR | XML_PARSE_NOWARNING`; these are per-call options, so no global libxml2 error handler is installed and other users of the library in the same process are unaffected.

## Breaking changes

* **`extratags`, `namedetails`, `addressdetails` and `entrances` are now `NULL` when they were not requested**: these columns were previously always populated, so a caller who left `extratags` at its default of `false` still got an empty `{}` back - indistinguishable from having asked for extra tags and the place having none. The empty object now carries that second meaning only, and "not requested" is reported as `NULL`. Queries that relied on these columns never being `NULL` - a `jsonb` operator applied directly to the column, for instance, or a `NOT NULL` assumption - need to set the corresponding parameter to `true`, or handle `NULL`.

## Security

* Pinned `CURLOPT_UNRESTRICTED_AUTH` to `0`, so that credentials from a `USER MAPPING` are never forwarded to a host the request was redirected to. This has always been libcurl's default; setting it explicitly makes the intent visible and keeps it from changing underneath the wrapper.

## Improvements

* **Changed `nominatim_search()`, `nominatim_lookup()` and `nominatim_reverse()` from `PARALLEL SAFE` to `PARALLEL RESTRICTED`**: they perform HTTP requests, and the previous marking allowed PostgreSQL to run them inside parallel workers, so one query could issue several concurrent requests to the endpoint - something public Nominatim instances explicitly ask clients not to do. Plans that previously parallelised over these functions will now run serially.
* Marked `nominatim_fdw_settings()` as `PARALLEL SAFE`, matching `nominatim_fdw_version()`. It only reports build information.
* Removed the unused `custom_params` and `proxy_type` fields from the internal session state. `custom_params` was never assigned at all, and `proxy_type` only ever held one value, making the test that guarded the proxy protocol always true. No behaviour changes.
* Removed a redundant `text_to_cstring()` call from each of the three query functions: the `accept_language` argument was converted twice per call.
* Removed the unused `request_redirect` field from the internal session state. It was hardcoded to `true` and never configurable, yet the code read as though redirects could be switched off independently of `max_connect_redirect`. Redirect behaviour is governed solely by `max_connect_redirect`, where `0` means "do not follow any redirect". No behaviour changes.

# 2.1
Release date: **2026-07-24**

## Enhancements

* **Add HTTP basic authentication in `USER MAPPING`**: This feature defines a mapping of a PostgreSQL user to an user in the target Nominatim server - `user` and `password`, so that the user can be authenticated.
* **Add `request_timeout` server option**: sets the maximum time in seconds allowed for a complete HTTP request (`CURLOPT_TIMEOUT`), defaulting to `0` (no limit). The pre-existing `connect_timeout` only bounds the connection phase, so a Nominatim server that accepted the connection and then stalled would occupy the backend indefinitely.

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
