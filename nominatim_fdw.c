/**********************************************************************
 *
 * nominatim_fdw - PostgreSQL Nominatim Extension
 *
 * nominatim_fdw is free software: you can redistribute it and/or modify
 * it under the terms of the MIT Licence.
 *
 * Copyright (C) 2024-2026 Jim Jones <jim.jones@uni-muenster.de>
 *
 **********************************************************************/

#include "postgres.h"
#include "fmgr.h"
#include "foreign/fdwapi.h"
#include "utils/rel.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/reloptions.h"
#if PG_VERSION_NUM >= 120000
#include "access/table.h"
#endif
#include "foreign/foreign.h"
#include "commands/defrem.h"
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <utils/builtins.h>
#include <utils/array.h>
#include <commands/explain.h>
#include <libxml/tree.h>
#include <catalog/pg_collation.h>
#include <funcapi.h>
#include "lib/stringinfo.h"
#include <utils/lsyscache.h>
#include "utils/datetime.h"
#include "utils/json.h"
#include "utils/timestamp.h"
#include "utils/formatting.h"
#include "catalog/pg_operator.h"
#include "utils/syscache.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_user_mapping.h"
#include "catalog/pg_type.h"
#include "catalog/pg_namespace.h"
#include "utils/date.h"
#include <utils/elog.h>
#include <access/tupdesc.h>
#include "miscadmin.h"

#define FDW_VERSION "2.3-dev"

/*
 * Maximum number of bytes from an HTTP error response body to include in
 * error messages and server logs.  Prevents huge HTML error pages (e.g.
 * from misconfigured proxies) from flooding PostgreSQL logs.
 */
#define NOMINATIM_FDW_MAX_ERROR_BODY 512

/*
 * Upper bound, in seconds, for the delay a server may request through the
 * "Retry-After" response header. Keeps a misbehaving or hostile endpoint
 * from parking a backend for an arbitrary amount of time.
 */
#define NOMINATIM_MAX_RETRY_AFTER 30

#define REQUEST_SUCCESS 0
#define REQUEST_FAIL -1
#define NOMINATIM_DEFAULT_CONNECTTIMEOUT 300
#define NOMINATIM_DEFAULT_REQUEST_TIMEOUT 0
#define NOMINATIM_DEFAULT_MAXRETRY 3
#define NOMINATIM_DEFAULT_MAXREDIRECT 1
#define NOMINATIM_DEFAULT_LANGUAGE "en-US,en;q=0.9"

#define NOMINATIM_REQUEST_SEARCH "search"
#define NOMINATIM_REQUEST_REVERSE "reverse"
#define NOMINATIM_REQUEST_LOOKUP "lookup"
#define NOMINATIM_SERVER_OPTION_URL "url"
#define NOMINATIM_SERVER_OPTION_CONNECTTIMEOUT "connect_timeout"
#define NOMINATIM_SERVER_OPTION_REQUEST_TIMEOUT "request_timeout"
#define NOMINATIM_SERVER_OPTION_MAXCONNECTRETRY "max_connect_retry"
#define NOMINATIM_SERVER_OPTION_MAXREDIRECT "max_connect_redirect"
#define NOMINATIM_SERVER_OPTION_HTTP_PROXY "http_proxy"
#define NOMINATIM_SERVER_OPTION_LANGUAGE "accept_language"
#define NOMINATIM_USERMAPPING_OPTION_PROXYUSER "proxy_user"
#define NOMINATIM_USERMAPPING_OPTION_PROXYPASSWORD "proxy_password"
#define NOMINATIM_USERMAPPING_OPTION_USER "user"
#define NOMINATIM_USERMAPPING_OPTION_PASSWORD "password"


PG_MODULE_MAGIC;

typedef struct NominatimFDWOption
{
    const char *optname;
    Oid optcontext;   /* Oid of catalog in which option may appear */
    bool optrequired; /* Flag mandatory options */
    bool optfound;    /* Flag whether options was specified by user */
} NominatimFDWOption;

typedef struct NominatimFDWState
{
    int zoom;                  /* Level of detail required for the address. */
    int limit;                 /* Limit the maximum number of returned results. */
    char *request_type;        /* one of: search, reverse or lookup*/
    char *url;                 /* URL of the Nominatim endpoint */
    char *osm_ids;             /* a comma-separated list of OSM ids each prefixed with its type: N, W or R */
    char *amenity;             /* name and/or type of POI */
    char *street;              /* housenumber and streetname */
    char *city;                /* city */
    char *county;              /* county */
    char *state;               /* state */
    char *country;             /* country */
    char *postalcode;          /* postalcode */
    char *user;                /* User name for HTTP basic authentication */
    char *password;            /* Password for HTTP basic authentication */
    char *proxy;               /* Proxy for HTTP requests, if necessary. */
    char *proxy_user;          /* User name for proxy authentication. */
    char *proxy_user_password; /* Password for proxy authentication. */
    char *query;               /* Free-form query string to search for */
    char *layer;               /* Comma-separated list of: address, poi, railway, natural, manmade*/
    char *countrycodes;        /* Comma-separated list of country codes */
    char *feature_type;        /* One of: country, state, city, settlement */
    char *exclude_place_ids;   /* Comma-separeted list of place ids */
    char *viewbox;             /* A bbox as in <x1>,<y1>,<x2>,<y2> */
    char *polygon_type;        /* One of: polygon_geojson, polygon_text, polygon_kml or polygon_svg*/
    char *email;               /* An e-mail address to identify the requests in the server */
    char *accept_language;     /* Preferred language order for showing search results */
    bool dedupe;               /* Remove duplicates? */
    bool bounded;              /* Exclude results outside the viewbox? */
    bool extratags;            /* Include any additional information in the result that is available in the database? */
    bool namedetails;          /* Include a full list of names for the result? */
    bool addressdetails;       /* Include a breakdown of the address into elements? */
    bool entrances;            /* tagged entrances in the result? */
    long request_max_redirect; /* Limit of how many times the URL redirection (jump) may occur. */
    long connect_timeout;      /* Timeout for the connection phase, in seconds */
    long request_timeout;      /* Timeout for the complete request, in seconds (0 = disabled) */
    long max_retries;          /* Number of re-try attemtps for failed requests */
    float8 lon;                /* Longitude (x) */
    float8 lat;                /* Latitude (y) */
    float8 polygon_threshold;  /* Tolerance in degrees with which the geometry may differ from the original geometry */
    xmlDocPtr xmldoc;          /* XML document where the results from the request will be stored before parsing */
    List *records;             /* List of records retrieved from the server after parsing */
    ForeignServer *server;     /* Foreign server associated with the request */
} NominatimFDWState;

typedef struct NominatimRecord
{
    char *timestamp;
    char *attribution;
    char *querystring;
    char *polygon;
    char *exclude_place_ids;
    char *more_url;
    char *place_id;
    char *osm_type;
    char *osm_id;
    char *ref;
    char *lat;
    char *lon;
    char *boundingbox;
    char *place_rank;
    char *address_rank;
    char *display_name;
    char *class;
    char *type;
    char *importance;
    char *icon;
    char *extratags;
    char *addressdetails;
    char *namedetails;
    char *entrances;
} NominatimRecord;

struct MemoryStruct
{
    char *memory;
    size_t size;
};

static struct NominatimFDWOption valid_options[] =
    {
        /* Foreign Servers */
        {NOMINATIM_SERVER_OPTION_URL, ForeignServerRelationId, true, false},
        {NOMINATIM_SERVER_OPTION_HTTP_PROXY, ForeignServerRelationId, false, false},
        {NOMINATIM_SERVER_OPTION_CONNECTTIMEOUT, ForeignServerRelationId, false, false},
        {NOMINATIM_SERVER_OPTION_REQUEST_TIMEOUT, ForeignServerRelationId, false, false},
        {NOMINATIM_SERVER_OPTION_MAXCONNECTRETRY, ForeignServerRelationId, false, false},
        {NOMINATIM_SERVER_OPTION_MAXREDIRECT, ForeignServerRelationId, false, false},
        {NOMINATIM_SERVER_OPTION_LANGUAGE, ForeignServerRelationId, false, false},
        /* User Mapping */
        {NOMINATIM_USERMAPPING_OPTION_PROXYUSER, UserMappingRelationId, false, false},
        {NOMINATIM_USERMAPPING_OPTION_PROXYPASSWORD, UserMappingRelationId, false, false},
        {NOMINATIM_USERMAPPING_OPTION_USER, UserMappingRelationId, false, false},
        {NOMINATIM_USERMAPPING_OPTION_PASSWORD, UserMappingRelationId, false, false},
        /* EOList option */
        {NULL, InvalidOid, false, false}};

extern Datum nominatim_fdw_handler(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_validator(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_version(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_settings(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_search(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_reverse(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_lookup(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(nominatim_fdw_handler);
PG_FUNCTION_INFO_V1(nominatim_fdw_validator);
PG_FUNCTION_INFO_V1(nominatim_fdw_version);
PG_FUNCTION_INFO_V1(nominatim_fdw_settings);
PG_FUNCTION_INFO_V1(nominatim_fdw_search);
PG_FUNCTION_INFO_V1(nominatim_fdw_reverse);
PG_FUNCTION_INFO_V1(nominatim_fdw_lookup);

static Datum CreateDatum(int pgtype, int pgtypmod, char *value);
static char *GetAttributeValue(Form_pg_attribute att, struct NominatimRecord *place);
static NominatimFDWState *InitSession(const char *srvname);
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
static size_t HeaderCallbackFunction(char *contents, size_t size, size_t nmemb, void *userp);
static void ParseNominatimSearchData(NominatimFDWState *state);
static void ParseNominatimReverseData(NominatimFDWState *state);
static void ParseNominatimResponse(NominatimFDWState *state);
static int ExecuteRequest(NominatimFDWState *state);
static int CheckURL(char *url);
static bool ParseNonNegativeLong(const char *value, long *result);
static bool ReportNominatimError(xmlNodePtr root);
static bool RequestFailed(CURLcode res, long response_code);
static bool IsRetryable(CURLcode res, long response_code);
static long ParseRetryAfter(const char *headers);
static void InterruptibleSleep(long seconds);
static bool IsPolygonTypeSupported(char *polygon_type);
static bool IsLayerValid(char *layer);
static bool IsFeatureTypeValid(char *layer);
void _PG_init(void);

void _PG_init(void)
{
    /*
     * Initialize libcurl's global state once per backend process.
     * Intentionally no matching _PG_fini()/curl_global_cleanup(): this is a
     * single-threaded, long-lived backend process that may share the address
     * space with other libcurl-using extensions (e.g. rdf_fdw), and
     * _PG_fini() is not guaranteed to run on backend exit anyway. Global
     * state is reclaimed by the OS when the backend process terminates.
     */
    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
        ereport(ERROR,
                (errcode(ERRCODE_FDW_ERROR),
                 errmsg("nominatim_fdw: could not initialise libcurl")));

    xmlInitParser();
}

Datum nominatim_fdw_handler(PG_FUNCTION_ARGS)
{
    /*
     * nominatim_fdw exposes its data through the nominatim_search(),
     * nominatim_lookup() and nominatim_reverse() functions rather than
     * through foreign tables, so none of the scan callbacks are implemented.
     * Returning an FdwRoutine with all-NULL callbacks would make the planner
     * jump through a NULL function pointer the moment a foreign table reached
     * it; failing here yields a comprehensible message instead. The validator
     * already rejects CREATE FOREIGN TABLE, so this is only reachable in
     * unusual situations - IMPORT FOREIGN SCHEMA, for instance.
     */
    ereport(ERROR,
            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
             errmsg("FOREIGN TABLE not supported"),
             errdetail("The nominatim_fdw does not support FOREIGN TABLE mapping. Use the query functions instead.")));

    PG_RETURN_POINTER(NULL); /* unreachable, keeps the compiler happy */
}

/*
 * ParseNonNegativeLong
 * ----------
 *
 * Parses a server option that is expected to hold a non-negative integer.
 *
 * Both nominatim_fdw_validator() and InitSession() have to interpret these
 * options, and they must agree: a value accepted by the validator but read
 * differently at request time silently changes the behaviour of the server.
 * That used to be the case for values such as '0x10', which the validator
 * read as 16 (strtol() base 0) while InitSession() read it as 0 (base 10).
 * Keeping the parsing in one place is what guarantees they cannot diverge
 * again, so both callers must go through this function.
 *
 * value: the option value as given by the user
 * result: receives the parsed value, untouched when parsing fails
 *
 * returns true if the whole string is a valid non-negative integer
 */
static bool
ParseNonNegativeLong(const char *value, long *result)
{
    char *endptr;
    long val;

    if (!value || value[0] == '\0')
        return false;

    errno = 0;
    val = strtol(value, &endptr, 10);

    /* reject trailing garbage, out-of-range values and negative numbers */
    if (*endptr != '\0' || errno == ERANGE || val < 0)
        return false;

    *result = val;
    return true;
}

Datum nominatim_fdw_validator(PG_FUNCTION_ARGS)
{
    List *options_list = untransformRelOptions(PG_GETARG_DATUM(0));
    Oid catalog = PG_GETARG_OID(1);
    ListCell *cell;
    struct NominatimFDWOption *opt;

    if (catalog == ForeignTableRelationId)
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("FOREIGN TABLE not supported"),
                 errdetail("The nominatim_fdw does not support FOREIGN TABLE mapping. Use the query functions instead.")));

    /* Initialize found state to not found */
    for (opt = valid_options; opt->optname; opt++)
        opt->optfound = false;

    foreach (cell, options_list)
    {
        DefElem *def = (DefElem *)lfirst(cell);
        bool optfound = false;

        for (opt = valid_options; opt->optname; opt++)
        {

            if (catalog == opt->optcontext && strcmp(opt->optname, def->defname) == 0)
            {

                /* Mark that this user option was found */
                opt->optfound = optfound = true;

                if (strlen(defGetString(def)) == 0)
                    ereport(ERROR,
                            (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                             errmsg("empty value in option '%s'", opt->optname)));

                if (strcmp(opt->optname, NOMINATIM_SERVER_OPTION_URL) == 0 ||
                    strcmp(opt->optname, NOMINATIM_SERVER_OPTION_HTTP_PROXY) == 0)
                {
                    int return_code = CheckURL(defGetString(def));

                    if (return_code != REQUEST_SUCCESS)
                        ereport(ERROR,
                                (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                                 errmsg("invalid %s: '%s'", opt->optname, defGetString(def))));
                }

                if (strcmp(opt->optname, NOMINATIM_SERVER_OPTION_CONNECTTIMEOUT) == 0)
                {
                    char *timeout_str = defGetString(def);
                    long timeout_val;

                    if (!ParseNonNegativeLong(timeout_str, &timeout_val))
                        ereport(ERROR,
                                (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                                 errmsg("invalid %s: '%s'", def->defname, timeout_str),
                                 errdetail("Expected values are non-negative integers (timeout in seconds)")));
                }

                if (strcmp(opt->optname, NOMINATIM_SERVER_OPTION_REQUEST_TIMEOUT) == 0)
                {
                    char *timeout_str = defGetString(def);
                    long timeout_val;

                    if (!ParseNonNegativeLong(timeout_str, &timeout_val))
                        ereport(ERROR,
                                (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                                 errmsg("invalid %s: '%s'", def->defname, timeout_str),
                                 errdetail("Expected values are non-negative integers (timeout in seconds, 0 = disabled)")));
                }

                if (strcmp(opt->optname, NOMINATIM_SERVER_OPTION_MAXCONNECTRETRY) == 0 || strcmp(opt->optname, NOMINATIM_SERVER_OPTION_MAXREDIRECT) == 0)
                {
                    char *retry_str = defGetString(def);
                    long retry_val;

                    if (!ParseNonNegativeLong(retry_str, &retry_val))
                        ereport(ERROR,
                                (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                                 errmsg("invalid %s: '%s'", def->defname, retry_str),
                                 errdetail("Expected values are non-negative integers")));
                }
            }
        }

        if (!optfound)
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                     errmsg("invalid nominatim_fdw option '%s'", def->defname)));
    }

    for (opt = valid_options; opt->optname; opt++)
    {
        /* Required option for this catalog type is missing? */
        if (catalog == opt->optcontext && opt->optrequired && !opt->optfound)
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED),
                     errmsg("required option '%s' is missing", opt->optname)));
    }

    PG_RETURN_VOID();
}

Datum nominatim_fdw_version(PG_FUNCTION_ARGS)
{
	StringInfoData buffer;
	curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);

	initStringInfo(&buffer);

	appendStringInfo(&buffer, "nominatim_fdw %s (PostgreSQL %s",
					 FDW_VERSION,
					 PG_VERSION);

#ifdef NOMINATIM_FDW_CC
	appendStringInfo(&buffer, ", compiled by %s", NOMINATIM_FDW_CC);
#endif

	appendStringInfo(&buffer, ", libxml %s, libcurl %s)",
					 LIBXML_DOTTED_VERSION,
					 ver->version);

	PG_RETURN_TEXT_P(cstring_to_text(buffer.data));
}

Datum nominatim_fdw_settings(PG_FUNCTION_ARGS)
{
	StringInfoData buffer;
	curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);

	initStringInfo(&buffer);

	appendStringInfo(&buffer, "nominatim_fdw %s,", FDW_VERSION);
	appendStringInfo(&buffer, "PostgreSQL %s,", PG_VERSION);
	appendStringInfo(&buffer, "libxml %s,", LIBXML_DOTTED_VERSION);
	appendStringInfo(&buffer, "libcurl %s,", ver->version);

	if (ver->ssl_version)
		appendStringInfo(&buffer, "ssl %s,", ver->ssl_version);
	if (ver->libz_version)
		appendStringInfo(&buffer, "zlib %s,", ver->libz_version);
	if (ver->libssh_version)
		appendStringInfo(&buffer, "libSSH %s,", ver->libssh_version);
	if (ver->nghttp2_version)
		appendStringInfo(&buffer, "nghttp2 %s,", ver->nghttp2_version);

#ifdef NOMINATIM_FDW_CC
	appendStringInfo(&buffer, "compiled by %s,", NOMINATIM_FDW_CC);
#endif

#ifdef NOMINATIM_FDW_BUILD_DATE
	appendStringInfo(&buffer, "built on %s", NOMINATIM_FDW_BUILD_DATE);
#endif

	PG_RETURN_TEXT_P(cstring_to_text(buffer.data));
}

/*
 * nominatim_fdw_reverse
 * ----------
 * Reverse geocoding generates an address from a coordinate given as latitude
 * and longitude.
 *
 * returns SETOF NominatimRecord
 */
Datum nominatim_fdw_reverse(PG_FUNCTION_ARGS)
{
    text *srvname_text = PG_GETARG_TEXT_P(0);
    float8 lon = PG_GETARG_FLOAT8(1);
    float8 lat = PG_GETARG_FLOAT8(2);
    int zoom = PG_GETARG_INT32(3);
    text *layer = PG_GETARG_TEXT_P(4);
    bool extratags = PG_GETARG_BOOL(5);
    bool addressdetails = PG_GETARG_BOOL(6);
    bool namedetails = PG_GETARG_BOOL(7);
    text *polygon_text = PG_GETARG_TEXT_P(8);
    text *language_text = PG_GETARG_TEXT_P(9);
    bool entrances = PG_GETARG_BOOL(10);
    float8 polygon_threshold = PG_GETARG_FLOAT8(11);
    text *email_text = PG_GETARG_TEXT_P(12);

    FuncCallContext *funcctx;
    TupleDesc tupdesc;

    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext oldcontext;
        NominatimFDWState *state = InitSession(text_to_cstring(srvname_text));

        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        {
            char *language = text_to_cstring(language_text);

            if (language[0] != '\0')
                state->accept_language = language;
        }

        state->lon = lon;
        state->lat = lat;
        state->zoom = zoom;
        state->layer = text_to_cstring(layer);
        state->polygon_type = text_to_cstring(polygon_text);
        state->extratags = extratags;
        state->addressdetails = addressdetails;
        state->namedetails = namedetails;
        state->entrances = entrances;
        state->polygon_threshold = polygon_threshold;
        state->email = text_to_cstring(email_text);
        state->request_type = NOMINATIM_REQUEST_REVERSE;

        if (state->layer && !IsLayerValid(state->layer))
            ereport(WARNING,
                    (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
                     errmsg("unrecognised layer '%s'", state->layer),
                     errdetail("Known values are: address, poi, railway, natural, manmade")));

        if (!IsPolygonTypeSupported(state->polygon_type))
            ereport(WARNING, (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
                              errmsg("invalid polygon type '%s'", state->polygon_type),
                              errdetail("This parameter expects one of the following formats: polygon_geojson, polygon_kml, polygon_svg, polygon_text")));

        if (zoom < -1 || zoom > 18)
            ereport(WARNING,
                    (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                     errmsg("zoom out of range: %d", zoom),
                     errdetail("zoom must be between 0 and 18 (-1 to disable it)")));

        if (lat < -90.0 || lat > 90.0)
            ereport(ERROR,
                    (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                     errmsg("latitude out of range: %f", lat),
                     errdetail("latitude must be between -90 and 90")));

        if (lon < -180.0 || lon > 180.0)
            ereport(ERROR,
                    (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                     errmsg("longitude out of range: %f", lon),
                     errdetail("longitude must be between -180 and 180")));

        elog(DEBUG2, "\n\n\t=== %s ===\n\tlon: '%f'\n\tlat: '%f'\n\tzoom: '%d'\n\tpolygon_type: '%s'\n\tlayer: '%s'\n", __func__,
             state->lon,
             state->lat,
             state->zoom,
             state->polygon_type,
             state->layer);

        ParseNominatimResponse(state);

        funcctx->user_fctx = state->records;

        if (state->records)
            funcctx->max_calls = state->records->length;

        elog(DEBUG2, "  %s: number of records retrieved = %ld ", __func__, funcctx->max_calls);

        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                            errmsg("function returning record called in context that cannot accept type record")));
        tupdesc = BlessTupleDesc(tupdesc);

        funcctx->attinmeta = TupleDescGetAttInMetadata(tupdesc);

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();

    if (funcctx->call_cntr < funcctx->max_calls)
    {
        int natts = funcctx->attinmeta->tupdesc->natts;
        Datum *values = palloc(natts * sizeof(Datum));
        bool *nulls = palloc(natts * sizeof(bool));
        HeapTuple tuple;
        Datum result;
        NominatimRecord *place =
            (NominatimRecord *)list_nth((List *)funcctx->user_fctx,
                                        (int)funcctx->call_cntr);

        memset(nulls, 0, natts * sizeof(bool));

        for (int i = 0; i < natts; i++)
        {
            Form_pg_attribute att = TupleDescAttr(funcctx->attinmeta->tupdesc, i);
            char *value = GetAttributeValue(att, place);

            if (value)
                values[i] = CreateDatum(att->atttypid, att->atttypmod, value);
            else
                nulls[i] = true;

            elog(DEBUG2, "%s: %s = '%s'", __func__, att->attname.data, value);
        }

        elog(DEBUG2, "%s: creating heap tuple", __func__);

        tuple = heap_form_tuple(funcctx->attinmeta->tupdesc, values, nulls);
        result = HeapTupleGetDatum(tuple);

        SRF_RETURN_NEXT(funcctx, result);
    }
    else
        SRF_RETURN_DONE(funcctx);
}

/*
 * nominatim_fdw_search
 * ----------
 * Look up a location from a textual description or structured address.
 *
 * returns SETOF NominatimRecord
 */
Datum nominatim_fdw_search(PG_FUNCTION_ARGS)
{
    text *srvname_text = PG_GETARG_TEXT_P(0);
    text *query_text = PG_GETARG_TEXT_P(1);
    text *amenity_text = PG_GETARG_TEXT_P(2);
    text *street = PG_GETARG_TEXT_P(3);
    text *city = PG_GETARG_TEXT_P(4);
    text *county = PG_GETARG_TEXT_P(5);
    text *tstate = PG_GETARG_TEXT_P(6);
    text *country = PG_GETARG_TEXT_P(7);
    text *postalcode = PG_GETARG_TEXT_P(8);
    bool extratags = PG_GETARG_BOOL(9);
    bool addressdetails = PG_GETARG_BOOL(10);
    bool namedetails = PG_GETARG_BOOL(11);
    text *polygon_text = PG_GETARG_TEXT_P(12);
    text *language_text = PG_GETARG_TEXT_P(13);
    text *countrycodes_text = PG_GETARG_TEXT_P(14);
    text *layer_text = PG_GETARG_TEXT_P(15);
    text *featuretype_text = PG_GETARG_TEXT_P(16);
    text *excludeids_text = PG_GETARG_TEXT_P(17);
    text *viewbox_text = PG_GETARG_TEXT_P(18);
    bool bounded = PG_GETARG_BOOL(19);
    float8 polygon_threshold = PG_GETARG_FLOAT8(20);
    text *email_text = PG_GETARG_TEXT_P(21);
    bool dedupe = PG_GETARG_BOOL(22);
    int limit = PG_GETARG_INT32(23);
    bool entrances = PG_GETARG_BOOL(24);

    FuncCallContext *funcctx;
    TupleDesc tupdesc;
    NominatimFDWState *state;

    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext oldcontext;
        state = InitSession(text_to_cstring(srvname_text));

        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        {
            char *language = text_to_cstring(language_text);

            if (language[0] != '\0')
                state->accept_language = language;
        }

        state->query = text_to_cstring(query_text);
        state->amenity = text_to_cstring(amenity_text);
        state->street = text_to_cstring(street);
        state->city = text_to_cstring(city);
        state->county = text_to_cstring(county);
        state->state = text_to_cstring(tstate);
        state->country = text_to_cstring(country);
        state->postalcode = text_to_cstring(postalcode);
        state->polygon_type = text_to_cstring(polygon_text);
        state->countrycodes = text_to_cstring(countrycodes_text);
        state->layer = text_to_cstring(layer_text);
        state->feature_type = text_to_cstring(featuretype_text);
        state->exclude_place_ids = text_to_cstring(excludeids_text);
        state->viewbox = text_to_cstring(viewbox_text);
        state->bounded = bounded;
        state->polygon_threshold = polygon_threshold;
        state->email = text_to_cstring(email_text);
        state->dedupe = dedupe;
        state->extratags = extratags;
        state->addressdetails = addressdetails;
        state->namedetails = namedetails;
        state->limit = limit;
        state->entrances = entrances;
        state->request_type = NOMINATIM_REQUEST_SEARCH;

        if (limit < 0)
            ereport(ERROR,
                    (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE),
                     errmsg("invalid limit: %d", limit),
                     errdetail("limit must be a positive number (0 to let the server decide)")));

        if (((state->amenity && strlen(state->amenity) > 0) ||
             (state->street && strlen(state->street) > 0) ||
             (state->city && strlen(state->city) > 0) ||
             (state->county && strlen(state->county) > 0) ||
             (state->state && strlen(state->state) > 0) ||
             (state->country && strlen(state->country) > 0) ||
             (state->postalcode && strlen(state->postalcode) > 0)) &&
            state->query && strlen(state->query) > 0)
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_ERROR),
                     errmsg("bad request => structured query parameters (amenity, street, city, county, state, postalcode, country) cannot be used together with 'q' parameter")));

        if ((strlen(state->amenity) == 0 && strlen(state->street) == 0 && strlen(state->city) == 0 && strlen(state->county) == 0 && strlen(state->state) == 0 && strlen(state->country) == 0 && strlen(state->postalcode) == 0) &&
            strlen(state->query) == 0)
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_ERROR),
                     errmsg("bad request => nothing to search for."),
                     errhint("A Nominatim Search request requires either a 'q' (free form parameter) or one of the structured query parameteres (amenity, street, city, county, state, postalcode, country)")));

        if (state->layer && !IsLayerValid(state->layer))
            ereport(WARNING,
                    (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
                     errmsg("unrecognised layer '%s'", state->layer),
                     errdetail("Known values are: address, poi, railway, natural, manmade")));

        if (state->feature_type && !IsFeatureTypeValid(state->feature_type))
            ereport(WARNING,
                    (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
                     errmsg("unrecognized featureType '%s'", state->feature_type),
                     errdetail("Known values are: country, state, city, settlement.")));

        if (!IsPolygonTypeSupported(state->polygon_type))
            ereport(WARNING,
                    (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
                     errmsg("invalid polygon type '%s'", state->polygon_type),
                     errdetail("This parameter expects one of the following formats: polygon_geojson, polygon_kml, polygon_svg, polygon_text")));

        elog(DEBUG2, "\n\n\t=== %s ===\n\tq:'%s'\n\tpolygon_type: '%s'\n", __func__,
             state->query,
             state->polygon_type);

        ParseNominatimResponse(state);

        funcctx->user_fctx = state->records;

        if (state->records)
            funcctx->max_calls = state->records->length;

        elog(DEBUG2, "  %s: number of records retrieved = %ld ", __func__, funcctx->max_calls);

        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("function returning record called in context that cannot accept type record")));
        tupdesc = BlessTupleDesc(tupdesc);

        funcctx->attinmeta = TupleDescGetAttInMetadata(tupdesc);

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();

    if (funcctx->call_cntr < funcctx->max_calls)
    {
        int natts = funcctx->attinmeta->tupdesc->natts;
        Datum *values = palloc(natts * sizeof(Datum));
        bool *nulls = palloc(natts * sizeof(bool));
        HeapTuple tuple;
        Datum result;
        NominatimRecord *place =
            (NominatimRecord *)list_nth((List *)funcctx->user_fctx,
                                        (int)funcctx->call_cntr);

        memset(nulls, 0, natts * sizeof(bool));

        for (int i = 0; i < natts; i++)
        {
            Form_pg_attribute att = TupleDescAttr(funcctx->attinmeta->tupdesc, i);
            char *value = GetAttributeValue(att, place);

            if (value)
                values[i] = CreateDatum(att->atttypid, att->atttypmod, value);
            else
                nulls[i] = true;
        }

        tuple = heap_form_tuple(funcctx->attinmeta->tupdesc, values, nulls);
        result = HeapTupleGetDatum(tuple);

        SRF_RETURN_NEXT(funcctx, result);
    }
    else
        SRF_RETURN_DONE(funcctx);
}

/*
 * nominatim_fdw_lookup
 * ----------
 * Query the address and other details of one or multiple OSM objects like node,
 * way or relation.
 *
 * returns SETOF NominatimRecord
 */
Datum nominatim_fdw_lookup(PG_FUNCTION_ARGS)
{
    text *srvname_text = PG_GETARG_TEXT_P(0);
    text *osm_ids_text = PG_GETARG_TEXT_P(1);
    bool extratags = PG_GETARG_BOOL(2);
    bool addressdetails = PG_GETARG_BOOL(3);
    bool namedetails = PG_GETARG_BOOL(4);
    text *polygon_text = PG_GETARG_TEXT_P(5);
    bool entrances = PG_GETARG_BOOL(6);
    text *language_text = PG_GETARG_TEXT_P(7);
    float8 polygon_threshold = PG_GETARG_FLOAT8(8);
    text *email_text = PG_GETARG_TEXT_P(9);

    FuncCallContext *funcctx;
    TupleDesc tupdesc;
    NominatimFDWState *state;

    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext oldcontext;
        funcctx = SRF_FIRSTCALL_INIT();
        state = InitSession(text_to_cstring(srvname_text));
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        state->osm_ids = text_to_cstring(osm_ids_text);

        if (!state->osm_ids || strlen(state->osm_ids) == 0)
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_ERROR),
                     errmsg("bad request => nothing to look up."),
                     errdetail("a nominatim lookup request requires the 'osm_ids' parameter (a comma-separated list of OSM ids)")));

        state->extratags = extratags;
        state->addressdetails = addressdetails;
        state->namedetails = namedetails;
        state->polygon_type = text_to_cstring(polygon_text);
        state->entrances = entrances;

        {
            char *language = text_to_cstring(language_text);

            if (language[0] != '\0')
                state->accept_language = language;
        }

        state->polygon_threshold = polygon_threshold;
        state->email = text_to_cstring(email_text);
        state->request_type = NOMINATIM_REQUEST_LOOKUP;

        if (!IsPolygonTypeSupported(state->polygon_type))
            ereport(WARNING,
                    (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
                     errmsg("invalid polygon type '%s'", state->polygon_type),
                     errdetail("This parameter expects one of the following formats: polygon_geojson, polygon_kml, polygon_svg, polygon_text")));

        elog(DEBUG2, "\n\n\t=== %s ===\n\tosm_ids:'%s'\n\tpolygon_type: '%s'\n", __func__,
             state->osm_ids,
             state->polygon_type);

        ParseNominatimResponse(state);

        funcctx->user_fctx = state->records;

        if (state->records)
            funcctx->max_calls = state->records->length;

        elog(DEBUG2, "  %s: number of records retrieved = %ld ", __func__, funcctx->max_calls);

        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("function returning record called in context that cannot accept type record")));

        tupdesc = BlessTupleDesc(tupdesc);

        funcctx->attinmeta = TupleDescGetAttInMetadata(tupdesc);

        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();

    if (funcctx->call_cntr < funcctx->max_calls)
    {
        int natts = funcctx->attinmeta->tupdesc->natts;
        Datum *values = palloc(natts * sizeof(Datum));
        bool *nulls = palloc(natts * sizeof(bool));
        HeapTuple tuple;
        Datum result;
        NominatimRecord *place = (NominatimRecord *)list_nth((List *)funcctx->user_fctx,
                                                             (int)funcctx->call_cntr);

        memset(nulls, 0, natts * sizeof(bool));

        for (int i = 0; i < natts; i++)
        {
            Form_pg_attribute att = TupleDescAttr(funcctx->attinmeta->tupdesc, i);
            char *value = GetAttributeValue(att, place);

            if (value)
                values[i] = CreateDatum(att->atttypid, att->atttypmod, value);
            else
                nulls[i] = true;
        }

        tuple = heap_form_tuple(funcctx->attinmeta->tupdesc, values, nulls);
        result = HeapTupleGetDatum(tuple);

        SRF_RETURN_NEXT(funcctx, result);
    }
    else
        SRF_RETURN_DONE(funcctx);
}

/*
 * GetAttributeValue
 * ----------
 * Extracts the value of a given attribute and sets the correspondent property
 * in the NominatimRecord struct. It returs NULL in case of no match.
 *
 * att: a Form_pg_attribute attribute
 * place: a NominatimRecord variable
 *
 * returns SETOF NominatimRecord
 */
static char *GetAttributeValue(Form_pg_attribute att, struct NominatimRecord *place)
{

    if (strcmp(NameStr(att->attname), "osm_id") == 0)
        return place->osm_id;
    else if (strcmp(NameStr(att->attname), "osm_type") == 0)
        return place->osm_type;
    else if (strcmp(NameStr(att->attname), "ref") == 0)
        return place->ref;
    else if (strcmp(NameStr(att->attname), "class") == 0)
        return place->class;
    else if (strcmp(NameStr(att->attname), "type") == 0)
        return place->type;
    else if (strcmp(NameStr(att->attname), "display_name") == 0)
        return place->display_name;
    else if (strcmp(NameStr(att->attname), "place_id") == 0)
        return place->place_id;
    else if (strcmp(NameStr(att->attname), "place_rank") == 0)
        return place->place_rank;
    else if (strcmp(NameStr(att->attname), "address_rank") == 0)
        return place->address_rank;
    else if (strcmp(NameStr(att->attname), "lon") == 0)
        return place->lon;
    else if (strcmp(NameStr(att->attname), "lat") == 0)
        return place->lat;
    else if (strcmp(NameStr(att->attname), "boundingbox") == 0)
        return place->boundingbox;
    else if (strcmp(NameStr(att->attname), "importance") == 0)
        return place->importance;
    else if (strcmp(NameStr(att->attname), "icon") == 0)
        return place->icon;
    else if (strcmp(NameStr(att->attname), "extratags") == 0)
        return place->extratags;
    else if (strcmp(NameStr(att->attname), "timestamp") == 0)
        return place->timestamp;
    else if (strcmp(NameStr(att->attname), "attribution") == 0)
        return place->attribution;
    else if (strcmp(NameStr(att->attname), "querystring") == 0)
        return place->querystring;
    else if (strcmp(NameStr(att->attname), "polygon") == 0)
        return place->polygon;
    else if (strcmp(NameStr(att->attname), "exclude_place_ids") == 0)
        return place->exclude_place_ids;
    else if (strcmp(NameStr(att->attname), "more_url") == 0)
        return place->more_url;
    else if (strcmp(NameStr(att->attname), "addressdetails") == 0)
        return place->addressdetails;
    else if (strcmp(NameStr(att->attname), "namedetails") == 0)
        return place->namedetails;
    else if (strcmp(NameStr(att->attname), "entrances") == 0)
        return place->entrances;
    else
        return NULL;
}

/*
 * CreateDatum
 * ----------
 *
 * Creates a Datum from a given value based on the postgres types and modifiers.
 *
 * tuple: a Heaptuple
 * pgtype: postgres type
 * pgtypemod: postgres type modifier
 * value: value to be converted
 *
 * returns Datum
 */
static Datum CreateDatum(int pgtype, int pgtypmod, char *value)
{
    regproc typinput;
    HeapTuple tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(pgtype));

    if (!HeapTupleIsValid(tuple))
        ereport(ERROR,
                (errcode(ERRCODE_FDW_INVALID_DATA_TYPE),
                 errmsg("cache lookup failed for type %u", pgtype)));

    typinput = ((Form_pg_type)GETSTRUCT(tuple))->typinput;
    ReleaseSysCache(tuple);

    if (pgtype == FLOAT4OID ||
        pgtype == FLOAT8OID ||
        pgtype == NUMERICOID ||
        pgtype == TIMESTAMPOID ||
        pgtype == TIMESTAMPTZOID ||
        pgtype == VARCHAROID)
        return OidFunctionCall3(
            typinput,
            CStringGetDatum(value),
            ObjectIdGetDatum(InvalidOid),
            Int32GetDatum(pgtypmod));
    else
        return OidFunctionCall1(typinput, CStringGetDatum(value));
}

static void LoadNominatimUserMapping(NominatimFDWState *state)
{
    Datum datum;
    HeapTuple tp;
    bool isnull;
    UserMapping *um;
    List *options = NIL;
    ListCell *cell;
    bool usermatch = true;

    elog(DEBUG2, "%s called", __func__);

    tp = SearchSysCache2(USERMAPPINGUSERSERVER,
                         ObjectIdGetDatum(GetUserId()),
                         ObjectIdGetDatum(state->server->serverid));

    if (!HeapTupleIsValid(tp))
    {
        elog(DEBUG2, "%s: not found for the specific user -- try PUBLIC", __func__);
        tp = SearchSysCache2(USERMAPPINGUSERSERVER,
                             ObjectIdGetDatum(InvalidOid),
                             ObjectIdGetDatum(state->server->serverid));
    }

    if (!HeapTupleIsValid(tp))
    {
        elog(DEBUG2, "%s: user mapping not found for user \"%s\", server \"%s\"",
             __func__, MappingUserName(GetUserId()), state->server->servername);

        usermatch = false;
    }

    if (usermatch)
    {
        elog(DEBUG2, "%s: setting UserMapping", __func__);
        um = (UserMapping *)palloc(sizeof(UserMapping));
#if PG_VERSION_NUM >= 120000
		um->umid = ((Form_pg_user_mapping)GETSTRUCT(tp))->oid;
#elif PG_VERSION_NUM >= 90600
		um->umid = HeapTupleGetOid(tp);
#endif
        um->userid = GetUserId();
        um->serverid = state->server->serverid;

        elog(DEBUG2, "%s: extract the umoptions", __func__);
        datum = SysCacheGetAttr(USERMAPPINGUSERSERVER,
                                tp,
                                Anum_pg_user_mapping_umoptions,
                                &isnull);
        if (isnull)
            um->options = NIL;
        else
            um->options = untransformRelOptions(datum);

        if (um->options != NIL)
        {
            options = list_concat(options, um->options);

            foreach (cell, options)
            {
                DefElem *def = (DefElem *)lfirst(cell);

                if (strcmp(def->defname, NOMINATIM_USERMAPPING_OPTION_PROXYUSER) == 0)
                {
                    state->proxy_user = pstrdup(defGetString(def));
                    elog(DEBUG2, "%s: proxy user '%s'", __func__, def->defname);
                }
                else if (strcmp(def->defname, NOMINATIM_USERMAPPING_OPTION_PROXYPASSWORD) == 0)
                {
                    state->proxy_user_password = pstrdup(defGetString(def));
                    elog(DEBUG2, "%s: proxy password '*******'", __func__);
                }
                else if (strcmp(def->defname, NOMINATIM_USERMAPPING_OPTION_USER) == 0)
                {
                    state->user = pstrdup(defGetString(def));
                    elog(DEBUG2, "%s: user '%s'", __func__, def->defname);
                }
                else if (strcmp(def->defname, NOMINATIM_USERMAPPING_OPTION_PASSWORD) == 0)
                {
                    state->password = pstrdup(defGetString(def));
                    elog(DEBUG2, "%s: password '*******'", __func__);
                }
            }
        }

        ReleaseSysCache(tp);
    }

    elog(DEBUG2, "%s exit", __func__);
}
/*
 * InitSession
 * ----------
 *
 * This function loads all session info from a specific foreign server data
 * into a NominatimFDWState.
 *
 * srvname: foreign server's name
 *
 * returns NominatimFDWState with the loaded session values
 */
static NominatimFDWState *InitSession(const char *srvname)
{
    NominatimFDWState *state = (NominatimFDWState *)palloc0(sizeof(NominatimFDWState));
    ForeignServer *server = GetForeignServerByName(srvname, true);
    ListCell *cell;

    state->max_retries = NOMINATIM_DEFAULT_MAXRETRY;
    state->request_max_redirect = NOMINATIM_DEFAULT_MAXREDIRECT;
    state->accept_language = NOMINATIM_DEFAULT_LANGUAGE;
    state->connect_timeout = NOMINATIM_DEFAULT_CONNECTTIMEOUT;
    state->request_timeout = NOMINATIM_DEFAULT_REQUEST_TIMEOUT;

    if (!server)
        ereport(ERROR,
                (errcode(ERRCODE_CONNECTION_DOES_NOT_EXIST),
                 errmsg("FOREIGN SERVER does not exist: '%s'", srvname)));

    state->server = server;
    LoadNominatimUserMapping(state);

    elog(DEBUG2, "%s called: '%s'", __func__, srvname);

    foreach (cell, server->options)
    {
        DefElem *def = lfirst_node(DefElem, cell);

        elog(DEBUG2, "  %s parsing node '%s': %s", __func__, def->defname, defGetString(def));

        if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_URL) == 0)
            state->url = defGetString(def);

        if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_HTTP_PROXY) == 0)
        {
            state->proxy = defGetString(def);
        }

        /*
         * The numeric options below were already checked by the validator
         * when the server was created or altered. Parsing them through the
         * very same helper keeps both readings identical; the error is a
         * safety net for options that predate a validator change and would
         * otherwise be silently misread.
         */
        if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_CONNECTTIMEOUT) == 0)
        {
            if (!ParseNonNegativeLong(defGetString(def), &state->connect_timeout))
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                         errmsg("invalid %s: '%s'", def->defname, defGetString(def)),
                         errdetail("Expected values are non-negative integers (timeout in seconds)")));
        }

        if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_REQUEST_TIMEOUT) == 0)
        {
            if (!ParseNonNegativeLong(defGetString(def), &state->request_timeout))
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                         errmsg("invalid %s: '%s'", def->defname, defGetString(def)),
                         errdetail("Expected values are non-negative integers (timeout in seconds, 0 = disabled)")));
        }

        if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_MAXREDIRECT) == 0)
        {
            if (!ParseNonNegativeLong(defGetString(def), &state->request_max_redirect))
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                         errmsg("invalid %s: '%s'", def->defname, defGetString(def)),
                         errdetail("Expected values are non-negative integers")));
        }

        if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_MAXCONNECTRETRY) == 0)
        {
            if (!ParseNonNegativeLong(defGetString(def), &state->max_retries))
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                         errmsg("invalid %s: '%s'", def->defname, defGetString(def)),
                         errdetail("Expected values are non-negative integers")));
        }

        if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_LANGUAGE) == 0)
            state->accept_language = defGetString(def);
    }

    return state;
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = repalloc(mem->memory, mem->size + realsize + 1);

    if (!ptr)
        ereport(ERROR,
                (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                 errmsg("out of memory (repalloc returned NULL)")));

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}
static size_t HeaderCallbackFunction(char *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr;

    Assert(contents);

    elog(DEBUG2, "%s: header = \"%s\"", __func__, contents);

    ptr = repalloc(mem->memory, mem->size + realsize + 1);

    if (!ptr)
        ereport(ERROR,
                (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                 errmsg("[%s] out of memory (repalloc returned NULL)", __func__)));

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

/*
 * xml_get_prop / xml_node_content
 * ----------
 *
 * Wrappers around xmlGetProp / xmlNodeGetContent that copy the result
 * into palloc'd memory and immediately free the libxml2 heap string.
 * Returns NULL when the attribute or content is absent.
 */
static char *
xml_get_prop(xmlNodePtr node, const char *name)
{
    xmlChar *val = xmlGetProp(node, (xmlChar *)name);
    char *result = val ? pstrdup((char *)val) : NULL;

    xmlFree(val);
    return result;
}

static char *
xml_node_content(xmlNodePtr node)
{
    xmlChar *val = xmlNodeGetContent(node);
    char *result = val ? pstrdup((char *)val) : NULL;

    xmlFree(val);
    return result;
}

/*
 * ReportNominatimError
 * ----------
 *
 * Nominatim describes problems that it can still answer with HTTP 200 - an
 * un-geocodable coordinate, for instance - in an <error> element instead of
 * failing the request. Without surfacing it the caller just sees an empty
 * result set and no reason for it.
 *
 * root: root element of the parsed response
 *
 * returns true when an <error> element was found and reported
 */
static bool
ReportNominatimError(xmlNodePtr root)
{
    xmlNodePtr error = NULL;
    xmlNodePtr node;
    char *message = NULL;

    if (xmlStrcmp(root->name, (xmlChar *)"error") == 0)
        error = root;
    else
    {
        for (node = root->children; node != NULL; node = node->next)
        {
            if (xmlStrcmp(node->name, (xmlChar *)"error") == 0)
            {
                error = node;
                break;
            }
        }
    }

    if (!error)
        return false;

    /*
     * The search endpoint wraps the text in <message>, while the reverse
     * endpoint puts it directly into <error>.
     */
    for (node = error->children; node != NULL; node = node->next)
    {
        if (xmlStrcmp(node->name, (xmlChar *)"message") == 0)
        {
            message = xml_node_content(node);
            break;
        }
    }

    if (!message)
        message = xml_node_content(error);

    ereport(WARNING,
            (errcode(ERRCODE_FDW_ERROR),
             errmsg("nominatim server returned an error"),
             errdetail("%s", (message && message[0] != '\0')
                             ? message : "no description provided")));

    return true;
}

/*
 * RequestFailed
 * ----------
 *
 * A request counts as failed when libcurl itself reported a problem or when
 * the server answered with an HTTP status of 400 or above.
 *
 * returns boolean (true: failed, false: succeeded)
 */
static bool
RequestFailed(CURLcode res, long response_code)
{
    return res != CURLE_OK || response_code >= 400;
}

/*
 * IsRetryable
 * ----------
 *
 * Decides whether a failed request is worth repeating. Transport failures
 * and server-side errors usually are; client errors are not, as an identical
 * request would be rejected again. HTTP 429 is the exception, being an
 * explicit "come back later".
 *
 * returns boolean (true: retry, false: give up)
 */
static bool
IsRetryable(CURLcode res, long response_code)
{
    /* the transfer was cancelled: retrying would only delay the interrupt */
    if (res == CURLE_ABORTED_BY_CALLBACK)
        return false;

    if (res != CURLE_OK)
        return true;

    return response_code == 429 || response_code >= 500;
}

/*
 * ParseRetryAfter
 * ----------
 *
 * Reads the delay requested through the "Retry-After" response header. Only
 * the delta-seconds form is honoured; the HTTP-date form is rare in practice
 * and would require full date parsing.
 *
 * headers: raw response headers as collected by HeaderCallbackFunction
 *
 * returns the delay in seconds, or -1 when absent or not understood
 */
static long
ParseRetryAfter(const char *headers)
{
    const char *line = headers;

    if (!headers)
        return -1;

    while (*line != '\0')
    {
        const char *eol = line + strcspn(line, "\r\n");

        if (strncasecmp(line, "Retry-After:", sizeof("Retry-After:") - 1) == 0)
        {
            const char *value = line + sizeof("Retry-After:") - 1;
            char *endptr;
            long seconds;

            while (value < eol && (*value == ' ' || *value == '\t'))
                value++;

            errno = 0;
            seconds = strtol(value, &endptr, 10);

            if (endptr != value && errno != ERANGE && seconds >= 0)
                return seconds;

            return -1;
        }

        line = eol;
        while (*line == '\r' || *line == '\n')
            line++;
    }

    return -1;
}

/*
 * InterruptibleSleep
 * ----------
 *
 * Sleeps for the given number of seconds, in slices, so that a query
 * cancellation arriving mid-sleep is noticed promptly. pg_usleep() on its
 * own neither processes nor notices interrupts.
 *
 * seconds: how long to sleep
 */
static void
InterruptibleSleep(long seconds)
{
    for (long slice = 0; slice < seconds * 10; slice++)
    {
        if (InterruptPending)
            return;

        pg_usleep(100000L); /* 100 ms */
    }
}

/*
 * ParseNominatimReverseData
 * ----------
 *
 * Parses the XML document returned from the Nominatim reverse endpoint
 * and creates a List of NominatimRecord. The paresed records are stored
 * in state->records.
 *
 * state: NominatimFDWState containing all session data
 *
 */
static void ParseNominatimReverseData(NominatimFDWState *state)
{
    struct NominatimRecord *place;
    xmlNodePtr reversegeocode;
    xmlNodePtr tag;
    xmlNodePtr root;
    StringInfoData addressdetails;
    StringInfoData extratags;
    StringInfoData namedetails;
    StringInfoData entrances;
    bool found = false;

    elog(DEBUG2, "%s called", __func__);

    if (ExecuteRequest(state) != REQUEST_SUCCESS)
        elog(ERROR, "%s -> request failed: '%s'", __func__, state->url);

    Assert(state->xmldoc);

    root = xmlDocGetRootElement(state->xmldoc);

    if (!root)
        elog(ERROR, "unable to parse root element: '%s'", state->url);

    /* an <error> here means there is nothing to parse */
    if (ReportNominatimError(root))
    {
        xmlFreeDoc(state->xmldoc);
        state->xmldoc = NULL;
        return;
    }

    place = (struct NominatimRecord *)palloc0(sizeof(struct NominatimRecord));

    initStringInfo(&addressdetails);
    initStringInfo(&extratags);
    initStringInfo(&namedetails);
    initStringInfo(&entrances);
    appendStringInfoChar(&addressdetails, '{');
    appendStringInfoChar(&extratags, '{');
    appendStringInfoChar(&namedetails, '{');
    appendStringInfoChar(&entrances, '[');

    place->timestamp = xml_get_prop(root, "timestamp");
    place->attribution = xml_get_prop(root, "attribution");
    place->querystring = xml_get_prop(root, "querystring");

    for (reversegeocode = root->children; reversegeocode != NULL; reversegeocode = reversegeocode->next)
    {
        if (xmlStrcmp(reversegeocode->name, (xmlChar *)"result") == 0)
        {
            found = true;
            place->ref = xml_get_prop(reversegeocode, "ref");
            place->address_rank = xml_get_prop(reversegeocode, "address_rank");
            place->boundingbox = xml_get_prop(reversegeocode, "boundingbox");
            place->class = xml_get_prop(reversegeocode, "class");
            place->type = xml_get_prop(reversegeocode, "type");
            place->icon = xml_get_prop(reversegeocode, "icon");
            place->importance = xml_get_prop(reversegeocode, "importance");
            place->lat = xml_get_prop(reversegeocode, "lat");
            place->lon = xml_get_prop(reversegeocode, "lon");
            place->osm_id = xml_get_prop(reversegeocode, "osm_id");
            place->osm_type = xml_get_prop(reversegeocode, "osm_type");
            place->place_id = xml_get_prop(reversegeocode, "place_id");
            place->place_rank = xml_get_prop(reversegeocode, "place_rank");

            place->polygon = xml_get_prop(reversegeocode, "geotext");
            if (!place->polygon)
                place->polygon = xml_get_prop(reversegeocode, "geojson");
            if (!place->polygon)
                place->polygon = xml_get_prop(reversegeocode, "geosvg");

            place->display_name = xml_node_content(reversegeocode);
        }
        else if (xmlStrcmp(reversegeocode->name, (xmlChar *)"addressparts") == 0)
        {
            for (tag = reversegeocode->children; tag != NULL; tag = tag->next)
            {
                char *content = xml_node_content(tag);
                if (addressdetails.len > 1)
                    appendStringInfoChar(&addressdetails, ',');
                escape_json(&addressdetails, (char *)tag->name);
                appendStringInfoChar(&addressdetails, ':');
                escape_json(&addressdetails, content ? content : "");
            }
        }
        else if (xmlStrcmp(reversegeocode->name, (xmlChar *)"extratags") == 0)
        {
            for (tag = reversegeocode->children; tag != NULL; tag = tag->next)
            {
                char *key = xml_get_prop(tag, "key");
                char *value = xml_get_prop(tag, "value");
                if (extratags.len > 1)
                    appendStringInfoChar(&extratags, ',');
                escape_json(&extratags, key ? key : "");
                appendStringInfoChar(&extratags, ':');
                escape_json(&extratags, value ? value : "");
            }
        }
        else if (xmlStrcmp(reversegeocode->name, (xmlChar *)"geokml") == 0)
        {
            int bytes;
            xmlBufferPtr buffer = xmlBufferCreate();
            bytes = xmlNodeDump(buffer, state->xmldoc, reversegeocode->children, 0, 0);

            if (bytes == -1)
                elog(ERROR, "unable to dump XML node: '%s'", state->url);

            place->polygon = pstrdup((char *)buffer->content);
            xmlBufferFree(buffer);
        }
        else if (xmlStrcmp(reversegeocode->name, (xmlChar *)"entrances") == 0)
        {
            bool first_entrance = true;

            for (tag = reversegeocode->children; tag != NULL; tag = tag->next)
            {
                xmlAttrPtr attr;
                bool first_attr = true;

                if (!first_entrance)
                    appendStringInfoChar(&entrances, ',');
                first_entrance = false;

                appendStringInfoChar(&entrances, '{');

                for (attr = tag->properties; attr != NULL; attr = attr->next)
                {
                    char *value = xml_get_prop(tag, (const char *)attr->name);

                    if (!first_attr)
                        appendStringInfoChar(&entrances, ',');
                    first_attr = false;

                    escape_json(&entrances, (char *)attr->name);
                    appendStringInfoChar(&entrances, ':');
                    escape_json(&entrances, value ? value : "");
                }

                appendStringInfoChar(&entrances, '}');
            }
        }
        else if (xmlStrcmp(reversegeocode->name, (xmlChar *)"namedetails") == 0)
        {
            for (tag = reversegeocode->children; tag != NULL; tag = tag->next)
            {
                char *desc = xml_get_prop(tag, "desc");
                char *content = xml_node_content(tag);
                if (namedetails.len > 1)
                    appendStringInfoChar(&namedetails, ',');
                escape_json(&namedetails, desc ? desc : "");
                appendStringInfoChar(&namedetails, ':');
                escape_json(&namedetails, content ? content : "");
            }
        }
    }

    appendStringInfoChar(&addressdetails, '}');
    appendStringInfoChar(&extratags, '}');
    appendStringInfoChar(&namedetails, '}');
    appendStringInfoChar(&entrances, ']');

    /* see the note in ParseNominatimSearchData() */
    place->addressdetails = state->addressdetails ? addressdetails.data : NULL;
    place->extratags = state->extratags ? extratags.data : NULL;
    place->namedetails = state->namedetails ? namedetails.data : NULL;
    place->entrances = state->entrances ? entrances.data : NULL;

    if (found)
        state->records = lappend(state->records, place);

    xmlFreeDoc(state->xmldoc);
    state->xmldoc = NULL;
}

/*
 * ParseNominatimSearchData
 * ----------
 *
 * Parses the XML document returned from the Nominatim search and lookup
 * endpoints, and creates a List of NominatimRecord. The paresed records
 * are stored in state->records.
 *
 * state: NominatimFDWState containing all session data
 *
 */
static void ParseNominatimSearchData(NominatimFDWState *state)
{
    xmlNodePtr searchresults;
    xmlNodePtr places;
    xmlNodePtr tag;
    xmlNodePtr root;

    state->records = NIL;

    elog(DEBUG2, "%s called", __func__);

    if (ExecuteRequest(state) != REQUEST_SUCCESS)
        elog(ERROR, "%s -> request failed: '%s'", __func__, state->url);

    Assert(state->xmldoc);

    root = xmlDocGetRootElement(state->xmldoc);

    if (!root)
        elog(ERROR, "unable to parse XML document: '%s'", state->url);

    /* an <error> here means there is nothing to parse */
    if (ReportNominatimError(root))
    {
        xmlFreeDoc(state->xmldoc);
        state->xmldoc = NULL;
        return;
    }

    for (searchresults = root->children; searchresults != NULL; searchresults = searchresults->next)
    {

        if (xmlStrcmp(searchresults->name, (xmlChar *)"place") == 0)
        {

            struct NominatimRecord *place = (struct NominatimRecord *)palloc0(sizeof(struct NominatimRecord));
            StringInfoData xtags;
            StringInfoData addressdetails;
            StringInfoData namedetails;
            StringInfoData entrances;

            initStringInfo(&xtags);
            initStringInfo(&addressdetails);
            initStringInfo(&namedetails);
            initStringInfo(&entrances);

            appendStringInfo(&xtags, "{");
            appendStringInfo(&addressdetails, "{");
            appendStringInfo(&namedetails, "{");
            appendStringInfoChar(&entrances, '[');

            place->ref = xml_get_prop(searchresults, "ref");
            place->address_rank = xml_get_prop(searchresults, "address_rank");
            place->attribution = xml_get_prop(xmlDocGetRootElement(state->xmldoc), "attribution");
            place->boundingbox = xml_get_prop(searchresults, "boundingbox");
            place->class = xml_get_prop(searchresults, "class");
            place->type = xml_get_prop(searchresults, "type");
            place->display_name = xml_get_prop(searchresults, "display_name");
            place->exclude_place_ids = xml_get_prop(xmlDocGetRootElement(state->xmldoc), "exclude_place_ids");
            place->icon = xml_get_prop(searchresults, "icon");
            place->importance = xml_get_prop(searchresults, "importance");
            place->lat = xml_get_prop(searchresults, "lat");
            place->lon = xml_get_prop(searchresults, "lon");
            place->more_url = xml_get_prop(xmlDocGetRootElement(state->xmldoc), "more_url");
            place->osm_id = xml_get_prop(searchresults, "osm_id");
            place->osm_type = xml_get_prop(searchresults, "osm_type");
            place->place_id = xml_get_prop(searchresults, "place_id");
            place->place_rank = xml_get_prop(searchresults, "place_rank");

            place->polygon = xml_get_prop(searchresults, "geotext");
            if (!place->polygon)
                place->polygon = xml_get_prop(searchresults, "geojson");
            if (!place->polygon)
                place->polygon = xml_get_prop(searchresults, "geosvg");

            place->querystring = xml_get_prop(xmlDocGetRootElement(state->xmldoc), "querystring");
            place->timestamp = xml_get_prop(xmlDocGetRootElement(state->xmldoc), "timestamp");

            for (places = searchresults->children; places != NULL; places = places->next)
            {
                if (xmlStrcmp(places->name, (xmlChar *)"extratags") == 0)
                {
                    for (tag = places->children; tag != NULL; tag = tag->next)
                    {
                        char *key = xml_get_prop(tag, "key");
                        char *value = xml_get_prop(tag, "value");
                        if (xtags.len > 1)
                            appendStringInfoChar(&xtags, ',');
                        escape_json(&xtags, key ? key : "");
                        appendStringInfoChar(&xtags, ':');
                        escape_json(&xtags, value ? value : "");
                    }
                }
                else if (xmlStrcmp(places->name, (xmlChar *)"namedetails") == 0)
                {
                    for (tag = places->children; tag != NULL; tag = tag->next)
                    {
                        char *desc = xml_get_prop(tag, "desc");
                        char *content = xml_node_content(tag);
                        if (namedetails.len > 1)
                            appendStringInfoChar(&namedetails, ',');
                        escape_json(&namedetails, desc ? desc : "");
                        appendStringInfoChar(&namedetails, ':');
                        escape_json(&namedetails, content ? content : "");
                    }
                }
                else if (xmlStrcmp(places->name, (xmlChar *)"geokml") == 0)
                {
                    int bytes;
                    xmlBufferPtr buffer = xmlBufferCreate();
                    bytes = xmlNodeDump(buffer, state->xmldoc, places->children, 0, 0);

                    if (bytes == -1)
                        elog(ERROR, "unable to dump XML node: '%s'", state->url);

                    place->polygon = pstrdup((char *)buffer->content);
                    xmlBufferFree(buffer);
                }
                else if (xmlStrcmp(places->name, (xmlChar *)"entrances") == 0)
                {
                    bool first_entrance = true;

                    for (tag = places->children; tag != NULL; tag = tag->next)
                    {
                        xmlAttrPtr attr;
                        bool first_attr = true;

                        if (!first_entrance)
                            appendStringInfoChar(&entrances, ',');
                        first_entrance = false;

                        appendStringInfoChar(&entrances, '{');

                        for (attr = tag->properties; attr != NULL; attr = attr->next)
                        {
                            char *value = xml_get_prop(tag, (const char *)attr->name);

                            if (!first_attr)
                                appendStringInfoChar(&entrances, ',');
                            first_attr = false;

                            escape_json(&entrances, (char *)attr->name);
                            appendStringInfoChar(&entrances, ':');
                            escape_json(&entrances, value ? value : "");
                        }

                        appendStringInfoChar(&entrances, '}');
                    }
                }
                else
                {
                    char *content = xml_node_content(places);
                    if (addressdetails.len > 1)
                        appendStringInfoChar(&addressdetails, ',');
                    escape_json(&addressdetails, (char *)places->name);
                    appendStringInfoChar(&addressdetails, ':');
                    escape_json(&addressdetails, content ? content : "");
                }
            }

            appendStringInfo(&xtags, "}");
            appendStringInfo(&addressdetails, "}");
            appendStringInfo(&namedetails, "}");
            appendStringInfoChar(&entrances, ']');

            /*
             * Leave the column NULL when the detail was not requested. The
             * server simply omits those elements, so building the JSON here
             * would yield an empty object indistinguishable from "requested,
             * and the place genuinely has none" - which is a real answer and
             * still comes back as {} (or [] for entrances).
             */
            place->extratags = state->extratags ? xtags.data : NULL;
            place->addressdetails = state->addressdetails ? addressdetails.data : NULL;
            place->namedetails = state->namedetails ? namedetails.data : NULL;
            place->entrances = state->entrances ? entrances.data : NULL;

            state->records = lappend(state->records, place);
        }
    }

    xmlFreeDoc(state->xmldoc);
    state->xmldoc = NULL;
}

/*
 * ParseNominatimResponse
 * ----------
 *
 * Runs the parser that matches the request type, guaranteeing that the
 * libxml2 document is released even when parsing raises an error.
 *
 * The document lives in libxml2's own heap rather than in a palloc context,
 * so unwinding an error would otherwise abandon it for the lifetime of the
 * backend. The parsers can fail part-way through - on a node that cannot be
 * dumped, or simply on out-of-memory - which is exactly when the leak would
 * happen.
 *
 * state: NominatimFDWState containing all session data
 */
static void
ParseNominatimResponse(NominatimFDWState *state)
{
    PG_TRY();
    {
        if (strcmp(state->request_type, NOMINATIM_REQUEST_REVERSE) == 0)
            ParseNominatimReverseData(state);
        else
            ParseNominatimSearchData(state);
    }
    PG_CATCH();
    {
        if (state->xmldoc)
        {
            xmlFreeDoc(state->xmldoc);
            state->xmldoc = NULL;
        }
        PG_RE_THROW();
    }
    PG_END_TRY();
}

static void AppendUrlParam(StringInfo buf, CURL *curl, const char *param, const char *value)
{
    char *escaped = curl_easy_escape(curl, value, 0);
    appendStringInfo(buf, "%s=%s&", param, escaped);
    curl_free(escaped);
}

/*
 * IsSensitiveHeader
 * -----------------
 * Returns the field-name of a sensitive HTTP header if `line` begins with
 * one, or NULL otherwise.  Comparison is case-insensitive (RFC 9110 §5.1).
 */
static const char *
IsSensitiveHeader(const char *line)
{
	static const struct
	{
		const char *name;
		size_t      len;
	} sensitive_headers[] = {
		{ "Authorization:",       sizeof("Authorization:")       - 1 },
		{ "Proxy-Authorization:", sizeof("Proxy-Authorization:") - 1 },
		{ NULL, 0 }
	};

	for (int i = 0; sensitive_headers[i].name != NULL; i++)
	{
		if (strncasecmp(line, sensitive_headers[i].name,
						sensitive_headers[i].len) == 0)
			return sensitive_headers[i].name;
	}

	return NULL;
}

/*
 * CURLDebugCallback
 * -----------------
 * Custom libcurl debug callback. Routes all verbose output through
 * PostgreSQL's elog() at DEBUG3 level rather than writing directly to
 * stderr, and redacts Authorization headers so credentials are never
 * written to server logs.
 *
 * handle  : the curl handle (unused)
 * type    : category of the debug data
 * data    : pointer to the debug data (NOT null-terminated)
 * size    : number of bytes in data
 * userptr : user-supplied pointer (unused)
 */
static int
CURLDebugCallback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr)
{
	const char    *prefix;
	StringInfoData buf;

	switch (type)
	{
		case CURLINFO_TEXT:       prefix = "* "; break;
		case CURLINFO_HEADER_IN:  prefix = "< "; break;
		case CURLINFO_HEADER_OUT: prefix = "> "; break;
		default:
			return 0;	/* skip raw data blobs (bodies, SSL frames) */
	}

	/*
	 * curl's data pointer is NOT null-terminated, so copy it into a palloc'd
	 * buffer before using any string functions on it.
	 */
	initStringInfo(&buf);
	appendBinaryStringInfo(&buf, data, (int) size);

	if (type == CURLINFO_HEADER_OUT)
	{
		/*
		 * CURLINFO_HEADER_OUT delivers the entire outgoing request header
		 * block (request line + all headers) as one multi-line chunk per
		 * invocation.  Split it line-by-line so each sensitive header can be
		 * redacted individually.
		 */
		char *pos = buf.data;

		while (*pos != '\0')
		{
			char       *eol   = pos + strcspn(pos, "\r\n");
			char        saved = *eol;
			const char *match;

			*eol = '\0';	/* temporarily terminate the line */

			if (*pos != '\0')	/* skip blank lines */
			{
				match = IsSensitiveHeader(pos);
				if (match)
					elog(DEBUG3, "[curl] > %s [REDACTED]", match);
				else
					elog(DEBUG3, "[curl] > %s", pos);
			}

			*eol = saved;
			pos  = eol;
			while (*pos == '\r' || *pos == '\n')
				pos++;
		}
	}
	else
	{
		const char *match;

		/* Strip trailing CRLF for cleaner log output. */
		while (buf.len > 0 &&
			   (buf.data[buf.len - 1] == '\n' || buf.data[buf.len - 1] == '\r'))
			buf.data[--buf.len] = '\0';

		/*
		 * Redact sensitive response headers.  Informational text lines
		 * (CURLINFO_TEXT) are logged as-is — they never contain raw
		 * credential values.
		 */
		match = (type == CURLINFO_HEADER_IN) ? IsSensitiveHeader(buf.data) : NULL;
		if (match)
			elog(DEBUG3, "[curl] %s%s [REDACTED]", prefix, match);
		else
			elog(DEBUG3, "[curl] %s%s", prefix, buf.data);
	}

	pfree(buf.data);
	return 0;
}

/*
 * CURLProgressCallback
 * --------------------
 * Progress callback function for cURL requests. libcurl calls it regularly
 * (at least once per second) while a transfer is in progress, which gives us
 * a chance to react to query cancellations and backend shutdown requests
 * instead of blocking until the server responds.
 *
 * It must NOT raise an error: ereport(ERROR) would longjmp out of libcurl's
 * call stack, leaving the easy handle - and the socket it owns - behind for
 * the lifetime of the backend. Instead we return a non-zero value, which
 * makes curl_easy_perform() abort the transfer and return
 * CURLE_ABORTED_BY_CALLBACK. The pending interrupt is then processed by the
 * CHECK_FOR_INTERRUPTS() in ExecuteRequest(), once the handle has been
 * properly cleaned up.
 *
 * dltotal: Total bytes to download
 * dlnow: Bytes downloaded so far
 * ultotal: Total bytes to upload
 * ulnow: Bytes uploaded so far
 */
static int CURLProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	return InterruptPending ? 1 : 0;
}

static int ExecuteRequest(NominatimFDWState *state)
{
    CURL *curl;
    CURLcode res;
    long response_code = 0;
    StringInfoData url_buffer;
    StringInfoData accept_header;
    StringInfoData user_agent;
    char errbuf[CURL_ERROR_SIZE];
    struct MemoryStruct chunk;
    struct MemoryStruct chunk_header;
    struct curl_slist *headers = NULL;

    chunk.memory = palloc(1);
    chunk.size = 0; /* no data at this point */
    chunk_header.memory = palloc(1);
    chunk_header.size = 0; /* no data at this point */

    elog(DEBUG2, "%s called", __func__);

    curl = curl_easy_init();

    initStringInfo(&url_buffer);
    appendStringInfoString(&url_buffer, state->url);

    /*
     * Endpoints are commonly written with a trailing slash. Appending the
     * request path blindly would then yield "https://host//search?...", so
     * trim whatever trailing slashes the URL carries before joining.
     */
    while (url_buffer.len > 0 && url_buffer.data[url_buffer.len - 1] == '/')
        url_buffer.data[--url_buffer.len] = '\0';

    appendStringInfo(&url_buffer, "/%s?", state->request_type);

    if (state->query && strlen(state->query) > 0)
        AppendUrlParam(&url_buffer, curl, "q", state->query);

    if (state->amenity && strlen(state->amenity) > 0)
        AppendUrlParam(&url_buffer, curl, "amenity", state->amenity);

    if (state->osm_ids && strlen(state->osm_ids) > 0)
        AppendUrlParam(&url_buffer, curl, "osm_ids", state->osm_ids);

    if (state->street && strlen(state->street) > 0)
        AppendUrlParam(&url_buffer, curl, "street", state->street);

    if (state->city && strlen(state->city) > 0)
        AppendUrlParam(&url_buffer, curl, "city", state->city);

    if (state->county && strlen(state->county) > 0)
        AppendUrlParam(&url_buffer, curl, "county", state->county);

    if (state->state && strlen(state->state) > 0)
        AppendUrlParam(&url_buffer, curl, "state", state->state);

    if (state->country && strlen(state->country) > 0)
        AppendUrlParam(&url_buffer, curl, "country", state->country);

    if (state->postalcode && strlen(state->postalcode) > 0)
        AppendUrlParam(&url_buffer, curl, "postalcode", state->postalcode);

    appendStringInfo(&url_buffer, "format=xml&");

    if (strcmp(state->request_type, NOMINATIM_REQUEST_REVERSE) == 0)
    {
        appendStringInfo(&url_buffer, "lon=%.8f&", state->lon);
        appendStringInfo(&url_buffer, "lat=%.8f&", state->lat);
    }

    if (strcmp(state->request_type, NOMINATIM_REQUEST_REVERSE) == 0 &&
        state->zoom >= 0)
        appendStringInfo(&url_buffer, "zoom=%d&", state->zoom);

    if (state->entrances)
        appendStringInfo(&url_buffer, "entrances=1&");

    if (state->extratags)
        appendStringInfo(&url_buffer, "extratags=1&");

    if (state->namedetails)
        appendStringInfo(&url_buffer, "namedetails=1&");

    if (state->addressdetails)
        appendStringInfo(&url_buffer, "addressdetails=1&");

    if (state->polygon_type && strlen(state->polygon_type) > 0)
    {    
        char *p = curl_easy_escape(curl, state->polygon_type, 0);
        appendStringInfo(&url_buffer, "%s=1&", p);
        curl_free(p);
    }

    if (state->accept_language && strlen(state->accept_language) > 0)
        AppendUrlParam(&url_buffer, curl, "accept-language", state->accept_language);

    if (state->countrycodes && strlen(state->countrycodes) > 0)
        AppendUrlParam(&url_buffer, curl, "countrycodes", state->countrycodes);

    if (state->layer && strlen(state->layer) > 0)
        AppendUrlParam(&url_buffer, curl, "layer", state->layer);

    if (state->feature_type && strlen(state->feature_type) > 0)
        AppendUrlParam(&url_buffer, curl, "featureType", state->feature_type);

    if (state->exclude_place_ids && strlen(state->exclude_place_ids) > 0)
        AppendUrlParam(&url_buffer, curl, "exclude_place_ids", state->exclude_place_ids);

    if (state->viewbox && strlen(state->viewbox) > 0)
        AppendUrlParam(&url_buffer, curl, "viewbox", state->viewbox);

    if (strcmp(state->request_type, NOMINATIM_REQUEST_SEARCH) == 0)
    {
        appendStringInfo(&url_buffer, "bounded=%d&", state->bounded ? 1 : 0);
        if (!state->dedupe)
            appendStringInfo(&url_buffer, "dedupe=0&");
    }

    if (state->polygon_threshold != 0.0)
        appendStringInfo(&url_buffer, "polygon_threshold=%f&", state->polygon_threshold);

    if (state->email && strlen(state->email) > 0)
        AppendUrlParam(&url_buffer, curl, "email", state->email);

    if (state->limit > 0)
        appendStringInfo(&url_buffer, "limit=%d", state->limit);

    if (curl)
    {
        errbuf[0] = 0;

        /* remove trailing & from URL, if any. */
        if (url_buffer.data[url_buffer.len-1] == '&')
            url_buffer.data[url_buffer.len-1] = '\0';

        elog(DEBUG1, "GET \"%s\"", url_buffer.data);

        curl_easy_setopt(curl, CURLOPT_URL, url_buffer.data);

#if ((LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR < 85) || LIBCURL_VERSION_MAJOR < 7)
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#else
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#endif

        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

        /*
         * CURLOPT_CONNECTTIMEOUT only bounds the connection phase: a server
         * that accepts the connection and then stalls would keep the backend
         * busy indefinitely. CURLOPT_TIMEOUT bounds the request as a whole.
         * It defaults to 0 (no limit), matching libcurl's own default, since
         * a hard cap is a poor fit for slow polygon-heavy responses.
         */
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, state->connect_timeout);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, state->request_timeout);
        elog(DEBUG2, "  %s: connect timeout > %ld", __func__, state->connect_timeout);
        elog(DEBUG2, "  %s: request timeout > %ld%s", __func__, state->request_timeout,
             state->request_timeout == 0 ? " (disabled)" : "");
        elog(DEBUG2, "  %s: max retry > %ld", __func__, state->max_retries);

        if (state->proxy)
        {
            elog(DEBUG2, "  %s: proxy URL > '%s'", __func__, state->proxy);

            curl_easy_setopt(curl, CURLOPT_PROXY, state->proxy);

            /*
             * http_proxy is the only proxy option there is, so the type is
             * fixed. A scheme given in the proxy URL itself still takes
             * precedence, which is how libcurl behaves.
             */
            elog(DEBUG2, "  %s: proxy protocol > 'HTTP'", __func__);
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);

            if (state->proxy_user)
            {
                elog(DEBUG2, "  %s: entering proxy user ('%s').", __func__, state->proxy_user);
                curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, state->proxy_user);
            }

            if (state->proxy_user_password)
            {
                elog(DEBUG2, "  %s: entering proxy user's password.", __func__);
                curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, state->proxy_user_password);
            }
        }

        /*
         * Redirects are always followed; how far is decided solely by
         * max_connect_redirect. With a limit of 0 libcurl refuses to follow
         * the first redirect and fails the request with
         * CURLE_TOO_MANY_REDIRECTS, which is reported to the user like any
         * other transport failure - so that single option covers "do not
         * redirect at all" as well, and no separate on/off switch is needed.
         */
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        /*
         * Never hand the credentials over to a host we were redirected to.
         * This is libcurl's default, set explicitly so that the intent is
         * visible and survives a change of that default.
         */
        curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 0L);

        elog(DEBUG2, "  %s: setting maxredirs: %ld", __func__, state->request_max_redirect);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, state->request_max_redirect);

        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallbackFunction);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&chunk_header);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        /*
         * CURLOPT_FAILONERROR is deliberately NOT set: it makes libcurl
         * discard the response body of a 4xx/5xx answer, which is exactly
         * where Nominatim explains what was wrong with the request. HTTP
         * status handling is done below instead.
         */
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

		/*
		 * Enable libcurl verbose output, but route it exclusively through
		 * CURLDebugCallback instead of stderr. The callback emits at DEBUG3
		 * (gated by log_min_messages) and redacts Authorization headers so
		 * credentials are never written to server logs.
		 */
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, CURLDebugCallback);
		curl_easy_setopt(curl, CURLOPT_DEBUGDATA, NULL);

		/*
		 * Set the progress callback function, so that in-flight requests can
		 * be aborted when the query is cancelled.
		 *
		 * CURLOPT_NOPROGRESS defaults to 1, which disables libcurl's progress
		 * machinery altogether - the callback registered above is then never
		 * invoked. It must be explicitly switched off for the callback to run.
		 */
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, CURLProgressCallback);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        initStringInfo(&user_agent);
        appendStringInfo(&user_agent, "PostgreSQL/%s nominatim_fdw/%s libxml2/%s %s", PG_VERSION, FDW_VERSION, LIBXML_DOTTED_VERSION, curl_version());

        elog(DEBUG2, "%s: \"Agent: %s\"", __func__, user_agent.data);

        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.data);

        initStringInfo(&accept_header);
        appendStringInfo(&accept_header, "Accept-Language: %s", state->accept_language);
        headers = curl_slist_append(headers, accept_header.data);
        elog(DEBUG2, "  adding header: %s", accept_header.data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        if (state->user && state->password)
		{
            elog(DEBUG2, "  %s: setting user and password for HTTP basic authentication (%s:********)", __func__, state->user);
			curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
			curl_easy_setopt(curl, CURLOPT_USERNAME, state->user);
			curl_easy_setopt(curl, CURLOPT_PASSWORD, state->password);
		}
		else if (state->user && !state->password)
		{
            elog(DEBUG2, "  %s: setting user %s for HTTP basic authentication (no password)", __func__, state->user);
			curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
			curl_easy_setopt(curl, CURLOPT_USERNAME, state->user);
		}

        elog(DEBUG2, "%s: performing cURL request ... ", __func__);

        /*
         * Anything raising an error while libcurl is on the stack - a write
         * callback running out of memory, for instance - would longjmp past
         * curl_easy_cleanup() and leak the easy handle together with its
         * socket for the lifetime of the backend. Release it here and let the
         * error propagate.
         */
        PG_TRY();
        {
            res = curl_easy_perform(curl);
            if (res == CURLE_OK)
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            for (long i = 1;
                 RequestFailed(res, response_code) && i <= state->max_retries;
                 i++)
            {
                long delay = 1; /* just being polite to the public server */

                if (!IsRetryable(res, response_code))
                    break;

                elog(WARNING, "request to '%s' failed (%ld/%ld)", state->url, i, state->max_retries);
                elog(DEBUG1, "the server returned HTTP code %ld", response_code);

                /*
                 * A 429 answer usually carries a Retry-After header saying how
                 * long to wait. Honouring it is the difference between backing
                 * off and hammering a rate-limited server.
                 */
                if (response_code == 429)
                {
                    long retry_after = ParseRetryAfter(chunk_header.memory);

                    if (retry_after >= 0)
                    {
                        delay = Min(retry_after, NOMINATIM_MAX_RETRY_AFTER);
                        elog(DEBUG1, "server asked to retry after %ld seconds, waiting %ld",
                             retry_after, delay);
                    }
                }

                /* discard whatever the failed attempt left behind before retrying */
                chunk.size = 0;
                chunk.memory[0] = '\0';
                chunk_header.size = 0;
                chunk_header.memory[0] = '\0';
                response_code = 0;

                InterruptibleSleep(delay);

                if (InterruptPending)
                    break;

                res = curl_easy_perform(curl);
                if (res == CURLE_OK)
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            }
        }
        PG_CATCH();
        {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            PG_RE_THROW();
        }
        PG_END_TRY();

        if (RequestFailed(res, response_code))
        {
            StringInfoData message;
            StringInfoData detail;

            initStringInfo(&message);
            initStringInfo(&detail);

            if (res != CURLE_OK)
                appendStringInfoString(&message,
                                       strlen(errbuf) > 0 ? errbuf : curl_easy_strerror(res));
            else
                appendStringInfo(&message,
                                 "the server returned HTTP status %ld", response_code);

            appendStringInfo(&detail, "URL: \"%s\"", url_buffer.data);

            /*
             * Report whatever the server sent along with the failure. This is
             * where Nominatim says which parameter it objected to, and it is
             * far more actionable than the status code on its own. Truncated,
             * so that a large HTML error page - from a misconfigured proxy,
             * say - cannot flood the logs.
             */
            if (res == CURLE_OK && chunk.size > 0)
            {
                int len = (int)Min(chunk.size, NOMINATIM_FDW_MAX_ERROR_BODY);

                appendStringInfoString(&detail, "\nResponse body: ");
                appendBinaryStringInfo(&detail, chunk.memory, len);

                if (chunk.size > NOMINATIM_FDW_MAX_ERROR_BODY)
                    appendStringInfo(&detail, " ... [%lu bytes truncated]",
                                     (unsigned long)(chunk.size - NOMINATIM_FDW_MAX_ERROR_BODY));
            }

            pfree(chunk.memory);
            pfree(chunk_header.memory);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);

            /*
             * Now that the easy handle is gone it is safe to act on a pending
             * interrupt: this reports the cancellation that made
             * CURLProgressCallback abort the transfer, rather than a generic
             * "request failed" error.
             */
            CHECK_FOR_INTERRUPTS();

            ereport(ERROR,
                    (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                     errmsg("%s", message.data),
                     errhint("Check your request parameters and credentials, if applicable, and try again."),
                     errdetail("%s", detail.data)));
          }
        else
        {
            /*
             * XML_PARSE_NOERROR / XML_PARSE_NOWARNING keep libxml2 from
             * writing parse diagnostics straight to stderr - which in a
             * backend means unstructured noise in the server log. A body that
             * does not parse simply yields a NULL document, which is handled
             * by the caller. These are per-call parser options, so no global
             * libxml2 error handler is touched and other users of the library
             * in this process are unaffected.
             */
            state->xmldoc = xmlReadMemory(chunk.memory, chunk.size, NULL, NULL,
                                          XML_PARSE_NOBLANKS | XML_PARSE_NONET |
                                          XML_PARSE_NOERROR | XML_PARSE_NOWARNING);

            elog(DEBUG1, "HTTP %ld, %ld bytes", response_code, chunk.size);
        }
    }

    pfree(chunk.memory);
    pfree(chunk_header.memory);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    /*
     * We thrown an error in case the server returns an empty XML doc
     */
    if (!state->xmldoc)
        return REQUEST_FAIL;

    return REQUEST_SUCCESS;
}

/*
 * CheckURL
 * --------
 * CheckS if an URL is valid.
 *
 * url: URL to be validated.
 *
 * returns REQUEST_SUCCESS or REQUEST_FAIL
 */
static int CheckURL(char *url)
{
    CURLUcode code;
    CURLU *handler = curl_url();

    elog(DEBUG2, "%s called > '%s'", __func__, url);

    code = curl_url_set(handler, CURLUPART_URL, url, 0);

    curl_url_cleanup(handler);

    elog(DEBUG2, "  %s handler return code: %u", __func__, code);

    if (code != 0)
    {
        elog(DEBUG2, "%s: invalid URL (%u) > '%s'", __func__, code, url);
        return code;
    }

    return REQUEST_SUCCESS;
}

/*
 * IsPolygonTypeSupported
 * ----------
 *
 * Checks if a polygon type is supported by the nominatim endpoint
 *
 * returns boolean (true: valid, false: invalid)
 */
static bool IsPolygonTypeSupported(char *polygon_type)
{
    if (!polygon_type)
        return false;

    return (strcmp(polygon_type, "") == 0 ||
            strcmp(polygon_type, "polygon_text") == 0 ||
            strcmp(polygon_type, "polygon_geojson") == 0 ||
            strcmp(polygon_type, "polygon_kml") == 0 ||
            strcmp(polygon_type, "polygon_svg") == 0);
}

/*
 * IsLayerValid
 * ----------
 *
 * Checks if a comma-separated list of layer values is valid.
 * Accepted tokens: address, poi, railway, natural, manmade
 *
 * returns boolean (true: valid, false: invalid)
 */
static bool IsLayerValid(char *layer)
{
    char *copy;
    char *token;
    char *saveptr = NULL;
    bool valid = true;

    if (!layer)
        return false;

    copy = pstrdup(layer);

    /*
     * strtok() keeps its parsing state in a static buffer shared by the whole
     * process, which makes it unsafe in backend code. strtok_r() keeps that
     * state in saveptr instead.
     */
    for (token = strtok_r(copy, ",", &saveptr);
         token != NULL;
         token = strtok_r(NULL, ",", &saveptr))
    {
        if (strcmp(token, "address") != 0 &&
            strcmp(token, "poi") != 0 &&
            strcmp(token, "railway") != 0 &&
            strcmp(token, "natural") != 0 &&
            strcmp(token, "manmade") != 0)
        {
            valid = false;
            break;
        }
    }

    pfree(copy);
    return valid;
}

/*
 * IsFeatureTypeValid
 * ----------
 *
 * Checks if a feature type is supported by the nominatim endpoint
 *
 * returns boolean (true: valid, false: invalid)
 */
static bool IsFeatureTypeValid(char *featuretype)
{
    if (!featuretype)
        return false;

    return (strcmp(featuretype, "") == 0 ||
            strcmp(featuretype, "country") == 0 ||
            strcmp(featuretype, "state") == 0 ||
            strcmp(featuretype, "city") == 0 ||
            strcmp(featuretype, "settlement") == 0);
}