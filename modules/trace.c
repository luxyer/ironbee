/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file
 * @brief IronBee - Trace Module
 *
 * This is a trace module
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */
#include <stdio.h>
#include <string.h>

#include <ironbee/engine.h>
#include <ironbee/util.h>
#include <ironbee/module.h>
#include <ironbee/cfgmap.h>
#include <ironbee/debug.h>
#include <ironbee/hash.h>
#include <ironbee/bytestr.h>
#include <../util/ironbee_util_private.h>
#include <../engine/ironbee_private.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        trace
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/* Trace module configuration */
typedef struct {
    const char *trace_mpools;   /**< Enable trace of memory pool usage yes/no */
} modtrace_config_t;

/* Event info structure */
typedef struct {
    int          number;
    const char  *name;	
} event_info_t;

/* Memory pool usage data */
typedef struct {
    size_t      size;
    size_t      inuse;
    size_t      count;
} mpool_usage_t;

/* Allocate our global configuration */
static modtrace_config_t modtrace_global_config;

/**
 * @internal
 * Trace generic event handler.
 *
 * Handles a generic event, dumping some info on the event.
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] cbdata Callback data: acutally an event_info_t describing the
 * event.
 */
static ib_status_t modtrace_event_callback(ib_engine_t *ib,
                                           void *param,
                                           void *cbdata)
{
    IB_FTRACE_INIT(event_callback);
    event_info_t *eventp = (event_info_t *)cbdata;
    ib_log_debug(ib, 1, "Callback: %s (%d)", eventp->name, eventp->number);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Trace connection data event handler.
 *
 * Handles handle_connect_event and conn_data_in_event, dumping some info on
 * the event.
 *
 * This function creates a 1024 byte buffer on the stack.  A real
 * (i.e. non-trace) module should probably do something different; perhaps
 * allocate the buffer from the connection's mpool, etc.
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] cbdata Callback data: acutally an event_info_t describing the
 * event.
 */
static void modtrace_handle_conn_data(ib_engine_t *ib,
                                      ib_conndata_t *cd,
                                      void *cbdata)
{
    IB_FTRACE_INIT(modtrace_handle_conn_data);
    event_info_t *eventp = (event_info_t *)cbdata;

    ib_log_debug(ib, 4, "handle_conn_data [%s]: data=%p dlen=%u",
                 eventp->name, cd->data, cd->dlen);
    if (eventp->number == conn_data_in_event) {
        char buf[1024];
        unsigned len = sizeof(buf)-1;
        if (cd->dlen < len) {
            len = cd->dlen;
        }
        memcpy(buf, cd->data, len);
        buf[len] = '\0';
        ib_log_debug(ib, 4, "%s: data=%s", eventp->name, buf);
    }
}

/**
 * @internal
 * Trace tx_data_in_event event handler.
 *
 * Handles a tx_data_in_event, dumping some info on the event.
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] cbdata Callback data: acutally an event_info_t describing the
 * event.
 */
static void modtrace_handle_tx(ib_engine_t *ib,
                               ib_tx_t *tx,
                               void *cbdata)
{
    IB_FTRACE_INIT(modtrace_handle_tx);
    const event_info_t *eventp = (const event_info_t *)cbdata;

    ib_log_debug(ib, 4, "handle_tx [%s]: data=%p tx->dpi=%p",
                 eventp->name, (void*)tx->data, (void*)tx->dpi);

    IB_FTRACE_RET_VOID();
}

/**
 * @internal
 * Add usage of the current memory pool to the usage data
 *
 * @param[in] mp The memory pool to look at
 * @param[in,out] usage The usage data to add the pool's summary to
 */
static void mempool_add_usage(const ib_mpool_t *mp,
                              mpool_usage_t *usage)
{
    usage->size += mp->size;
    usage->inuse += mp->inuse;
    ++(usage->count);
}

/**
 * @internal
 * Walk through the memory pools, calculating total allocations.
 *
 * @param[in] ib IronBee object
 * @param[in] first First memory pool to examine
 * @param[in,out] anon Total of anonymous memory usage
 * @param[in,out] total Total memory usage
 */
static void mempool_walk(ib_engine_t *ib,
                         const ib_mpool_t *first,
                         mpool_usage_t *anon,
                         mpool_usage_t *total)
{
    const ib_mpool_t *mp;

    /* Loop through all of the memory pools, print out memory usage */
    for (mp = first;  mp != NULL;  mp = mp->next ) {
        if (mp->name != NULL) {
            const char *parent;
            if (mp->parent == NULL) {
                parent = "None";
            }
            else if (mp->parent->name == NULL) {
                parent = "Anonymous";
            }
            else {
                parent = mp->parent->name;
            }
            ib_log_debug(ib, 9,
                         "Memory pool '%s': parent='%s' size=%zd inuse=%zd",
                         mp->name, parent, mp->size, mp->inuse);
        }
        else {
            mempool_add_usage(mp, anon);
        }
        mempool_add_usage(mp, total);

        /* Walk through my children */
        if (mp->child != NULL) {
            mempool_walk(ib, mp->child, anon, total);
        }
    }
}

/**
 * @internal
 * Trace tx_{started,finished}_event event handler.
 *
 * Handles tx started and finsihed events, dumping some memory info on the
 * event.
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] cbdata Callback data: acutally an event_info_t describing the
 * event.
 */
static void modtrace_handle_tx_mem(ib_engine_t *ib,
                                   ib_tx_t *tx,
                                   void *cbdata)
{
    IB_FTRACE_INIT(modtrace_handle_tx_mem);
    const event_info_t *eventp = (const event_info_t *)cbdata;
    mpool_usage_t anon  = {0,0,0};
    mpool_usage_t total = {0,0,0};
    modtrace_config_t *config;
    ib_status_t rc;

    modtrace_handle_tx(ib, tx, cbdata);

    /* Get our current configuration */
    rc = ib_context_module_config(tx->ctx,
                                  IB_MODULE_STRUCT_PTR,
                                  (void *)&config);
    if (rc != IB_OK) {
        ib_log_error(ib, 0, "Failed to fetch module %s config: %d",
                     MODULE_NAME_STR, rc);
        IB_FTRACE_RET_VOID();
    }

    /* If mpool tracing is turned off, we're done */
    if (strcmp(config->trace_mpools, "yes") != 0) {
        IB_FTRACE_RET_VOID();
    }

    ib_log_debug(ib, 9, "=== Start Memory Pool Dump (%s) ===", eventp->name);

    /* Walk through all of the memory pools */
    mempool_walk(ib, ib->mp, &anon, &total);

    /* Dump totals */
    ib_log_debug(ib, 9,
                 "Anonymous memory pools: num=%zd size=%zd inuse=%zd",
                 anon.count, anon.size, anon.inuse);
    ib_log_debug(ib, 9,
                 "Memory pool totals: num=%zd size=%zd inuse=%zd",
                 total.count, total.size, total.inuse);
    ib_log_debug(ib, 9, "=== End Memory Pool Dump (%s) ===", eventp->name);

    IB_FTRACE_RET_VOID();
}

/**
 * @internal
 * Trace request_headers_event event handler.
 *
 * Handles a request_headers_event, dumping some info on the event.
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] cbdata Callback data: acutally an event_info_t describing the
 * event.
 */
static ib_status_t modtrace_handle_req_headers(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               void *cbdata)
{
    IB_FTRACE_INIT(modtrace_handle_req_headers);
    event_info_t *eventp = (event_info_t *)cbdata;
    ib_field_t *req = NULL;
    ib_status_t rc = IB_OK;
    ib_list_t *lst = NULL;
    ib_list_node_t *node = NULL;

    /* Extract the request headers field from the provider instance */
    rc = ib_data_get(tx->dpi, "request_headers", &req);
    if ( (req == NULL) || (rc != IB_OK) ) {
        ib_log_debug(ib, 4, "%s: no request headers", eventp->name);
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* The field value *should* be a list, extract it as such */
    lst = ib_field_value_list(req);
    if (lst == NULL) {
        ib_log_debug(ib, 4, "%s: Field list missing / incorrect type",
                     eventp->name );
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Loop through the list */
    IB_LIST_LOOP(lst, node) {
        ib_field_t *field = (ib_field_t *)ib_list_node_data(node);
        ib_bytestr_t *bs;
        unsigned len;
        char buf[128];

        /* Get the bytestr that's the field value */
        bs = ib_field_value_bytestr(field);

        /* Copy the value into a buffer */
        memset(buf, 0, sizeof(buf));
        len = sizeof(buf) - 1;
        if (len > ib_bytestr_length(bs) ) {
            len = ib_bytestr_length(bs);
        }
        memcpy(buf, ib_bytestr_ptr(bs), len);

        /* And, log it
         * Note: field->name is not always a null ('\0') terminated string,
         *       so you must use field->nlen as it's length. */
        ib_log_debug(ib, 4, "%.*s = '%s'", field->nlen, field->name, buf);
    }
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Initialize the trace module.
 *
 * Called when module is loaded.
 * Registers handlers for all IronBee events.
 *
 * @param[in] ib IronBee object
 * @param[in] m Module object
 */
static ib_status_t modtrace_init(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(modtrace_init);
    static event_info_t event_info[IB_STATE_EVENT_NUM];
    ib_status_t rc;
    int event;

    /* Register specific handlers for specific events, and a
     * generic handler for the rest */
    for (event = 0; event < IB_STATE_EVENT_NUM; ++event) {
        ib_void_fn_t  handler = NULL;
        event_info_t *eventp = &event_info[event];

        /* Record event info */
        eventp->number = event;
        eventp->name   = ib_state_event_name((ib_state_event_type_t)event);

        /* For these specific ones, use more spefic handlers */
        switch( event ) {
            case conn_data_in_event:
                handler = (ib_void_fn_t)modtrace_handle_conn_data;
                break;

            case handle_connect_event:
                handler = (ib_void_fn_t)modtrace_handle_conn_data;
                break;

            case tx_data_in_event:
                handler = (ib_void_fn_t)modtrace_handle_tx;
                break;

            case tx_started_event:
            case tx_finished_event:
                handler = (ib_void_fn_t)modtrace_handle_tx_mem;
                break;

            case request_headers_event:
                handler = (ib_void_fn_t)modtrace_handle_req_headers;
                break;

            default:
                handler = (ib_void_fn_t)modtrace_event_callback;
        }
        rc = ib_hook_register(ib, (ib_state_event_type_t)event,
                              handler, (void*)eventp);
        if (rc != IB_OK) {
            ib_log_error(ib, 4, "Hook register for %d/%s returned %d",
                         rc, eventp->number, eventp->name);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Uninitialize the trace module.
 *
 * Called when module is unloaded.
 *
 * @param[in] ib IronBee object
 * @param[in] m Module object
 */
static ib_status_t modtrace_finish(ib_engine_t *ib, ib_module_t *m)
{
    IB_FTRACE_INIT(modtrace_finish);
    ib_log_debug(ib, 4, "Trace module unloaded.");
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Initialize a context for the trace module.
 *
 * Called when the context is available
 *
 * @param[in] ib IronBee object
 * @param[in] m Module object
 * @param[in] ctx Context object
 */
static ib_status_t modtrace_context_init(ib_engine_t *ib,
                                        ib_module_t *m,
                                        ib_context_t *ctx)
{
    IB_FTRACE_INIT(modtrace_context_init);
    ib_log_debug(ib, 4, "Trace module initializing context=%p.", ctx);
    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * @internal
 * Finish a context for the trace module.
 *
 * Called when the context is available
 *
 * @param[in] ib IronBee object
 * @param[in] m Module object
 * @param[in] ctx Context object
 */
static ib_status_t modtrace_context_finish(ib_engine_t *ib,
                                           ib_module_t *m,
                                           ib_context_t *ctx)
{
    IB_FTRACE_INIT(modtrace_context_finish);
    ib_log_debug(ib, 4, "Trace module finishing context=%p.", ctx);
    IB_FTRACE_RET_STATUS(IB_OK);
}

static IB_CFGMAP_INIT_STRUCTURE(modtrace_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".trace_mpools",
        IB_FTYPE_NULSTR,
        &modtrace_global_config,
        trace_mpools,
        "no"
    ),
    IB_CFGMAP_INIT_LAST
};

/*
 * Module initialization data, used by IB when it loads the module
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,      /* Default metadata */
    MODULE_NAME_STR,                /* Module name */
    IB_MODULE_CONFIG(&modtrace_global_config),/**< Global config data */
    modtrace_config_map,            /* Module config map */
    NULL,                           /* Module directive map */
    modtrace_init,                  /* Initialize function */
    modtrace_finish,                /* Finish function */
    modtrace_context_init,          /* Context init function */
    modtrace_context_finish         /* Context finish function */
);
