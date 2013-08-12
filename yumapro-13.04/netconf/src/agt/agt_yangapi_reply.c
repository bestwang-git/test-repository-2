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
/*  FILE: agt_yangapi_reply.c

   Yuma REST API Message Handler for YANG-API protocol
   Reply handler
 

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
17mar13      abb      begun; split from agt_yangapi.c

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
//#define AGT_YANGAPI_REPLY_DEBUG 1
#endif


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


/********************************************************************
* FUNCTION output_full_valnode
*
* Output a complete node from a val_value_t tree
* Number of levels printed depends on depth parameter
*
* INPUTS:
*   scb == session to use
*   rcb = yangcpi control block to use
*   msg == rpc_msg_t in progress
*   valnode == value tree to output
*   indent == start indent amount
*   force_get == TRUE if the config=true|false should be
*     ignored and config=false used instead; needed for generic
*     RPC output
*********************************************************************/
static void
    output_full_valnode (ses_cb_t *scb,
                         yangapi_cb_t *rcb,
                         rpc_msg_t *msg,
                         val_value_t *valnode,
                         int32 indent,
                         boolean force_get)
{
    ncx_display_mode_t display_mode = ses_get_out_encoding(scb);

    boolean getop = TRUE;
    switch (rcb->request_launchpt) {
    case YANGAPI_LAUNCHPT_DATASTORE:
        if (!force_get) {
            getop = !agt_yangapi_get_config_parm(rcb, NULL);
        }
        break;
    case YANGAPI_LAUNCHPT_DATA:
        if (!force_get) {
            /* if the target resource is non-config then get it
             * even if the config=true|false node is true    */
            getop = !agt_yangapi_get_config_parm(rcb, valnode);
        }
        break;
    default:
        ;
    }

    val_nodetest_fn_t testfn = NULL;
    if (getop) {
        if (rcb->request_launchpt == YANGAPI_LAUNCHPT_DATA ||
            rcb->request_launchpt == YANGAPI_LAUNCHPT_DATASTORE) {
            testfn = agt_check_config_false;
        } else {
            testfn = agt_check_default;
        }
    } else {
        testfn = agt_check_config;
    }

    switch (display_mode) {
    case NCX_DISPLAY_MODE_XML:
        xml_wr_max_check_val(scb, &msg->mhdr, valnode, indent, testfn, TRUE);
        break;
    case NCX_DISPLAY_MODE_JSON:
        json_wr_full_check_val(scb, &msg->mhdr, 0, valnode, indent, testfn);
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

} /* output_full_valnode */


/********************************************************************
* FUNCTION output_full_valnode_ex
*
* Output a complete node from a val_value_t tree
* Number of levels printed depends on depth parameter
*
* INPUTS:
*   scb == session to use
*   rcb = yangcpi control block to use
*   msg == rpc_msg_t in progress
*   valnode == value tree to output
*   indent == start indent amount
*   force_get == TRUE if the config=true|false should be
*     ignored and config=false used instead; needed for generic
*     RPC output
*********************************************************************/
static void
    output_full_valnode_ex (ses_cb_t *scb,
                            yangapi_cb_t *rcb,
                            rpc_msg_t *msg,
                            val_value_t *valnode,
                            int32 indent,
                            boolean force_get,
                            boolean isfirst,
                            boolean islast,
                            boolean isfirstchild,
                            boolean isfirstsib,
                            boolean force_lastsibling,
                            boolean force_lastsib_value,
                            boolean force_array_obj)
{
    ncx_display_mode_t display_mode = ses_get_out_encoding(scb);

    boolean getop = TRUE;
    switch (rcb->request_launchpt) {
    case YANGAPI_LAUNCHPT_DATASTORE:
        if (!force_get) {
            getop = !agt_yangapi_get_config_parm(rcb, NULL);
        }
        break;
    case YANGAPI_LAUNCHPT_DATA:
        if (!force_get) {
            /* if the target resource is non-config then get it
             * even if the config=true|false node is true    */
            getop = !agt_yangapi_get_config_parm(rcb, valnode);
        }
        break;
    default:
        ;
    }

    val_nodetest_fn_t testfn = NULL;
    if (getop) {
        testfn = agt_check_default;
    } else {
        testfn = agt_check_config;
    }

    switch (display_mode) {
    case NCX_DISPLAY_MODE_XML:
        xml_wr_max_check_val(scb, &msg->mhdr, valnode, indent, testfn, TRUE);
        break;
    case NCX_DISPLAY_MODE_JSON:
        json_wr_max_check_val(scb, &msg->mhdr, 0, valnode, indent, testfn,
                              isfirst, islast, isfirstchild, isfirstsib, 
                              force_lastsibling, force_lastsib_value,
                              force_array_obj);
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

} /* output_full_valnode_ex */


/********************************************************************
* FUNCTION output_data_result
*
* Output the result nodeset in YANG-API format
*
* INPUTS:
*    scb == session control block
*    rcb = yangcpi control block to use
*    msg == rpc_msg_t in progress
*********************************************************************/
static void
    output_data_result (ses_cb_t *scb,
                        yangapi_cb_t *rcb,
                        rpc_msg_t *msg)
{
    uint32 resnode_count = 0;
    xpath_resnode_t *resnode = NULL;

    if (rcb->query_select_xpath_result) {
        log_debug3("\nagt_yangapi: Output from select result");
        resnode = xpath_get_first_resnode(rcb->query_select_xpath_result);
        resnode_count = rcb->query_select_xpath_result_count;
        if (resnode == NULL) {
            log_debug("\nagt_yangapi: No select nodes matched expr '%s'",
                      rcb->query_select);
        }
    } else {
        log_debug3("\nagt_yangapi: Output from context result");
        resnode = xpath_get_first_resnode(rcb->request_xpath_result);
        resnode_count = rcb->request_xpath_result_count;
        if (resnode == NULL && resnode_count) {
            log_error("\nError: agt_yangapi: Expected %u resnodes, got zero",
                      resnode_count);
        }
    }

    /* !!! USING <data> wrapper on every GET for a data resource
     * !!! or else NACM can filter the node and produce an empty
     * !!! XML or JSON document
     */
    boolean need_wrapper = TRUE;

    int32 indent_amount = ses_message_indent_count(scb);
    int32 indent = min(indent_amount, 0);
    agt_yangapi_context_t *context = agt_yangapi_get_context();

    ses_start_msg(scb);

    if (need_wrapper) {
        agt_output_start_container(scb, &msg->mhdr, 0, context->mod_nsid,
                                   YANGAPI_WRAPPER_NODE, indent);
        indent = ses_new_indent_count(TRUE, indent, indent_amount);
    }

    boolean depth1 = (rcb->query_depth == 1);
    boolean isfirst = TRUE;
    boolean isfirstchild = TRUE;
    xpath_resnode_t *nextnode = NULL;
    val_value_t *lastval = NULL;

    if (rcb->empty_read) {
        resnode = NULL;
    }

    uint64 outbytes = SES_OUT_BYTES(scb);

    while (resnode) {
        nextnode = xpath_get_next_resnode(resnode);
        val_value_t *nextval = NULL;
        if (nextnode) {
            nextval = xpath_get_resnode_valptr(nextnode);
        }
        val_value_t *curval = xpath_get_resnode_valptr(resnode);
        if (curval) {

            boolean skip_it = FALSE;

            if (lastval && depth1) {
                if (lastval->obj == curval->obj) {
                    /* skip this object already printed 1 */
                    skip_it = TRUE;
                }
            }

            if (!skip_it) {
                boolean isfirstsib =
                    !(lastval && lastval->obj == curval->obj);

                boolean force_lastsib_value =
                    !(nextval && nextval->obj == curval->obj);

                if (depth1) {
                    force_lastsib_value = TRUE;
                }

                boolean force_array =
                    (isfirstsib) ? !force_lastsib_value : TRUE;

                boolean islast = (nextnode == NULL);

                /* force with-defaults=report-all if the requested
                 * context node is set-by-default; otherwise need
                 * to return UNKNOWN_RESOURCE which is not correct
                 */
                boolean set_default = FALSE;
                ncx_withdefaults_t savedef = xml_msg_get_withdef(&msg->mhdr);
                if (val_set_by_default(curval)) {
                    set_default = TRUE;
                    xml_msg_set_withdef(&msg->mhdr, NCX_WITHDEF_REPORT_ALL);
                }
                output_full_valnode_ex(scb, rcb, msg, curval, indent,
                                       FALSE, isfirst, islast, isfirstchild,
                                       isfirstsib, TRUE, force_lastsib_value,
                                       force_array);
                if (set_default) {
                    xml_msg_set_withdef(&msg->mhdr, savedef);
                }
            }
        }

        lastval = curval;
        resnode = nextnode;
        isfirst = FALSE;
        isfirstchild = FALSE;
    }

    boolean datawritten = (outbytes != SES_OUT_BYTES(scb));
    if (!datawritten) {
        if (ses_get_out_encoding(scb) == NCX_DISPLAY_MODE_JSON) {
            json_wr_output_null(scb, -1);
        }
    }

    if (need_wrapper) {
        indent = ses_new_indent_count(FALSE, indent, indent_amount);
        agt_output_end_container(scb, &msg->mhdr, context->mod_nsid,
                                 YANGAPI_WRAPPER_NODE, indent);
    }

} /* output_data_result */


/********************************************************************
* FUNCTION send_content_type_header
*
* Operation succeeded or failed
* Generate the Content-Type header for the response message
* 
* INPUTS:
*   scb == session control block
*********************************************************************/
static void
    send_content_type_header (ses_cb_t *scb)
{
    ncx_display_mode_t display_mode = ses_get_out_encoding(scb);

    /* print the Content-type first */
    switch (display_mode) {
    case NCX_DISPLAY_MODE_XML:
        ses_putstr(scb, (const xmlChar *)"Content-type: application/xml\r\n");
        break;
    case NCX_DISPLAY_MODE_JSON:
        ses_putstr(scb, (const xmlChar *)"Content-type: application/json\r\n");
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

}  /* send_content_type_header */


/********************************************************************
* FUNCTION send_allow_header
*
* Method is OPTIONS
* Generate the Allow header line for the target resource
* 
* INPUTS:
*   scb == session control block
*   rcb == yangapi context control block
*********************************************************************/
static void
    send_allow_header (ses_cb_t *scb,
                       yangapi_cb_t *rcb)
{
    // boolean options_set = TRUE; (not needed)
    boolean head_set = FALSE;
    boolean get_set = FALSE;
    boolean post_set = FALSE;
    boolean put_set = FALSE;
    boolean patch_set = FALSE;
    boolean delete_set = FALSE;

    switch (rcb->request_launchpt) {

    case YANGAPI_LAUNCHPT_TOP:
    case YANGAPI_LAUNCHPT_DATASTORE:
    case YANGAPI_LAUNCHPT_MODULES:
    case YANGAPI_LAUNCHPT_OPERATIONS:
    case YANGAPI_LAUNCHPT_VERSION:
        head_set = TRUE;
        get_set = TRUE;
        break;
    case YANGAPI_LAUNCHPT_OPERATION:
        post_set = TRUE;
        break;
    case YANGAPI_LAUNCHPT_DATA:
    case YANGAPI_LAUNCHPT_NEW_DATA:
        {
            head_set = TRUE;
            get_set = TRUE;

            obj_template_t *testobj = NULL;
            if (rcb->request_target) {
                testobj = rcb->request_target->obj;
            } else if (rcb->request_target_obj) {
                testobj = rcb->request_target_obj;
            }

            if (testobj && obj_is_config(testobj)) {
                post_set = TRUE;
                put_set = TRUE;
                patch_set = TRUE;
                delete_set = TRUE;
            }
        }
        break;
    case YANGAPI_LAUNCHPT_NONE:
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return;
    }

    /* print the Allow header for the target resource */
    ses_putstr(scb, (const xmlChar *)"Allow: OPTIONS");

    if (head_set) {
        ses_putstr(scb, (const xmlChar *)",HEAD");
    }
    if (get_set) {
        ses_putstr(scb, (const xmlChar *)",GET");
    }
    if (post_set) {
        ses_putstr(scb, (const xmlChar *)",POST");
    }
    if (put_set) {
        ses_putstr(scb, (const xmlChar *)",PUT");
    }
    if (patch_set) {
        ses_putstr(scb, (const xmlChar *)",PATCH");
    }
    if (delete_set) {
        ses_putstr(scb, (const xmlChar *)",DELETE");
    }

    ses_putstr(scb, (const xmlChar *)"\r\n");

}  /* send_allow_header */


/********************************************************************
* FUNCTION send_location_header
*
* Method is POST for a YANGAPI_LAUNCHPT_NEW_DATA
* Generate the Location header line for the new target resource
* 
* INPUTS:
*   scb == session control block
*   rcb == yangapi context control block
*********************************************************************/
static void
    send_location_header (ses_cb_t *scb,
                          yangapi_cb_t *rcb)
{
    /* print the Location header for the target resource */
    ses_putstr(scb, (const xmlChar *)"Location: ");

    xmlChar savech = 0;
    if (rcb->query_start) {
        savech = rcb->query_start[0];
        rcb->query_start[0] = 0;
    }

    /* get URL in 2 parts <server-url> + <target-resource> */
    agt_profile_t *profile = agt_get_profile();
    ses_putstr(scb, profile->agt_yangapi_server_url);
    ses_putstr(scb, rcb->request_uri);

    if (savech) {
        rcb->query_start[0] = savech;
    }

    ses_putstr(scb, (const xmlChar *)"\r\n");

}  /* send_location_header */


/********************************************************************
* FUNCTION send_cache_headers
*
* Generate the Cache and Pragma cache header lines
* 
* INPUTS:
*   scb == session control block
*   rcb == yangapi context control block
*********************************************************************/
static void
    send_cache_headers (ses_cb_t *scb,
                        yangapi_cb_t *rcb)
{
    (void)rcb;

    /* print the Cache-Control header */
    ses_putstr(scb, (const xmlChar *)"Cache-Control: no-cache\r\n");

    /* print the Pragma header */
    ses_putstr(scb, (const xmlChar *)"Pragma: no-cache\r\n");

}  /* send_cache_headers */


/********************************************************************
* FUNCTION send_etag_header
*
* Target resource is YANGAPI_LAUNCHPT_DATA or YANGAPI_LAUNCHPT_NEW_DATA
* Generate the ETag header line for the target resource
* 
* INPUTS:
*   scb == session control block
*   rcb == yangapi context control block
*********************************************************************/
static void
    send_etag_header (ses_cb_t *scb,
                      yangapi_cb_t *rcb)
{
    xmlChar buff[NCX_MAX_NUMLEN+1];

    val_value_t *root = rcb->request_target;
    if (rcb->request_launchpt == YANGAPI_LAUNCHPT_DATASTORE) {
        root = cfg_get_root(NCX_CFGID_RUNNING);
    }

    status_t res = val_sprintf_etag(root, buff, NCX_MAX_NUMLEN+1);
    if (res == NO_ERR) {
        /* print the ETag header for the target resource */
        ses_putstr(scb, (const xmlChar *)"ETag: ");
        ses_putstr(scb, buff);
        ses_putstr(scb, (const xmlChar *)"\r\n");
    } else {
        log_error("\nError: could not get ETag (%s)", get_error_string(res));
    }

}  /* send_etag_header */


/********************************************************************
* FUNCTION send_last_modified_header
*
* Target resource is YANGAPI_LAUNCHPT_DATA or YANGAPI_LAUNCHPT_NEW_DATA
* Generate the LastModified header line for the target resource
* 
* INPUTS:
*   scb == session control block
*   rcb == yangapi context control block
*********************************************************************/
static void
    send_last_modified_header (ses_cb_t *scb,
                               yangapi_cb_t *rcb)
{
    xmlChar buff[TSTAMP_HTML_SIZE];

    val_value_t *root = rcb->request_target;
    if (rcb->request_launchpt == YANGAPI_LAUNCHPT_DATASTORE) {
        root = cfg_get_root(NCX_CFGID_RUNNING);
    }

    ses_putstr(scb, (const xmlChar *)"Last-Modified: ");

    tstamp_time2htmltime(&root->last_modified, buff, TSTAMP_HTML_SIZE);
    ses_putstr(scb, buff);

    ses_putstr(scb, (const xmlChar *)"\r\n");

}  /* send_last_modified_header */


/********************************************************************
* FUNCTION send_yangapi_headers
*
* Operation succeeded or failed
* Generate all the response headers needed by this message
* 
* The FCGI program will generate most of the response headers
* so this function only adds the ones that FCGI will not add
* by default:
*
* SET BY FCGI:
*   Date
*   Server
*   Vary  (if Content-Type not in Accept request header)
*   Content-Encoding
*   Content-Length
*   Keep-Alive
*   Connection
*
* MUST BE SET BY THIS FUNCTION
*   Allow
*   Content-Type
*   Status (if not 200 OK)
*   Location (set for 201 status return)
*
* INPUTS:
*   scb == session control block
*   rcb == yangapi control block to use
*   msg == rpc_msg_t in progress
*   res == general request status
*   ok_variant == TRUE if no output (HTTP <ok/> not sent)
*                 FALSE if this is some other form of response
*********************************************************************/
static void
    send_yangapi_headers (ses_cb_t *scb,
                          yangapi_cb_t *rcb,
                          rpc_msg_t *msg,
                          status_t res,
                          boolean ok_variant)
{
    boolean skip_read = rcb->skip_read;
    if (!dlq_empty(&msg->mhdr.errQ)) {
        skip_read = FALSE;
    }

    boolean skip_content_type =
        (skip_read || ok_variant || rcb->method == YANGAPI_METHOD_OPTIONS);

    if (!skip_content_type) {
        send_content_type_header(scb);
    }

    /* Status header line */
    uint32 statusnum = (skip_content_type) ? 204 : 200;

    boolean newdata =
        (rcb->method == YANGAPI_METHOD_POST &&
         rcb->request_launchpt == YANGAPI_LAUNCHPT_NEW_DATA &&
         res == NO_ERR);

    if (newdata) {
        statusnum = 201;
    }

    if (skip_read) {
        statusnum = 304;
    }

    if (rcb->return_code) {
        /* the return_code field can be used to override a generic error
         * code conversion or set one of the 2xx (200, 201, or 204) codes
         */
        statusnum = rcb->return_code;
    } else if (res != NO_ERR) {
        if (res == ERR_NCX_PRECONDITION_FAILED) {
            statusnum = 412;
        } else {
            /* Generate an error status line */
            rpc_err_t rpcerr = agt_rpcerr_get_rpcerr(res);
            statusnum = agt_rpcerr_get_http_status_code(rpcerr);
        }
    }

    if (statusnum) {
        const xmlChar *statusline =
            agt_rpcerr_get_http_status_string(statusnum);
        if (statusline) {
            ses_putstr(scb, (const xmlChar *)"Status: ");
            ses_putstr(scb, statusline);
            ses_putchar(scb, '\n');
        }
    }

    /* Check if this is the options variant */
    if (rcb->method == YANGAPI_METHOD_OPTIONS) {
        boolean no_errors = dlq_empty(&msg->mhdr.errQ);
        if (no_errors) {
            /* only generate Allow header if there were no errors */
            send_allow_header(scb, rcb);
        }
    } else {
        if (newdata) {
            /* generate Location header */
            send_location_header(scb, rcb);
        }

        send_cache_headers(scb, rcb);

        /* Last-Modified and ETag headers for config=true data resources */
        if (!skip_read && res == NO_ERR &&
            (rcb->request_launchpt == YANGAPI_LAUNCHPT_DATA ||
             rcb->request_launchpt == YANGAPI_LAUNCHPT_NEW_DATA ||
             rcb->request_launchpt == YANGAPI_LAUNCHPT_DATASTORE) &&
            rcb->request_target &&  obj_is_config(rcb->request_target->obj)) {

            send_last_modified_header(scb, rcb);
            send_etag_header(scb, rcb);
        }
    }

    // after last line put a blank line
    ses_putstr(scb, (const xmlChar *)"\r\n");

}  /* send_yangapi_headers */


/********************************************************************
* FUNCTION send_yangapi_errors
*
* Operation failed
* Generate the <errors> subtree
* 
* INPUTS:
*   scb == session control block
*   msg == rpc_msg_t in progress
*
* RETURNS:
*   TRUE if any errors printed; FALSE if not
*********************************************************************/
static boolean
    send_yangapi_errors (ses_cb_t *scb,
                         rpc_msg_t *msg)
{
    boolean msg_started = FALSE;

    const rpc_err_rec_t *err =
        (const rpc_err_rec_t *)dlq_firstEntry(&msg->mhdr.errQ);
    if (err) {
        ses_start_msg(scb);
        msg_started = TRUE;

        int32 indent_amount = ses_message_indent_count(scb);
        int32 indent = min(indent_amount, 0);
        int32 err_indent = ses_new_indent_count(TRUE, indent, indent_amount);

        /* using YumaWorks YANG module namespace */
        agt_yangapi_context_t *context = agt_yangapi_get_context();
        xmlns_id_t errid = ncx_get_mod_nsid(context->mod);

        agt_output_start_container(scb, &msg->mhdr, 0, errid, NCX_EL_ERRORS,
                                   indent);

        boolean isfirst = TRUE;
        boolean islast = FALSE;

        const rpc_err_rec_t *nexterr = NULL;
        for (; err != NULL; err = nexterr) {
            nexterr = (const rpc_err_rec_t *)dlq_nextEntry(err);
            islast = (!nexterr);

            status_t res =
                agt_rpc_send_rpc_error(scb, &msg->mhdr, err, 
                                       err_indent, isfirst, islast);
            if (res != NO_ERR) {
                log_error("\nError: send yang-api error failed (%s)",
                          get_error_string(res));
            }
            isfirst = FALSE;
        }

        agt_output_end_container(scb, &msg->mhdr, errid, NCX_EL_ERRORS,
                                 min(indent, 0));
    }

    return msg_started;

} /* send_yangapi_errors */


/********************************************************************
* FUNCTION send_yangapi_data
*
* Retrieval operation succeeded
* Generate the proper data subtree
* 
* INPUTS:
*   scb == session control block
*   rcb == yangapi control block to use
*   msg == rpc_msg_t in progress
*********************************************************************/
static void
    send_yangapi_data (ses_cb_t *scb,
                       yangapi_cb_t *rcb,
                       rpc_msg_t *msg)
{
    val_value_t *startnode = NULL;
    boolean forceget = TRUE;

    switch (rcb->request_launchpt) {
    case YANGAPI_LAUNCHPT_NONE:
    case YANGAPI_LAUNCHPT_NEW_DATA:
        /* get not allowed for these targets */
        break;
    case YANGAPI_LAUNCHPT_OPERATION:
        break;
    case YANGAPI_LAUNCHPT_TOP:
    case YANGAPI_LAUNCHPT_MODULES:
    case YANGAPI_LAUNCHPT_VERSION:
    case YANGAPI_LAUNCHPT_OPERATIONS:
        startnode = rcb->request_target;
        break;
    case YANGAPI_LAUNCHPT_DATASTORE:
        forceget = FALSE;
        startnode = cfg_get_root(NCX_CFGID_RUNNING);
        break;
    case YANGAPI_LAUNCHPT_MODULE:
        /* Should not happen: saved for TBD wrapper resolution
         * Using the modules wrapper so this launch point will
         * not really be seen and MODULES will be used instead
         */
        break;
    case YANGAPI_LAUNCHPT_DATA:
        output_data_result(scb, rcb, msg);
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (startnode) {
        xml_msg_set_withdef(&msg->mhdr, ses_withdef(scb));
        ses_start_msg(scb);
        output_full_valnode(scb, rcb, msg, startnode, 0, forceget);
    }

} /* send_yangapi_data */


/************** E X T E R N A L   F U N C T I O N S  ***************/


/********************************************************************
* FUNCTION agt_yangapi_reply_send
*
* Operation succeeded or failed
* Return a REST API HTTP message
* 
* INPUTS:
*   scb == session control block
*   rcb == yangapi control block to use
*   msg == rpc_msg_t in progress
*   res == general request status
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_yangapi_reply_send (ses_cb_t *scb,
                            yangapi_cb_t *rcb,
                            rpc_msg_t *msg,
                            status_t res)
{
    boolean datasend = (!dlq_empty(&msg->rpc_dataQ) 
                        || msg->rpc_datacb) ? TRUE : FALSE;

    boolean api_datasend = agt_yangapi_method_is_read(rcb->method);

    boolean ok_variant =
        (dlq_empty(&msg->mhdr.errQ) && !datasend && !api_datasend);

    boolean std_data = (msg->rpc_data_type == RPC_DATA_STD);

    msg->rpc_agt_state = AGT_RPC_PH_REPLY;

    /* set the initial namespace prefix map for the response */
    xml_attrs_t attrs;
    xml_init_attrs(&attrs);

    status_t res2 =
        xml_msg_build_prefix_map(&msg->mhdr, &attrs, std_data,
                                 !dlq_empty(&msg->mhdr.errQ));
    if (res == NO_ERR && res2 != NO_ERR) {
        res = res2;
        agt_yangapi_record_error(scb, msg, res, NULL, NULL);
    }

    send_yangapi_headers(scb, rcb, msg, res, ok_variant);

    /* flag if ses_start_msg already called
     * used in XML display mode to print the <?xml...?> decl */
    boolean msg_done = FALSE;

    /* check which reply variant needs to be sent */
    if (ok_variant) {
        /* no errors and no data, so this is the <ok> variant,
         * except no data is returned in HTTP in this case
         */
        msg_done = TRUE;
    }

    if (!msg_done) {
        /* see if there are any errors to send
         * in YANG-API either errors or data can be sent but not both */
        msg_done = send_yangapi_errors(scb, msg);
    }

    if (rcb->method == YANGAPI_METHOD_HEAD ||
        rcb->method == YANGAPI_METHOD_OPTIONS) {
        /* skip the rest of the output if method was HEAD instead of GET
         * or if Allow header was sent for an OPTIONS request
         */
        msg_done = TRUE;
    }

    if (rcb->skip_read) {
        /* returned 304 Mot Modified instead of data */
        msg_done = TRUE;
    }

    if (!msg_done) {
        /* check generation of data contents
         * If the rpc_dataQ is non-empty, then there is
         * staticly filled data to send
         * If the rpc_datacb function pointer is non-NULL,
         * then this is dynamic reply content, even if the rpc_data
         * node is not NULL
         * check if there is any data to send set by an RPC function */
        if (datasend) {
            ses_start_msg(scb);
            msg_done = TRUE;

            agt_yangapi_context_t *context = agt_yangapi_get_context();
            xmlns_id_t wrap_nsid = context->mod_nsid;
            int32 indent_amount = ses_message_indent_count(scb);
            int32 indent = min(indent_amount, 0);
            boolean need_data_wrapper = (msg->rpc_data_type == RPC_DATA_STD);

            if (msg->rpc_datacb) {

                /* send the <output> wrapper */
                agt_output_start_container(scb, &msg->mhdr, 0, wrap_nsid, 
                                           NCX_EL_OUTPUT, indent);
                indent = ses_new_indent_count(TRUE, indent, indent_amount);

                if (need_data_wrapper) {
                    agt_output_start_container(scb, &msg->mhdr, wrap_nsid,
                                               xmlns_nc_id(), NCX_EL_DATA,
                                               indent);
                    indent = ses_new_indent_count(TRUE, indent, indent_amount);
                }

                uint64 outbytes = SES_OUT_BYTES(scb);

                /* use user callback to generate the contents */
                agt_rpc_data_cb_t agtcb = (agt_rpc_data_cb_t)msg->rpc_datacb;
                res = (*agtcb)(scb, msg, indent);
                if (res != NO_ERR) {
                    log_error("\nError: SIL data callback failed (%s)",
                              get_error_string(res));
                }

                if (need_data_wrapper) {
                    /* only indent the </data> end tag if any data written */
                    boolean datawritten = (outbytes != SES_OUT_BYTES(scb));

                    if (!datawritten && 
                        ses_get_out_encoding(scb) == NCX_DISPLAY_MODE_JSON) {
                        json_wr_output_null(scb, -1);
                    }
                    indent = ses_new_indent_count(FALSE, indent, indent_amount);

                    agt_output_end_container(scb, &msg->mhdr, xmlns_nc_id(),
                                             NCX_EL_DATA,
                                             (datawritten) ? indent : -1);
                }

                indent = ses_new_indent_count(FALSE, indent, indent_amount);
                agt_output_end_container(scb, &msg->mhdr, wrap_nsid,
                                         NCX_EL_OUTPUT, indent);
            } else {
                val_value_t *pointnode = NULL;
                val_value_t *node = NULL;
                val_value_t *topcon =
                    xml_val_new_struct(NCX_EL_OUTPUT, wrap_nsid);
                if (topcon == NULL) {
                    return ERR_INTERNAL_MEM;
                }
                pointnode = topcon;

                if (need_data_wrapper) {
                    node = xml_val_new_struct(NCX_EL_DATA, xmlns_nc_id());
                    if (node == NULL) {
                        val_free_value(topcon);
                        return ERR_INTERNAL_MEM;
                    }
                    val_add_child(node, topcon);
                    pointnode = node;
                }

                /* just write the contents of the rpc <data> varQ */
                val_value_t *val;
                for (val = (val_value_t *)dlq_firstEntry(&msg->rpc_dataQ);
                     val != NULL;
                     val = (val_value_t *)dlq_nextEntry(val)) {
                    val->parent = pointnode;
                }
                dlq_block_enque(&msg->rpc_dataQ, &pointnode->v.childQ);

                output_full_valnode(scb, rcb, msg, topcon, indent, TRUE);

                val_free_value(topcon);
            }
        } else if (api_datasend) {
            /* the requested data is in the /yang-api resources */
            (void)send_yangapi_data(scb, rcb, msg);
        }
    }

    /* finish the msg and have it queued to send to the client */
    ses_finish_msg(scb);

    xml_clean_attrs(&attrs);

    return NO_ERR;

}  /* agt_yangapi_reply_send */


/* END file agt_yangapi_reply.c */
#endif   // WITH_YANGAPI

