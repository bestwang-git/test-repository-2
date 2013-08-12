/*
 * Copyright (c) 2012-2013, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
#ifdef WITH_YANGAPI
/*  FILE: agt_yangapi.c

   Yuma REST API Message Handler for YANG-API protocol
 
   See draft-bierman-netconf-yang-api-00.txt for details.

   A YANG-API request URI has the following fields,
   following the format for RFC 3986 URIs:

 POST /<script-name>/<top-rc>/<sub-rc>.../<path>?<qstring>#<frag>

  ^         ^          ^          ^         ^       ^         ^
  |         |          |          |         |       |         |
method  entry pt.  top-resource sub-rc   data-path qstring fragment
 M         M           O          O         O         O       O

 M=mandatory, O=optional

Path Parser

  The path parser uses a data tree of the /yang-api in
  its 'running' state.

  Operations:

  1               N            N+1
  *---------------*------------*
  ^               ^            ^
  |               |            |
 root        rpc launch     rpc name
             ywx:rpcroot


  Existing Data:

     1               N        N+1       N+1+m
     *---------------*---------*---------*
     ^               ^         ^    m    ^
     |               |         |         |
    root        data launch  top-node  target
                 ncx:root

    m >= 0

  New Data: 

     1               N        N+1       N+1+m     N+m+2
     *---------------*---------*---------*----------*
     ^               ^         ^    m    ^          ^
     |               |         |         |          |
    root        data launch  top-node terminal    target
                 ncx:root  

    m >= 0

If the path reaches a 'root' launch point for rpc or data nodes,
the launch point will be saved (e.g.,datastore, transaction, operations).

For operations, the target is expected to be a child of the rpcroot,
representing the RPC operation to invoke.  The parameters (if any) will
be in the message body.

For data, the target is expected to be a valid YANG data node,
or creating a new node.  The top-level entry point (top-node)
will be saved.

If the path extends past the existing tree, the request_target_obj
will be set instead of request_target.  The final path components
can reference key leaf values (for GET operation).  If so, they will
be stored in the context->keyvalQ

The last node matched in the tree will be saved (terminal).

In all cases, the target resource (target) will be saved, and
set to the terminal node.

Reference on HTTP Headers:
http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
10apr12      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <memory.h>
#include <assert.h>

#include "procdefs.h"
#include "agt_acm.h"
#include "agt_cap.h"
#include "agt_cfg.h"
#include "agt_ncx.h"
#include "agt_rpc.h"
#include "agt_rpcerr.h"
#include "agt_ses.h"
#include "agt_sys.h"
#include "agt_util.h"
#include "agt_val.h"
#include "agt_val_parse.h"
#include "agt_xml.h"
#include "agt_yangapi.h"
#include "agt_yangapi_edit.h"
#include "agt_yangapi_reply.h"
#include "cap.h"
#include "cfg.h"
#include "dlq.h"
#include "json_wr.h"
#include "log.h"
#include "ncx.h"
#include "ncx_num.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "rpc.h"
#include "rpc_err.h"
#include "ses.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "xmlns.h"
#include "xml_msg.h"
#include "xml_util.h"
#include "xml_val.h"
#include "xml_wr.h"
#include "xpath.h"
#include "xpath_yang.h"
#include "xpath1.h"
#include "yangapi.h"
#include "yangconst.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#ifdef DEBUG
#define AGT_YANGAPI_DEBUG 1
#endif

/* used by parse_path to URL decode a key value inline
 * instead of mallocing the buffer
 */
#define TEMP_BUFF_SIZE  1024

#define record_error agt_yangapi_record_error

/********************************************************************
*                                                                   *
*                           T Y P E S                               *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                            *
*                                                                   *
*********************************************************************/
static boolean agt_yangapi_init_done = FALSE;

static agt_yangapi_context_t api_context;


/********************************************************************
* FUNCTION print_debug_line
*
* Print the message received debug line
*
* INPUTS:
*   scb == session control block to use
*   rcb == yangapi control block to use
*********************************************************************/
static void
    print_debug_line (ses_cb_t *scb,
                      yangapi_cb_t *rcb)
{
    if (LOGDEBUG) {
        xmlChar tstampbuff[TSTAMP_MIN_SIZE];
        tstamp_datetime(tstampbuff);
        log_debug("\nagt_yangapi: %s %s for %u=%s@%s [%s]", 
                  rcb->request_method,
                  rcb->request_uri,
                  scb->sid,
                  scb->username ? scb->username : NCX_EL_NONE,
                  scb->peeraddr ? scb->peeraddr : NCX_EL_NONE,
                  tstampbuff);
    }

} /* print_debug_line */


/********************************************************************
* FUNCTION record_obj_error
*
* Record an rpc-error for YANG-API response translation
* the error node is an object, not a value node
*
* INPUTS:
*   scb == session control block to use
*   msg == rpc_msg_t to use
*   res == error status code
*   errobj == object node in API schema tree or data schema tree
*     associated with the error
*   badval == error-info node to use
*********************************************************************/
static void
    record_obj_error (ses_cb_t *scb,
                      rpc_msg_t *msg,
                      status_t res,
                      obj_template_t *errobj,
                      const xmlChar *badval)
{
    const void *errinfo = NULL;
    ncx_node_t errinfo_type = NCX_NT_NONE;
    void *errpath = NULL;
    ncx_node_t errpath_type = NCX_NT_NONE;

    if (errobj && badval) {
        errpath = errobj;
        errpath_type = NCX_NT_OBJ;
        errinfo = badval;
        errinfo_type = NCX_NT_STRING;
    } else if (errobj) {
        errpath = errobj;
        errpath_type = NCX_NT_OBJ;
        errinfo = errobj;
        errinfo_type = NCX_NT_OBJ;
    } else if (badval) {
        errinfo = badval;
        errinfo_type = NCX_NT_STRING;
    }
    agt_record_error(scb, &msg->mhdr, NCX_LAYER_RPC, res, NULL, 
                     errinfo_type, errinfo, errpath_type, errpath);

}  /* record_obj_error */


/********************************************************************
 * FUNCTION get_select_param_result
 *
 * Process the XPath structs for the select parameter
 *
 * INPUTS:
 *   scb == session control block to use
 *   msg == message to record errors
 *   rcb == YANG-API control block to use
 *   valnode == valur root and context node to use
 *   retres == address of return status
 * OUTPUTS:
 *   *retres == return status
 * RETURNS:
 *   pointer to malloced xpath result (must be freed by called)
 *********************************************************************/
static xpath_result_t *
    get_select_param_result (ses_cb_t *scb,
                             rpc_msg_t *msg,
                             yangapi_cb_t *rcb,
                             val_value_t *valnode,
                             status_t *retres)
{
    if (rcb->query_select_xpath == NULL) {
        *retres = SET_ERROR(ERR_INTERNAL_VAL);
        return NULL;
    }

    boolean config_only = agt_yangapi_get_config_parm(rcb, valnode);

    status_t res = NO_ERR;

    /* get the instance(s) from the running config */
    xpath_result_t *result =
        xpath1_eval_expr(rcb->query_select_xpath, valnode, valnode,
                         FALSE,   /* logerrors */
                         config_only, &res);

    if (result == NULL || res != NO_ERR) {
        if (res == NO_ERR) {
            res = ERR_NCX_OPERATION_FAILED;
        }
        record_error(scb, msg, res, NULL, rcb->query_select);
        xpath_free_result(result);
        result = NULL;
    }

    if (res == NO_ERR) {
        /* check the result type */
        dlq_hdr_t *resnodeQ = xpath_get_resnodeQ(result);
        if (resnodeQ == NULL) {
            if (res == NO_ERR) {
                res = ERR_NCX_INVALID_VALUE;
            }
            record_error(scb, msg, res, NULL, rcb->query_select);
            xpath_free_result(result);
            result = NULL;            
        }
    }

    *retres = res;
    return result;

}  /* get_select_param_result */


/********************************************************************
 * FUNCTION get_test_param_result
 *
 * Process the XPath structs for the test parameter
 *
 * INPUTS:
 *   scb == session control block to use
 *   msg == message to record errors
 *   rcb == YANG-API control block to use
 *   valnode == value context node to use
 *   retres == address of return status
 * OUTPUTS:
 *   *retres == return status
 * RETURNS:
 *   pointer to malloced xpath result (must be freed by called)
 *********************************************************************/
static xpath_result_t *
    get_test_param_result (ses_cb_t *scb,
                           rpc_msg_t *msg,
                           yangapi_cb_t *rcb,
                           val_value_t *valnode,
                           status_t *retres)
{
    if (rcb->query_test_xpath == NULL) {
        *retres = SET_ERROR(ERR_INTERNAL_VAL);
        return NULL;
    }

    boolean config_only = FALSE;

    status_t res = NO_ERR;

    /* get the instance(s) from the running config */
    xpath_result_t *result =
        xpath1_eval_expr(rcb->query_test_xpath, valnode, 
                         cfg_get_root(NCX_CFGID_RUNNING),
                         FALSE,   /* logerrors */
                         config_only, &res);

    if (result == NULL || res != NO_ERR) {
        if (res == NO_ERR) {
            res = ERR_NCX_OPERATION_FAILED;
        }
        record_error(scb, msg, res, NULL, rcb->query_test);
        xpath_free_result(result);
        result = NULL;
    }

    *retres = res;
    return result;

}  /* get_test_param_result */


/********************************************************************
* FUNCTION setup_select_nodes
*
* Process all the context nodes found and create a new XPath result
* containing the output of the select operation
* Number of levels printed depends on depth parameter
*
* INPUTS:
*   scb == session to use
*   rcb = yangcpi control block to use
*   msg == message in progress
*   errdone == address of error done flag
*
* OUTPUTS:
*   *errdone = TRUE if error already recorded
*   rcb->query_select_xpath_result is set
*   rcb->query_select_xpath_result_count is set
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    setup_select_nodes (ses_cb_t *scb,
                        yangapi_cb_t *rcb,
                        rpc_msg_t *msg,
                        boolean *errdone)
{
    *errdone = FALSE;

    /* check id select parm given */
    if (rcb->query_select == NULL) {
        return NO_ERR;
    }

    /* check if any context nodes found */
    xpath_resnode_t *resnode =
        xpath_get_first_resnode(rcb->request_xpath_result);
    if (resnode == NULL) {
        return NO_ERR;
    }

    /* save up all the results in 1 total result */
    xpath_result_t *total_result = NULL;
    status_t res = NO_ERR;

    /* process each context node and gather the results */
    while (resnode && res == NO_ERR) {

        /* should have a value pointer in the context node */
        val_value_t *curval = xpath_get_resnode_valptr(resnode);
        if (curval == NULL) {
            res = ERR_NCX_OPERATION_FAILED;
            continue;
        }

        if (LOGDEBUG3) {
            log_debug3("\nagt_yangapi: process select (%s) for %s:%s",
                      rcb->query_select, val_get_mod_name(curval),
                      curval->name);
        }

        /* get the result for current context node */
        xpath_result_t *result =
            get_select_param_result(scb, msg, rcb, curval, &res);
        if (res != NO_ERR) {
            log_error("\nError: select (%s) for %s:%s failed (%s)",
                      rcb->query_select, val_get_mod_name(curval),
                      curval->name, get_error_string(res));
            *errdone = TRUE;
            continue;
        }

        if (total_result) {
            /* add this result to the totals */
            res = xpath_add_to_result(result, total_result);
            xpath_free_result(result);
            if (res != NO_ERR) {
                continue;
            }
        } else {
            /* save this first result as the totals */
            total_result = result;
        }

        resnode = xpath_get_next_resnode(resnode);
    }

    if (res == NO_ERR) {
        rcb->query_select_xpath_result = total_result;
        rcb->query_select_xpath_result_count =
            xpath_resnode_count(total_result);
    } else {
        xpath_free_result(total_result);
    }

    return res;

} /* setup_select_nodes */


/********************************************************************
* FUNCTION get_modules
*
* <get> operation handler for the /yang-api/modules container
*
* INPUTS:
*    see ncx/getcb.h getcb_fn_t for details
*
* RETURNS:
*    status
*********************************************************************/
static status_t 
    get_modules (ses_cb_t *scb,
                 getcb_mode_t cbmode,
                 const val_value_t *virval,
                 val_value_t  *dstval)
{
    (void)scb;

    if (cbmode != GETCB_GET_VALUE) {
        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    status_t res = NO_ERR;
    cap_list_t *caplist = agt_cap_get_caps();
    if (caplist) {
        cap_rec_t *caprec = cap_first_modcap(caplist);
        for (; caprec != NULL && res == NO_ERR; 
             caprec = cap_next_modcap(caprec)) {
            val_value_t *leafval = 
                agt_make_leaf(virval->obj, NCX_EL_MODULE, 
                              caprec->cap_uri, &res);
            if (leafval) {
                val_add_child(leafval, dstval);
            }
        }
    }
         
    return res;

} /* get_modules */


/********************************************************************
* FUNCTION get_operations
*
* <get> operation handler for the /yang-api/operations container
*
* INPUTS:
*    see ncx/getcb.h getcb_fn_t for details
*
* RETURNS:
*    status
*********************************************************************/
static status_t 
    get_operations (ses_cb_t *scb,
                    getcb_mode_t cbmode,
                    const val_value_t *virval,
                    val_value_t  *dstval)
{
    (void)scb;
    (void)virval;

    if (cbmode != GETCB_GET_VALUE) {
        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    ncx_backptr_t *backptr = agt_rpc_get_first_backptr();
    for (; backptr; backptr = agt_rpc_get_next_backptr(backptr)) {
        obj_template_t *obj =
            (obj_template_t *)ncx_get_backptr_node(backptr);

        val_value_t *leafval = 
            xml_val_new_flag(obj_get_name(obj), obj_get_nsid(obj));
        if (leafval) {
            leafval->dataclass = NCX_DC_CONFIG;
            val_add_child(leafval, dstval);
        } else {
            log_error("\nError: malloc failed for new operation leaf");
            return ERR_INTERNAL_MEM;
        }
    }
         
    return NO_ERR;

} /* get_operations */


/********************************************************************
* FUNCTION get_method
*
* Get the HTTP method enum from the string
* 
* INPUTS:
*   method == method name string
*********************************************************************/
static yangapi_method_t
    get_method (const xmlChar *method)
{
    /* get the HTTP method that was used */
    if (!xml_stricmp(method, (const xmlChar *)"OPTIONS")) {
        return YANGAPI_METHOD_OPTIONS;
    } else if (!xml_stricmp(method, (const xmlChar *)"HEAD")) {
        return YANGAPI_METHOD_HEAD;
    } else if (!xml_stricmp(method, (const xmlChar *)"GET")) {
        return YANGAPI_METHOD_GET;
    } else if (!xml_stricmp(method, (const xmlChar *)"POST")) {
        return YANGAPI_METHOD_POST;
    } else if (!xml_stricmp(method, (const xmlChar *)"PUT")) {
        return YANGAPI_METHOD_PUT;
    } else if (!xml_stricmp(method, (const xmlChar *)"PATCH")) {
        return YANGAPI_METHOD_PATCH;
    } else if (!xml_stricmp(method, (const xmlChar *)"DELETE")) {
        return YANGAPI_METHOD_DELETE;
    } else {
        return YANGAPI_METHOD_NONE;
    }
}   /* get_method */


/********************************************************************
* FUNCTION get_param
*
* Find a query parameter and return its value
* 
* INPUTS:
*   rcb == YANG-API request control block to use
*   name == param name to find
*   found = address of return found flag
* OUTPUTS:
*   *found == TRUE if parameter found
* RETURNS:
*  back pointer to param value if found, NULL if not found
*  The value is allowed to be NULL (empty string) so the
*  found flag is needed
*********************************************************************/
static xmlChar *
    get_param (yangapi_cb_t *rcb,
               const xmlChar *name,
               boolean *found)
{
    *found = FALSE;
    yangapi_param_t *param = (yangapi_param_t *)dlq_firstEntry(&rcb->paramQ);
    for (; param != NULL; param = (yangapi_param_t *)dlq_nextEntry(param)) {
        if (!xml_strcmp(name, param->name)) {
            *found = TRUE;
            return param->value;
        }
    }
    return NULL;

}  /* get_param */


/********************************************************************
* FUNCTION get_param_count
*
* Return the number of instances of the parameter
* 
* INPUTS:
*   rcb == YANG-API request control block to use
*   name == param name to find
*
* RETURNS:
*  count of param instances
*********************************************************************/
static uint32
    get_param_count (yangapi_cb_t *rcb,
                     const xmlChar *name)
{
    uint32 count = 0;
    yangapi_param_t *param = (yangapi_param_t *)
        dlq_firstEntry(&rcb->paramQ);
    for (; param != NULL; param = (yangapi_param_t *)dlq_nextEntry(param)) {
        if (!xml_strcmp(name, param->name)) {
            count++;
        }
    }
    return count;

}  /* get_param_count */


/********************************************************************
* FUNCTION param_supported
*
* Check if a parameter name is supported
*
* INPUTS:
*   name == param name to check
*
* RETURNS:
* Return TRUE if the parameter name is supported
*         FALSE if the parameter name is not known
*  
*********************************************************************/
static boolean
    param_supported (const xmlChar *name)
{

    if (!xml_strcmp(name, NCX_EL_CONFIG)) {
        return TRUE;
    }

    if (!xml_strcmp(name, NCX_EL_DEPTH)) {
        return TRUE;
    }

    if (!xml_strcmp(name, NCX_EL_FORMAT)) {
        return TRUE;
    }

    if (!xml_strcmp(name, NCX_EL_INSERT)) {
        return TRUE;
    }

    if (!xml_strcmp(name, NCX_EL_POINT)) {
        return TRUE;
    }

    if (!xml_strcmp(name, NCX_EL_SELECT)) {
        return TRUE;
    }

    if (!xml_strcmp(name, NCX_EL_TEST)) {
        return TRUE;
    }

    return FALSE;

}  /* param_supported */


/********************************************************************
* FUNCTION param_is_set
*
* Check if the specified parameter is set
* 
* INPUTS:
*   rcb == YANG-API request control block to use
*   name == param name to find
*
* RETURNS:
*  TRUE is set, FALSE if not or some error
*********************************************************************/
static boolean
    param_is_set (yangapi_cb_t *rcb,
                  const xmlChar *name)
{
    yangapi_param_t *param = (yangapi_param_t *)dlq_firstEntry(&rcb->paramQ);
    for (; param != NULL; param = (yangapi_param_t *)dlq_nextEntry(param)) {
        if (!xml_strcmp(name, param->name)) {
            return TRUE;
        }
    }
    return FALSE;

}  /* param_is_set */



/********************************************************************
* FUNCTION free_msg
*
* Free an rpc_msg_t
* 
* INPUTS:
*   msg == rpc_msg_t to delete
*********************************************************************/
static void
    free_msg (rpc_msg_t *msg)
{
    agt_cfg_free_transaction(msg->rpc_txcb);
    rpc_free_msg(msg);

}  /* free_msg */



/********************************************************************
* FUNCTION parse_fragment
*
* parse the request URI fragment
*
* INPUTS:
*   rcb == YANG-API control block to use
*   uri == request URI string to use
*   retlen == address of return byte count parsed
*
* OUTPUTS:
*   *retlen == byte count of uri parsed
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    parse_fragment (yangapi_cb_t *rcb,
                    xmlChar *uri,
                    uint32 *retlen)
{
    *retlen = 1;
    ++uri;

    /* at end of URI, so rest of string is the fragment */
    uint32 len = xml_strlen(uri);
    if (len) {
        rcb->fragment = uri;
    }
    *retlen = len + 1;
    return NO_ERR;

}  /* parse_fragment */


/********************************************************************
* FUNCTION parse_query_string
*
* parse the request URI query string
*
* INPUTS:
*   scb == session control block to use
*   msg == message to use to record errors
*   rcb == YANG-API control block to use
*   uri == request URI string to use
*   retlen == address of return byte count parsed
*
* OUTPUTS:
*   *retlen == byte count of uri parsed
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    parse_query_string (ses_cb_t *scb,
                        rpc_msg_t *msg,
                        yangapi_cb_t *rcb,
                        xmlChar *uri,
                        uint32 *retlen)
{
    *retlen = 1;
    rcb->query_start = uri;

    xmlChar *p = uri+1;
    xmlChar *q = p;
    yangapi_param_t *param = NULL;

    /* go through entire query string
     * expecting name=value[&name=value]* 
     * until fragment or end or URI
     * allow name only parameters, so '=' is optional
     */
    boolean done = FALSE;
    while (!done) {
        while (*q && *q != '=' && *q != '&' && *q != '#') {
            q++;
        }
        uint32 len = (uint32)(q - p);
        if (len == 0) {
            if (*q == '&') {
                p = ++q;
            } else {
                *retlen = (uint32)(q - uri);
                done = TRUE;
            }
            /* corner-case zero-length name component */
            continue;
        }

        if (*q == 0 || *q == '&' || *q == '#') {
            /* found only 1 string no name=value pair */
            param = yangapi_new_param(p, len, NULL, 0);
            if (param == NULL) {
                record_error(scb, msg, ERR_INTERNAL_MEM, NULL, NULL);
                return ERR_INTERNAL_MEM;
            }
        } else {
            /* found '=' so parse the value string */
            xmlChar *valname = p;
            p = ++q;
            while (*q && *q != '&' && *q != '#') {
                q++;
            }
            uint32 valuelen = (uint32)(q - p);
            if (valuelen) {
                /* found a value string */
                param = yangapi_new_param(valname, len, p, valuelen);
            } else {
                /* got name= with no value string */
                param = yangapi_new_param(valname, len, NULL, 0);
            }
            if (param == NULL) {
                record_error(scb, msg, ERR_INTERNAL_MEM, NULL, NULL);
                return ERR_INTERNAL_MEM;
            }
        }

        if (!param_supported(param->name)) {
            record_error(scb, msg, ERR_NCX_UNKNOWN_PARM, NULL, param->name);
            yangapi_free_param(param);
            return ERR_NCX_UNKNOWN_PARM;
        }
        dlq_enque(param, &rcb->paramQ);

        /* setup next loop or exit */
        if (*q == '&') {
            p = ++q;
        } else {
            *retlen = (uint32)(q - uri);
            done = TRUE;
        }
    }

    return NO_ERR;

}  /* parse_query_string */


/********************************************************************
* FUNCTION setup_query_string_defaults
*
* Set the default value for the query string parameters
*
* INPUTS:
*   rcb == YANG-API request control block to use
*
* OUTPUTS:
*   rcb->query_* fields will be filled in with defaults
*********************************************************************/
static void
    setup_query_string_defaults (yangapi_cb_t *rcb)
{
    rcb->query_config = TRUE;
    rcb->query_depth = YANGAPI_DEF_DEPTH;
    rcb->query_format = NCX_DISPLAY_MODE_JSON;
    rcb->query_insert = OP_INSOP_NONE;
    rcb->query_point = NULL;
    rcb->query_select = NULL;
    rcb->query_test = NULL;

}  /* setup_query_string_defaults */


/********************************************************************
* FUNCTION setup_select_param
*
* Setup the select parm and do initial validation
*
* INPUTS:
*   scb == session to use
*   rcb == yangapi control block to use
*   msg == response message in progress
*
* OUTPUTS:
*   rcb->query_select_xpath is initialized but not parsed
* RETURNS:
*   status
*********************************************************************/
static status_t
    setup_select_param (ses_cb_t *scb,
                       yangapi_cb_t *rcb,
                       rpc_msg_t *msg)
{
    if (rcb->query_select == NULL) {
        return NO_ERR;
    }

    status_t res = NO_ERR;
    rcb->query_select_xpath = xpath_new_pcb(rcb->query_select, NULL);
    if (rcb->query_select_xpath == NULL) {
        res = ERR_INTERNAL_MEM;
        record_error(scb, msg, res, NULL, NULL);
    }

    return res;

}  /* setup_select_param */


/********************************************************************
* FUNCTION setup_test_param
*
* Setup the test parm and do initial validation
*
* INPUTS:
*   scb == session to use
*   rcb == yangapi control block to use
*   msg == response message in progress
*
* OUTPUTS:
*   rcb->query_test_xpath is initialized but not parsed
* RETURNS:
*   status
*********************************************************************/
static status_t
    setup_test_param (ses_cb_t *scb,
                      yangapi_cb_t *rcb,
                      rpc_msg_t *msg)
{
    if (rcb->query_test == NULL) {
        return NO_ERR;
    }

    status_t res = NO_ERR;
    rcb->query_test_xpath = xpath_new_pcb(rcb->query_test, NULL);
    if (rcb->query_test_xpath == NULL) {
        res = ERR_INTERNAL_MEM;
        record_error(scb, msg, res, NULL, NULL);
    }

    return res;

}  /* setup_test_param */


/********************************************************************
* FUNCTION validate_query_string
*
* validate the parameters in the request URI query string
* Errors are recorded by this function
*
* INPUTS:
*   scb == session control block to use
*   msg == msg to use for recording errors
*   rcb == YANG-API request control block to use
* OUTPUTS:
*   rcb->query_* fields will be filled in
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    validate_query_string (ses_cb_t *scb,
                           rpc_msg_t *msg,
                           yangapi_cb_t *rcb)
{

    status_t res = NO_ERR;
    boolean found = FALSE;

    /* look for config param first; default is true */
    rcb->query_config = TRUE;
    xmlChar *parmval = NULL;
    uint32 count = get_param_count(rcb, NCX_EL_CONFIG);
    if (count > 1) {
        res = ERR_NCX_EXTRA_VAL_INST;
        record_error(scb, msg, res, NULL, NCX_EL_CONFIG);
        return res;
    } else if (count == 1) {
        parmval = get_param(rcb, NCX_EL_CONFIG, &found);
        if (ncx_is_false(parmval)) {
            rcb->query_config = FALSE;
        } else if (!ncx_is_true(parmval)) {
            res = ERR_NCX_INVALID_VALUE;
            record_error(scb, msg, res, NULL, parmval);
            return res;
        }
    }

    /* look for depth param; default is 2  (was 1) */
    rcb->query_depth = YANGAPI_DEF_DEPTH;
    count = get_param_count(rcb, NCX_EL_DEPTH);
    if (count > 1) {
        res = ERR_NCX_EXTRA_VAL_INST;
        record_error(scb, msg, res, NULL, NCX_EL_DEPTH);
        return res;
    } else if (count == 1) {
        parmval = get_param(rcb, NCX_EL_DEPTH, &found);
        if (!xml_strcmp(parmval, NCX_EL_UNBOUNDED)) {
            rcb->query_depth = 0;
        } else {
            ncx_num_t num;
            ncx_init_num(&num);
            res = ncx_decode_num(parmval, NCX_BT_UINT32, &num);
            if (res == NO_ERR) {
                if (num.u == 0) {
                    res = ERR_NCX_INVALID_VALUE;
                    record_error(scb, msg, res, NULL, parmval);
                    ncx_clean_num(NCX_BT_UINT32, &num);
                    return res;
                } else {
                    rcb->query_depth = num.u;
                }
            } else {
                record_error(scb, msg, res, NULL, parmval);
                ncx_clean_num(NCX_BT_UINT32, &num);
                return res;
            }
            ncx_clean_num(NCX_BT_UINT32, &num);
        }
    }

    /* look for format param; default is JSON */
    rcb->query_format = NCX_DISPLAY_MODE_JSON;
    count = get_param_count(rcb, NCX_EL_FORMAT);
    if (count > 1) {
        res = ERR_NCX_EXTRA_VAL_INST;
        record_error(scb, msg, res, NULL, NCX_EL_FORMAT);
        return res;
    } else if (count == 1) {
        parmval = get_param(rcb, NCX_EL_FORMAT, &found);
        if (!xml_strcmp(parmval, NCX_EL_XML)) {
            rcb->query_format = NCX_DISPLAY_MODE_XML;
        } else if (xml_strcmp(parmval, NCX_EL_JSON)) {
            res = ERR_NCX_INVALID_VALUE;
            record_error(scb, msg, res, NULL, parmval);
            return res;
        }
    }

    /* look for insert param; default is last */
    rcb->query_insert = OP_INSOP_LAST;
    count = get_param_count(rcb, NCX_EL_INSERT);
    if (count > 1) {
        res = ERR_NCX_EXTRA_VAL_INST;
        record_error(scb, msg, res, NULL, NCX_EL_INSERT);
        return res;
    } else if (count == 1) {
        parmval = get_param(rcb, NCX_EL_INSERT, &found);
        rcb->query_insert = op_insertop_id(parmval);
        if (rcb->query_insert == OP_INSOP_NONE) {
            res = ERR_NCX_INVALID_VALUE;
            record_error(scb, msg, res, NULL, parmval);
            return res;
        }
    }

    /* look for point param; no default */
    count = get_param_count(rcb, NCX_EL_POINT);
    if (count > 1) {
        res = ERR_NCX_EXTRA_VAL_INST;
        record_error(scb, msg, res, NULL, NCX_EL_POINT);
        return res;
    } else if (count == 1) {
        rcb->query_point = parmval = get_param(rcb, NCX_EL_POINT, &found);
        /* actual valid data node instance id checked later */
        if (rcb->query_point == NULL || *rcb->query_point == 0) {
            res = ERR_NCX_INVALID_VALUE;
            record_error(scb, msg, res, NULL, parmval);
            return res;
        }

        /* check if insert also set correctly */
        if (!(rcb->query_insert == OP_INSOP_BEFORE ||
              rcb->query_insert == OP_INSOP_AFTER)) {
            res = ERR_NCX_UNEXPECTED_INSERT_ATTRS;
            record_error(scb, msg, res, NULL, NCX_EL_POINT);
            return res;
        }
    } else if (rcb->query_insert == OP_INSOP_BEFORE ||
               rcb->query_insert == OP_INSOP_AFTER) {
        /* point is not present but the insert parm was set */
        res = ERR_NCX_MISSING_PARM;
        record_error(scb, msg, res, NULL, NCX_EL_INSERT);
            return res;
    }

    /* look for select param; no default */
    count = get_param_count(rcb, NCX_EL_SELECT);
    if (count > 1) {
        res = ERR_NCX_EXTRA_VAL_INST;
        record_error(scb, msg, res, NULL, NCX_EL_SELECT);
        return res;
    } else if (count == 1) {
        rcb->query_select = parmval = get_param(rcb, NCX_EL_SELECT, &found);

        /* malloc an XPath struct now */
        if (rcb->query_select && *rcb->query_select) {
            /* actual valid XPath path syntax checked later */
            res = setup_select_param(scb, rcb, msg);
        } else {
            res = ERR_NCX_INVALID_VALUE;
            record_error(scb, msg, res, NULL, parmval);
            return res;
        }
    }

    /* look for test param; no default */
    count = get_param_count(rcb, NCX_EL_TEST);
    if (count > 1) {
        res = ERR_NCX_EXTRA_VAL_INST;
        record_error(scb, msg, res, NULL, NCX_EL_TEST);
        return res;
    } else if (count == 1) {
        rcb->query_test = parmval = get_param(rcb, NCX_EL_TEST, &found);

        /* malloc an XPath struct now */
        if (rcb->query_test && *rcb->query_test) {
            /* actual valid XPath path syntax checked later */
            res = setup_test_param(scb, rcb, msg);
        } else {
            res = ERR_NCX_INVALID_VALUE;
            record_error(scb, msg, res, NULL, parmval);
            return res;
        }
    }

    return NO_ERR;

}  /* validate_query_string */


/********************************************************************
 * FUNCTION setup_launch_point
 *
 * parse the request URI path from top to the launch point
 * in the API scaffolding
 * fill in rcb->request_tree and set request_launch if needed
 * fill in request_target_obj if the target does not exist,
 * such as a new transaction
 *
 * INPUTS:
 *   rcb == YANG-API control block to use
 *   path == YANG-API path string to use
 *   pathlen == length of path string
 *   retlen == address of return bytes used
 *
 * OUTPUTS:
 *  *retlen number of bytes parsed in path
 *   rcb->request_launch set
 *   rcb->request_launchpt set
 *
 *********************************************************************/
static void
    setup_launch_point (yangapi_cb_t *rcb)
{
    if (rcb->request_xpath_result) {
        ;  /* this step has already been done */
    } else if (rcb->request_target) {
        /* an existing node was found as the target */
        if (rcb->request_launch) {
            if (rcb->request_top_data) {
                rcb->request_launchpt = YANGAPI_LAUNCHPT_DATA;
            } else {
                const xmlChar *name = rcb->request_target->name;
                if (!xml_strcmp(name, NCX_EL_DATASTORE)) {
                    rcb->request_launchpt = YANGAPI_LAUNCHPT_DATASTORE;
                } else if (!xml_strcmp(name, NCX_EL_OPERATIONS)) {
                    rcb->request_launchpt = YANGAPI_LAUNCHPT_OPERATIONS;
                } else {
                    SET_ERROR(ERR_INTERNAL_VAL);
                }
            }
        } else {
            /* expecting some node inside the /yang-api resource */
            const xmlChar *name = rcb->request_target->name;
            if (!xml_strcmp(name, NCX_EL_MODULES)) {
                rcb->request_launchpt = YANGAPI_LAUNCHPT_MODULES;
            } else if (!xml_strcmp(name, NCX_EL_OPERATIONS)) {
                rcb->request_launchpt = YANGAPI_LAUNCHPT_OPERATIONS;
            } else if (!xml_strcmp(name, NCX_EL_VERSION)) {
                rcb->request_launchpt = YANGAPI_LAUNCHPT_VERSION;
            } else if (!xml_strcmp(name, YANG_API_ROOT)) {
                rcb->request_launchpt = YANGAPI_LAUNCHPT_TOP;
            } else {
                SET_ERROR(ERR_INTERNAL_VAL);
            }
        }
    } else if (rcb->request_target_obj) {
        /* target node not found so figure out what operation
         * and launch point is being used from other data         */
        if (obj_is_rpc(rcb->request_target_obj)) {
            rcb->request_launchpt = YANGAPI_LAUNCHPT_OPERATION;
        } else if (rcb->request_top_data) {
            rcb->request_launchpt = YANGAPI_LAUNCHPT_DATA;
        } else if (rcb->request_terminal) {
            const xmlChar *tname = rcb->request_terminal->name;
            if (!xml_strcmp(tname, NCX_EL_OPERATIONS)) {
                rcb->request_launchpt = YANGAPI_LAUNCHPT_OPERATION;
            } else if (!xml_strcmp(tname, NCX_EL_MODULES)) {
                /* do not create a <data> wrapper for the
                 * /yang-api/modules/module corner-case
                 * just force the return value to be the
                 * parent /yang-api/modules instead
                 */
                rcb->request_launchpt = YANGAPI_LAUNCHPT_MODULES;
                rcb->request_target = rcb->request_terminal;
            } else if (!xml_strcmp(tname, NCX_EL_DATASTORE)) {
                rcb->request_launchpt = YANGAPI_LAUNCHPT_NEW_DATA;
            } else {
                SET_ERROR(ERR_INTERNAL_VAL);
            }
        }
    } else {
        // no launch point set means it is unresolved data
        rcb->request_launchpt = YANGAPI_LAUNCHPT_DATA;
    }

} /* setup_launch_point */


/********************************************************************
 * FUNCTION setup_context_nodes
 *
 * parse the request URI path part after /yang-api/datastore
 * get an XPath result holding a nodeset pointing to the
 * nodes in the running config that match the request URI path
 *
 * INPUTS:
 *   scb == session control block to use
 *   msg == message to record errors
 *   rcb == YANG-API control block to use
 *   path == YANG-API path string to use
 *
 * OUTPUTS:
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    setup_context_nodes (ses_cb_t *scb,
                         rpc_msg_t *msg,
                         yangapi_cb_t *rcb,
                         const xmlChar *path)
{
    if (*path == 0 || (*path == '/' && path[1] == 0)) {
        rcb->request_launchpt = YANGAPI_LAUNCHPT_DATASTORE;
        return NO_ERR;
    }

    agt_profile_t *profile = agt_get_profile();
    boolean wildcards = profile->agt_wildcards;
    if (!agt_yangapi_method_is_read(rcb->method)) {
        wildcards = FALSE;
    }

    /* convert the UrlPath string to an Xpath string */
    status_t res = NO_ERR;
    xmlChar *fromurl =
        xpath_convert_url_to_path(path,
                                  profile->agt_match_names,
                                  profile->agt_alt_names,
                                  wildcards,
                                  TRUE,       /* withkeys */
                                  &res);
    if (fromurl == NULL || res != NO_ERR) {
        if (res == ERR_NCX_DEF_NOT_FOUND) {
            res = ERR_NCX_RESOURCE_UNKNOWN;
        }
        record_error(scb, msg, res, NULL, path);
        m__free(fromurl);
        return res;
    }

    rcb->request_xpath = xpath_new_pcb(NULL, NULL);
    if (rcb->request_xpath == NULL) {
        res = ERR_INTERNAL_MEM;
        record_error(scb, msg, res, NULL, NULL);
        m__free(fromurl);
        return res;
    }

    rcb->request_xpath->exprstr = fromurl;  // pass off memory here

    val_value_t *rootval = cfg_get_root(NCX_CFGID_RUNNING);
    if (rootval == NULL) {
        res = ERR_NCX_OPERATION_FAILED;
        record_error(scb, msg, res, NULL, NULL);
        return res;
    }

    boolean config_only = FALSE;  // get_config_parm(rcb, NULL);

    /* get the instance(s) from the running config */
    xpath_result_t *result =
        xpath1_eval_expr(rcb->request_xpath, rootval, rootval,
                         FALSE,   /* logerrors */
                         config_only, &res);

    if (result == NULL || res != NO_ERR) {
        if (res == NO_ERR) {
            res = ERR_NCX_OPERATION_FAILED;
        }
        record_error(scb, msg, res, NULL, fromurl);

        xpath_free_result(result);
        return res;
    }

    /* check the result type */
    dlq_hdr_t *resnodeQ = xpath_get_resnodeQ(result);
    if (resnodeQ == NULL) {
        if (res == NO_ERR) {
            res = ERR_NCX_INVALID_VALUE;
        }
        record_error(scb, msg, res, NULL, fromurl);
        xpath_free_result(result);
        return res;
    }

    rcb->request_xpath_result = result;
    rcb->request_xpath_result_count = dlq_count(resnodeQ);

    xpath_resnode_t *resnode =
        xpath_get_first_resnode(rcb->request_xpath_result);

    if (resnode) {
        /* set request_target to first node */
        rcb->request_target = xpath_get_resnode_valptr(resnode);
        rcb->request_launchpt = YANGAPI_LAUNCHPT_DATA;
    } else {
        rcb->request_target = NULL;
        rcb->request_launchpt = YANGAPI_LAUNCHPT_NEW_DATA;
    }
    return NO_ERR;

}  /* setup_context_nodes */



/********************************************************************
 * FUNCTION prune_one_context_node
 *
 * Use the test XPath expression to filter out context nodes
 *
 * INPUTS:
 *   scb == session control block to use
 *   msg == message to record errors
 *   rcb == YANG-API control block to use
 *   curval == context node value to check
 *   prune == address of return prune statue
 *
 * OUTPUTS:
 *   *prune == TRUE if this node failed the XPath test 
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    prune_one_context_node (ses_cb_t *scb,
                            rpc_msg_t *msg,
                            yangapi_cb_t *rcb,
                            val_value_t *curval,
                            boolean *prune)
{
    *prune = FALSE;

    status_t res = NO_ERR;
    xpath_result_t *result =
        get_test_param_result(scb, msg, rcb, curval, &res);
    if (result == NULL) {
        return res;
    }

    boolean test = xpath_cvt_boolean(result);
    if (!test) {
        log_debug2("\nagt_yangapi: pruning context node %s:%s: "
                   "test expr=false",
                   val_get_mod_name(curval), curval->name);
        *prune = TRUE;
    }

    xpath_free_result(result);
    return NO_ERR;

}  /* prune_one_context_node */


/********************************************************************
 * FUNCTION prune_context_nodes
 *
 * Use the test XPath expression to filter out context nodes
 *
 * INPUTS:
 *   scb == session control block to use
 *   msg == message to record errors
 *   rcb == YANG-API control block to use
 *
 * OUTPUTS:
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    prune_context_nodes (ses_cb_t *scb,
                         rpc_msg_t *msg,
                         yangapi_cb_t *rcb)
{
    if (rcb->query_test_xpath == NULL) {
        return NO_ERR;
    }

    if (rcb->request_launchpt != YANGAPI_LAUNCHPT_DATA) {
        return NO_ERR;
    }

    boolean prune_it = FALSE;
    status_t res = NO_ERR;

    if (rcb->request_xpath_result == NULL) {
        /* just run the test on the request_target */
        res = prune_one_context_node(scb, msg, rcb, rcb->request_target,
                                     &prune_it);
        if (res != NO_ERR) {
            return res;
        }
        if (prune_it) {
            rcb->empty_read = TRUE;
        }
        return NO_ERR;
    }
        
    xpath_resnode_t *next_resnode = NULL;
    xpath_resnode_t *resnode =
        xpath_get_first_resnode(rcb->request_xpath_result);

    for (; resnode; resnode = next_resnode) {

        next_resnode = xpath_get_next_resnode(resnode);

        val_value_t *curval = xpath_get_resnode_valptr(resnode);

        prune_it = FALSE;
        res = prune_one_context_node(scb, msg, rcb, curval, &prune_it);
        if (res != NO_ERR) {
            return res;
        }
        if (prune_it) {
            xpath_discard_resnode(resnode);
        }
    }

    rcb->request_xpath_result_count =
        xpath_resnode_count(rcb->request_xpath_result);
    if (rcb->request_xpath_result_count == 0) {
        rcb->empty_read = TRUE;
    }

    return NO_ERR;

}  /* prune_context_nodes */


/********************************************************************
 * FUNCTION parse_path
 *
 * parse the request URI path
 * fill in rcb->request_tree and set request_launch if needed
 * fill in request_target_obj if the target does not exist,
 * such as a new transaction
 *
 * INPUTS:
 *   scb == session control block to use
 *   msg == message to record errors
 *   rcb == YANG-API control block to use
 *   context == context to use for parse root
 *   path == YANG-API path string to use
 *
 * OUTPUTS:
 *   rcb->request_launch set if the path uses a root or rpc-root
 *   rcb->request_top_data set if the path uses a root and includes
 *       at least 1 top-level YANG data node
 *   rcb->request_terminal set the the path leaves the data tree
 *      to specify a new node, which will be set in request_target_obj
 *   rcb->request_target set (back-ptr, do not free)
 *   rcb->request_target_obj set (back-ptr, do not free)

 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    parse_path (ses_cb_t *scb,
                rpc_msg_t *msg,
                yangapi_cb_t *rcb,
                agt_yangapi_context_t *context,
                const xmlChar *path)
{
    const xmlChar *str = path;
    status_t res = NO_ERR;

    /* handle starting forward slash special so loop can look
     * for the next forward slash to end the word-in-progress
     */
    if (*str != '/') {
        res = ERR_NCX_INVALID_VALUE;
        record_error(scb, msg, res, NULL, NULL);
        return res;
    }
    if (*++str == 0) {
        res = ERR_NCX_INVALID_VALUE;
        record_error(scb, msg, res, NULL, NULL);
        return res;
    }

    const xmlChar *errstr = path;
    obj_template_t *errobj = NULL;
    val_value_t *curmatch = NULL;
    agt_profile_t *profile = agt_get_profile();
    xmlChar *tempbuff = NULL;
    obj_key_t *objkey = NULL;

    uint32 level = 0;
    uint32 tempbufflen = 0;
    boolean done = FALSE;
    boolean expectnode = TRUE;

    while (!done && res == NO_ERR) {
        const xmlChar *end = str;

        while (*end != 0 && *end != '/') {
            end++;
        }

        level++;

        uint32 inlen = (uint32)(end - str);
        if (inlen == 0) {
            /* invalid URL got double // */
            res = ERR_NCX_INVALID_VALUE;
            continue;
        }

        xmlChar *usebuff = NULL;
        xmlChar namebuff[TEMP_BUFF_SIZE];
        if (inlen >= TEMP_BUFF_SIZE) {
            if (tempbuff && tempbufflen <= inlen) {
                m__free(tempbuff);
                tempbuff = NULL;
                tempbufflen = 0;
            }
            if (tempbuff == NULL) {
                tempbuff = m__getMem(inlen + 1);
                if (tempbuff == NULL) {
                    res = ERR_INTERNAL_MEM;
                    continue;
                }
                tempbufflen = inlen + 1;
            }
            usebuff = tempbuff;
        } else {
            usebuff = namebuff;
        }
            
        /* get the identifier into the name buffer first */
        uint32 outlen = 0;
        res = ncx_decode_url_string(str, inlen, usebuff, &outlen);
        if (res != NO_ERR) {
            continue;
        }

        if (expectnode) {
            xmlChar *tempstr = usebuff;
            xmlChar *colon = NULL;
            uint32 coloncnt = 0;

            while (*tempstr != 0) {
                if (*tempstr == ':') {
                    colon = tempstr;
                    coloncnt++;
                }
                tempstr++;
            }

            if (coloncnt > 1) {
                /* invalid URL got double :: in identifier string */
                res = ERR_NCX_INVALID_VALUE;
                continue;
            }

            /* strings from the path URL */
            const xmlChar *modname = NULL;
            const xmlChar *name = usebuff;

            /* the [modname:]nodename is supposed to match node */
            if (coloncnt) {
                modname = usebuff;
                name = colon + 1;
                *colon = 0;
            }

            /* strings from the schema/data tree to match */
            const xmlChar *curmodname = NULL;
            const xmlChar *curname = NULL;

            if (curmatch == NULL) {
                /* need to match the root */
                curmatch = context->root;

                /* check module name only if present */
                if (modname) {
                    curmodname = val_get_mod_name(curmatch);
                    if (xml_strcmp(curmodname, modname)) {
                        res = ERR_NCX_UNKNOWN_MODULE;
                        continue;
                    }
                }

                /* check node name */
                curname = curmatch->name;
                if (xml_strcmp(curname, curmatch->name)) {
                    res = ERR_NCX_RESOURCE_UNKNOWN;
                    continue;
                }

                /* node is a match so continue on if res == NO_ERR */
                rcb->request_target = curmatch;
            } else {
                /* curmatch is already set so find a child node */
                obj_template_t *targobj = NULL;
                val_value_t *targval = NULL;  
                boolean isrpc = FALSE;
                boolean istop = FALSE;

                if (obj_is_root(curmatch->obj)) {
                    /* look for a top-node data object */
                    targobj = ncx_match_any_object_ex(modname, name, TRUE,
                                                      profile->agt_match_names,
                                                      profile->agt_alt_names,
                                                      &res);
                    istop = TRUE;
                } else if (obj_is_rpc_root(curmatch->obj)) {
                    /* look for a top-node RPC operation */
                    targobj = ncx_match_any_object_ex(modname, name, FALSE,
                                                      profile->agt_match_names,
                                                      profile->agt_alt_names,
                                                      &res);
                    if (targobj) {
                        if (obj_is_rpc(targobj)) {
                            isrpc = TRUE;
                        } else {
                            targobj = NULL;
                            res = ERR_NCX_INVALID_VALUE;
                        }
                    }
                } else if (obj_has_children(curmatch->obj)) {
                    /* look for an accessible child node
                     * first try to find a child object node
                     * in order to utilize match-names and alt-names
                     */
                    targobj = obj_find_child_ex(curmatch->obj, modname, name,
                                                profile->agt_match_names,
                                                profile->agt_alt_names,
                                                TRUE,  /* dataonly */
                                                &res);
                } else {
                    /* this is a leaf, leaflist, or anyxml */
                    res = ERR_NCX_DEF_NOT_FOUND;
                    errstr = name;
                }

                if (res != NO_ERR) {
                    continue;
                }

                errobj = targobj;

                if (targobj && !isrpc) {
                    if (obj_is_root(curmatch->obj)) {
                        /* find a top-level data node in the running config */
                        val_value_t *running = cfg_get_root(NCX_CFGID_RUNNING);
                        if (running == NULL) {
                            res = ERR_NCX_OPERATION_FAILED;
                            continue;
                        }

                        /* find a child of the database root */
                        targval = val_find_child(running, 
                                                 obj_get_mod_name(targobj), 
                                                 obj_get_name(targobj));
                    } else {
                        /* find a child of the current data node */
                        targval = val_find_child(curmatch, 
                                                 obj_get_mod_name(targobj), 
                                                 obj_get_name(targobj));
                    }

                    if (obj_is_list(targobj)) {
                        objkey = obj_first_key(targobj);
                        if (objkey) {
                            expectnode = FALSE;
                        } // else list has no keys defined 
                    } // else only looking for keys for existing nodes
                    if (targval && istop) {
                        rcb->request_top_data = targval;
                    }
                }

                if (targval) {
                    /* check if this is the /yang-api/datastore node
                     * and if so handle the rest of the path string
                     * with existing code for yangcli; need to convert
                     * the string to XPath, then get a result node-set
                     * with all the matching nodes; these are the
                     * target nodes and also the context nodes for the
                     * select parameter in GET requests
                     */
                    if (level == 2 && 
                        !xml_strcmp(targval->name, NCX_EL_DATASTORE)) {
                        rcb->request_target = targval;
                        rcb->request_launch = targval;
                        res = setup_context_nodes(scb, msg, rcb, end);
                        if (res == NO_ERR &&
                            rcb->content_len &&
                            rcb->request_launchpt==YANGAPI_LAUNCHPT_NEW_DATA) {
                            ;  // keep going
                        } else {
                            return res;
                        }
                    }

                    /* update the current target pointers */
                    curmatch = targval;
                    rcb->request_target = targval;
                    if (obj_is_root(targval->obj) ||
                        obj_is_rpc_root(targval->obj)) {
                        rcb->request_launch = targval;
                    }
                } else if (targobj) {
                    /* curmatch will no longer be updated!
                     * not expecting any nodes past the new node
                     * identified by targobj
                     */
                    curmatch = NULL;
                    rcb->request_target = NULL;
                    rcb->request_terminal = curmatch;
                    rcb->request_target_obj = targobj;
                } else {
                    /* should not happen */
                    res = ERR_NCX_OPERATION_FAILED;
                }
            }  // end else curmatch already set
        } else {

            /* expecting a key value for a list node */
            yangapi_keyval_t *keyval = yangapi_new_keyval(usebuff);
            if (keyval == NULL) {
                res = ERR_INTERNAL_MEM;
            } else {
                dlq_enque(keyval, &rcb->keyvalQ);
                objkey = obj_next_key(objkey);
                if (objkey == NULL) {
                    expectnode = TRUE;
                    val_value_t *newmatch = NULL;
                    if (curmatch) {
                        /* find the list entry to match these keys */
                        newmatch =
                            val_find_list_from_keyvalQ(curmatch->parent,
                                                       curmatch,
                                                       &rcb->keyvalQ,
                                                       &res);
                    }
                    if (newmatch) {
                        /* possibly change the list instance from
                         * first to the specified entry; update ptrs
                         */
                        curmatch = newmatch;
                        rcb->request_target = newmatch;
                        if (rcb->request_top_data &&
                            rcb->request_top_data->obj == newmatch->obj) {
                            rcb->request_top_data = newmatch;
                        }
                    } else if (rcb->method == YANGAPI_METHOD_POST ||
                               rcb->method == YANGAPI_METHOD_PUT) {
                        curmatch = NULL;

                        /* OK that the instance was not found */
                        if (rcb->request_target) {
                            if (rcb->request_target->parent) {
                                rcb->request_terminal =
                                    rcb->request_target->parent;
                            }
                            rcb->request_target_obj = rcb->request_target->obj;
                            rcb->request_target = NULL;
                        }
                    } else {
                        /* the specified complete list index did not
                         * produce any matches    */
                        res = ERR_NCX_RESOURCE_INSTANCE_UNKNOWN;
                        curmatch = NULL;
                    }

                    /* cleanup the keyvalQ */
                    yangapi_clean_keyvalQ(rcb);
                }
            }
        }  // else expecting a key value

        /* stopped on either EOS or '/' char */
        if (res == NO_ERR) {
            if (*end == '/') {
                if (end[1] == '\0') {
                    /* got a trailing forward slash, so end here */
                    done = TRUE;
                } else if (curmatch || !expectnode) {
                    /* keep going */
                    str = end+1;
                } else {
                    /* reached a terminal so not expecting any more path */
                    res = ERR_NCX_NOT_FOUND;
                }
            } else {
                /* reached end of path string */
                done = TRUE;
                if (!expectnode) {
                    /* stopped at end waiting for 1 or more keys to a list */
                    curmatch = NULL;
                    rcb->request_target = NULL;
                }
            }
        }
    }  // while !done

    /* record the error using the curmatch and errobj pointers */
    if (res != NO_ERR) {
        if (curmatch == NULL) {
            curmatch = context->root;
        }
        if (res == ERR_NCX_DEF_NOT_FOUND) {
            res = ERR_NCX_RESOURCE_UNKNOWN;
        }
        if (errstr == NULL) {
            errstr = curmatch->name;
        }
        if (errobj) {
            record_obj_error(scb, msg, res, errobj, errstr);
        } else {
            record_error(scb, msg, res, curmatch, errstr);
        }
    }

    /* cleanup malloced memory */
    if (res != NO_ERR) {
        yangapi_clean_keyvalQ(rcb);
    }    
    m__free(tempbuff);

    return res;

}  /* parse_path */


/********************************************************************
* FUNCTION parse_request_uri
*
* parse the request URI into its components in the RCB
*
* Errors are recorded by this function.
* Do not call record_error again if error returned
*
* /yuma-api[/]  -> request_uri_top = true
*
* /yuma-api/<top-resource>[/] -> request_uri_root = true
*
* /yuma-api/<top-resource>/<path>[?<query-string>]
*
* INPUTS:
*   scb == session control block to use
*   rcb == yangapi control block to use
*   msg == msg to use for storing error records
*   context == YANG-API context to use
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    parse_request_uri (ses_cb_t *scb,
                       yangapi_cb_t *rcb,
                       rpc_msg_t *msg,
                       agt_yangapi_context_t *context)
{
    xmlChar *p = rcb->request_uri;
    uint32 retlen = 0, pathlen = 0;
    status_t res = NO_ERR;

    /* check start of URI -- expecting /API-program-name */
    if (*p != '/') {
        res = ERR_NCX_INVALID_VALUE;
        record_error(scb, msg, res, NULL, NULL);
        return res;
    }

    /* find end of path portion */
    ++p;
    while (*p && *p != '#' && *p != '?') {
        p++;
    }

    rcb->pathlen = pathlen = (uint32)(p - rcb->request_uri);

    /* check just script name and fragment and/or params */
    if (*p == '?') {
        res = parse_query_string(scb, msg, rcb, p, &retlen);
        if (res != NO_ERR) {
            /* error already recorded */
            return res;
        }
        res = validate_query_string(scb, msg, rcb);
        if (res != NO_ERR) {
            /* error already recorded */
            return res;
        }

        p += retlen;
        if (*p == '#') {
            retlen = 0;
            res = parse_fragment(rcb, p, &retlen);
        }
    } else if (*p == '#') {
        res = parse_fragment(rcb, p, &retlen);
    }

    if (res != NO_ERR) {
        record_error(scb, msg, res, NULL, NULL);
        return res;
    }

    xmlChar savechar = rcb->request_uri[pathlen];
    rcb->request_uri[pathlen] = 0;
    res = parse_path(scb, msg, rcb, context, rcb->request_uri);
    rcb->request_uri[pathlen] = savechar;

    /* error already recorded by parse_path if any */

#ifdef AGT_YANGAPI_DEBUG
    if (LOGDEBUG2) {
        if (rcb->request_uri) {
            log_debug2("\nagt_yangapi: Got request_uri '%s'",
                       rcb->request_uri);
        }
        if (rcb->request_target) {
            log_debug2("\nagt_yangapi: Got request_target '%s:%s'",
                       val_get_mod_name(rcb->request_target),
                       rcb->request_target->name);
        } else if (rcb->request_target_obj) {
            log_debug2("\nagt_yangapi: Got request_target_obj '%s:%s'",
                       obj_get_mod_name(rcb->request_target_obj),
                       obj_get_name(rcb->request_target_obj));
        } else {
            log_debug2("\nagt_yangapi: Did not get a target value or object");
        }
        if (rcb->request_launch) {
            log_debug2("\nagt_yangapi: Got request_launch value '%s:%s'",
                       val_get_mod_name(rcb->request_launch),
                       rcb->request_launch->name);
        }
        if (rcb->request_top_data) {
            log_debug2("\nagt_yangapi: Got request_top_data value '%s:%s'",
                       val_get_mod_name(rcb->request_top_data),
                       rcb->request_top_data->name);
        }
        if (rcb->request_terminal) {
            log_debug2("\nagt_yangapi: Got request_terminal value '%s:%s'",
                       val_get_mod_name(rcb->request_terminal),
                       rcb->request_terminal->name);
        }

        boolean found = FALSE;
        const xmlChar *parmval = get_param(rcb, NCX_EL_CONFIG, &found);
        if (found) {
            log_debug2("\nagt_yangapi: '%s' param set to '%s'",
                       NCX_EL_CONFIG, parmval ? parmval : EMPTY_STRING);
        }

        parmval = get_param(rcb, NCX_EL_DEPTH, &found);
        if (found) {
            log_debug2("\nagt_yangapi: '%s' param set to '%s'",
                       NCX_EL_DEPTH, parmval ? parmval : EMPTY_STRING);
        }

        parmval = get_param(rcb, NCX_EL_FORMAT, &found);
        if (found) {
            log_debug2("\nagt_yangapi: '%s' param set to '%s'",
                       NCX_EL_FORMAT, parmval ? parmval : EMPTY_STRING);
        }

        parmval = get_param(rcb, NCX_EL_INSERT, &found);
        if (found) {
            log_debug2("\nagt_yangapi: '%s' param set to '%s'",
                       NCX_EL_INSERT, parmval ? parmval : EMPTY_STRING);
        }

        parmval = get_param(rcb, NCX_EL_POINT, &found);
        if (found) {
            log_debug2("\nagt_yangapi: '%s' param set to '%s'",
                       NCX_EL_POINT, parmval ? parmval : EMPTY_STRING);
        }

        parmval = get_param(rcb, NCX_EL_SELECT, &found);
        if (found) {
            log_debug2("\nagt_yangapi: '%s' param set to '%s'",
                       NCX_EL_SELECT, parmval ? parmval : EMPTY_STRING);
        }
    }
#endif

    return res;

}  /* parse_request_uri */



/********************************************************************
* FUNCTION check_method_param_error
*
* validate that the URI parameters given are supported for the method
*
* Errors are recorded by this function.
* Do not call record_error again if error returned
*
* INPUTS:
*   scb == session control block to use
*   rcb == yangapi control block to use
*   msg == msg to use for storing error records
* OUTPUTS:
*   msg erros set as needed
* RETURNS:
*   status
*********************************************************************/
static status_t
    check_method_param_error (ses_cb_t *scb,
                              yangapi_cb_t *rcb,
                              rpc_msg_t *msg,
                              const xmlChar *parmname)
{
    status_t res = NO_ERR;
    boolean parm_set = param_is_set(rcb, parmname);
    if (parm_set) {
        res = ERR_NCX_QPARAM_NOT_ALLOWED;
        record_error(scb, msg, res, NULL, parmname);
    }
    return res;

}  /* check_method_param_error */


/********************************************************************
* FUNCTION validate_method_params
*
* validate that the operation is supported for the URI parameters given.
*
* Errors are recorded by this function.
* Do not call record_error again if error returned
*
* INPUTS:
*   scb == session control block to use
*   rcb == yangapi control block to use
*   msg == msg to use for storing error records
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    validate_method_params (ses_cb_t *scb,
                            yangapi_cb_t *rcb,
                            rpc_msg_t *msg)
{
    status_t res = NO_ERR;
    status_t retres = NO_ERR;

    /* check the individual method against the launchpt
     * and other data from the request
     */
    switch (rcb->method) {
    case YANGAPI_METHOD_NONE:
        retres = SET_ERROR(ERR_NCX_INVALID_VALUE);
        break;
    case YANGAPI_METHOD_OPTIONS:
        /* no parameter are allowed; check each one */
        res = check_method_param_error(scb, rcb, msg, NCX_EL_CONFIG);
        if (res != NO_ERR) {
            retres = res;
        }
        res = check_method_param_error(scb, rcb, msg, NCX_EL_DEPTH);
        if (res != NO_ERR) {
            retres = res;
        }
        res = check_method_param_error(scb, rcb, msg, NCX_EL_FORMAT);
        if (res != NO_ERR) {
            retres = res;
        }
        res = check_method_param_error(scb, rcb, msg, NCX_EL_INSERT);
        if (res != NO_ERR) {
            retres = res;
        }
        res = check_method_param_error(scb, rcb, msg, NCX_EL_POINT);
        if (res != NO_ERR) {
            retres = res;
        }
        res = check_method_param_error(scb, rcb, msg, NCX_EL_SELECT);
        if (res != NO_ERR) {
            retres = res;
        }
        res = check_method_param_error(scb, rcb, msg, NCX_EL_TEST);
        if (res != NO_ERR) {
            retres = res;
        }
        break;
    case YANGAPI_METHOD_HEAD:
    case YANGAPI_METHOD_GET:
        /* read-only parameters are allowed; check each one */
        res = check_method_param_error(scb, rcb, msg, NCX_EL_INSERT);
        if (res != NO_ERR) {
            retres = res;
        }
        res = check_method_param_error(scb, rcb, msg, NCX_EL_POINT);
        if (res != NO_ERR) {
            retres = res;
        }
        break;
    case YANGAPI_METHOD_POST:
    case YANGAPI_METHOD_PUT:
    case YANGAPI_METHOD_PATCH:
    case YANGAPI_METHOD_DELETE:
        if (rcb->request_launchpt == YANGAPI_LAUNCHPT_OPERATION) {
            /* no parameter except format allowed; check each one
             * this case will only be true for POST; already checked */
            res = check_method_param_error(scb, rcb, msg, NCX_EL_CONFIG);
            if (res != NO_ERR) {
                retres = res;
            }
            res = check_method_param_error(scb, rcb, msg, NCX_EL_DEPTH);
            if (res != NO_ERR) {
                retres = res;
            }
            res = check_method_param_error(scb, rcb, msg, NCX_EL_INSERT);
            if (res != NO_ERR) {
                retres = res;
            }
            res = check_method_param_error(scb, rcb, msg, NCX_EL_POINT);
            if (res != NO_ERR) {
                retres = res;
            }
            res = check_method_param_error(scb, rcb, msg, NCX_EL_SELECT);
            if (res != NO_ERR) {
                retres = res;
            }
            res = check_method_param_error(scb, rcb, msg, NCX_EL_TEST);
            if (res != NO_ERR) {
                retres = res;
            }
        } else {
            /* check edit parameters */
            res = check_method_param_error(scb, rcb, msg, NCX_EL_CONFIG);
            if (res != NO_ERR) {
                retres = res;
            }
            res = check_method_param_error(scb, rcb, msg, NCX_EL_DEPTH);
            if (res != NO_ERR) {
                retres = res;
            }
            res = check_method_param_error(scb, rcb, msg, NCX_EL_SELECT);
            if (res != NO_ERR) {
                retres = res;
            }
            res = check_method_param_error(scb, rcb, msg, NCX_EL_TEST);
            if (res != NO_ERR) {
                retres = res;
            }
        }
        break;
    default:
        retres = SET_ERROR(ERR_INTERNAL_VAL);
    }

    return retres;

}  /* validate_method_params */


/********************************************************************
* FUNCTION validate_method_headers
*
* validate that the operation is supported for the headers that
* were included
*
* Errors are recorded by this function.
* Do not call record_error again if error returned
*
* INPUTS:
*   scb == session control block to use
*   rcb == yangapi control block to use
*   msg == msg to use for storing error records
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    validate_method_headers (ses_cb_t *scb,
                             yangapi_cb_t *rcb,
                             rpc_msg_t *msg)
{
    status_t res = NO_ERR;
    const xmlChar *badval = NULL;

    /* check the individual method against the launchpt
     * and other data from the request
     */
    switch (rcb->method) {
    case YANGAPI_METHOD_NONE:
        res = SET_ERROR(ERR_NCX_INVALID_VALUE);
        break;
    case YANGAPI_METHOD_OPTIONS:
        break;
    case YANGAPI_METHOD_HEAD:
    case YANGAPI_METHOD_GET:
        /* read-only parameters are allowed; check each one */
        break;
    case YANGAPI_METHOD_POST:
    case YANGAPI_METHOD_PUT:
    case YANGAPI_METHOD_PATCH:
    case YANGAPI_METHOD_DELETE:
        if (rcb->request_launchpt == YANGAPI_LAUNCHPT_OPERATION) {
        } else {
            /* check edit parameters */
        }
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (res == NO_ERR &&
        rcb->if_modified_since && rcb->if_unmodified_since) {
        res = ERR_NCX_HEADER_NOT_ALLOWED;
        badval = rcb->if_unmodified_since;
    }

    if (res == NO_ERR && rcb->if_match && rcb->if_none_match) {
        res = ERR_NCX_HEADER_NOT_ALLOWED;
        badval = rcb->if_none_match;
    }
    
    if (res == NO_ERR) {
        const xmlChar *parm = NULL;
        if (rcb->if_modified_since) {
            parm = rcb->if_modified_since;
        } else if (rcb->if_unmodified_since) {
            parm = rcb->if_unmodified_since;
        }
        if (parm) {
            res = tstamp_htmltime2time(parm, &rcb->query_tstamp);
            if (res != NO_ERR) {
                badval = parm;
            }
        }
    }

    if (res != NO_ERR) {
        record_error(scb, msg, res, NULL, badval);
    }

    return res;

}  /* validate_method_headers */


/********************************************************************
* FUNCTION validate_method
*
* validate that the method is supported for the URI given.
* validate that the query parameters given are allowed for
* this operation.
*
* Errors are recorded by this function.
* Do not call record_error again if error returned
*
* INPUTS:
*   scb == session control block to use
*   rcb == yangapi control block to use
*   msg == msg to use for storing error records
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    validate_method (ses_cb_t *scb,
                     yangapi_cb_t *rcb,
                     rpc_msg_t *msg)
{
    val_value_t *errnode = rcb->request_target;
    obj_template_t *errobj = rcb->request_target_obj;
    const xmlChar *badval = NULL;
    status_t res = NO_ERR;

    /* check if write non-read method and the target is not
     * something that supports any non-read methods
     */
    if (agt_yangapi_method_is_read(rcb->method)) {
        switch (rcb->request_launchpt) {
        case YANGAPI_LAUNCHPT_NEW_DATA:
            res = ERR_NCX_RESOURCE_INSTANCE_UNKNOWN;
            break;
        case YANGAPI_LAUNCHPT_OPERATION:
            if (rcb->method != YANGAPI_METHOD_OPTIONS) {
                res = ERR_NCX_METHOD_NOT_ALLOWED;
            }
            break;
        default:
            ;
        }
    } else {
        switch (rcb->request_launchpt) {
        case YANGAPI_LAUNCHPT_DATA:
        case YANGAPI_LAUNCHPT_NEW_DATA:
            /* TBD: allow wildcard write mechanism */
            if (rcb->request_xpath_result_count > 1) {
                res = ERR_NCX_MULTIPLE_MATCHES;
            }
            break;
        case YANGAPI_LAUNCHPT_NONE:
        case YANGAPI_LAUNCHPT_OPERATION:
            break;
        case YANGAPI_LAUNCHPT_TOP:
        case YANGAPI_LAUNCHPT_MODULES:
        case YANGAPI_LAUNCHPT_MODULE:
        case YANGAPI_LAUNCHPT_VERSION:
        case YANGAPI_LAUNCHPT_OPERATIONS:
        case YANGAPI_LAUNCHPT_DATASTORE:
            res = ERR_NCX_METHOD_NOT_ALLOWED;
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
    }

    if (res == NO_ERR) {
        /* check the individual method against the launchpt
         * and other data from the request
         */
        switch (rcb->method) {
        case YANGAPI_METHOD_NONE:
            res = ERR_NCX_INVALID_VALUE;
            break;
        case YANGAPI_METHOD_OPTIONS:
            break;
        case YANGAPI_METHOD_HEAD:
        case YANGAPI_METHOD_GET:
            break;
        case YANGAPI_METHOD_POST:
            switch (rcb->request_launchpt) {
            case YANGAPI_LAUNCHPT_OPERATION:
                break;  // OK
            case YANGAPI_LAUNCHPT_DATA:
                /* the target object should be set if the schema node
                 * exists but no instances are in the running config
                 */
                if (rcb->request_target_obj == NULL) {
                    if (rcb->request_target) {
                        if (!val_set_by_default(rcb->request_target)) {
                            res = ERR_NCX_DATA_EXISTS;
                            errobj = rcb->request_target->obj;
                            rcb->return_code = 400;
                        }
                    }
                }
                break;
            default:
                ;
            }
            break;
        case YANGAPI_METHOD_PUT:
        case YANGAPI_METHOD_PATCH:
            if (rcb->request_launchpt == YANGAPI_LAUNCHPT_OPERATION) {
                res = ERR_NCX_METHOD_NOT_ALLOWED;
            }
            break;
        case YANGAPI_METHOD_DELETE:
            if (rcb->request_launchpt == YANGAPI_LAUNCHPT_OPERATION) {
                res = ERR_NCX_METHOD_NOT_ALLOWED;
            } else if (rcb->request_target == NULL) {
                if (rcb->request_target_obj) {
                    res = ERR_NCX_DATA_MISSING;
                    rcb->return_code = 400;
                }
            }
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
    }

    if (res != NO_ERR) {
        if (errobj) {
            record_obj_error(scb, msg, res, errobj, badval);
        } else {
            record_error(scb, msg, res, errnode, badval);
        }
    } else {
        res = validate_method_params(scb, rcb, msg);
        /* errors already recorded if any */

        if (res == NO_ERR) {
            res = validate_method_headers(scb, rcb, msg);
            /* errors already recorded if any */
        }
    }

    return res;

}  /* validate_method */


/********************************************************************
* FUNCTION make_context_tree
*
* Initialize the context parse tree for this module
*
* INPUTS:
*     context == REST API context to use
*
* OUTPUTS:
*     context->yangapi-root is malloced and set
*
* RETURNS:
*   status
*********************************************************************/
static status_t 
    make_context_tree (agt_yangapi_context_t *context)
{
    status_t res = NO_ERR;

    /* start /yang-api */
    val_value_t *valnode = val_new_value();
    if (valnode == NULL) {
        return ERR_INTERNAL_MEM;
    }

    /* pass off valnode memory here to context */
    context->root = valnode;
    val_init_from_template(valnode, context->obj);

    /* set /yang-api/datastore container */
    val_value_t *childnode =
        agt_make_object(context->obj, NCX_EL_DATASTORE, &res);
    if (childnode == NULL) {
        return res;
    }
    val_add_child(childnode, valnode);


    /* set /yang-api/modules as a virtual node
     * so the list of modules is retrieved and cached here 
     */
    childnode = agt_make_virtual_node(context->obj, NCX_EL_MODULES,
                                      get_modules, &res);
    if (childnode == NULL) {
        return res;
    }
    val_add_child(childnode, valnode);


    /* set /yang-api/operations as a virtual node
     * so the list of operations is retrieved and cached here 
     */
    childnode = agt_make_virtual_node(context->obj, NCX_EL_OPERATIONS,
                                      get_operations, &res);
    if (childnode == NULL) {
        return res;
    }
    val_add_child(childnode, valnode);

    /* set the /yang-api/version leaf */
    childnode = agt_make_leaf(context->obj, NCX_EL_VERSION, 
                              YANG_API_VERSION, &res);
    if (childnode == NULL) {
        return res;
    }
    val_add_child(childnode, valnode);
    
    return NO_ERR;

} /* make_context_tree */


/********************************************************************
* FUNCTION init_context
*
* Initialize the context for this module
* INPUTS:
*   context == context to initialize
* RETURNS:
*   status
*********************************************************************/
static status_t 
    init_context (agt_yangapi_context_t *context)
{
    agt_profile_t *profile = agt_get_profile();
    status_t res = NO_ERR;

    memset(context, 0x0, sizeof(agt_yangapi_context_t));

    if (profile->agt_use_yangapi) {
        /* load the module for yang-api */
        res = ncxmod_load_module(YANG_API_MODULE, NULL, 
                                 &profile->agt_savedevQ,
                                 &context->mod);
        if (res != NO_ERR) {
            return res;
        }

        context->mod_nsid = ncx_get_mod_nsid(context->mod);

        /* find the yang-api root object */
        context->obj = 
            obj_find_template_top(context->mod, 
                                  YANG_API_MODULE, YANG_API_ROOT);
        if (context->obj == NULL) {
            return ERR_NCX_DEF_NOT_FOUND;
        }

        res = make_context_tree(context);
        if (res != NO_ERR) {
            return res;
        }
        
        context->enabled = TRUE;
    }
    return res;

} /* init_context */


/********************************************************************
* FUNCTION clean_context
*
* Clean the context for this module
* INPUTS:
*   context == context to clean
*********************************************************************/
static void
    clean_context (agt_yangapi_context_t *context)
{
    if (!context) {
        return;
    }
    if (context->root) {
        val_free_value(context->root);
        context->root = NULL;
    }
    memset(context, 0x0, sizeof(agt_yangapi_context_t));

} /* clean_context */


/********************************************************************
* FUNCTION get_http_xml_input
*
* Get the input from the session input buffers and parse it
* as the specified target object type -- XML version
*
* INPUTS:
*   scb == session to use
*   rcb == yangapi control block to use
*   msg == response message in progress
*   retres == address of return status
*
* OUTPUTS:
*   *retres == return status
* RETURNS:
*   pointer to malloced value tree representing the parsed content.
*   Only the basic input parsing has been done, not any referential
*   integrity tests or instance tests
*   -- NCX_ERR_SKIPPED if there was no content to read
*********************************************************************/
static status_t
    get_http_xml_input (ses_cb_t *scb,
                        yangapi_cb_t *rcb,
                        rpc_msg_t *msg,
                        obj_template_t *target_obj)
{
    xml_node_t input_node;
    xml_init_node(&input_node);

    /* get the first XML node, which is the generic <input> node
     * for an operation and the target object for a data target
     */
    status_t res =
        agt_xml_consume_node(scb, &input_node, NCX_LAYER_RPC, &msg->mhdr);
    if (res != NO_ERR) {
        record_obj_error(scb, msg, res, target_obj, NULL);
    } else {
        switch (rcb->request_launchpt) {
        case YANGAPI_LAUNCHPT_OPERATION:
            /* the RPC handler expects the obj_template to be for the
             * RPC operation, not the 'input' node, and that is what is set
             * the top node is checked for meta-data and none is defined
             * so the switch from <foo-command>  .. </foo-command>
             * to <input> .. </input> should not impact the RPC handler
             */
            res = agt_rpc_parse_rpc_input(scb, msg, target_obj, &input_node);
            res = agt_rpc_post_psd_state(scb, msg, res);
            break;
        case YANGAPI_LAUNCHPT_DATA:
        case YANGAPI_LAUNCHPT_NEW_DATA:
            /* the msg->rpc_input struct was initialized when the msg
             * when the message was created with rpc_new_msg()
             * The parse function will finish setting up the node based on
             * the input_node content
             */
            res = agt_val_parse_nc(scb, &msg->mhdr, target_obj, &input_node,
                                   NCX_DC_CONFIG, msg->rpc_input);
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
    }

    if (LOGDEBUG2 && res == NO_ERR) {
        log_debug2("\nGot XML input:\n");
        val_dump_value(msg->rpc_input, 0);
    } else if (res != NO_ERR) {
        log_error("\nError: Get XML input failed (%s)", 
                  get_error_string(res));
    }

    xml_clean_node(&input_node);

    return res;

} /* get_http_xml_input */


/********************************************************************
* FUNCTION get_content_len
*
* Get the content length for the input request message body
*
* INPUTS:
*   rcb == yangapi control block to use
*
* RETURNS:
*   content length in bytes; < 0 == error
*********************************************************************/
static int
    get_content_len (yangapi_cb_t *rcb)
{


    if (rcb->content_length == NULL) {
        return 0;
    }
    
    int content_len = strtol((const char *)rcb->content_length, NULL, 10);
    return content_len;

}  /* get_content_len */


/********************************************************************
* FUNCTION set_content_encoding
*
* Get the content encoding used
*
* INPUTS:
*   rcb == yangapi control block to use
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    set_content_encoding (ses_cb_t *scb,
                          yangapi_cb_t *rcb)
{

    // TBD: parse and validate Accept header

    SES_IN_ENCODING(scb) = rcb->query_format;
    SES_OUT_ENCODING(scb) = rcb->query_format;

    return NO_ERR;

}  /* set_content_encoding */


/********************************************************************
* FUNCTION get_http_input
*
* Get the input from the session input buffers and parse it
* as the specified target object type
*
* INPUTS:
*   scb == session to use
*   rcb == yangapi control block to use
*   msg == message control block in progress
*   content_len == number of expected input bytes
*
* OUTPUTS:
*   msg->rpc_input value is set representing the parsed content.
*   Only the basic input parsing has been done, not any referential
*   integrity tests or instance tests
* RETURNS:
* status
*********************************************************************/
static status_t
    get_http_input (ses_cb_t *scb,
                    yangapi_cb_t *rcb,
                    rpc_msg_t *msg,
                    int content_len)
{
    log_debug2("\nagt_yangapi: About to retrieve HTTP content "
               "(len=%d)", content_len);

    obj_template_t *targetobj = NULL;

    switch (rcb->request_launchpt) {
    case YANGAPI_LAUNCHPT_OPERATION:
    case YANGAPI_LAUNCHPT_NEW_DATA:
        targetobj = rcb->request_target_obj;
        break;
    case YANGAPI_LAUNCHPT_DATA:
        if (rcb->request_xpath_result_count >= 1) {
            xpath_resnode_t *resnode =
                xpath_get_first_resnode(rcb->request_xpath_result);
            if (resnode) {
                val_value_t *curval = xpath_get_resnode_valptr(resnode);
                if (curval) {
                    targetobj = curval->obj;
                }
            }
        } else if (rcb->request_target) {
            targetobj = rcb->request_target->obj;
        } else {
            targetobj = rcb->request_target_obj;
        }
        break;
    default:
        return SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (targetobj == NULL) {
        log_error("\nError: no HTTP target object template set");
        return ERR_NCX_OPERATION_FAILED;
    }
        
    /* the XmlReader is already setup and the <input> element
     * is the XML or JSON document that is expected
     *
     * TBD:: switch to JSON reader if JSON output!!!
     */

    status_t res = NO_ERR;

    // TBD: get real output mode XML or JSON
    ncx_display_mode_t input_mode = SES_IN_ENCODING(scb);

    switch (input_mode) {
    case NCX_DISPLAY_MODE_XML:
        res = get_http_xml_input(scb, rcb, msg, targetobj);
        break;
    case NCX_DISPLAY_MODE_JSON:
        /* temp: JSON input not implemented yet */
        res = ERR_NCX_OPERATION_FAILED;
        record_error(scb, msg, res, NULL, NCX_EL_FORMAT);
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }
    
    return res;

}  /* get_http_input */


/********************************************************************
 * FUNCTION test_conditions_on_node
 *
 * Check if the context node(s) will match the filters (if any)
 *
 * INPUTS:
 *   rcb == YANG-API control block to use
 *   testnode == value node to check
 * RETURNS:
 *   TRUE if condition passes or skipped
 *   FALSE if condition fails 
 *********************************************************************/
static boolean
    test_conditions_on_node (yangapi_cb_t *rcb,
                             val_value_t *testnode)
{
    boolean do_test = FALSE;
    boolean test_sense = FALSE;
    boolean result = FALSE;

    if (testnode->obj && !obj_is_config(testnode->obj)) {
        return TRUE;
    }

    if (rcb->if_match) {
        do_test = TRUE;
        test_sense = TRUE;
    } else if (rcb->if_none_match) {
        do_test = TRUE;
    }
    if (do_test) {
        if (LOGDEBUG3) {
            log_debug3("\nChecking etag %s match for %s:%s",
                       (test_sense) ? EMPTY_STRING : NCX_EL_NO,
                       val_get_mod_name(testnode), testnode->name);
        }

        result = agt_match_etag_binary(testnode, rcb->query_etag);
        if (test_sense) {
            if (!result) {
                if (LOGDEBUG2) {
                    log_debug2("\netag match %s:%s failed",
                       val_get_mod_name(testnode), testnode->name);
                }
                return FALSE;
            }
        } else {
            if (result) {
                if (LOGDEBUG2) {
                    log_debug2("\netag no match %s:%s failed",
                       val_get_mod_name(testnode), testnode->name);
                }
                return FALSE;
            }
        }
    }

    do_test = FALSE;
    test_sense = FALSE;

    if (rcb->if_modified_since) {
        do_test = TRUE;
        test_sense = TRUE;
    } else if (rcb->if_unmodified_since) {
        do_test = TRUE;
    }
    if (do_test) {
        if (LOGDEBUG3) {
            log_debug3("\nChecking %s modified match for %s:%s",
                       (test_sense) ? EMPTY_STRING : NCX_EL_NO,
                       val_get_mod_name(testnode), testnode->name);
        }

        result = agt_modified_since(testnode, &rcb->query_tstamp);
        if (test_sense) {
            if (!result) {
                if (LOGDEBUG2) {
                    log_debug2("\nemodified match %s:%s failed",
                               val_get_mod_name(testnode), testnode->name);
                }
                return FALSE;
            }
        } else {
            if (result) {
                if (LOGDEBUG2) {
                    log_debug2("\nemodified no match %s:%s failed",
                               val_get_mod_name(testnode), testnode->name);
                }
                return FALSE;
            }
        }
    }

    return TRUE;

}  /* test_conditions_on_node */


/********************************************************************
 * FUNCTION check_unmodified_return
 *
 * Check if the context node(s) will match the filters (if any)
 *
 * INPUTS:
 *   rcb == YANG-API control block to use
 *
 * RETURNS:
 *   TRUE if condition passes or skipped
 *   FALSE if condition fails 
 *********************************************************************/
static boolean
    check_unmodified_return (yangapi_cb_t *rcb)
{
    if (rcb->if_match == NULL && rcb->if_modified_since == NULL &&
        rcb->if_none_match == NULL && rcb->if_unmodified_since == NULL) {
        return TRUE;
    }

    val_value_t *testnode = NULL;
    switch (rcb->request_launchpt) {
    case YANGAPI_LAUNCHPT_DATASTORE:
        testnode = cfg_get_root(NCX_CFGID_RUNNING);
        return test_conditions_on_node(rcb, testnode);
    case YANGAPI_LAUNCHPT_DATA:
        break;
    default:
        return TRUE;
    }

    uint32 match_count = 0;
    uint32 resnode_count = 0;
    xpath_resnode_t *resnode = NULL;
    if (rcb->query_select) {
        resnode = xpath_get_first_resnode(rcb->query_select_xpath_result);
    } else {
        resnode = xpath_get_first_resnode(rcb->request_xpath_result);
    }
    for (; resnode; resnode = xpath_get_next_resnode(resnode)) {
        resnode_count++;
        testnode = xpath_get_resnode_valptr(resnode);
        boolean result = test_conditions_on_node(rcb, testnode);
        if (result) {
            match_count++;
        }
    }

    if (resnode_count) {
        return (match_count);
    }
    return TRUE;

} /* check_unmodified_return */


/********************************************************************
 * FUNCTION setup_read_params
 *
 * Setup the message parameters from the yangapi_cb values
 *
 * Records errors! Do not duplicate!
 * INPUTS:
 *   rcb == YANG-API control block to use
 *   msg == message to record errors
 *
 * OUTPUTS:
 *   msg->mhdr fields set as needed
 *   rcb->skip_read may be set
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    setup_read_params (ses_cb_t *scb,
                       yangapi_cb_t *rcb,
                       rpc_msg_t *msg)
{
    status_t res = NO_ERR;

    rcb->skip_read = FALSE;

    if (rcb->query_test_xpath) {
        res = prune_context_nodes(scb, msg, rcb);
        if (res != NO_ERR) {
            record_error(scb, msg, res, NULL, NULL);
            return res;
        }
    }

    xml_msg_set_max_depth(&msg->mhdr, rcb->query_depth);

    if (rcb->request_target &&
        agt_yangapi_get_config_parm(rcb, rcb->request_target)) {
        if (rcb->if_match && *rcb->if_match != '*') {
            /* convert string to ncx_transaction_id_t */
            ncx_num_t num;
            ncx_init_num(&num);
            res = ncx_convert_num(rcb->if_match, NCX_NF_DEC,
                                  NCX_BT_UINT64, &num);
            if (res == NO_ERR) {
                ncx_etag_t etag = num.ul;
                rcb->query_etag = etag;
                xml_msg_set_etag(&msg->mhdr, etag, TRUE);
            } else {
                // else nothing will match so skip read */
                rcb->skip_read = TRUE;
            }
            ncx_clean_num(NCX_BT_UINT64, &num);
        } else if (rcb->if_none_match && *rcb->if_none_match != '*') {
            /* convert string to ncx_transaction_id_t */
            ncx_num_t num;
            ncx_init_num(&num);
            res = ncx_convert_num(rcb->if_none_match, NCX_NF_DEC,
                                  NCX_BT_UINT64, &num);
            if (res == NO_ERR) {
                ncx_etag_t etag = num.ul;
                xml_msg_set_etag(&msg->mhdr, etag, FALSE);
                rcb->query_etag = etag;
            }   /* else everything will match so ignore */
            ncx_clean_num(NCX_BT_UINT64, &num);
        }

        if (rcb->if_modified_since) {
            xml_msg_set_tstamp(&msg->mhdr, rcb->query_tstamp, TRUE);
        } else if (rcb->if_unmodified_since) {
            xml_msg_set_tstamp(&msg->mhdr, rcb->query_tstamp, FALSE);
        }
    }

    boolean errdone = FALSE;
    res = setup_select_nodes(scb, rcb, msg, &errdone);
    if (res != NO_ERR) {
        if (!errdone) {
            record_error(scb, msg, res, NULL, NULL);
        }
    }

    return res;

}  /* setup_read_params */


/************** E X T E R N A L   F U N C T I O N S  ***************/


/********************************************************************
* FUNCTION agt_yangapi_init
*
* Initialize the agt_yangapi module
*
* RETURNS:
*   status
*********************************************************************/
status_t 
    agt_yangapi_init (void)
{
    if (!agt_yangapi_init_done) {
        agt_yangapi_init_done = TRUE;
    }

    status_t res = init_context(&api_context);
    if (res != NO_ERR) {
        log_error("\nagt_yangapi: init failed (%s)", 
                  get_error_string(res));
    } else if (!api_context.enabled) {
        log_info("\nagt_yangapi: init skipped - disabled");
    }
    return res;

} /* agt_yangapi_init */


/********************************************************************
* FUNCTION agt_yangapi_cleanup
*
* Cleanup the agt_yangapi module.
*
*********************************************************************/
void 
    agt_yangapi_cleanup (void)
{
    if (agt_yangapi_init_done) {
        agt_yangapi_init_done = FALSE;
        clean_context(&api_context);
    }

} /* agt_yangapi_cleanup */


/********************************************************************
* FUNCTION agt_yangapi_dispatch
*
* Dispatch an incoming Yuma REST API request
*
* INPUTS:
*   scb == session control block
*********************************************************************/
void 
    agt_yangapi_dispatch (ses_cb_t *scb)
{
    assert( scb && "scb is NULL!" );

    //ses_total_stats_t     *agttotals = ses_get_total_stats();
    status_t res = NO_ERR;

    /* this was deferred in agt_connect.c -- assuming the
     * transport was HTTP/S; only supported transports for YANG-API  */
    scb->transport = SES_TRANSPORT_HTTP;

    /* the current node is 'rpc' in the netconf namespace
     * First get a new RPC message struct              */
    rpc_msg_t *msg = rpc_new_msg();
    if (!msg) {
        res = ERR_INTERNAL_MEM;
        //agttotals->droppedSessions++;
        if (LOGINFO) {
            log_info("\nError: malloc failed, dropping session %d (%d) %s",
                     scb->sid, res, get_error_string(res));
        }
        agt_ses_request_close(scb, scb->sid, SES_TR_DROPPED);
        return;
    }

    /* change the session state */
    scb->state = SES_ST_IN_MSG;

    /* set the default for the with-defaults parameter */
    xml_msg_set_withdef(&msg->mhdr, ses_withdef(scb));

    /* init agt_acm cache struct even though it may not
     * get used if the RPC method is invalid
     */
    res = agt_acm_init_msg_cache(scb, &msg->mhdr);
    if (res != NO_ERR) {
        record_error(scb, msg, res, NULL, NULL);
    }

    agt_yangapi_context_t *context = &api_context;
    if (res == NO_ERR && !context->enabled) {
        res = ERR_NCX_OPERATION_NOT_SUPPORTED;
        record_error(scb, msg, res, NULL, NULL);
    }

    yangapi_cb_t *rcb = scb->rcb;

    /* get the HTTP method that was used */
    if (res == NO_ERR) {
        rcb->method = get_method(rcb->request_method);
        if (rcb->method == YANGAPI_METHOD_NONE) {
            log_error("\nagt_yangapi: unsupported method '%s'", 
                      rcb->request_method);
            res = ERR_NCX_OPERATION_NOT_SUPPORTED;
            record_error(scb, msg, res, NULL, NULL);
        }
    }

    if (res == NO_ERR) {
        setup_query_string_defaults(rcb);
        rcb->content_len = get_content_len(rcb);
    }

    if (res == NO_ERR) {
        res = parse_request_uri(scb, rcb, msg, context);
        /* error recorded already if any */
    }

    boolean is_operation = FALSE;
    if (res == NO_ERR) {
        setup_launch_point(rcb);
        is_operation =
            (rcb->request_launchpt == YANGAPI_LAUNCHPT_OPERATION);
        if (is_operation) {
            msg->rpc_method = rcb->request_target_obj;
            log_debug2("\nagt_yangapi: got invoke request for operation %s:%s",
                       obj_get_mod_name(msg->rpc_method),
                       obj_get_name(msg->rpc_method));
        }
    }

    if (res == NO_ERR) {
        res = validate_method(scb, rcb, msg);
        /* error recorded already if any */
    }

    if (rcb->query_format == NCX_DISPLAY_MODE_JSON) {
        xml_msg_set_encoding(&msg->mhdr, TRUE, rcb->query_format);
        ses_set_out_encoding(scb, NCX_DISPLAY_MODE_JSON);
    }

    if (res == NO_ERR && is_operation) {
        /* check if operation allowed to be invoked by this user */
        res = agt_rpc_check_rpc_invoke(scb, msg, rcb->request_target_obj);
        if (res != NO_ERR) {
            record_obj_error(scb, msg, res, rcb->request_target_obj, NULL);
        }

        /* check temporary error: JSON output on operation resource
         * is still unimplemented */
        if (res == NO_ERR && rcb->query_format == NCX_DISPLAY_MODE_JSON) {
            res = ERR_NCX_OPERATION_FAILED;
            record_error(scb, msg, res, NULL, NCX_EL_FORMAT);
        }
    }

    if (res == NO_ERR) {
        /* reset idle timeout */
        (void)time(&scb->last_rpc_time);

        print_debug_line(scb, rcb);


            // setup condition-failed return code
            //  TBD!!!!

        if (agt_yangapi_method_is_read(rcb->method)) {
            res = setup_read_params(scb, rcb, msg);
            /* errors recorded already if any */

            if (res == NO_ERR && rcb->query_config) {
                /* check for 304 Not Modified return status
                 * unless they asked for config=false
                 */
                rcb->skip_read = !check_unmodified_return(rcb);
            }
        } else {
            boolean has_input = TRUE;

            if (is_operation) {
                has_input = obj_rpc_has_input(rcb->request_target_obj);
            }

            /* check if content_len matches expected content length */
            if (rcb->content_len && !has_input) {
                /* error, not expecting input */
                res = ERR_NCX_UNEXPECTED_INPUT;
            }

            if (res != NO_ERR) {
                record_error(scb, msg, res, NULL, NULL);
            } else if (rcb->content_len) {
                res = set_content_encoding(scb, rcb);
                if (res != NO_ERR) {
                    record_error(scb, msg, res, NULL, NULL);
                } else {
                    res = get_http_input(scb, rcb, msg, rcb->content_len);
                    if (res != NO_ERR) {
                        // assume record_error has been done
                    }
                }
            }
                
            if (res == NO_ERR) {
                // invoke the YANG-API operation
                if (is_operation) {
                    res = agt_rpc_invoke_rpc(scb, msg, NULL);
                    /* errors already recorded if any */
                } else {
                    /* handle the data resource edit operation */
                    res = agt_yangapi_edit_request(scb, rcb, msg);
                    /* errors already recorded if any */
                }
            }
        }
    }

    /* always send a response to an request or HTTP server will hang
     * and eventually timeout eith a 500 error     */
    ses_start_msg_mode(scb);
    agt_yangapi_reply_send(scb, rcb, msg, res);
    ses_stop_msg_mode(scb);

    /* check if there is any auditQ because changes to 
     * the running config were made
     */
    if (res == NO_ERR && msg->rpc_txcb && 
        !dlq_empty(&msg->rpc_txcb->auditQ)) {
        agt_sys_send_sysConfigChange(scb, &msg->rpc_txcb->auditQ);
    }

    /* only reset the session state to idle if was not changed
     * to SES_ST_SHUTDOWN_REQ during this RPC call
     */
    if (scb->state == SES_ST_IN_MSG) {
        scb->state = SES_ST_IDLE;
    }

    agt_acm_clear_msg_cache(&msg->mhdr);
    free_msg(msg);

    if (scb->state != SES_ST_SHUTDOWN_REQ) {
        agt_ses_request_close(scb, scb->sid, SES_TR_CLOSED);
    }

    print_errors();
    clear_errors();

} /* agt_yangapi_dispatch */


/********************************************************************
* FUNCTION agt_yangapi_get_nsid
*
* Get the YANG-API XML namespace ID
*
* RETURNS:
*  YANG-API namespace ID
*********************************************************************/
xmlns_id_t
    agt_yangapi_get_nsid (void)
{
    xmlns_id_t id = 0;
    if (agt_yangapi_init_done) {
        id = api_context.mod_nsid;
    }
    return id;

} /* agt_yangapi_get_nsid */


/********************************************************************
* FUNCTION agt_yangapi_get_context
*
* Get the YANG-API Context structure
*
* RETURNS:
*  YANG-API namespace ID
*********************************************************************/
agt_yangapi_context_t *
    agt_yangapi_get_context (void)
{
    return &api_context;

} /* agt_yangapi_get_context */


/********************************************************************
* FUNCTION agt_yangapi_get_config_parm
*
* Get the value of the config parm to use
*
* INPUTS:
*   rcb == yangapi control block to use
*   context_node == XPath context node to check
* RETURNS:
*    TRUE if config=true or FALSE if config=false
*********************************************************************/
boolean
    agt_yangapi_get_config_parm (yangapi_cb_t *rcb,
                                 val_value_t *context_node)
{
    boolean config_parm = rcb->query_config;
    
    if (context_node && !obj_is_config(context_node->obj) &&
        (rcb->request_launchpt == YANGAPI_LAUNCHPT_DATASTORE ||
         rcb->request_launchpt == YANGAPI_LAUNCHPT_DATA)) {
        config_parm = FALSE;
    }

    return config_parm;

}  /* agt_yangapi_get_config_parm */


/********************************************************************
* FUNCTION agt_yangapi_method_is_read
*
* Check if this is a read method
*
* INPUTS:
*   method == enum to check
*
* RETURNS:
*   TRUE if some sort of read or FALSE if some sort of write
*********************************************************************/
boolean
    agt_yangapi_method_is_read (yangapi_method_t method)
{
    boolean ret = FALSE;
    switch (method) {
    case YANGAPI_METHOD_NONE:
    case YANGAPI_METHOD_OPTIONS:
    case YANGAPI_METHOD_HEAD:
    case YANGAPI_METHOD_GET:
        ret = TRUE;
        break;
    case YANGAPI_METHOD_POST:
    case YANGAPI_METHOD_PUT:
    case YANGAPI_METHOD_PATCH:
    case YANGAPI_METHOD_DELETE:
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

    return ret;

}  /* agt_yangapi_method_is_read */


/********************************************************************
* FUNCTION agt_yangapi_record_error
*
* Record an rpc-error for YANG-API response translation
* 
* INPUTS:
*   scb == session control block to use
*   msg == rpc_msg_t to use
*   res == error status code
*   errnode == node in API tree or data tree associated with the error
*   badval == error-info string to use
*********************************************************************/
void
    agt_yangapi_record_error (ses_cb_t *scb,
                              rpc_msg_t *msg,
                              status_t res,
                              val_value_t *errnode,
                              const xmlChar *badval)
{
    if (res == ERR_NCX_DEF_NOT_FOUND) {
        res = ERR_NCX_RESOURCE_UNKNOWN;
    }
    agt_record_error(scb, &msg->mhdr, NCX_LAYER_RPC, res, NULL, 
                     badval ? NCX_NT_STRING : NCX_NT_NONE, badval,
                     errnode ? NCX_NT_VAL : NCX_NT_NONE, errnode);
}  /* agt_yangapi_record_error */


/* END file agt_yangapi.c */
#endif   // WITH_YANGAPI

