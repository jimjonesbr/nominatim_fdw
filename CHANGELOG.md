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
