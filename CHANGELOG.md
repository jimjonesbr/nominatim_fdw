# 2.4
Release date: **unreleased**

## Bug Fixes

* **Fixed the build with libcurl older than 7.66**: `nghttp2_version` is only read when libcurl provides it (e.g. not on RHEL / Rocky Linux 8).
* **Fixed retries of a proxy's refusal to open the tunnel**: a 4xx answer to `CONNECT` (e.g. a wrong proxy password) is now reported immediately instead of being retried.
* **Fixed a cancel during the retry wait being reported as an invalid response**: it is now reported as a cancellation.

## Improvements

* **Longer waits between retries**: retries now back off exponentially (5, 10, 20, ... seconds, capped at 5 minutes) instead of waiting a fixed second. `Retry-After` is honoured on 503 as well as 429, and never for less than a second. The retry warning shows how long the next wait is.

# 2.3
Release date: **2026-09-28**

## Bug Fixes

* **Fixed header callback reading past the provided buffer**: `HeaderCallbackFunction()` logged each header with `"%s"`, reading until a NUL byte and ignoring the length libcurl provided. Now uses `"%.*s"` to print exactly the bytes handed over.
* **Fixed curl handle leaks on errors during request setup**: Handle creation was not wrapped in `PG_TRY`, so errors raised during parameter conversion or header construction bypassed `curl_easy_cleanup()`. Handle is now tied to the current memory context with a reset callback, releasing it on any error path.
* **Fixed libxml2 string leaks on encoding conversion errors**: `xml_get_prop()` and `xml_node_content()` converted strings before freeing them, so encoding errors raised before `xmlFree()` was called, leaking the libxml2-heap string. Both now convert and free on all paths, including error paths.
* **Fixed entrances losing their tags**: The parser read `<entrance>` attributes but ignored child `<tag>` elements. Tags are now collected into an "extratags" object per entrance.
* **Fixed control bytes in error messages**: Error bodies containing NUL or control bytes were either cut at the first NUL or included raw in the log. Bodies are now sanitized byte-by-byte, with multi-byte UTF-8 sequences kept intact and non-printable bytes shown as `?`.
* **Fixed acceptance of non-Nominatim responses**: HTTP 200 responses were parsed without checking the root element, so a misconfigured endpoint returning JSON, empty body, or HTML went silently undetected. Parsers now validate root elements and report unexpected content.
* **Fixed `polygon_threshold` losing precision and accepting invalid values**: Formatted with `"%f"` (six decimal places), it was rounded; `NaN`, infinity and negative values were forwarded as-is. Now formatted with `"%.15g"` and validated to reject non-finite and negative values.
* **Fixed `addressdetails=0` not being sent for `/reverse` and `/lookup`**: These endpoints default to `addressdetails=1`, so `nominatim_reverse(addressdetails => false)` and `nominatim_lookup(addressdetails => false)` made the server compute the breakdown anyway. Both endpoints now explicitly request `addressdetails=0`.
* **Fixed request URLs with query strings or fragments**: The URL was built as `<url> + "/" + <endpoint> + "?" + <params>`. A `url` containing a query string got the endpoint appended to the query instead of the path, and fragments corrupted the request. URLs are now parsed and reassembled with `curl_url()`, keeping existing query strings and appending the endpoint to the path.
* **Fixed validator accepting non-HTTP schemes for `url`**: `file://`, `ftp://`, ... were accepted and failed only at first use. Validator now rejects any scheme other than `http` and `https`.
* **Fixed retries on permanent transport errors**: `IsRetryable()` treated every libcurl error except `CURLE_ABORTED_BY_CALLBACK` as transient. Certificate verification failures, malformed URLs, unsupported protocols, etc. now fail immediately. Retries are reserved for network-level failures (DNS, connect, timeouts).
* **Fixed `nominatim_reverse()` accepting `NaN` coordinates**: Range checks like `lat > 90.0` are always false for `NaN`, so invalid coordinates passed validation. Now explicitly check `isnan()`.
* **Fixed header injection via `accept_language`**: Control characters in `accept_language` were pasted verbatim into HTTP headers, allowing CR/LF injection. New `CheckAcceptLanguage()` rejects control characters in both the parameter and the server option.
* **Fixed encoding mismatches**: Request parameters were percent-encoded in the database encoding instead of UTF-8; response values were stored as raw UTF-8 bytes, producing mojibake in non-UTF-8 databases. Parameters are now converted to UTF-8 before escaping, and response values are converted from UTF-8 to the database encoding.
* **Fixed format string mismatches**: Several `elog()` calls passed arguments of mismatched types: `"%ld"` for `uint64`, `"%ld"` for `size_t`, `"%u"` for `Oid`. Now use appropriate format specifiers and casts.
* **Fixed `zoom` out-of-range values being applied inconsistently**: Values above 18 were sent as-is, values below -1 were dropped. Both are now clamped to 0 or 18 with a `WARNING` naming which value is used; `-1` (disabled) stays unchanged.
* **Fixed oversized responses failing inside libcurl**: `WriteMemoryCallback()` raised "invalid memory alloc request size" from within libcurl's frames, making it impossible to distinguish from a real OOM. Responses now refuse data beyond a per-buffer limit by returning a short count, causing libcurl to abort cleanly with `CURLE_WRITE_ERROR`. Oversized responses are not retried.

## Enhancements

* **Added `max_response_size` server option**: maximum size in bytes of a response body (default `0`, unlimited). A larger response is aborted as soon as it exceeds the limit, and the query fails with "response exceeds max_response_size limit of ... bytes", a hint to raise the limit, and `ERRCODE_PROGRAM_LIMIT_EXCEEDED`. Independently of the option, a response can never exceed 1 GB, the most PostgreSQL can hold in a single buffer.

## Breaking changes

* **The query functions require the `USAGE` privilege on the foreign server**: roles that used a server without it now get `permission denied for foreign server ...` (see *Security* below). Grant it where it is needed: `GRANT USAGE ON FOREIGN SERVER osm TO some_role;`.
* **Invalid `polygon_threshold` values are rejected**: negative, `NaN` and infinite values now raise an error instead of being forwarded to the server.
* **`zoom` values below `-1` now mean country level**: they are clamped to `0`, as the server does, instead of being dropped - which made the server fall back to building level (`18`).

## Security

* **Added USAGE privilege check on foreign servers**: The query functions (`nominatim_search`, `nominatim_lookup`, `nominatim_reverse`) looked up the server by name without verifying that the caller has `USAGE` on it. This bypassed the ACL checks that `CREATE FOREIGN TABLE` enforces, allowing any role to send requests through any server — including with credentials of a `PUBLIC` user mapping. `InitSession()` now checks `ACL_USAGE` and raises the standard "permission denied" error.

## Improvements

* **Made upgrade paths produce identical objects to fresh installs**: Upgrades from 1.x and 2.2 left `NominatimRecord`'s column order and `nominatim_fdw_version()`'s function properties different from a fresh install. Now consistent across all paths.
* **Fixed query cancellation sensitivity**: The request and retry loop gave up on any `InterruptPending` flag, including non-cancellation interrupts like `pg_log_backend_memory_contexts()`, causing queries to fail when they should continue. Now checks `QueryCancelPending` and `ProcDiePending` explicitly.
* **Removed always-on verbose curl logging**: `CURLOPT_VERBOSE` was on unconditionally, so every request was parsed for `DEBUG3`-level output and discarded. Now only enabled when `DEBUG3` would actually be logged.
* **Removed reading of non-existent reverse parser fields**: `ParseNominatimReverseData()` read `class`, `type` and `importance` from `<result>`, which Nominatim's reverse endpoint never sends. Stopped reading them.
* **Improved debug output**: DEBUG2 messages now log actual option values instead of the option names.
* **Updated README**: Clarified libxml2 minimum version (2.6.0, not 2.5.0), fixed example parameters, documented `zoom` clamping, noted that `icon` is always `NULL` in reverse results.
* **Made regression tests opt-in for network access**: Tests against a Nominatim server now require `INCLUDE_EXTERNAL_TESTS=1`, preventing timeouts in isolated package-build environments.
* **Made builds reproducible**: the build date reported by `nominatim_fdw_settings()` honours `SOURCE_DATE_EPOCH`, which package builds set, instead of always taking the wall clock.
* **Passed `long` values to the libcurl options that expect them**: `CURLOPT_PROXYTYPE` and `CURLOPT_PROTOCOLS` were handed `int` constants with libcurl releases before 8, which `curl_easy_setopt()` reads as `long`.

# 2.2
Release date: **2026-09-10**

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
