
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

#define NOMINATIM_SERVER_OPTION_URL "url"
#define NOMINATIM_SERVER_OPTION_FORMAT "format"
#define NOMINATIM_SERVER_OPTION_CONNECTTIMEOUT "connect_timeout"
#define NOMINATIM_SERVER_OPTION_CONNECTRETRY "connect_retry"
#define NOMINATIM_SERVER_OPTION_REQUEST_REDIRECT "request_redirect"
#define NOMINATIM_SERVER_OPTION_REQUEST_MAX_REDIRECT "request_max_redirect"
#define NOMINATIM_SERVER_OPTION_HTTP_PROXY "http_proxy"
#define NOMINATIM_SERVER_OPTION_HTTPS_PROXY "https_proxy"
#define NOMINATIM_SERVER_OPTION_PROXY_USER "proxy_user"
#define NOMINATIM_SERVER_OPTION_PROXY_USER_PASSWORD "proxy_user_password"
#define NOMINATIM_SERVER_OPTION_QUERY "query"

#define NOMINATIM_TAG_CITY "city"
#define NOMINATIM_TAG_STREET "street"
#define NOMINATIM_TAG_COUNTY "county"
#define NOMINATIM_TAG_COUNTRY "COUNTRY"
#define NOMINATIM_TAG_STATE "STATE"
#define NOMINATIM_TAG_POSTALCODE "postalcode"

#define NOMINATIM_COLUMN_OPTION_TAG "tag"

#define NOMINATIM_DEFAULT_CONNECTTIMEOUT 300
#define NOMINATIM_DEFAULT_MAXRETRY 3
#define NOMINATIM_DEFAULT_FORMAT "xml"
#define NOMINATIM_DEFAULT_URL "https://nominatim.openstreetmap.org/search"

PG_MODULE_MAGIC;

typedef struct NominatimFDWState
{
	int numcols;       /* Total number of columns in the foreign table. */
	int rowcount;      /* Number of rows currently returned to the client */
	int pagesize;      /* Total number of records retrieved from the SPARQL endpoint*/
	char* url;
    char *amenity;     /*  */
    char *street;      /*  */
    char *city;        /*  */
    char *couny;       /*  */
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
	bool request_redirect;       /* Enables or disables URL redirecting. */
   	long request_max_redirect;   /* Limit of how many times the URL redirection (jump) may occur. */
	long connect_timeout;        /* Timeout for SPARQL queries */
	long max_retries;            /* Number of re-try attemtps for failed SPARQL queries */
	xmlDocPtr xmldoc;            
    List *records;
	struct NominatimFDWTable *nominatim_table; /* */
	;     /* */
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
	{NOMINATIM_SERVER_OPTION_FORMAT, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_HTTP_PROXY, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_HTTPS_PROXY, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_PROXY_USER, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_PROXY_USER_PASSWORD, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_CONNECTTIMEOUT, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_CONNECTRETRY, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_REQUEST_REDIRECT, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_REQUEST_MAX_REDIRECT, ForeignServerRelationId, false, false},
	{NOMINATIM_SERVER_OPTION_QUERY, ForeignServerRelationId, false, false},
	/* Foreign Tables */
	{NOMINATIM_TAG_CITY, ForeignTableRelationId, true, false},
	{NOMINATIM_TAG_COUNTRY, ForeignTableRelationId, true, false},
    {NOMINATIM_TAG_COUNTY, ForeignTableRelationId, true, false},
    {NOMINATIM_TAG_POSTALCODE, ForeignTableRelationId, true, false},
    {NOMINATIM_TAG_STATE, ForeignTableRelationId, true, false},
    {NOMINATIM_TAG_STREET, ForeignTableRelationId, true, false},
	/* Options for Foreign Table's Columns */
	{NOMINATIM_COLUMN_OPTION_TAG, AttributeRelationId, true, true},
	/* EOList option */
	{NULL, InvalidOid, false, false}
};

extern Datum nominatim_fdw_handler(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_validator(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_version(PG_FUNCTION_ARGS);
extern Datum nominatim_fdw_query(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(nominatim_fdw_handler);
PG_FUNCTION_INFO_V1(nominatim_fdw_validator);
PG_FUNCTION_INFO_V1(nominatim_fdw_version);
PG_FUNCTION_INFO_V1(nominatim_fdw_query);

static void NominatimGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static void NominatimGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static ForeignScan *NominatimGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses, Plan *outer_plan);
static void NominatimBeginForeignScan(ForeignScanState *node, int eflags);
static TupleTableSlot *NominatimIterateForeignScan(ForeignScanState *node);
static void NominatimReScanForeignScan(ForeignScanState *node);
static void NominatimEndForeignScan(ForeignScanState *node);
static Datum ConvertDatum(HeapTuple tuple, int pgtype, int pgtypemod, char *value);
static NominatimFDWState *GetServerInfo(const char *srvname);

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
static size_t HeaderCallbackFunction(char *contents, size_t size, size_t nmemb, void *userp);
static void InitSession(struct NominatimFDWState *state, RelOptInfo *baserel, PlannerInfo *root);
static void LoadData(NominatimFDWState *state);
static int ExecuteRequest(NominatimFDWState *state);

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
	Datum res = 1;
	return res;
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

Datum nominatim_fdw_query(PG_FUNCTION_ARGS)
{
	text *srvname_text = PG_GETARG_TEXT_P(0);
    text *query_text = PG_GETARG_TEXT_P(1);
	FuncCallContext *funcctx;
	TupleDesc tupdesc;
    NominatimFDWState *state = GetServerInfo(text_to_cstring(srvname_text));
    
	state->query = text_to_cstring(query_text);
 

	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcontext;
		
       		
		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

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
		
		for (size_t i = 0; i < tupdesc->natts; i++)
		{
			char *value = NULL;
			Form_pg_attribute att = TupleDescAttr(tupdesc, i);					

			if(strcmp(NameStr(att->attname),"osm_id") == 0)
				value = place->osm_id;
			else if(strcmp(NameStr(att->attname),"osm_type") == 0)
				value = place->osm_type;
			else if(strcmp(NameStr(att->attname),"ref") == 0)
				value = place->ref;
			else if(strcmp(NameStr(att->attname),"class") == 0)
				value = place->class;
			else if(strcmp(NameStr(att->attname),"display_name") == 0)
				value = place->display_name;
			else if(strcmp(NameStr(att->attname),"display_rank") == 0)
				value = place->display_rank;
			else if(strcmp(att->attname.data,"place_id") == 0)
				value = place->place_id;
			else if(strcmp(NameStr(att->attname),"place_rank") == 0)
				value = place->place_rank;
			else if(strcmp(NameStr(att->attname),"address_rank") == 0)
				value = place->address_rank;
			else if(strcmp(NameStr(att->attname),"lon") == 0)
				value = place->lon;
			else if(strcmp(NameStr(att->attname),"lat") == 0)
				value = place->lat;
			else if(strcmp(NameStr(att->attname),"boundingbox") == 0)
				value = place->boundingbox;
			else if(strcmp(NameStr(att->attname),"importance") == 0)
				value = place->importance;
			else if(strcmp(NameStr(att->attname),"icon") == 0)
				value = place->icon;
			else if(strcmp(NameStr(att->attname),"extratags") == 0)
				value = place->extratags;
			else if(strcmp(NameStr(att->attname),"timestamp") == 0)
				value = place->timestamp;
			else if(strcmp(NameStr(att->attname),"attribution") == 0)
				value = place->attribution;
			else if(strcmp(NameStr(att->attname),"querystring") == 0)
				value = place->querystring;
			else if(strcmp(NameStr(att->attname),"polygon") == 0)
				value = place->polygon;
			else if(strcmp(NameStr(att->attname),"exclude_place_ids") == 0)
				value = place->exclude_place_ids;
			else if(strcmp(NameStr(att->attname),"more_url") == 0)
				value = place->more_url;
            else if(strcmp(NameStr(att->attname),"addressdetails") == 0)
				value = place->addressdetails;
            else if(strcmp(NameStr(att->attname),"namedetails") == 0)
				value = place->namedetails;

			if(value)
				values[i] = ConvertDatum(tuple, att->atttypid, att->atttypmod, value);
			else
				nulls[i] = true;

            elog(DEBUG2,"  %s = '%s'",NameStr(att->attname), value);

		}
		
		tuple = heap_form_tuple(funcctx->attinmeta->tupdesc, values, nulls);
		result = HeapTupleGetDatum(tuple);

		SRF_RETURN_NEXT(funcctx, result);
	}
	else
	{
		SRF_RETURN_DONE(funcctx);
	}
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

static NominatimFDWState *GetServerInfo(const char *srvname)
{
	NominatimFDWState *state = (NominatimFDWState *)palloc0(sizeof(NominatimFDWState));
	ForeignServer *server = GetForeignServerByName(srvname, true);

	state->request_max_redirect = 0L;
    state->request_redirect = 1L;
    state->max_retries = 3;
    state->request_max_redirect = 5;

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

            if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_REQUEST_REDIRECT) == 0)
			{
				state->request_redirect = defGetBoolean(def);

				elog(DEBUG1, "  %s: setting \"%s\": %d", __func__, NOMINATIM_SERVER_OPTION_REQUEST_REDIRECT, state->request_redirect);
			}

			if (strcmp(def->defname, NOMINATIM_SERVER_OPTION_REQUEST_MAX_REDIRECT) == 0)
			{
				char *tailpt;
				char *maxredirect_str = defGetString(def);

				state->request_max_redirect = strtol(maxredirect_str, &tailpt, 10);

				elog(DEBUG1, "  %s: setting \"%s\": %ld", __func__, NOMINATIM_SERVER_OPTION_REQUEST_MAX_REDIRECT, state->request_max_redirect);

				if (strcmp(defGetString(def), "0") != 0 && state->request_max_redirect == 0)
				{
					elog(ERROR, "invalid value for \"%s\"", NOMINATIM_SERVER_OPTION_REQUEST_MAX_REDIRECT);
				}
			}
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

static void InitSession(struct NominatimFDWState *state, RelOptInfo *baserel, PlannerInfo *root) 
{

    //ForeignTable *ft = GetForeignTable(state->nominatim_table->foreigntableid);
	//ForeignServer *server = GetForeignServer(ft->serverid);	
    //ListCell *cell;
#if PG_VERSION_NUM < 130000
	Relation rel = heap_open(ft->relid, NoLock);
#else
	Relation rel = table_open(state->nominatim_table->foreigntableid, NoLock);
#endif

	elog(DEBUG1,"%s called",__func__);

	/*
	 * Setting session's default values.
	 */
	state->url = NOMINATIM_DEFAULT_URL;
    state->format = NOMINATIM_DEFAULT_FORMAT;
	state->connect_timeout = NOMINATIM_DEFAULT_CONNECTTIMEOUT;
	state->max_retries = NOMINATIM_DEFAULT_MAXRETRY;	
	state->numcols = rel->rd_att->natts;

}

static void LoadData(NominatimFDWState *state)
{
	xmlNodePtr searchresults;
    xmlNodePtr places;
    xmlNodePtr tag;
	
	state->rowcount = 0;
	state->records = NIL;

	elog(DEBUG1, "%s called",__func__);

	if (ExecuteRequest(state) != REQUEST_SUCCESS)
		elog(ERROR, "%s -> request failed: '%s'", __func__, state->url);

	Assert(state->xmldoc);

   elog(DEBUG2, "  %s: loading '%s'",__func__, xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"querystring"));

    
    for (searchresults = xmlDocGetRootElement(state->xmldoc)->children; searchresults != NULL; searchresults = searchresults->next)
	{
        
		if (xmlStrcmp(searchresults->name, (xmlChar *)"place") == 0)
		{                              

            struct NominatimRecord *place = (struct NominatimRecord *) palloc0(sizeof(struct NominatimRecord));
            StringInfoData xtags;
            StringInfoData addressdetails;
            StringInfoData namedetails;

            initStringInfo(&xtags);
            initStringInfo(&addressdetails);
            initStringInfo(&namedetails);
            
            appendStringInfo(&xtags,"{");
            appendStringInfo(&addressdetails,"{");
            appendStringInfo(&namedetails,"{");

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
            place->polygon = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"polygon");
            place->querystring = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"querystring");
            place->timestamp = (char *)xmlGetProp(xmlDocGetRootElement(state->xmldoc), (xmlChar *)"timestamp");

            for (places = searchresults->children; places != NULL; places = places->next)
            {
                if (xmlStrcmp(places->name, (xmlChar *)"extratags") == 0)
                {       
                    for (tag = places->children; tag != NULL; tag = tag->next)
                    {
                        appendStringInfo(&xtags,"%s\"%s\":\"%s\"", 
                            xtags.len == 1 ? "" : ",",
                            (char *)xmlGetProp(tag, (xmlChar *)"key"), 
                            (char *)xmlGetProp(tag, (xmlChar *)"value"));
                    }   
                } 
                else if (xmlStrcmp(places->name, (xmlChar *)"namedetails") == 0)
                {
                    for (tag = places->children; tag != NULL; tag = tag->next)
                    {
                        appendStringInfo(&namedetails,"%s\"%s\":\"%s\"", 
                            namedetails.len == 1 ? "" : ",",
                            (char *)xmlGetProp(tag, (xmlChar *)"desc"), 
                            (char *)xmlNodeGetContent(tag));
                    }
                } 
                else
                {
                    appendStringInfo(&addressdetails,"%s\"%s\":\"%s\"", 
                        addressdetails.len == 1 ? "" : ",",
                        (char *)places->name, 
                        (char *)xmlNodeGetContent(places));
                }

            }

            appendStringInfo(&xtags,"}");
            appendStringInfo(&addressdetails,"}");
            appendStringInfo(&namedetails,"}");
            
            place->extratags = NameStr(xtags);
            place->addressdetails = NameStr(addressdetails);
            place->namedetails = NameStr(namedetails);

            state->records = lappend(state->records, place);
		}
	}

    if(tag)
	    xmlFreeNode(tag);

    if(places)
	 	xmlFreeNode(places);

	if(searchresults)
		xmlFreeNode(searchresults);    
    
}

static int ExecuteRequest(NominatimFDWState *state)
{
    CURL *curl;
    CURLcode res;
    StringInfoData url_buffer;
    StringInfoData user_agent;
    StringInfoData accept_header;

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

    initStringInfo(&accept_header);
    appendStringInfo(&accept_header, "Accept: text/xml");

    initStringInfo(&url_buffer);
    appendStringInfo(&url_buffer, "%s?", state->url);

    if (state->city)
        appendStringInfo(&url_buffer, "%s=%s", NOMINATIM_TAG_CITY, state->city);

    if (state->query)
        appendStringInfo(&url_buffer, "q=%s", curl_easy_escape(curl, state->query, 0));

    if (!state->format)
        appendStringInfo(&url_buffer, "&format=%s&extratags=1&addressdetails=1&namedetails=1", curl_easy_escape(curl, NOMINATIM_DEFAULT_FORMAT, 0));

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
        appendStringInfo(&user_agent, "PostgreSQL/%s rdf_fdw/%s libxml2/%s %s", PG_VERSION, FDW_VERSION, LIBXML_DOTTED_VERSION, curl_version());
        // appendStringInfo(&user_agent, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

        elog(DEBUG1, "  %s: \"Agent: %s\"", __func__, user_agent.data);

        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.data);

        headers = curl_slist_append(headers, accept_header.data);
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
