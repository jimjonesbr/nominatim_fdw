
/**********************************************************************
 *
 * nominatim_fdw - PostgreSQL Nominatim Extension
 *
 * nominatim_fdw is free software: you can redistribute it and/or modify
 * it under the terms of the MIT Licence.
 *
 * Copyright (C) 2024 University of Münster, Germany
 * Written by Jim Jones <jim.jones@uni-muenster.de>
 *
 **********************************************************************/

#include "postgres.h"
#include "fmgr.h"
#include "foreign/fdwapi.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/planmain.h"
#include "utils/rel.h"

#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/reloptions.h"

#if PG_VERSION_NUM >= 120000
#include "access/table.h"
#endif

#include "foreign/foreign.h"
#include "commands/defrem.h"

#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pg_list.h"
#include "optimizer/pathnode.h"

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
#include "utils/timestamp.h"
#include "utils/formatting.h"
#include "catalog/pg_operator.h"
#include "utils/syscache.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_user_mapping.h"
#include "catalog/pg_type.h"
#include "access/reloptions.h"
#include "catalog/pg_namespace.h"

#if PG_VERSION_NUM < 120000
#include "nodes/relation.h"
#include "optimizer/var.h"
#include "utils/tqual.h"
#else
#include "nodes/pathnodes.h"
#include "optimizer/optimizer.h"
#include "access/heapam.h"
#endif
#include "utils/date.h"
#include <utils/elog.h>
#include <access/tupdesc.h>

#define FDW_VERSION "1.0.0-dev"
#define REQUEST_SUCCESS 0
#define REQUEST_FAIL -1

#define NOMINATIM_REQUEST_SEARCH "search"
#define NOMINATIM_REQUEST_REVERSE "reverse"
#define NOMINATIM_REQUEST_LOOKUP "lookup"

#define NOMINATIM_SERVER_OPTION_URL "url"
#define NOMINATIM_SERVER_OPTION_FORMAT "format"
#define NOMINATIM_SERVER_OPTION_CONNECTTIMEOUT "connect_timeout"
#define NOMINATIM_SERVER_OPTION_MAXCONNECTRETRY "max_connect_retry"
#define NOMINATIM_SERVER_OPTION_MAXREDIRECT "max_connect_redirect"
#define NOMINATIM_SERVER_OPTION_HTTP_PROXY "http_proxy"
#define NOMINATIM_SERVER_OPTION_HTTPS_PROXY "https_proxy"
#define NOMINATIM_SERVER_OPTION_PROXY_USER "proxy_user"
#define NOMINATIM_SERVER_OPTION_PROXY_USER_PASSWORD "proxy_user_password"
#define NOMINATIM_SERVER_OPTION_LANGUAGE "accept_language"

#define NOMINATIM_SERVER_OPTION_QUERY "query"
#define NOMINATIM_TABLE_OPTION_QUERY "q"
#define NOMINATIM_TABLE_OPTION_AMENITY "amenity"
#define NOMINATIM_TABLE_OPTION_STREET "street"
#define NOMINATIM_TABLE_OPTION_CITY "city"
#define NOMINATIM_TABLE_OPTION_COUNTY "county"
#define NOMINATIM_TABLE_OPTION_STATE "state"
#define NOMINATIM_TABLE_OPTION_COUNTRY "country"
#define NOMINATIM_TABLE_OPTION_POSTALCODE "postalcode"
#define NOMINATIM_TABLE_OPTION_EXTRATAGS "extratags"
#define NOMINATIM_TABLE_OPTION_NAMEDETAILS "namedetails"
#define NOMINATIM_TABLE_OPTION_ADDRESSDETAILS "addressdetails"
#define NOMINATIM_TABLE_OPTION_ADDRESSPARTS "addressparts"
#define NOMINATIM_TABLE_OPTION_ACCEPTLANGUAGE "accept_language"

#define NOMINATIM_COLUMN_OPTION_PROPERTY "property"
#define NOMINATIM_COLUMN_OPTION_FORMAT "format"

#define NOMINATIM_TAG_CITY "city"
#define NOMINATIM_TAG_STREET "street"
#define NOMINATIM_TAG_COUNTY "county"
#define NOMINATIM_TAG_COUNTRY "COUNTRY"
#define NOMINATIM_TAG_STATE "STATE"
#define NOMINATIM_TAG_POSTALCODE "postalcode"

#define NOMINATIM_COLUMN_OPTION_TAG "tag"

#define NOMINATIM_DEFAULT_CONNECTTIMEOUT 300
#define NOMINATIM_DEFAULT_MAXRETRY 3
#define NOMINATIM_DEFAULT_MAXREDIRECT 1
#define NOMINATIM_DEFAULT_FORMAT "xml"
#define NOMINATIM_DEFAULT_LANGUAGE "en-US,en;q=0.9"

PG_MODULE_MAGIC;

typedef struct NominatimFDWState
{
	int numcols;       /* Total number of columns in the foreign table. */
	int rowcount;      /* Number of rows currently returned to the client */
	int pagesize;      /* Total number of records retrieved from the SPARQL endpoint*/
    int zoom;
    int limit;
    int offset;
    char *request_type;
	char* url;
    char *osm_ids;
    char *amenity;     /*  */
    char *street;      /*  */
    char *city;        /*  */
    char *county;       /*  */
    char *state;       /*  */
    char *country;     /*  */
    char *postalcode;  /*  */
   	char *proxy;                 /* Proxy for HTTP requests, if necessary. */
	char *proxy_type;            /* Proxy protocol (HTTPS, HTTP). */
	char *proxy_user;            /* User name for proxy authentication. */
	char *proxy_user_password;   /* Password for proxy authentication. */
	char *custom_params;         /* Custom parameters used to compose the request URL */
    char *format;  /*  */
    char *query;  /*  */
    char *layer;
    char *countrycodes;
    char *feature_type;
    char *exclude_place_ids;
    char *viewbox;    
    char *polygon_type;
    char *email;    
    char *accept_language;
    bool dedupe;
    bool bounded;
	bool request_redirect;       /* Enables or disables URL redirecting. */
    bool is_query_structured;
    bool extratags;
    bool namedetails;
    bool addressdetails;
   	long request_max_redirect;   /* Limit of how many times the URL redirection (jump) may occur. */
	long connect_timeout;        /* Timeout for SPARQL queries */
	long max_retries;            /* Number of re-try attemtps for failed SPARQL queries */
    float8 lon;
    float8 lat;
    float8 polygon_threshold;
	xmlDocPtr xmldoc;            
    List *records;
	struct NominatimFDWTable *nominatim_table; /* */
} NominatimFDWState;

typedef struct NominatimFDWTable
{	
	Oid foreigntableid;
    char *name;                        /* FOREIGN TABLE name */
	struct NominatimFDWColumn **cols;  /* List of columns of a FOREIGN TABLE */
} NominatimFDWTable;

typedef struct NominatimFDWColumn
{	
	char *name;          /* Column name */
	char *tag;           
	Oid  pgtype;                 /* PostgreSQL data type */
	int  pgtypmod;               /* PostgreSQL type modifier */
	int  pgattnum;               /* PostgreSQL attribute number */
	bool used;                   /* Is the column used in the current SQL query? */ 
} NominatimFDWColumn;


struct NominatimFDWOption
{
	const char *optname;
	Oid optcontext;	  /* Oid of catalog in which option may appear */
	bool optrequired; /* Flag mandatory options */
	bool optfound;	  /* Flag whether options was specified by user */
} NomiatimFDWOption;

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
    char* ref;
    char* lat;
    char* lon;
    char* boundingbox;
    char *place_rank;
    char *address_rank;
    char *display_rank;
    char *display_name;
    char *class;
    char *type;
    char *importance;
    char *icon;
	char *extratags;
    char *addressdetails;    
    char *namedetails;
    char *addressparts;
    char* result;
} NominatimRecord;

struct string
{
	char *ptr;
	size_t len;
};

struct MemoryStruct
{
	char *memory;
	size_t size;
};

static struct NominatimFDWOption valid_options[] =
{
	/* Foreign Servers */
	{NOMINATIM_SERVER_OPTION_URL, ForeignServerRelationId, true, false},
    {NOMINATIM_SERVER_OPTION_FORMAT, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_HTTP_PROXY, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_HTTPS_PROXY, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_PROXY_USER, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_PROXY_USER_PASSWORD, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_CONNECTTIMEOUT, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_MAXCONNECTRETRY, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_MAXREDIRECT, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_QUERY, ForeignServerRelationId, false, false},
    {NOMINATIM_SERVER_OPTION_LANGUAGE, ForeignServerRelationId, false, false},
	/* EOList option */
	{NULL, InvalidOid, false, false}
};

extern Datum nominatim_fdw_handler(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_validator(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_version(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_search(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_reverse(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_lookup(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(nominatim_fdw_handler);
PG_FUNCTION_INFO_V1(nominatim_fdw_validator);
PG_FUNCTION_INFO_V1(nominatim_fdw_version);
PG_FUNCTION_INFO_V1(nominatim_fdw_search);
PG_FUNCTION_INFO_V1(nominatim_fdw_reverse);
PG_FUNCTION_INFO_V1(nominatim_fdw_lookup);

static void NominatimGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static void NominatimGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static ForeignScan *NominatimGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses, Plan *outer_plan);
static void NominatimBeginForeignScan(ForeignScanState *node, int eflags);
static TupleTableSlot *NominatimIterateForeignScan(ForeignScanState *node);
static void NominatimReScanForeignScan(ForeignScanState *node);
static void NominatimEndForeignScan(ForeignScanState *node);
static Datum ConvertDatum(HeapTuple tuple, int pgtype, int pgtypemod, char *value);
static char *GetAttributeValue(Form_pg_attribute att, struct NominatimRecord *place);
static NominatimFDWState *InitSession(const char *srvname);

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
static size_t HeaderCallbackFunction(char *contents, size_t size, size_t nmemb, void *userp);
//static void InitSession(struct NominatimFDWState *state, RelOptInfo *baserel, PlannerInfo *root);
//static void RaiseNominatimException(xmlNodePtr error);
static void LoadData(NominatimFDWState *state);
static void LoadDataReverseLookup(NominatimFDWState *state);
static int ExecuteRequest(NominatimFDWState *state);
static int CheckURL(char *url);
static bool IsPolygonTypeSupported(char *polygon_type);

static void NominatimGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
	ForeignTable *ft = GetForeignTable(foreigntableid);	
	NominatimFDWTable *opts = (NominatimFDWTable *)palloc0(sizeof(NominatimFDWTable));
	
	elog(DEBUG1, "%s called", __func__);
			
	opts->foreigntableid = ft->relid;
	baserel->fdw_private = opts;
}

static void NominatimGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{

	Path *path = (Path *)create_foreignscan_path(root, baserel,
												 NULL,				/* default pathtarget */
												 baserel->rows,		/* rows */
												 1,					/* startup cost */
												 1 + baserel->rows, /* total cost */
												 NIL,				/* no pathkeys */
												 NULL,				/* no required outer relids */
												 NULL,				/* no fdw_outerpath */
												 NIL);				/* no fdw_private */
	add_path(baserel, path);
}

static ForeignScan *NominatimGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses, Plan *outer_plan)
{
	List *fdw_private;
	//NominatimFDWTable *opts = baserel->fdw_private;
	NominatimFDWState *state = (NominatimFDWState *)palloc0(sizeof(NominatimFDWState));
	
	fdw_private = list_make1(state);

	scan_clauses = extract_actual_clauses(scan_clauses, false);

	return make_foreignscan(tlist,
							scan_clauses,
							baserel->relid,
							NIL,		 /* no expressions we will evaluate */
							fdw_private, /* pass along our start and end */
							NIL,		 /* no custom tlist; our scan tuple looks like tlist */
							NIL,		 /* no quals we will recheck */
							outer_plan);
}

static void NominatimBeginForeignScan(ForeignScanState *node, int eflags)
{
	ForeignScan *fs = (ForeignScan *)node->ss.ps.plan;
	NominatimFDWState *state = (NominatimFDWState *)linitial(fs->fdw_private);

	if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
		return;

	node->fdw_state = (void *)state;	
}

static TupleTableSlot *NominatimIterateForeignScan(ForeignScanState *node)
{
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
	//struct NominatimFDWState *state = (struct NominatimFDWState *) node->fdw_state;
		
	elog(DEBUG2,"%s called",__func__);

	ExecClearTuple(slot);	

	return slot;
	
}

static void NominatimReScanForeignScan(ForeignScanState *node)
{
}

static void NominatimEndForeignScan(ForeignScanState *node)
{
}

Datum nominatim_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *fdwroutine = makeNode(FdwRoutine);
	fdwroutine->GetForeignRelSize = NominatimGetForeignRelSize;
	fdwroutine->GetForeignPaths = NominatimGetForeignPaths;
	fdwroutine->GetForeignPlan = NominatimGetForeignPlan;
	fdwroutine->BeginForeignScan = NominatimBeginForeignScan;
	fdwroutine->IterateForeignScan = NominatimIterateForeignScan;
	fdwroutine->ReScanForeignScan = NominatimReScanForeignScan;
	fdwroutine->EndForeignScan = NominatimEndForeignScan;
	//fdwroutine->ExecForeignInsert = rdfExecForeignInsert;
	//fdwroutine->ExecForeignUpdate = rdfExecForeignUpdate;
	//fdwroutine->ExecForeignDelete = rdfExecForeignDelete;
	
	PG_RETURN_POINTER(fdwroutine);
}

Datum nominatim_fdw_validator(PG_FUNCTION_ARGS)
{
    List *options_list = untransformRelOptions(PG_GETARG_DATUM(0));
    Oid catalog = PG_GETARG_OID(1);
    ListCell *cell;
    struct NominatimFDWOption *opt;

     if(catalog == ForeignTableRelationId)
            ereport(ERROR,
                    (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                     errmsg("FOREIGN TABLE not supported"),
                     errhint("The nominatim_fdw does not support FOREIGN TABLE mapping. Use the query functions instead.")));

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
                {
                    ereport(ERROR,
                            (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                             errmsg("empty value in option '%s'", opt->optname)));
                }

                if (strcmp(opt->optname, NOMINATIM_SERVER_OPTION_URL) == 0 ||
                    strcmp(opt->optname, NOMINATIM_SERVER_OPTION_HTTP_PROXY) == 0 ||
                    strcmp(opt->optname, NOMINATIM_SERVER_OPTION_HTTPS_PROXY) == 0)
                {
                    int return_code = CheckURL(defGetString(def));

                    if (return_code != REQUEST_SUCCESS)
                    {
                        ereport(ERROR,
                                (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                                 errmsg("invalid %s: '%s'", opt->optname, defGetString(def))));
                    }
                }

                if (strcmp(opt->optname, NOMINATIM_SERVER_OPTION_CONNECTTIMEOUT) == 0)
                {
                    char *endptr;
                    char *timeout_str = defGetString(def);
                    long timeout_val = strtol(timeout_str, &endptr, 0);

                    if (timeout_str[0] == '\0' || *endptr != '\0' || timeout_val < 0)
                    {
                        ereport(ERROR,
                                (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                                 errmsg("invalid %s: '%s'", def->defname, timeout_str),
                                 errhint("expected values are positive integers (timeout in seconds)")));
                    }
                }

                if (strcmp(opt->optname, NOMINATIM_SERVER_OPTION_MAXCONNECTRETRY) == 0 || strcmp(opt->optname, NOMINATIM_SERVER_OPTION_MAXREDIRECT) == 0)
                {
                    char *endptr;
                    char *retry_str = defGetString(def);
                    long retry_val = strtol(retry_str, &endptr, 0);

                    if (retry_str[0] == '\0' || *endptr != '\0' || retry_val < 0)
                    {
                        ereport(ERROR,
                                (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                                 errmsg("invalid %s: '%s'", def->defname, retry_str),
                                 errhint("expected values are positive integers")));
                    }
                }
                
            }
        }

        if (!optfound)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                     errmsg("invalid rdf_fdw option '%s'", def->defname)));
        }
    }

    for (opt = valid_options; opt->optname; opt++)
    {
        /* Required option for this catalog type is missing? */
        if (catalog == opt->optcontext && opt->optrequired && !opt->optfound)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_DYNAMIC_PARAMETER_VALUE_NEEDED),
                     errmsg("required option '%s' is missing", opt->optname)));
        }
    }

    PG_RETURN_VOID();
}

Datum nominatim_fdw_version(PG_FUNCTION_ARGS)
{
	StringInfoData buffer;
	initStringInfo(&buffer);

	appendStringInfo(&buffer, "nominatim_fdw = %s,", FDW_VERSION);
	appendStringInfo(&buffer, " libxml/%s", LIBXML_DOTTED_VERSION);
	appendStringInfo(&buffer, " %s", curl_version());

	PG_RETURN_TEXT_P(cstring_to_text(buffer.data));
}

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


    FuncCallContext *funcctx;
	TupleDesc tupdesc;

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
        NominatimFDWState *state = InitSession(text_to_cstring(srvname_text));

		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        if(language_text && strlen(text_to_cstring(language_text))>0)
            state->accept_language = text_to_cstring(language_text);
        
        state->lon = lon;
        state->lat = lat;
        state->zoom = zoom;
        state->layer = strcmp(text_to_cstring(layer),"") == 0 ? NULL : text_to_cstring(layer);        
        state->request_type = NOMINATIM_REQUEST_REVERSE;

        state->polygon_type = text_to_cstring(polygon_text);
        state->extratags = extratags;
        state->addressdetails = addressdetails;
        state->namedetails = namedetails;

        if(!IsPolygonTypeSupported(state->polygon_type))
            ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
							errmsg("invalid polygon type '%s'",state->polygon_type),
                            errhint("this parameter expects one of the following formats: polygon_geojson, polygon_kml, polygon_svg, polygon_text")));

        elog(DEBUG1,"\n\n\t=== %s ===\n\tlon: '%f'\n\tlat: '%f'\n\tzoom: '%d'\n\tpolygon_type: '%s'\n\tlayer: '%s'\n"
            ,__func__,
            state->lon,
            state->lat,
            state->zoom,
            state->polygon_type,
            state->layer);
            
		LoadDataReverseLookup(state);
				
		funcctx->user_fctx = state->records;

		if (state->records)
			funcctx->max_calls = state->records->length;

		elog(DEBUG1,"  %s: number of records retrieved = %ld ",__func__, funcctx->max_calls);

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
		Datum		values[18];
		bool		nulls[18];		
		HeapTuple	tuple;
		Datum		result;
		NominatimRecord *place = (NominatimRecord *)list_nth((List *)funcctx->user_fctx, (int)funcctx->call_cntr);

		memset(nulls, 0, sizeof(nulls));
		
		for (size_t i = 0; i < funcctx->attinmeta->tupdesc->natts; i++)
		{
			Form_pg_attribute att = TupleDescAttr(funcctx->attinmeta->tupdesc, i);					
            char *value = GetAttributeValue(att,place);
           
			if(value)
				values[i] = ConvertDatum(tuple, att->atttypid, att->atttypmod, value);
			else
				nulls[i] = true;

            elog(DEBUG2,"  %s = '%s'",NameStr(att->attname), value);
		}
		
        elog(DEBUG2,"  %s: creating heap tuple",__func__);

		tuple = heap_form_tuple(funcctx->attinmeta->tupdesc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
	{
		SRF_RETURN_DONE(funcctx);
	}
}

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
    int offset = PG_GETARG_INT32(24);       
   
    FuncCallContext *funcctx;
	TupleDesc tupdesc;
    NominatimFDWState *state = (NominatimFDWState *)palloc0(sizeof(NominatimFDWState));
        
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
        state = InitSession(text_to_cstring(srvname_text));
        
		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        if(language_text && strlen(text_to_cstring(language_text))>0)
            state->accept_language = text_to_cstring(language_text);
            
        state->query = text_to_cstring(query_text);
        state->amenity = text_to_cstring(amenity_text);
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
        state->polygon_threshold =  polygon_threshold;
        state->email = text_to_cstring(email_text);
        state->dedupe = dedupe;
        state->is_query_structured = false;
        state->extratags = extratags;
        state->addressdetails = addressdetails;
        state->namedetails = namedetails;
        state->limit = limit;
        state->offset = offset;
        state->request_type = NOMINATIM_REQUEST_SEARCH;

        

        if(state->amenity && strlen(state->amenity)>0 &&  
           state->query && strlen(state->query)>0)
            ereport(ERROR, (errcode(ERRCODE_FDW_ERROR),
							errmsg("bad request => structured query parameters (amenity, street, city, county, state, postalcode, country) cannot be used together with 'q' parameter")));

        if((state->amenity && strlen(state->amenity)==0) &&  
           (state->query && strlen(state->query)==0))
            ereport(ERROR, (errcode(ERRCODE_FDW_ERROR),
							errmsg("bad request => nothing to search for."),
                            errhint("a '%s' request requires either a 'q' (free form parameter) or one of the structured query parameteres (amenity, street, city, county, state, postalcode, country)",__func__)));


        if(!IsPolygonTypeSupported(state->polygon_type))
            ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
							errmsg("invalid polygon type '%s'",state->polygon_type),
                            errhint("this parameter expects one of the following formats: polygon_geojson, polygon_kml, polygon_svg, polygon_text")));
        
        elog(DEBUG1,"\n\n\t=== %s ===\n\tq:'%s'\n\tpolygon_type: '%s'\n"
        ,__func__,
        state->query,
        state->polygon_type);
       	
		LoadData(state);
				
		funcctx->user_fctx = state->records;

		if (state->records)
			funcctx->max_calls = state->records->length;

		elog(DEBUG1,"  %s: number of records retrieved = %ld ",__func__, funcctx->max_calls);

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
		Datum		values[23];
		bool		nulls[23];		
		HeapTuple	tuple;
		Datum		result;
		NominatimRecord *place = (NominatimRecord *)list_nth((List *)funcctx->user_fctx, (int)funcctx->call_cntr);

		memset(nulls, 0, sizeof(nulls));
		
		for (size_t i = 0; i < funcctx->attinmeta->tupdesc->natts; i++)
		{
			Form_pg_attribute att = TupleDescAttr(funcctx->attinmeta->tupdesc, i);						
            char *value = GetAttributeValue(att,place);
           
			if(value)
				values[i] = ConvertDatum(tuple, att->atttypid, att->atttypmod, value);
			else
				nulls[i] = true;

            elog(DEBUG2,"  %s = '%s'",NameStr(att->attname), value);
		}
		
        elog(DEBUG2,"  %s: creating heap tuple",__func__);

		tuple = heap_form_tuple(funcctx->attinmeta->tupdesc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
	{
		SRF_RETURN_DONE(funcctx);
	}
}

Datum nominatim_fdw_lookup(PG_FUNCTION_ARGS)
{
	text *srvname_text = PG_GETARG_TEXT_P(0);
    text *osm_ids_text = PG_GETARG_TEXT_P(1);
    bool extratags = PG_GETARG_BOOL(2);
    bool addressdetails = PG_GETARG_BOOL(3);
    bool namedetails = PG_GETARG_BOOL(4);
    text *polygon_text = PG_GETARG_TEXT_P(5);
    text *language_text = PG_GETARG_TEXT_P(6);
    text *countrycodes_text = PG_GETARG_TEXT_P(7);
    text *layer_text = PG_GETARG_TEXT_P(8);
    text *featuretype_text = PG_GETARG_TEXT_P(9);
    text *excludeids_text = PG_GETARG_TEXT_P(10);
    text *viewbox_text = PG_GETARG_TEXT_P(11);
    bool bounded = PG_GETARG_BOOL(12);
    float8 polygon_threshold = PG_GETARG_FLOAT8(13);
    text *email_text = PG_GETARG_TEXT_P(14);
    bool dedupe = PG_GETARG_BOOL(15);

    FuncCallContext *funcctx;
	TupleDesc tupdesc;
    NominatimFDWState *state = (NominatimFDWState *)palloc0(sizeof(NominatimFDWState));
    
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		funcctx = SRF_FIRSTCALL_INIT();
        state = InitSession(text_to_cstring(srvname_text));
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        if(language_text && strlen(text_to_cstring(language_text))>0)
            state->accept_language = text_to_cstring(language_text);

        state->osm_ids = text_to_cstring(osm_ids_text);    
        state->polygon_type = text_to_cstring(polygon_text);        
        state->countrycodes = text_to_cstring(countrycodes_text);
        state->layer = text_to_cstring(layer_text);
        state->feature_type = text_to_cstring(featuretype_text);
        state->exclude_place_ids = text_to_cstring(excludeids_text);
        state->viewbox = text_to_cstring(viewbox_text);
        state->bounded = bounded;
        state->polygon_threshold =  polygon_threshold;
        state->email = text_to_cstring(email_text);
        state->dedupe = dedupe;    
        state->is_query_structured = false;
        state->extratags = extratags;
        state->addressdetails = addressdetails;
        state->namedetails = namedetails;
        state->request_type = NOMINATIM_REQUEST_LOOKUP;

        // if(state->accept_language && strlen(state->accept_language)>0)
        //     state->accept_language = text_to_cstring(language_text);

        if(!IsPolygonTypeSupported(state->polygon_type))
            ereport(ERROR, (errcode(ERRCODE_FDW_INVALID_STRING_FORMAT),
							errmsg("invalid polygon type '%s'",state->polygon_type),
                            errhint("this parameter expects one of the following formats: polygon_geojson, polygon_kml, polygon_svg, polygon_text")));

        elog(DEBUG1,"\n\n\t=== %s ===\n\tosm_ids:'%s'\n\tpolygon_type: '%s'\n"
        ,__func__,
        state->osm_ids,
        state->polygon_type);
       	
		LoadData(state);
				
		funcctx->user_fctx = state->records;

		if (state->records)
			funcctx->max_calls = state->records->length;

		elog(DEBUG1,"  %s: number of records retrieved = %ld ",__func__, funcctx->max_calls);

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
		Datum		values[23];
		bool		nulls[23];		
		HeapTuple	tuple;
		Datum		result;
		NominatimRecord *place = (NominatimRecord *)list_nth((List *)funcctx->user_fctx, (int)funcctx->call_cntr);

		memset(nulls, 0, sizeof(nulls));
				
        for (size_t i = 0; i < funcctx->attinmeta->tupdesc->natts; i++)
		{
			Form_pg_attribute att = TupleDescAttr(funcctx->attinmeta->tupdesc, i);					
            char *value = GetAttributeValue(att, place);
           
			if(value)
				values[i] = ConvertDatum(tuple, att->atttypid, att->atttypmod, value);
			else
				nulls[i] = true;

            elog(DEBUG2,"  %s: %s = '%s'",__func__,NameStr(att->attname), value);
		}
		
        elog(DEBUG2,"  %s: creating heap tuple",__func__);

		tuple = heap_form_tuple(funcctx->attinmeta->tupdesc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
	{
		SRF_RETURN_DONE(funcctx);
	}
}

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
    else if (strcmp(NameStr(att->attname), "display_name") == 0)
        return place->display_name;
    else if (strcmp(NameStr(att->attname), "display_rank") == 0)
        return place->display_rank;
    else if (strcmp(att->attname.data, "place_id") == 0)
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
    else if (strcmp(NameStr(att->attname), "result") == 0)
        return place->result;
    else if (strcmp(NameStr(att->attname), "addressparts") == 0)
        return place->addressparts;
    else 
        return NULL;

}

static Datum ConvertDatum(HeapTuple tuple, int pgtype, int pgtypmod, char *value)
{

    regproc typinput;
    
    tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(pgtype));

    if (!HeapTupleIsValid(tuple)) 
    {
        ereport(ERROR, 
            (errcode(ERRCODE_FDW_INVALID_DATA_TYPE),
                errmsg("cache lookup failed for type %u (osm_id)", pgtype)));
    }

    typinput = ((Form_pg_type)GETSTRUCT(tuple))->typinput;
    ReleaseSysCache(tuple);

	if(pgtype == FLOAT4OID || 
	   pgtype == FLOAT8OID || 
	   pgtype == NUMERICOID || 
	   pgtype == TIMESTAMPOID || 
	   pgtype == TIMESTAMPTZOID || 
	   pgtype == VARCHAROID)
		return  OidFunctionCall3(
					typinput,
					CStringGetDatum(value),
					ObjectIdGetDatum(InvalidOid),
					Int32GetDatum(pgtypmod));
	else
		return OidFunctionCall1(typinput, CStringGetDatum(value));

}

static NominatimFDWState *InitSession(const char *srvname)
{
	NominatimFDWState *state = (NominatimFDWState *)palloc0(sizeof(NominatimFDWState));
	ForeignServer *server = GetForeignServerByName(srvname, true);

    state->request_redirect = 1L;
    state->max_retries = NOMINATIM_DEFAULT_MAXRETRY;
    state->request_max_redirect = NOMINATIM_DEFAULT_MAXREDIRECT;
    state->accept_language = NOMINATIM_DEFAULT_LANGUAGE;

	elog(DEBUG1, "%s called: '%s'", __func__, srvname);

	if (server)
	{
		ListCell *cell;

		foreach (cell, server->options)
		{
			DefElem *def = lfirst_node(DefElem, cell);

			elog(DEBUG1, "  %s parsing node '%s': %s", __func__, def->defname, defGetString(def));

			if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_URL) == 0)
			{
				state->url = defGetString(def);
			}

			if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_HTTP_PROXY) == 0)
			{
				state->proxy = defGetString(def);
				state->proxy_type = NOMINATIM_SERVER_OPTION_HTTP_PROXY;
			}

			if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_PROXY_USER) == 0)
			{
				state->proxy_user = defGetString(def);
			}

			if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_PROXY_USER_PASSWORD) == 0)
			{
				state->proxy_user_password = defGetString(def);
			}

			if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_CONNECTTIMEOUT) == 0)
			{
				char *tailpt;
				char *timeout_str = defGetString(def);

				state->connect_timeout = strtol(timeout_str, &tailpt, 0);
			}

			if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_MAXREDIRECT) == 0)
			{
				char *tailpt;
				char *maxredirect_str = defGetString(def);

				state->request_max_redirect = strtol(maxredirect_str, &tailpt, 10); 
			}

   			if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_MAXCONNECTRETRY) == 0)
			{
				char *tailpt;
				char *val = defGetString(def);

				state->max_retries = strtol(val, &tailpt, 10);              
			} 
                       
            if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_LANGUAGE) == 0)
				state->accept_language = defGetString(def);
		}
	}
	else
	{
		ereport(ERROR,
				(errcode(ERRCODE_CONNECTION_DOES_NOT_EXIST),
				 errmsg("FOREIGN SERVER does not exist: '%s'", srvname)));
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
	char *sparqlxml = "content-type: text/xml";
	char *sparqlxmlutf8 = "content-type: text/xml; charset=utf-8";

	Assert(contents);

    //elog(DEBUG2,"Contents > %s", contents);

	/* is it a "content-type" entry? "*/	
	if (strncasecmp(contents, sparqlxml, 13) == 0)
	{

		if (strncasecmp(contents, sparqlxml, strlen(sparqlxml)) != 0 &&
			strncasecmp(contents, sparqlxmlutf8, strlen(sparqlxmlutf8)) != 0)
		{
			/* remove crlf */
			contents[strlen(contents) - 2] = '\0';
			elog(WARNING, "%s: unsupported header entry: \"%s\"", __func__, contents);
			return 0;
		}
	}

	ptr = repalloc(mem->memory, mem->size + realsize + 1);

	if (!ptr)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
				 errmsg("[%s] out of memory (repalloc returned NULL)", __func__)));
	}

	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

// static void InitSession(struct NominatimFDWState *state, RelOptInfo *baserel, PlannerInfo *root) 
// {

//     //ForeignTable *ft = GetForeignTable(state->nominatim_table->foreigntableid);
// 	//ForeignServer *server = GetForeignServer(ft->serverid);	
//     //ListCell *cell;
// #if PG_VERSION_NUM < 130000
// 	Relation rel = heap_open(ft->relid, NoLock);
// #else
// 	Relation rel = table_open(state->nominatim_table->foreigntableid, NoLock);
// #endif

// 	elog(DEBUG1,"%s called",__func__);

// 	/*
// 	 * Setting session's default values.
// 	 */
//     state->format = NOMINATIM_DEFAULT_FORMAT;
// 	state->connect_timeout = NOMINATIM_DEFAULT_CONNECTTIMEOUT;
// 	state->max_retries = NOMINATIM_DEFAULT_MAXRETRY;	
// 	state->numcols = rel->rd_att->natts;

// }


// static void RaiseNominatimException(xmlNodePtr error)
// {
//     xmlNodePtr node;

//     if (xmlStrcmp(error->name, (xmlChar *)"error") == 0)
//     {
//         char *code = NULL;
//         char *message = NULL;

//         for (node = xmlDocGetRootElement(error->doc)->children; node != NULL; node = node->next)
//         {
//             if (xmlStrcmp(node->name, (xmlChar *)"code") == 0)
//                 code = (char *)xmlNodeGetContent(node);
//             else if (xmlStrcmp(node->name, (xmlChar *)"message") == 0)
//                 message = (char *)xmlNodeGetContent(node);
//         }

//         if(code && message)
//             ereport(ERROR,
//                     (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_REPLY),
//                     errmsg("code %s: %s", code, message)));

//     }
// }

static void LoadDataReverseLookup(NominatimFDWState *state)
{
    struct NominatimRecord *place = (struct NominatimRecord *)palloc0(sizeof(struct NominatimRecord));
    xmlNodePtr reversegeocode;
    xmlNodePtr tag;
    StringInfoData addressparts;
    StringInfoData extratags;
    StringInfoData namedetails;
           
    initStringInfo(&addressparts);
    initStringInfo(&extratags);
    initStringInfo(&namedetails);

    appendStringInfo(&addressparts, "{");
    appendStringInfo(&extratags, "{");
    appendStringInfo(&namedetails, "{");

    elog(DEBUG1, "%s called", __func__);

    if (ExecuteRequest(state) != REQUEST_SUCCESS)
        elog(ERROR, "%s -> request failed: '%s'", __func__, state->url);

    Assert(state->xmldoc);

    place->querystring = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"querystring");
    place->timestamp = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"timestamp");
    place->attribution = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"attribution");
    place->querystring = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"querystring");

    for (reversegeocode = xmlDocGetRootElement(state->xmldoc)->children; reversegeocode != NULL; reversegeocode = reversegeocode->next)
    {
    
        if (xmlStrcmp(reversegeocode->name, (xmlChar *)"result") == 0)
        {

            place->ref = (char *)xmlGetProp(reversegeocode, (xmlChar *)"ref");
            place->address_rank = (char *)xmlGetProp(reversegeocode, (xmlChar *)"address_rank");
            place->boundingbox = (char *)xmlGetProp(reversegeocode, (xmlChar *)"boundingbox");
            place->class = (char *)xmlGetProp(reversegeocode, (xmlChar *)"class");                
            place->icon = (char *)xmlGetProp(reversegeocode, (xmlChar *)"icon");
            place->importance = (char *)xmlGetProp(reversegeocode, (xmlChar *)"importance");
            place->lat = (char *)xmlGetProp(reversegeocode, (xmlChar *)"lat");
            place->lon = (char *)xmlGetProp(reversegeocode, (xmlChar *)"lon");
            place->osm_id = (char *)xmlGetProp(reversegeocode, (xmlChar *)"osm_id");
            place->osm_type = (char *)xmlGetProp(reversegeocode, (xmlChar *)"osm_type");
            place->place_id = (char *)xmlGetProp(reversegeocode, (xmlChar *)"place_id");
            place->place_rank = (char *)xmlGetProp(reversegeocode, (xmlChar *)"place_rank");

            if (xmlGetProp(reversegeocode, (xmlChar *)"geotext"))
                place->polygon = (char *)xmlGetProp(reversegeocode, (xmlChar *)"geotext");
            else if (xmlGetProp(reversegeocode, (xmlChar *)"geojson"))
                place->polygon = (char *)xmlGetProp(reversegeocode, (xmlChar *)"geojson");
            else if (xmlGetProp(reversegeocode, (xmlChar *)"geosvg"))
                place->polygon = (char *)xmlGetProp(reversegeocode, (xmlChar *)"geosvg");
                
            place->result = pstrdup((char *)xmlNodeGetContent(reversegeocode));

        }
        else if (xmlStrcmp(reversegeocode->name, (xmlChar *)"addressparts") == 0)
        {

            for (tag = reversegeocode->children; tag != NULL; tag = tag->next)
            {
                
                appendStringInfo(&addressparts, "%s\"%s\":\"%s\"",
                                 addressparts.len == 1 ? "" : ",",
                                 (char *)tag->name,
                                 (char *)xmlNodeGetContent(tag));
            }

            place->addressparts = NameStr(addressparts);
        } 
        else if (xmlStrcmp(reversegeocode->name, (xmlChar *)"extratags") == 0)
        {

            for (tag = reversegeocode->children; tag != NULL; tag = tag->next)
            {
                appendStringInfo(&extratags, "%s\"%s\":\"%s\"",
                                extratags.len == 1 ? "" : ",",
                                (char *)xmlGetProp(tag, (xmlChar *)"key"),
                                (char *)xmlGetProp(tag, (xmlChar *)"value"));
            }

        }
        else if (xmlStrcmp(reversegeocode->name, (xmlChar *)"namedetails") == 0)
        {
            for (tag = reversegeocode->children; tag != NULL; tag = tag->next)
            {
                appendStringInfo(&namedetails, "%s\"%s\":\"%s\"",
                                    namedetails.len == 1 ? "" : ",",
                                    (char *)xmlGetProp(tag, (xmlChar *)"desc"),
                                    (char *)xmlNodeGetContent(tag));
                
            }

        }

    }

    appendStringInfo(&addressparts, "}");
    appendStringInfo(&extratags, "}");
    appendStringInfo(&namedetails, "}");
    
    place->addressparts = NameStr(addressparts);
    place->extratags = NameStr(extratags);
    place->namedetails = NameStr(namedetails);

    state->records = lappend(state->records, place);

}

static void LoadData(NominatimFDWState * state)
{
        xmlNodePtr searchresults;
        xmlNodePtr places;
        xmlNodePtr tag;

        // state->rowcount = 0;
        state->records = NIL;

        elog(DEBUG1, "%s called", __func__);

        if (ExecuteRequest(state) != REQUEST_SUCCESS)
            elog(ERROR, "%s -> request failed: '%s'", __func__, state->url);

        Assert(state->xmldoc);

        //elog(DEBUG2, "  %s: loading '%s'", __func__, xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"querystring"));

        for (searchresults = xmlDocGetRootElement(state->xmldoc)->children; searchresults != NULL; searchresults = searchresults->next)
        {

            if (xmlStrcmp(searchresults->name, (xmlChar *)"place") == 0)
            {

                struct NominatimRecord *place = (struct NominatimRecord *)palloc0(sizeof(struct NominatimRecord));
                StringInfoData xtags;
                StringInfoData addressdetails;
                StringInfoData namedetails;

                initStringInfo(&xtags);
                initStringInfo(&addressdetails);
                initStringInfo(&namedetails);

                appendStringInfo(&xtags, "{");
                appendStringInfo(&addressdetails, "{");
                appendStringInfo(&namedetails, "{");

                place->ref = (char *)xmlGetProp(searchresults, (xmlChar *)"ref");
                place->address_rank = (char *)xmlGetProp(searchresults, (xmlChar *)"address_rank");
                place->attribution = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"attribution");
                place->boundingbox = (char *)xmlGetProp(searchresults, (xmlChar *)"boundingbox");
                place->class = (char *)xmlGetProp(searchresults, (xmlChar *)"class");
                place->display_name = (char *)xmlGetProp(searchresults, (xmlChar *)"display_name");
                place->display_rank = (char *)xmlGetProp(searchresults, (xmlChar *)"display_rank");
                place->exclude_place_ids = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"exclude_place_ids");
                place->icon = (char *)xmlGetProp(searchresults, (xmlChar *)"icon");
                place->importance = (char *)xmlGetProp(searchresults, (xmlChar *)"importance");
                place->lat = (char *)xmlGetProp(searchresults, (xmlChar *)"lat");
                place->lon = (char *)xmlGetProp(searchresults, (xmlChar *)"lon");
                place->more_url = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"more_url");
                place->osm_id = (char *)xmlGetProp(searchresults, (xmlChar *)"osm_id");
                place->osm_type = (char *)xmlGetProp(searchresults, (xmlChar *)"osm_type");
                place->place_id = (char *)xmlGetProp(searchresults, (xmlChar *)"place_id");
                place->place_rank = (char *)xmlGetProp(searchresults, (xmlChar *)"place_rank");

                if (xmlGetProp(searchresults, (xmlChar *)"geotext"))
                    place->polygon = (char *)xmlGetProp(searchresults, (xmlChar *)"geotext");
                else if (xmlGetProp(searchresults, (xmlChar *)"geojson"))
                    place->polygon = (char *)xmlGetProp(searchresults, (xmlChar *)"geojson");
                else if (xmlGetProp(searchresults, (xmlChar *)"geosvg"))
                    place->polygon = (char *)xmlGetProp(searchresults, (xmlChar *)"geosvg");

                place->querystring = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"querystring");
                place->timestamp = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"timestamp");

                for (places = searchresults->children; places != NULL; places = places->next)
                {
                    if (xmlStrcmp(places->name, (xmlChar *)"extratags") == 0)
                    {
                        for (tag = places->children; tag != NULL; tag = tag->next)
                        {
                            appendStringInfo(&xtags, "%s\"%s\":\"%s\"",
                                             xtags.len == 1 ? "" : ",",
                                             (char *)xmlGetProp(tag, (xmlChar *)"key"),
                                             (char *)xmlGetProp(tag, (xmlChar *)"value"));
                        }
                    }
                    else if (xmlStrcmp(places->name, (xmlChar *)"namedetails") == 0)
                    {
                        for (tag = places->children; tag != NULL; tag = tag->next)
                        {
                            appendStringInfo(&namedetails, "%s\"%s\":\"%s\"",
                                             namedetails.len == 1 ? "" : ",",
                                             (char *)xmlGetProp(tag, (xmlChar *)"desc"),
                                             (char *)xmlNodeGetContent(tag));
                        }
                    }
                    else if (xmlStrcmp(places->name, (xmlChar *)"geokml") == 0)
                    {
                        xmlBufferPtr buffer = xmlBufferCreate();
                        xmlNodeDump(buffer, state->xmldoc, places->children, 0, 0);
                        place->polygon = pstrdup((char *)buffer->content);
                        xmlBufferFree(buffer);
                    }
                    else
                    {
                        appendStringInfo(&addressdetails, "%s\"%s\":\"%s\"",
                                         addressdetails.len == 1 ? "" : ",",
                                         (char *)places->name,
                                         (char *)xmlNodeGetContent(places));
                    }
                }

                appendStringInfo(&xtags, "}");
                appendStringInfo(&addressdetails, "}");
                appendStringInfo(&namedetails, "}");

                place->extratags = NameStr(xtags);
                place->addressdetails = NameStr(addressdetails);
                place->namedetails = NameStr(namedetails);

                state->records = lappend(state->records, place);
            }
        }

        if (tag)
            xmlFreeNode(tag);

        if (places)
            xmlFreeNode(places);

        if (searchresults)
            xmlFreeNode(searchresults);
    }

static int ExecuteRequest(NominatimFDWState *state)
{
    CURL *curl;
    CURLcode res;
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

    elog(DEBUG1, "%s called", __func__);

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    initStringInfo(&url_buffer);
    appendStringInfo(&url_buffer, "%s", state->url);

    // if (state->city)
    //     appendStringInfo(&url_buffer, "%s=%s", NOMINATIM_TAG_CITY, state->city);

    appendStringInfo(&url_buffer, "/%s?", state->request_type);

    if (state->query && strlen(state->query)>0)
        appendStringInfo(&url_buffer, "q=%s&", curl_easy_escape(curl, state->query, 0));
    
    if (state->amenity && strlen(state->amenity)>0)
        appendStringInfo(&url_buffer, "amenity=%s&", curl_easy_escape(curl, state->amenity, 0));

    if (state->osm_ids && strlen(state->osm_ids)>0)
        appendStringInfo(&url_buffer, "osm_ids=%s&", curl_easy_escape(curl, state->osm_ids, 0));

    if (state->street && strlen(state->street)>0)
        appendStringInfo(&url_buffer, "street=%s&", curl_easy_escape(curl, state->street, 0));

    if (state->city && strlen(state->city)>0)
        appendStringInfo(&url_buffer, "city=%s&", curl_easy_escape(curl, state->city, 0));

    if (state->county && strlen(state->county)>0)
        appendStringInfo(&url_buffer, "county=%s&", curl_easy_escape(curl, state->county, 0));

    if (state->state && strlen(state->state)>0)
        appendStringInfo(&url_buffer, "state=%s&", curl_easy_escape(curl, state->state, 0));

    if (state->country && strlen(state->country)>0)
        appendStringInfo(&url_buffer, "country=%s&", curl_easy_escape(curl, state->country, 0));

    if (state->postalcode && strlen(state->postalcode)>0)
        appendStringInfo(&url_buffer, "postalcode=%s&", curl_easy_escape(curl, state->postalcode, 0));

    if (!state->format)
        appendStringInfo(&url_buffer, "format=%s&", curl_easy_escape(curl, NOMINATIM_DEFAULT_FORMAT, 0));

    if (state->lon)
        appendStringInfo(&url_buffer, "lon=%f&", state->lon);

    if (state->lat)
        appendStringInfo(&url_buffer, "lat=%f&", state->lat);

    if (state->zoom)
        appendStringInfo(&url_buffer, "zoom=%d&", state->zoom);

    if (state->layer && strlen(state->layer)>0)
        appendStringInfo(&url_buffer, "layer=%s&", state->layer);
    // appendStringInfo(&url_buffer, "%s%s&",strcmp(state->layer,"") == 0 ? "" : "layer=", state->layer);
    
    if(state->extratags)
        appendStringInfo(&url_buffer, "extratags=1&");
    
    if(state->namedetails)
        appendStringInfo(&url_buffer, "namedetails=1&");
    
    if(state->addressdetails)
        appendStringInfo(&url_buffer, "addressdetails=1&");
    else
        appendStringInfo(&url_buffer, "addressdetails=0&");
    
    if(state->polygon_type && strlen(state->polygon_type)>0)
        appendStringInfo(&url_buffer, "%s=1&",state->polygon_type);

    if (state->accept_language && strlen(state->accept_language)>0)
        appendStringInfo(&url_buffer, "accept-language=%s&", curl_easy_escape(curl, state->accept_language, 0));

    if (state->countrycodes && strlen(state->countrycodes)>0)
        appendStringInfo(&url_buffer, "countrycodes=%s&", curl_easy_escape(curl, state->countrycodes, 0));
    
    if (state->layer && strlen(state->layer)>0)
        appendStringInfo(&url_buffer, "layer=%s&", curl_easy_escape(curl, state->layer, 0));

    if (state->feature_type && strlen(state->feature_type)>0)
        appendStringInfo(&url_buffer, "featureType=%s&", curl_easy_escape(curl, state->feature_type, 0));

    if (state->exclude_place_ids && strlen(state->exclude_place_ids)>0)
        appendStringInfo(&url_buffer, "exclude_place_ids=%s&", curl_easy_escape(curl, state->exclude_place_ids, 0));

    if (state->viewbox && strlen(state->viewbox)>0)
        appendStringInfo(&url_buffer, "viewbox=%s&", curl_easy_escape(curl, state->viewbox, 0));

    if (state->bounded)
        appendStringInfo(&url_buffer, "bounded=1&");
    else
        appendStringInfo(&url_buffer, "bounded=0&");

    if (state->polygon_threshold && state->polygon_threshold != 0.0)
        appendStringInfo(&url_buffer, "polygon_threshold=%f&",state->polygon_threshold);

    if (state->email && strlen(state->email)>0)
        appendStringInfo(&url_buffer, "email=%s&", curl_easy_escape(curl, state->email, 0));

    if (state->dedupe)
        appendStringInfo(&url_buffer, "dedupe=1&");
    else
        appendStringInfo(&url_buffer, "dedupe=0&");
    
    if(state->limit > 0)
        appendStringInfo(&url_buffer, "limit=%d&",state->limit);
    
    if(state->offset > 0)
        appendStringInfo(&url_buffer, "offset=%d&",state->offset);
    // if (strcmp(state->extra_params, "") != 0)
    //     appendStringInfo(&url_buffer, "%s", state->extra_params);

    if (curl)
    {
        errbuf[0] = 0;

        elog(DEBUG1, "  %s: setting URL: %s", __func__, url_buffer.data);
        // curl_easy_setopt(curl, CURLOPT_URL, state->url);
        curl_easy_setopt(curl, CURLOPT_URL, url_buffer.data);

        // elog(DEBUG1, "  %s: url build > %s?%s", __func__, state->url, url_buffer.data);

#if ((LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR < 85) || LIBCURL_VERSION_MAJOR < 7)
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#else
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#endif

        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, state->connect_timeout);
        elog(DEBUG1, "  %s: timeout > %ld", __func__, state->connect_timeout);
        elog(DEBUG1, "  %s: max retry > %ld", __func__, state->max_retries);

        if (state->proxy)
        {
            elog(DEBUG1, "  %s: proxy URL > '%s'", __func__, state->proxy);

            curl_easy_setopt(curl, CURLOPT_PROXY, state->proxy);

            if (strcmp(state->proxy_type, NOMINATIM_SERVER_OPTION_HTTP_PROXY) == 0)
            {
                elog(DEBUG1, "  %s: proxy protocol > 'HTTP'", __func__);
                curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
            }
            else if (strcmp(state->proxy_type, NOMINATIM_SERVER_OPTION_HTTPS_PROXY) == 0)
            {
                elog(DEBUG1, "  %s: proxy protocol > 'HTTPS'", __func__);
                curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_HTTPS);
            }

            if (state->proxy_user)
            {
                elog(DEBUG1, "  %s: entering proxy user ('%s').", __func__, state->proxy_user);
                curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, state->proxy_user);
            }

            if (state->proxy_user_password)
            {
                elog(DEBUG1, "  %s: entering proxy user's password.", __func__);
                curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, state->proxy_user_password);
            }
        }

        if (state->request_redirect)
        {
            elog(DEBUG1, "  %s: setting request redirect: %d (%s)", __func__, state->request_redirect, state->request_redirect ? "true" : "false");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            if (state->request_max_redirect)
            {
                elog(DEBUG1, "  %s: setting maxredirs: %ld", __func__, state->request_max_redirect);
                curl_easy_setopt(curl, CURLOPT_MAXREDIRS, state->request_max_redirect);
            }
        }

        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        // curl_easy_setopt(curl, CURLOPT_POSTFIELDS, url_buffer.data);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallbackFunction);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&chunk_header);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

        // curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        // curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, 'GET');

        initStringInfo(&user_agent);
        appendStringInfo(&user_agent, "PostgreSQL/%s nominatim_fdw/%s libxml2/%s %s", PG_VERSION, FDW_VERSION, LIBXML_DOTTED_VERSION, curl_version());
        // appendStringInfo(&user_agent, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

        elog(DEBUG1, "  %s: \"Agent: %s\"", __func__, user_agent.data);

        //curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, user_agent.data);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.data);

        initStringInfo(&accept_header);
        appendStringInfo(&accept_header, "Accept-Language: %s", state->accept_language);
        headers = curl_slist_append(headers, NameStr(accept_header));
        elog(DEBUG1,"  adding header: %s",NameStr(accept_header));
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        elog(DEBUG2, "  %s: performing cURL request ... ", __func__);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
        {

            for (long i = 1; i <= state->max_retries && (res = curl_easy_perform(curl)) != CURLE_OK; i++)
            {
                long response_code;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                elog(WARNING, "  %s: request to '%s' failed with return code %ld (%ld)", __func__, state->url, response_code, i);
            }
        }

        if (res != CURLE_OK)
        {
            size_t len = strlen(errbuf);
            fprintf(stderr, "\nlibcurl: (%d) ", res);

            // if(chunk.memory)
            //     RaiseNominatimException(xmlReadMemory(chunk.memory, chunk.size, NULL, NULL, XML_PARSE_NOBLANKS));

            xmlFreeDoc(state->xmldoc);
            pfree(chunk.memory);
            pfree(chunk_header.memory);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            curl_global_cleanup();

            if (len)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
                            errmsg("%s => (%u) %s%s", __func__, res, errbuf,
                                ((errbuf[len - 1] != '\n') ? "\n" : ""))));
            }
            else
            {
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
                            errmsg("%s => (%u) '%s'\n", __func__, res, curl_easy_strerror(res))));
            }
        }
        else
        {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            state->xmldoc = xmlReadMemory(chunk.memory, chunk.size, NULL, NULL, XML_PARSE_NOBLANKS);

            elog(DEBUG2, "  %s: http response code = %ld", __func__, response_code);
            elog(DEBUG2, "  %s: http response size = %ld", __func__, chunk.size);
            elog(DEBUG2, "  %s: http response header = \n%s", __func__, chunk_header.memory);
        }
    }

    pfree(chunk.memory);
    pfree(chunk_header.memory);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    /*
        * We thrown an error in case the SPARQL endpoint returns an empty XML doc
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

	elog(DEBUG1, "%s called > '%s'", __func__, url);

	code = curl_url_set(handler, CURLUPART_URL, url, 0);

	curl_url_cleanup(handler);

	elog(DEBUG1, "  %s handler return code: %u", __func__, code);

	if (code != 0)
	{
		elog(DEBUG1, "%s: invalid URL (%u) > '%s'", __func__, code, url);
		return code;
	}

	return REQUEST_SUCCESS;
}

static bool IsPolygonTypeSupported(char *polygon_type)
{
    if(!polygon_type)
        return false;

    return (strcmp(polygon_type,"") == 0 ||
           strcmp(polygon_type,"polygon_text") == 0 ||
           strcmp(polygon_type,"polygon_geojson") == 0 ||
           strcmp(polygon_type,"polygon_kml") == 0 ||
           strcmp(polygon_type,"polygon_svg") == 0);
          
}