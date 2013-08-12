/*
 * Copyright (c) 2012, Andy Bierman, All Rights Reserved.
 * Copyright (c) 2012, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
#ifndef _H_yangapi
#define _H_yangapi
/*  FILE: yangapi.h
*********************************************************************
*                                                                   *
*                         P U R P O S E                             *
*                                                                   *
*********************************************************************

   YANG-API definitions

*********************************************************************
*                                                                   *
*                   C H A N G E         H I S T O R Y               *
*                                                                   *
*********************************************************************

date             init     comment
----------------------------------------------------------------------
11-apr-12    abb      Begun.
*/

/* used by the agent for the xmlTextReader interface */
#include <xmlreader.h>

#ifndef _H_dlq
#include "dlq.h"
#endif

#ifndef _H_ncxtypes
#include "ncxtypes.h"
#endif

#ifndef _H_op
#include "op.h"
#endif

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_xpath
#include "xpath.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*                                                                   *
*                         C O N S T A N T S                         *
*                                                                   *
*********************************************************************/

#define YANGAPI_RET_OK   200
#define YANGAPI_RET_CREATE_OK 201

#define YANGAPI_RET_BAD_METHOD  405

// 1 in -01 draft!
#define YANGAPI_DEF_DEPTH 2
    
/********************************************************************
*                                                                   *
*                             T Y P E S                             *
*                                                                   *
*********************************************************************/

        
typedef enum yangapi_method_t_ {
    YANGAPI_METHOD_NONE,
    YANGAPI_METHOD_OPTIONS,
    YANGAPI_METHOD_HEAD,
    YANGAPI_METHOD_GET,
    YANGAPI_METHOD_POST,
    YANGAPI_METHOD_PUT,
    YANGAPI_METHOD_PATCH,
    YANGAPI_METHOD_DELETE
} yangapi_method_t;


/* describes the type of node indicated by the RequestURI path */
typedef enum yangapi_launchpt_t_ {
    YANGAPI_LAUNCHPT_NONE,
    YANGAPI_LAUNCHPT_TOP,
    YANGAPI_LAUNCHPT_DATASTORE,
    YANGAPI_LAUNCHPT_DATA,
    YANGAPI_LAUNCHPT_NEW_DATA,
    YANGAPI_LAUNCHPT_MODULES,
    YANGAPI_LAUNCHPT_MODULE,
    YANGAPI_LAUNCHPT_OPERATIONS,
    YANGAPI_LAUNCHPT_OPERATION,
    YANGAPI_LAUNCHPT_VERSION
} yangapi_launchpt_t;

/* YANG-API Key value holder, temp Q of ordered key leaf values
 * used while parsing a path to store the keys until they are all collected
 */
typedef struct yangapi_keyval_t_ {
    dlq_hdr_t     qhdr;
    xmlChar      *value;
} yangapi_keyval_t;


/* YANG-API Query string parameter */
typedef struct yangapi_param_t_ {
    dlq_hdr_t     qhdr;   /* in case added to a queue */
    xmlChar      *name;
    xmlChar      *value;
} yangapi_param_t;


/* YANG-API Control Block */
typedef struct yangapi_cb_t_ {
    dlq_hdr_t     qhdr;      /* in case added to a queue */
    dlq_hdr_t     paramQ;       /* Q of yangapi_param_t */
    dlq_hdr_t     keyvalQ;      /* Q of yangapi_keyval_t */
    xmlChar      *accept;

    xmlChar      *request_method;
    xmlChar      *request_uri; 
    xpath_pcb_t  *request_xpath;
    xpath_result_t *request_xpath_result;
    uint32         request_xpath_result_count;

    val_value_t  *request_target;   // backptr inside root_tree
    obj_template_t *request_target_obj;  // backptr inside schema_tree
    yangapi_launchpt_t request_launchpt;

    val_value_t  *request_launch;   // backptr inside root tree
    val_value_t  *request_top_data;   // backptr inside root tree
    val_value_t  *request_terminal;   // backptr inside root tree

    boolean       query_config;

    uint32        query_depth;
    ncx_display_mode_t query_format;
    op_insertop_t  query_insert;
    xmlChar      *query_point;    // back-ptr into paramQ
    xmlChar      *query_select;   // back-ptr into paramQ
    xpath_pcb_t  *query_select_xpath;
    xpath_result_t *query_select_xpath_result;
    uint32        query_select_xpath_result_count;
    xmlChar      *query_start;    // back-ptr into request_uri
    time_t        query_tstamp;  // converted timestamp
    ncx_etag_t    query_etag;  // converted etag

    xmlChar      *query_test;   // back-ptr into paramQ
    xpath_pcb_t  *query_test_xpath;
    xpath_result_t *query_test_xpath_result;
    uint32        query_test_xpath_result_count;

    xmlChar      *content_type;  
    xmlChar      *content_length;
    xmlChar      *if_modified_since;
    xmlChar      *if_unmodified_since;
    xmlChar      *if_match;
    xmlChar      *if_none_match;

    int           content_len;
    yangapi_method_t method;
    xmlChar      *fragment;   /* back-ptr to fragment if any */
    uint32        pathlen;  /* len from request_uri of resource path */

    /* response info */
    uint32        return_code;
    op_editop_t   editop;
    val_value_t  *curnode;
    boolean       skip_read;
    boolean       empty_read;
} yangapi_cb_t;


/********************************************************************
*                                                                   *
*                        F U N C T I O N S                          *
*                                                                   *
*********************************************************************/


/********************************************************************
* FUNCTION yangapi_new_param
*
* Create a new YANGAPI paramater
*
* INPUTS:
*   parmname == parameter name
*   parmnamelen == parameter name string length
*   parmval == parameter value
*   parmvallen == parameter value string length
* RETURNS:
*   pointer to initialized param, or NULL if malloc error
*********************************************************************/
extern yangapi_param_t *
    yangapi_new_param (const xmlChar *parmname,
                       uint32 parmnamelen,
                       const xmlChar *parmval,
                       uint32 parmvallen);


/********************************************************************
* FUNCTION yangapi_free_param
*
* Free a YANG-API parameter
*
* INPUTS:
*   param == Yuma YANG-API parameter to free
* RETURNS:
*   none
*********************************************************************/
extern void
    yangapi_free_param (yangapi_param_t *param);


/********************************************************************
* FUNCTION yangapi_new_keyval
*
* Create a new YANGAPI keyval holder
*
* INPUTS:
*   keyval == key valuse string
* RETURNS:
*   pointer to initialized keyval, or NULL if malloc error
*********************************************************************/
extern yangapi_keyval_t *
    yangapi_new_keyval (const xmlChar *keyval);


/********************************************************************
* FUNCTION yangapi_free_keyval
*
* Free a YANGAPI keyval
*
* INPUTS:
*   param == Yuma REST-API keyval to free
* RETURNS:
*   none
*********************************************************************/
extern void
    yangapi_free_keyval (yangapi_keyval_t *keyval);


/********************************************************************
* FUNCTION yangapi_clean_keyvalQ
*
* Free all the YANGAPI keyval
*
* INPUTS:
*   rcb == control block to use
* RETURNS:
*   none
*********************************************************************/
extern void
    yangapi_clean_keyvalQ (yangapi_cb_t *rcb);


/********************************************************************
* FUNCTION yangapi_new_rcb
*
* Create a new YANG-API control block
*
* INPUTS:
*   none
* RETURNS:
*   pointer to initialized RCB, or NULL if malloc error
*********************************************************************/
extern yangapi_cb_t *
    yangapi_new_rcb (void);


/********************************************************************
* FUNCTION yangapi_free_rcb
*
* Free a YANGAPI control block
*
* INPUTS:
*   rcb == Yuma YANG-API control block to free
* RETURNS:
*   none
*********************************************************************/
extern void
    yangapi_free_rcb (yangapi_cb_t *rcb);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif            /* _H_yangapi */
