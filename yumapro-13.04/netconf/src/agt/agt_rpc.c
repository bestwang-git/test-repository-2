/*
 * Copyright (c) 2008 - 2012, Andy Bierman, All Rights Reserved.
 * Copyright (c) 2012, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: agt_rpc.c

   NETCONF Protocol Operations: RPC Agent Side Support

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
17may05      abb      begun

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
#include "agt_cfg.h"
#include "agt_cli.h"
#include "agt_rpc.h"
#include "agt_rpcerr.h"
#include "agt_ses.h"
#include "agt_sys.h"
#include "agt_util.h"
#include "agt_val.h"
#include "agt_val_parse.h"
#include "agt_xml.h"

#ifdef WITH_YANGAPI
#include "agt_yangapi.h"
#endif

#include "dlq.h"
#include "json_wr.h"
#include "log.h"
#include "ncx.h"
#include "ncx_num.h"
#include "ncxconst.h"
#include "obj.h"
#include "rpc.h"
#include "rpc_err.h"
#include "ses.h"
#include "status.h"
#include "top.h"
#include "val.h"
#include "val_util.h"
#include "xmlns.h"
#include "xml_msg.h"
#include "xml_util.h"
#include "xml_wr.h"
#include "yangconst.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

/* hack for <error-path> for a couple corner case errors */
#define RPC_ROOT ((const xmlChar *)"/nc:rpc")


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
static boolean agt_rpc_init_done = FALSE;

/* Q of ncx_backptr_t for all registered RPC methods */
static dlq_hdr_t agt_rpc_methodQ;


/********************************************************************
* FUNCTION find_method_backptr
*
* Find an RPC object template backptr in the agt_rpc_methodQ
* 
* INPUTS:
*   rpcobj == rpc template pointer to find
*********************************************************************/
static ncx_backptr_t *
    find_method_backptr (obj_template_t *rpcobj)
{
    ncx_backptr_t *backptr =
        ncx_find_backptr(&agt_rpc_methodQ, rpcobj);
    return backptr;

}  /* find_method_backptr */


/********************************************************************
* FUNCTION add_method_backptr
*
* Add an RPC object template backptr to the agt_rpc_methodQ
* 
* INPUTS:
*   backptr == backptr to add
*********************************************************************/
static void
    add_method_backptr (ncx_backptr_t *backptr)
{
    obj_template_t *obj =
        (obj_template_t *)ncx_get_backptr_node(backptr);
    const xmlChar *objname = obj_get_name(obj);

    ncx_backptr_t *testptr = ncx_first_backptr(&agt_rpc_methodQ);
    for (; testptr; testptr = ncx_next_backptr(testptr)) {

        obj_template_t *testobj =
            (obj_template_t *)ncx_get_backptr_node(testptr);
        const xmlChar *testname = obj_get_name(testobj);

        int32 ret = xml_strcmp(objname, testname);
        if (ret < 0) {
            dlq_insertAhead(backptr, testptr);
            return;
        } else if (ret == 0) {
            const xmlChar *modname = obj_get_mod_name(obj);
            const xmlChar *testmodname = obj_get_mod_name(testobj);
            ret = xml_strcmp(modname, testmodname);
            if (ret < 0) {
                dlq_insertAhead(backptr, testptr);
                return;
            } else if (ret == 0) {
                log_warn("\nWarning: backptr %s:%s "
                         "entry already exists in Q",
                         modname, objname);
                dlq_insertAhead(backptr, testptr);
                return;
            }
        }
    }
    dlq_enque(backptr, &agt_rpc_methodQ);

}  /* add_method_backptr */


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
* FUNCTION send_rpc_error_info_xml
*
* Send one <error-info> element on the specified session
* print all the rpc_err_info_t records as child nodes
*
* 
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   err == error record to use for an error_info Q
*   indent == indent amount
*   use_nsid == namespace ID to use for error-info element
*
* RETURNS:
*   none
*********************************************************************/
static void
    send_rpc_error_info_xml (ses_cb_t *scb,
                             xml_msg_hdr_t  *msg,
                             const rpc_err_rec_t *err,
                             int32  indent,
                             xmlns_id_t use_nsid)
{
    /* check if there is any error_info data */
    if (dlq_empty(&err->error_info)) {
        return;
    }

    /* init local vars */
    xmlns_id_t nsid = xmlns_nc_id();

#ifdef WITH_YANGAPI
    if (scb->protocol == NCX_PROTO_YUMA_YANGAPI) {
        nsid = agt_yangapi_get_nsid();
    }
#endif

    /* generate the <error-info> start tag */
    xml_wr_begin_elem_ex(scb, msg, nsid, use_nsid, NCX_EL_ERROR_INFO, 
                         NULL, FALSE, indent, START);

    int32 indent_amount = ses_message_indent_count(scb);
    indent = ses_new_indent_count(TRUE, indent, indent_amount);

    /* generate each child variable */
    rpc_err_info_t *errinfo;
    for (errinfo = (rpc_err_info_t *)
             dlq_firstEntry(&err->error_info);
         errinfo != NULL;
         errinfo = (rpc_err_info_t *)dlq_nextEntry(errinfo)) {

        switch (errinfo->val_btype) {
        case NCX_BT_STRING:
        case NCX_BT_INSTANCE_ID:
        case NCX_BT_LEAFREF:
            if (errinfo->v.strval) {
                if (errinfo->isqname) {
                    xml_wr_qname_elem(scb, msg, errinfo->val_nsid,
                                      errinfo->v.strval, nsid,
                                      errinfo->name_nsid, errinfo->name,
                                      NULL, FALSE, indent, FALSE);
                } else {
                    xml_wr_string_elem(scb, msg,  errinfo->v.strval, nsid,
                                       errinfo->name_nsid, errinfo->name,
                                       NULL, FALSE, indent);
                }
            } else {
                SET_ERROR(ERR_INTERNAL_PTR);
            }
            break;
        case NCX_BT_DECIMAL64:
        case NCX_BT_INT8:
        case NCX_BT_INT16:
        case NCX_BT_INT32:
        case NCX_BT_INT64:
        case NCX_BT_UINT8:
        case NCX_BT_UINT16:
        case NCX_BT_UINT32:
        case NCX_BT_UINT64:
        case NCX_BT_FLOAT64:
            {
                uint32 len = 0;
                xmlChar numbuff[NCX_MAX_NUMLEN];
                status_t res =
                    ncx_sprintf_num(numbuff, &errinfo->v.numval,
                                    errinfo->val_btype, &len);
                if (res == NO_ERR) {
                    xml_wr_string_elem(scb, msg, numbuff, nsid,
                                       errinfo->name_nsid,
                                       errinfo->name, NULL, FALSE, indent);
                } else {
                    SET_ERROR(res);
                }
            }
            break;
        case NCX_BT_ANY:
        case NCX_BT_BITS:
        case NCX_BT_ENUM:
        case NCX_BT_EMPTY:
        case NCX_BT_BOOLEAN:
        case NCX_BT_BINARY:
        case NCX_BT_UNION:
        case NCX_BT_INTERN:
        case NCX_BT_EXTERN:
        case NCX_BT_IDREF:
        case NCX_BT_SLIST:
            // not supported
            SET_ERROR(ERR_INTERNAL_VAL);
            break;
        default:
            if (typ_is_simple(errinfo->val_btype)) {
                SET_ERROR(ERR_INTERNAL_VAL);
            } else if (errinfo->v.cpxval) {
                xml_wr_full_val(scb, msg, errinfo->v.cpxval, indent);
            } else {
                SET_ERROR(ERR_INTERNAL_PTR);
            }
        }
    }

    indent = ses_new_indent_count(FALSE, indent, indent_amount);

    /* generate the <error-info> end tag */
    xml_wr_end_elem(scb, msg, use_nsid, NCX_EL_ERROR_INFO, indent);

}  /* send_rpc_error_info_xml */


/********************************************************************
* FUNCTION send_rpc_error_info_json
*
* Send one <error-info> element on the specified session
* print all the rpc_err_info_t records as child nodes
*
* 
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   err == error record to use for an error_info Q
*   indent == indent amount
*   use_nsid == namespace ID to use for error-info element
*
* RETURNS:
*   none
*********************************************************************/
static void
    send_rpc_error_info_json (ses_cb_t *scb,
                              xml_msg_hdr_t  *msg,
                              const rpc_err_rec_t *err,
                              int32  indent,
                              xmlns_id_t use_nsid)
{
    /* check if there is any error_info data */
    if (dlq_empty(&err->error_info)) {
        return;
    }

    /* hack, know this was not the first element in the error object,
     * so finish the previous line with a comma char
     */
    ses_putchar(scb, ',');   // TBD: move to API

    /* generate the <error-info> start tag */
    json_wr_start_object_ex(scb, msg, NULL, NCX_EL_ERROR_INFO, indent, FALSE);

    int32 indent_amount = ses_message_indent_count(scb);
    indent = ses_new_indent_count(TRUE, indent, indent_amount);

    /* generate each child variable */
    boolean isfirst = TRUE;

    rpc_err_info_t *next_errinfo = NULL;
    rpc_err_info_t *errinfo;
    for (errinfo = (rpc_err_info_t *)dlq_firstEntry(&err->error_info);
         errinfo != NULL; errinfo = next_errinfo) {

        next_errinfo = (rpc_err_info_t *)dlq_nextEntry(errinfo);
        switch (errinfo->val_btype) {
        case NCX_BT_STRING:
        case NCX_BT_INSTANCE_ID:
        case NCX_BT_LEAFREF:
            if (errinfo->v.strval) {
                json_wr_simval_line(scb, msg, errinfo->val_nsid,
                                    errinfo->name, errinfo->v.strval,
                                    errinfo->val_btype, indent, isfirst);
            } else {
                SET_ERROR(ERR_INTERNAL_PTR);
            }
            break;
        case NCX_BT_DECIMAL64:
        case NCX_BT_INT8:
        case NCX_BT_INT16:
        case NCX_BT_INT32:
        case NCX_BT_INT64:
        case NCX_BT_UINT8:
        case NCX_BT_UINT16:
        case NCX_BT_UINT32:
        case NCX_BT_UINT64:
        case NCX_BT_FLOAT64:
            {
                uint32 len = 0;
                xmlChar numbuff[NCX_MAX_NUMLEN];
                status_t res =
                    ncx_sprintf_num(numbuff, &errinfo->v.numval,
                                    errinfo->val_btype, &len);
                if (res == NO_ERR) {
                    json_wr_simval_line(scb, msg, errinfo->val_nsid,
                                        errinfo->name, numbuff,
                                        errinfo->val_btype, indent, isfirst);
                } else {
                    SET_ERROR(res);
                }
            }
            break;
        case NCX_BT_ANY:
        case NCX_BT_BITS:
        case NCX_BT_ENUM:
        case NCX_BT_EMPTY:
        case NCX_BT_BOOLEAN:
        case NCX_BT_BINARY:
        case NCX_BT_UNION:
        case NCX_BT_INTERN:
        case NCX_BT_EXTERN:
        case NCX_BT_IDREF:
        case NCX_BT_SLIST:
            // not supported
            SET_ERROR(ERR_INTERNAL_VAL);
            break;
        default:
            if (typ_is_simple(errinfo->val_btype)) {
                SET_ERROR(ERR_INTERNAL_VAL);
            } else if (errinfo->v.cpxval) {
                json_wr_max_check_val(scb, msg, use_nsid, errinfo->v.cpxval,
                                      indent, NULL, FALSE, isfirst,
                                      (next_errinfo == NULL),
                                      TRUE, TRUE, TRUE, FALSE);
            } else {
                SET_ERROR(ERR_INTERNAL_PTR);
            }
        }

        isfirst = FALSE;
    }

    indent = ses_new_indent_count(FALSE, indent, indent_amount);

    /* generate the <error-info> end tag */
    json_wr_end_object(scb, msg, indent);

}  /* send_rpc_error_info_json */


/********************************************************************
* FUNCTION check_add_changed_since_attr
*
* Check if the changed-since attribute should be added to the
* message attributes
* 
* INPUTS:
*   msg == rpc_msg_t in progress
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    check_add_changed_since_attr (rpc_msg_t *msg)
{
    if (msg->rpc_method == NULL) {
        return NO_ERR;
    }

    xmlns_id_t ncid = xmlns_nc_id();

    if (obj_get_nsid(msg->rpc_method) != ncid) {
        /* not the NETCONF module */
        return NO_ERR;
    }

    const xmlChar *rpcname = obj_get_name(msg->rpc_method);

    /* check if <get> or <get-config> */
    if (!xml_strcmp(rpcname, NCX_EL_GET) ||
        !xml_strcmp(rpcname, NCX_EL_GET_CONFIG)) {

        /* !!! this code is coupled to agt_ncx.c get_validate
         * !!! and get_config_validate. Those functions set
         * !!! the rpc_user1 field to the config target if
         * !!! everything went well.  Only add last-modified
         * !!! if rpc_user1 is set
         */
        if (msg->rpc_user1 == NULL) {
            return NO_ERR;
        }
        cfg_template_t *cfg = (cfg_template_t *)msg->rpc_user1;

        /* check if this attr already present in rpc_in_attrs
         * This cannot be generated an an unqualified attribute
         * or it will be interpreted as belonging to the
         * NETCONF namespace; put in NCX namepsace
         */
        xml_attr_t *attr = 
            xml_find_attr_q(msg->rpc_in_attrs, 
                            xmlns_ncx_id(), NCX_EL_LAST_MODIFIED);
        if (attr != NULL) {
            return NO_ERR;
        }

        /* make a new attr and add it to rpc_in_attrs */
        status_t res = 
            xml_add_attr(msg->rpc_in_attrs, 
                         xmlns_ncx_id(), NCX_EL_LAST_MODIFIED,
                         cfg->last_ch_time);

        return res;
    }

    return NO_ERR;

}  /* check_add_changed_since_attr */


/********************************************************************
* FUNCTION send_rpc_reply
*
* Operation succeeded or failed
* Return an <rpc-reply>
* 
* INPUTS:
*   scb == session control block
*   msg == rpc_msg_t in progress
*
*********************************************************************/
static void
    send_rpc_reply (ses_cb_t *scb,
                    rpc_msg_t  *msg)
{
    status_t res = ses_start_msg(scb);
    if (res != NO_ERR) {
        return;
    }

    ses_start_msg_mode(scb);

    ses_total_stats_t *agttotals = ses_get_total_stats();
    boolean errsend = !dlq_empty(&msg->mhdr.errQ);

    res = xml_msg_clean_defns_attr(msg->rpc_in_attrs);

    if (res == NO_ERR) {
        res = check_add_changed_since_attr(msg);
    }

    if (res == NO_ERR) { 
        res = xml_msg_gen_xmlns_attrs(&msg->mhdr, msg->rpc_in_attrs, errsend);
    }

    if (res != NO_ERR) {
        ses_finish_msg(scb);
        ses_stop_msg_mode(scb);
        return;
    }

    xmlns_id_t ncid = xmlns_nc_id();
    xmlns_id_t rpcid;
    if (msg->rpc_method) {
        rpcid = obj_get_nsid(msg->rpc_method);
    } else {
        rpcid = ncid;
    }

    int32 indent = ses_message_indent_count(scb);

    /* generate the <rpc-reply> start tag */
    xml_wr_begin_elem_ex(scb, &msg->mhdr, 0, ncid, NCX_EL_RPC_REPLY, 
                         msg->rpc_in_attrs, ATTRQ, min(indent, 0), START);

    /* check if there is data to send */
    boolean datasend = (!dlq_empty(&msg->rpc_dataQ) 
                        || msg->rpc_datacb) ? TRUE : FALSE;

    /* check which reply variant needs to be sent */
    if (dlq_empty(&msg->mhdr.errQ) && !datasend) {
        /* no errors and no data, so send <ok> variant */
        xml_wr_empty_elem(scb, &msg->mhdr, ncid, ncid, NCX_EL_OK, indent);
    } else {
        /* send rpcResponse variant
         * 0 or <rpc-error> elements followed by
         * 0 to N  data elements, specified in the foo-rpc/output
         *
         * first send all the buffered errors
         */
        boolean isfirst = TRUE;
        boolean islast = FALSE;

        const rpc_err_rec_t *err =
            (const rpc_err_rec_t *)dlq_firstEntry(&msg->mhdr.errQ);
        const rpc_err_rec_t *nexterr = NULL;
        for (; err != NULL; err = nexterr) {

            nexterr = (const rpc_err_rec_t *)dlq_nextEntry(err);
            islast = (!nexterr);

            res = agt_rpc_send_rpc_error(scb, &msg->mhdr, err, indent,
                                         isfirst, islast);
            if (res != NO_ERR) {
                SET_ERROR(res);
            }
            isfirst = FALSE;
        }

        /* check generation of data contents
         * if the element
         * If the rpc_dataQ is non-empty, then there is
         * staticly filled data to send
         * If the rpc_datacb function pointer is non-NULL,
         * then this is dynamic reply content, even if the rpc_data
         * node is not NULL
         */
        if (datasend) {
            if (msg->rpc_data_type == RPC_DATA_STD) {
                xml_wr_begin_elem(scb, &msg->mhdr, ncid, rpcid, NCX_EL_DATA, 
                                  indent);
                if (indent > 0) {
                    int32 indent_amount = ses_message_indent_count(scb);
                    if (indent_amount > 0) {
                        indent += indent_amount;
                    }
                }
            }

            uint64 outbytes = SES_OUT_BYTES(scb);

            if (msg->rpc_datacb) {
                /* use user callback to generate the contents */
                agt_rpc_data_cb_t agtcb = (agt_rpc_data_cb_t)msg->rpc_datacb;
                res = (*agtcb)(scb, msg, indent);
                if (res == ERR_NCX_SKIPPED) {
                    log_debug2("\nagt_rpc: SIL callback returned "
                               "ERR_NCX_SKIPPED");
                    res = NO_ERR;
                }
                if (res != NO_ERR) {
                    log_error("\nError: SIL data callback failed (%s)",
                              get_error_string(res));
                }
            } else {
                /* just write the contents of the rpc <data> varQ */
                val_value_t *val;
                for (val = (val_value_t *)
                         dlq_firstEntry(&msg->rpc_dataQ);
                     val != NULL;
                     val = (val_value_t *)dlq_nextEntry(val)) {

                    xml_wr_full_val(scb, &msg->mhdr, val, indent);
                }
            }

            /* only indent the </data> end tag if any data written */
            boolean datawritten = (outbytes != SES_OUT_BYTES(scb));

            if (msg->rpc_data_type == RPC_DATA_STD) {
                if (indent > 0) {
                    indent -= ses_message_indent_count(scb);
                }
                xml_wr_end_elem(scb, &msg->mhdr, rpcid, NCX_EL_DATA, 
                                (datawritten) ? indent : -1);
            }
        }
    } 

    /* generate the <rpc-reply> end tag */
    xml_wr_end_elem(scb, &msg->mhdr, ncid, NCX_EL_RPC_REPLY, min(indent, 0));

    /* finish the message */
    ses_finish_msg(scb);
    ses_stop_msg_mode(scb);

    if (errsend) {
        scb->stats.inBadRpcs++;
        agttotals->stats.inBadRpcs++;
        scb->stats.outRpcErrors++;
        agttotals->stats.outRpcErrors++;
    } else {
        scb->stats.inRpcs++;
        agttotals->stats.inRpcs++;
    }

}  /* send_rpc_reply */


/********************************************************************
* FUNCTION find_rpc
*
* Find the specified rpc_template_t
*
* INPUTS:
*   modname == module name containing the RPC to find
*   rpcname == RPC method name to find
*
* RETURNS:
*   pointer to retrieved obj_template_t or NULL if not found
*********************************************************************/
static obj_template_t *
    find_rpc (const xmlChar *modname,
              const xmlChar *rpcname)
{
    ncx_module_t    *mod;
    obj_template_t  *rpc;

    /* look for the RPC method in the definition registry */
    rpc = NULL;
    mod = ncx_find_module(modname, NULL);
    if (mod) {
        rpc = ncx_find_rpc(mod, rpcname);
    }
    return rpc;

}  /* find_rpc */


/********************************************************************
* FUNCTION load_config_file
*
* Dispatch an internal <load-config> request
* used for OP_EDITOP_LOAD to load the running from startup
* and OP_EDITOP_REPLACE to restore running from backup
*
*    - Create a dummy session and RPC message
*    - Create a config transaction control block
*    - Call a special agt_val_parse function to parse the config file
*    - Call the special agt_ncx function to invoke the proper 
*      parmset and application 'validate' callback functions, and 
*      record all the error/warning messages
*    - Call the special agt_ncx function to invoke all the 'apply'
*      callbacks as needed
*    - transfer any error messages to the cfg->load_errQ
*    - Dispose the transaction CB (do not send to audit)
* INPUTS:
*   filespec == XML config filespec to load
*   cfg == cfg_template_t to fill in
*   isload == TRUE for normal load-config
*             FALSE for restore backup load-config
*   use_sid == session ID to use for the access control
*   justval == TRUE if just the <config> element is needed
*   configval == address of return config val
*   errorQ == address of Q to hold errors if justval is TRUE
*
* OUTPUTS:
*    *configval set to the processed config node
*       if NO_ERR, it has been removed from the rpc_input
*       data structure and needs to be freed by the caller
*    errorQ contains any rpc_err_rec_t structs that were
*       generated during this RPC operation
*
* RETURNS:
*     status
*********************************************************************/
static status_t
    load_config_file (const xmlChar *filespec,
                      cfg_template_t  *cfg,
                      boolean isload,
                      ses_id_t  use_sid,
                      boolean justval,
                      val_value_t **configval,
                      dlq_hdr_t *errorQ)
{
    /* first make sure the load-config RPC is registered */
    obj_template_t *rpcobj = find_rpc(NC_MODULE, NCX_EL_LOAD_CONFIG);
    if (!rpcobj) {
        return SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* make sure the agent callback set for this RPC is ok */
    agt_rpc_cbset_t *cbset = (agt_rpc_cbset_t *)rpcobj->cbset;
    if (!cbset) {
        return SET_ERROR(ERR_INTERNAL_VAL);
    }

    /* make sure no transaction already in progress unless it is a 
     * rollback by the system (SID==0)    */
    if (agt_cfg_txid_in_progress(NCX_CFGID_RUNNING) && use_sid) {
        return SET_ERROR(ERR_INTERNAL_INIT_SEQ);
    }

    /* create a dummy session control block */
    ses_cb_t *scb = agt_ses_new_dummy_session();
    if (!scb) {
        return ERR_INTERNAL_MEM;
    }

    status_t retres = NO_ERR;

    /* set the ACM profile to use for rollback */
    status_t res = agt_ses_set_dummy_session_acm(scb, use_sid);
    if (res != NO_ERR) {
        agt_ses_free_dummy_session(scb);        
        return res;
    }

    /* create a dummy RPC msg */
    rpc_msg_t *msg = rpc_new_msg();
    if (!msg) {
        agt_ses_free_dummy_session(scb);
        return ERR_INTERNAL_MEM;
    }

    /* setup the config file as the xmlTextReader input */
    res = xml_get_reader_from_filespec((const char *)filespec, &scb->reader);
    if (res != NO_ERR) {
        free_msg(msg);
        agt_ses_free_dummy_session(scb);
        return res;
    }

    msg->rpc_in_attrs = NULL;

    /* create a dummy method XML node */
    xml_node_t  method;
    xml_init_node(&method);
    method.nodetyp = XML_NT_START;
    method.nsid = xmlns_nc_id();
    method.module = NC_MODULE;

    method.qname = xml_strdup(NCX_EL_LOAD_CONFIG);
    if (method.qname == NULL) {
        free_msg(msg);
        agt_ses_free_dummy_session(scb);
        return ERR_INTERNAL_MEM;
    }
        
    method.elname = NCX_EL_LOAD_CONFIG;
    method.depth = 1;

    msg->rpc_method = rpcobj;
    msg->rpc_user1 = cfg;
    msg->rpc_err_option = OP_ERROP_CONTINUE;
    if (isload) {
        msg->rpc_top_editop = OP_EDITOP_LOAD;
    } else {
        msg->rpc_top_editop = OP_EDITOP_REPLACE;
    }

    /* parse the config file as a root object */
    res = agt_rpc_parse_rpc_input(scb, msg, rpcobj, &method);
    if (res != NO_ERR) {
        retres = res;
    }
    if (!(NEED_EXIT(res) || res==ERR_XML_READER_EOF)) {
        /* keep going if there were errors in the input
         * in case more errors can be found or 
         * continue-on-error is in use
         * Each value node has a status field (val->res)
         * which will retain the error status 
         */
        res = agt_rpc_post_psd_state(scb, msg, res);
        if (res != NO_ERR) {
            retres = res;
        }
    }

    if (retres != NO_ERR) {
        agt_profile_t *profile = agt_get_profile();
        if (NEED_EXIT(retres)) {
            res = retres; // force exit
            log_error("\nError: Parse <load-config> failed with fatal errors");
        } else if (profile->agt_startup_error) {
            res = retres; // force exit
            log_error("\nError: Parse <load-config> failed and "
                      "--startup-error=stop");
        } else {
            log_warn("\nWarning: Parse <load-config> failed and "
                     "--startup-error=continue");
            msg->rpc_parse_errors = TRUE;
            retres = NO_ERR;
        }
    }

    if (res == NO_ERR && !isload) {
        agt_val_clean_cached_results();
    }

    /* call all the object validate callbacks
     * but only if there were no parse errors so far
     */
    boolean valdone = FALSE;
    if (res == NO_ERR && cbset->acb[AGT_RPC_PH_VALIDATE]) {
        msg->rpc_agt_state = AGT_RPC_PH_VALIDATE;
        res = (*cbset->acb[AGT_RPC_PH_VALIDATE])(scb, msg, &method);
        if (res != NO_ERR) {
            retres = res;
        } else {
            valdone = TRUE;
        }
    }

    /* get rid of the error nodes after the validation and instance
     * checks are done; must checks and unique checks should also
     * be done by now,
     * Also set the canonical order for the root node
     */
    if (valdone) {
        val_value_t *testval = val_find_child(msg->rpc_input, NULL, 
                                              NCX_EL_CONFIG);
        if (testval) {
            //val_purge_errors_from_root(testval); !! already done
            val_set_canonical_order(testval);

            if (justval) {
                /* remove this node and return it */
                val_remove_child(testval);
                *configval = testval;
            }
        }
    }

    /* call all the object invoke callbacks, only callbacks for valid
     * subtrees will be called; skip if valonly mode
     */
    if (!justval && valdone && cbset->acb[AGT_RPC_PH_INVOKE]) {
        msg->rpc_agt_state = AGT_RPC_PH_INVOKE;
        res = (*cbset->acb[AGT_RPC_PH_INVOKE])(scb, msg, &method);
        if (res != NO_ERR) {
            /*** DO NOT ROLLBACK, JUST CONTINUE !!!! ***/
            retres = res;
        }
    }

    if (LOGDEBUG) {
        if (!dlq_empty(&msg->mhdr.errQ)) {
            rpc_err_dump_errors(msg);
        }
    }

    /* move any error messages to the config error Q */
    dlq_block_enque(&msg->mhdr.errQ, errorQ);

    /* cleanup and exit */
    xml_clean_node(&method);
    free_msg(msg);
    agt_ses_free_dummy_session(scb);

    return retres;

} /* load_config_file */


/********************************************************************
* FUNCTION send_rpc_error_xml
*
* Send one <rpc-error> element on the specified session
* XML encoding
*
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   err == error record to send
*   indent == starting indent count
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    send_rpc_error_xml (ses_cb_t *scb,
                        xml_msg_hdr_t *msg,
                        const rpc_err_rec_t *err,
                        int32 indent)
{
    /* init local vars */
    status_t res = NO_ERR;
    status_t retres = NO_ERR;
    xmlns_id_t nsid = xmlns_nc_id();

#ifdef WITH_YANGAPI
    if (scb->protocol == NCX_PROTO_YUMA_YANGAPI) {
        nsid = agt_yangapi_get_nsid();
    }
#endif

    /* check if any XMLNS decls need to be added to 
     * the <rpc-error> element                */
    xml_attrs_t attrs;
    xml_init_attrs(&attrs);

    /* asetup the return XML attributes; will get ignored
     * if the encoding is JSON                     */
    rpc_err_info_t *errinfo;
    for (errinfo = (rpc_err_info_t *)dlq_firstEntry(&err->error_info);
         errinfo != NULL;
         errinfo = (rpc_err_info_t *)dlq_nextEntry(errinfo)) {
        res = xml_msg_check_xmlns_attr(msg, errinfo->name_nsid,
                                       errinfo->badns, &attrs);
        if (res == NO_ERR) {
            res = xml_msg_check_xmlns_attr(msg, errinfo->val_nsid,
                                           errinfo->badns, &attrs);
        }
        if (res != NO_ERR) {
            SET_ERROR(res);  /***/
            retres = res;
        }
    }

    const xmlChar *top_name = (scb->protocol == NCX_PROTO_YUMA_YANGAPI)
        ? NCX_EL_ERROR : NCX_EL_RPC_ERROR;

    /* generate the <rpc-error> or <error> start tag */
    xml_wr_begin_elem_ex(scb, msg, nsid, nsid, top_name, &attrs, 
                         ATTRQ, indent, START);

    xml_clean_attrs(&attrs);

    int32 indent_amount = ses_message_indent_count(scb);
    indent = ses_new_indent_count(TRUE, indent, indent_amount);

    /* generate the <error-type> field */
    xml_wr_string_elem(scb, msg, ncx_get_layer(err->error_type),
                       nsid, nsid, NCX_EL_ERROR_TYPE, NULL, FALSE, indent);

    /* generate the <error-tag> field */
    xml_wr_string_elem(scb, msg, err->error_tag, nsid, nsid,
                       NCX_EL_ERROR_TAG, NULL, FALSE, indent);

    if (scb->protocol != NCX_PROTO_YUMA_YANGAPI) {
        /* generate the <error-severity> field */
        xml_wr_string_elem(scb, msg, rpc_err_get_severity(err->error_severity), 
                           nsid, nsid, NCX_EL_ERROR_SEVERITY, NULL, FALSE,
                           indent);
    }

    /* generate the <error-app-tag> field */
    if (err->error_app_tag) {
        xml_wr_string_elem(scb, msg, err->error_app_tag, nsid, nsid, 
                           NCX_EL_ERROR_APP_TAG, NULL, FALSE, indent);
    } else if (err->error_res != NO_ERR) {
        /* use the internal error code instead */
        xmlChar buff[12];
        *buff = 0;
        snprintf((char *)buff, sizeof(buff), "%u", err->error_res);
        if (*buff) {
            xml_wr_string_elem(scb, msg, buff, nsid, nsid,
                               NCX_EL_ERROR_APP_TAG, NULL, FALSE, indent);
        }
    }

    /* generate the <error-path> field */
    if (err->error_path) {
        xml_wr_string_elem(scb, msg, err->error_path, nsid, nsid,
                           NCX_EL_ERROR_PATH, NULL, FALSE, indent);
    }

    /* generate the <error-message> field */
    if (err->error_message) {
        /* see if there is a xml:lang attribute */
        if (err->error_message_lang) {
            res = xml_add_attr(&attrs, 0, NCX_EL_LANG,
                               err->error_message_lang);
            if (res != NO_ERR) {
                SET_ERROR(res);
                retres = res;
            }
        }
        xml_wr_string_elem(scb, msg, err->error_message, nsid, nsid,
                           NCX_EL_ERROR_MESSAGE, &attrs, TRUE, indent);
        xml_clean_attrs(&attrs);
    }

    /* print all the <error-info> elements */    
    send_rpc_error_info_xml(scb, msg,  err, indent, nsid);

    indent = ses_new_indent_count(FALSE, indent, indent_amount);

    /* generate the <rpc-error> or <error> end tag */
    xml_wr_end_elem(scb, msg, nsid, top_name, indent);

    return retres;

}  /* send_rpc_error_xml */


/********************************************************************
* FUNCTION send_rpc_error_json
*
* Send one <rpc-error> element on the specified session
* JSON encoding
*
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   err == error record to send
*   indent == starting indent count
*
*********************************************************************/
static void
    send_rpc_error_json (ses_cb_t *scb,
                         xml_msg_hdr_t *msg,
                         const rpc_err_rec_t *err,
                         int32 indent,
                         boolean isfirst,
                         boolean islast)
{
    /* init local vars */
    boolean yangapi_mode = FALSE;
    xmlns_id_t nsid = xmlns_nc_id();

#ifdef WITH_YANGAPI
    yangapi_mode = (scb->protocol == NCX_PROTO_YUMA_YANGAPI);
    if (yangapi_mode) {
        nsid = agt_yangapi_get_nsid();
    }
#endif

    const xmlChar *top_name = NULL;

    if (isfirst) {
        top_name = (yangapi_mode) ? NCX_EL_ERROR : NCX_EL_RPC_ERROR;

        /* generate the <rpc-error> or <error> start tag */
        json_wr_start_object(scb, msg, NULL, top_name, indent);
        json_wr_start_array(scb, msg, -1);
    }

    int32 indent_amount = ses_message_indent_count(scb);
    indent = ses_new_indent_count(TRUE, indent, indent_amount);
    indent = ses_new_indent_count(TRUE, indent, indent_amount);

    // start the error object
    ses_indent(scb, indent);
    ses_putchar(scb, '{');

    indent = ses_new_indent_count(TRUE, indent, indent_amount);

    /* generate the <error-type> field */
    json_wr_simval_line(scb, msg, nsid, NCX_EL_ERROR_TYPE,
                        ncx_get_layer(err->error_type),
                        NCX_BT_STRING, indent, TRUE);

    /* generate the <error-tag> field */
    json_wr_simval_line(scb, msg, nsid, NCX_EL_ERROR_TAG,
                        err->error_tag, NCX_BT_STRING, indent, FALSE);

    if (scb->protocol != NCX_PROTO_YUMA_YANGAPI) {
        /* generate the <error-severity> field */
        json_wr_simval_line(scb, msg, nsid, NCX_EL_ERROR_SEVERITY,
                            rpc_err_get_severity(err->error_severity), 
                            NCX_BT_STRING, indent, FALSE);
    }

    /* generate the <error-app-tag> field */
    if (err->error_app_tag) {
        json_wr_simval_line(scb, msg, nsid, NCX_EL_ERROR_APP_TAG,
                            err->error_app_tag, NCX_BT_STRING,
                            indent, FALSE);
    } else if (err->error_res != NO_ERR) {
        /* use the internal error code instead */
        xmlChar buff[12];
        *buff = 0;
        snprintf((char *)buff, sizeof(buff), "%u", err->error_res);
        if (*buff) {
            json_wr_simval_line(scb, msg, nsid, NCX_EL_ERROR_APP_TAG,
                                buff, NCX_BT_STRING, indent, FALSE);
        }
    }

    /* generate the <error-path> field */
    if (err->error_path) {
        json_wr_simval_line(scb, msg, nsid, NCX_EL_ERROR_PATH,
                            err->error_path, NCX_BT_STRING, indent, FALSE);
    }

    /* generate the <error-message> field */
    if (err->error_message) {

        /* see if there is a xml:lang attribute */
        if (err->error_message_lang) {
            // TBD: support attributes in JSON
        }

        json_wr_simval_line(scb, msg, nsid, NCX_EL_ERROR_MESSAGE,
                            err->error_message, NCX_BT_STRING, indent, FALSE);

    }

    /* print all the <error-info> elements */    
    send_rpc_error_info_json(scb, msg,  err, indent, nsid);

    // end the error object
    indent = ses_new_indent_count(FALSE, indent, indent_amount);
    json_wr_end_object(scb, msg, indent);

    indent = ses_new_indent_count(FALSE, indent, indent_amount);
    json_wr_end_array(scb, msg, indent);

    if (islast) {
        indent = ses_new_indent_count(FALSE, indent, indent_amount);

        /* generate the <rpc-error> or <error> end tag */
        json_wr_end_object(scb, msg, indent);
    }

}  /* send_rpc_error_json */


/************** E X T E R N A L   F U N C T I O N S  ***************/

/********************************************************************
* FUNCTION agt_rpc_init
*
* Initialize the agt_rpc module
* Adds the agt_rpc_dispatch function as the handler
* for the NETCONF <rpc> top-level element.
* should call once to init RPC agent module
*
* INPUTS:
*   none
* RETURNS:
*   NO_ERR if all okay, the minimum spare requests will be malloced
*********************************************************************/
status_t 
    agt_rpc_init (void)
{
    status_t  res;

    if (!agt_rpc_init_done) {
        dlq_createSQue(&agt_rpc_methodQ);
        res = top_register_node(NC_MODULE, NCX_EL_RPC, agt_rpc_dispatch);
        if (res != NO_ERR) {
            return res;
        }

        agt_rpc_init_done = TRUE;
    }
    return NO_ERR;

} /* agt_rpc_init */


/********************************************************************
* FUNCTION agt_rpc_cleanup
*
* Cleanup the agt_rpc module.
* Unregister the top-level NETCONF <rpc> element
* should call once to cleanup RPC agent module
*
*********************************************************************/
void 
    agt_rpc_cleanup (void)
{
    if (agt_rpc_init_done) {
        ncx_clean_backptrQ(&agt_rpc_methodQ);
        top_unregister_node(NC_MODULE, NCX_EL_RPC);
        agt_rpc_init_done = FALSE;
    }

} /* agt_rpc_cleanup */


/********************************************************************
* FUNCTION agt_rpc_register_method
*
* add callback for 1 phase of RPC processing 
*
* INPUTS:
*    module == module name or RPC method
*    method_name == RPC method name
*    phase == RPC agent callback phase for this callback
*    method == pointer to callback function
*
* RETURNS:
*    status of the operation
*********************************************************************/
status_t 
    agt_rpc_register_method (const xmlChar *module,
                             const xmlChar *method_name,
                             agt_rpc_phase_t  phase,
                             agt_rpc_method_t method)
{
    obj_template_t  *rpcobj;
    agt_rpc_cbset_t *cbset;

#ifdef DEBUG
    if (!module || !method_name || !method) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
    if (phase >= AGT_RPC_NUM_PHASES) {
        return SET_ERROR(ERR_INTERNAL_VAL);
    }
#endif

    /* find the RPC template */
    rpcobj = find_rpc(module, method_name);
    if (!rpcobj) {
        return SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
    }

    boolean backptr_todo = TRUE;
    ncx_backptr_t *backptr = find_method_backptr(rpcobj);
    if (backptr) {
        backptr_todo = FALSE;
    } else {
        backptr = ncx_new_backptr(rpcobj);
        if (backptr == NULL) {
            return ERR_INTERNAL_MEM;
        }
    }

    /* get the agent CB set, or create a new one */
    if (rpcobj->cbset) {
        cbset = (agt_rpc_cbset_t *)rpcobj->cbset;
    } else {
        cbset = m__getObj(agt_rpc_cbset_t);
        if (!cbset) {
            if (backptr_todo) {
                ncx_free_backptr(backptr);
            }
            return ERR_INTERNAL_MEM;
        }
        memset(cbset, 0x0, sizeof(agt_rpc_cbset_t));
        rpcobj->cbset = (void *)cbset;
    }
    rpcobj->def.rpc->supported = TRUE;

    /* add the specified method */
    cbset->acb[phase] = method;

    if (backptr_todo) {
        add_method_backptr(backptr);
    }

    return NO_ERR;

} /* agt_rpc_register_method */


/********************************************************************
* FUNCTION agt_rpc_support_method
*
* mark an RPC method as supported within the agent
* this is needed for operations dependent on capabilities
*
* INPUTS:
*    module == module name of RPC method (really module name)
*    method_name == RPC method name
*********************************************************************/
void 
    agt_rpc_support_method (const xmlChar *module,
                            const xmlChar *method_name)
{
    obj_template_t  *rpcobj;

#ifdef DEBUG
    if (!module || !method_name) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    /* find the RPC template */
    rpcobj = find_rpc(module, method_name);
    if (!rpcobj) {
        SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
        return;
    }

    rpcobj->def.rpc->supported = TRUE;

    ncx_backptr_t *backptr = find_method_backptr(rpcobj);
    if (backptr == NULL) {
        backptr = ncx_new_backptr(rpcobj);
        if (backptr == NULL) {
            log_error("\nError: could not add rpc backptr %s",
                      obj_get_name(rpcobj));
        } else {
            add_method_backptr(backptr);
        }
    }

} /* agt_rpc_support_method */


/********************************************************************
* FUNCTION agt_rpc_unsupport_method
*
* mark an RPC method as unsupported within the agent
* this is needed for operations dependent on capabilities
*
* INPUTS:
*    module == module name of RPC method (really module name)
*    method_name == RPC method name
*********************************************************************/
void 
    agt_rpc_unsupport_method (const xmlChar *module,
                              const xmlChar *method_name)
{
    obj_template_t  *rpcobj;

#ifdef DEBUG
    if (!module || !method_name) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    /* find the RPC template */
    rpcobj = find_rpc(module, method_name);
    if (!rpcobj) {
        SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
        return;
    }

    rpcobj->def.rpc->supported = FALSE;

    ncx_backptr_t *backptr = find_method_backptr(rpcobj);
    if (backptr) {
        ncx_remove_backptr(backptr);
        ncx_free_backptr(backptr);
    }

} /* agt_rpc_unsupport_method */


/********************************************************************
* FUNCTION agt_rpc_unregister_method
*
* remove the callback functions for all phases of RPC processing 
* for the specified RPC method
*
* INPUTS:
*    module == module name of RPC method (really module name)
*    method_name == RPC method name
*********************************************************************/
void 
    agt_rpc_unregister_method (const xmlChar *module,
                               const xmlChar *method_name)
{
    obj_template_t       *rpcobj;

#ifdef DEBUG
    if (!module || !method_name) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    /* find the RPC template */
    rpcobj = find_rpc(module, method_name);
    if (!rpcobj) {
        SET_ERROR(ERR_NCX_DEF_NOT_FOUND);
        return;
    }

    /* get the agent CB set and delete it */
    if (rpcobj->cbset) {
        m__free(rpcobj->cbset);
        rpcobj->cbset = NULL;
    }

    ncx_backptr_t *backptr = find_method_backptr(rpcobj);
    if (backptr) {
        ncx_remove_backptr(backptr);
        ncx_free_backptr(backptr);
    }

} /* agt_rpc_unregister_method */


/********************************************************************
* FUNCTION agt_rpc_dispatch
*
* Dispatch an incoming <rpc> request
* called by top.c: 
* This function is registered with top_register_node
* for the module 'yuma-netconf', top-node 'rpc'
*
* INPUTS:
*   scb == session control block
*   top == top element descriptor
*********************************************************************/
void 
    agt_rpc_dispatch (ses_cb_t *scb,
                      xml_node_t *top)
{
    assert( scb && "scb is NULL!" );
    assert( top && "top is NULL!" );

    /* init local vars */
    status_t res = NO_ERR;
    status_t res2 = NO_ERR;
    agt_rpc_cbset_t *cbset = NULL;
    obj_template_t *rpcobj = NULL;
    ses_total_stats_t *agttotals = ses_get_total_stats();

    /* make sure any real session has been properly established */
    if (scb->type != SES_TYP_DUMMY && scb->state != SES_ST_IDLE) {
        res = ERR_NCX_ACCESS_DENIED;
        if (LOGINFO) {
            log_info("\nagt_rpc dropping session %d (%d) %s",
                     scb->sid, 
                     res, 
                     get_error_string(res));
        }
        agttotals->stats.inBadRpcs++;
        agttotals->droppedSessions++;
        agt_ses_request_close(scb, scb->sid, SES_TR_DROPPED);

        return;
    }

    /* the current node is 'rpc' in the netconf namespace
     * First get a new RPC message struct
     */
    rpc_msg_t *msg = rpc_new_msg();
    if (!msg) {
        res = ERR_INTERNAL_MEM;
        agttotals->droppedSessions++;
        if (LOGINFO) {
            log_info("\nError: malloc failed, dropping session %d (%d) %s",
                     scb->sid, res, get_error_string(res));
        }
        agt_ses_request_close(scb, scb->sid, SES_TR_DROPPED);
        return;
    }

    /* make sure 'rpc' is the right kind of node */
    if (top->nodetyp != XML_NT_START) {
        res = ERR_NCX_WRONG_NODETYP;
        scb->stats.inBadRpcs++;
        agttotals->stats.inBadRpcs++;
        agttotals->droppedSessions++;
        if (LOGINFO) {
            log_info("\nagt_rpc dropping session %d (%d) %s",
                     scb->sid, 
                     res, 
                     get_error_string(res));
        }
        agt_ses_request_close(scb, scb->sid, SES_TR_OTHER);
        free_msg(msg);
        return;
    }

    /* setup the struct as an incoming RPC message
     * borrow the top->attrs queue without copying it 
     * The rpc-reply phase may add attributes to this list
     * that the caller will free ater this function returns
     */
    msg->rpc_in_attrs = &top->attrs;

    /* set the default for the with-defaults parameter */
    xml_msg_set_withdef(&msg->mhdr, ses_withdef(scb));

    /* get the NC RPC message-id attribute; must be present */
    xml_attr_t *attr = xml_find_attr(top, 0, NCX_EL_MESSAGE_ID);

#ifndef IGNORE_STRICT_RFC4741
    if (!attr || !attr->attr_val) {
        xml_attr_t             errattr;

        res2 = ERR_NCX_MISSING_ATTRIBUTE;
        memset(&errattr, 0x0, sizeof(xml_attr_t));
        errattr.attr_ns = xmlns_nc_id();
        errattr.attr_name = NCX_EL_MESSAGE_ID;
        errattr.attr_val = (xmlChar *)NULL;
        agt_record_attr_error(scb, 
                              &msg->mhdr, 
                              NCX_LAYER_RPC,
                              res2, 
                              &errattr, 
                              top, 
                              NULL, 
                              NCX_NT_STRING,
                              RPC_ROOT);
    }
#endif

    /* analyze the <rpc> element and populate the 
     * initial namespace prefix map for the <rpc-reply> message
     */
    res = xml_msg_build_prefix_map(&msg->mhdr, 
                                   msg->rpc_in_attrs, 
                                   TRUE, 
                                   !dlq_empty(&msg->mhdr.errQ));

    /* init agt_acm cache struct even though it may not
     * get used if the RPC method is invalid
     */
    if (res == NO_ERR) {
        res = agt_acm_init_msg_cache(scb, &msg->mhdr);
    }

    if (res != NO_ERR) {
        agt_record_error(scb, 
                         &msg->mhdr, 
                         NCX_LAYER_RPC, 
                         res, 
                         top, 
                         NCX_NT_NONE, 
                         NULL, 
                         NCX_NT_NONE, 
                         NULL);
    }

    /* check any errors in the <rpc> node */
    if (res != NO_ERR || res2 != NO_ERR) {
        send_rpc_reply(scb, msg);
        agt_acm_clear_msg_cache(&msg->mhdr);
        free_msg(msg);
        return;
    }
    
    /* get the next XML node, which is the RPC method name */
    xml_node_t method;
    xml_init_node(&method);
    res = agt_xml_consume_node(scb, &method, NCX_LAYER_RPC, &msg->mhdr);
    boolean errdone;
    if (res != NO_ERR) {
        errdone = TRUE;
    } else {
        errdone = FALSE;

        /* reset idle timeout */
        (void)time(&scb->last_rpc_time);

        if (LOGDEBUG) {
            const xmlChar *msgid;
            xmlChar tstampbuff[TSTAMP_MIN_SIZE];
            tstamp_datetime(tstampbuff);
            if (attr != NULL && attr->attr_val != NULL) {
                msgid = attr->attr_val;
            } else {
                msgid = NCX_EL_NONE;
            }
	    log_debug("\nagt_rpc: <%s> for %u=%s@%s (m:%s) [%s]", 
                      method.elname,
                      scb->sid,
                      scb->username ? scb->username : NCX_EL_NONE,
                      scb->peeraddr ? scb->peeraddr : NCX_EL_NONE,
                      msgid,
                      tstampbuff);
            if (LOGDEBUG2) {
                xml_dump_node(&method);
            }
        }

        /* check the node type which should be type start or simple */
        if (!(method.nodetyp==XML_NT_START || 
              method.nodetyp==XML_NT_EMPTY)) {
            res = ERR_NCX_WRONG_NODETYP;
        } else {
            /* look for the RPC method in the definition registry */
            rpcobj = find_rpc(method.module, method.elname);
            msg->rpc_method = rpcobj;

            res = agt_rpc_check_rpc_invoke(scb, msg, rpcobj);
            if (res == NO_ERR) {
                /* get the agent callback set for this RPC 
                 * if NULL, no agent instrumentation has been 
                 * registered yet for this RPC method
                 */
                cbset = (agt_rpc_cbset_t *)rpcobj->cbset;
            }
        }
    }

    /* check any errors in the RPC method node */
    if (res != NO_ERR && !errdone) {
        xmlChar *buff = NULL;
        if (res != ERR_NCX_ACCESS_DENIED) {
            /* construct an error-path string */
            const xmlChar *pfix = NULL;
            if (method.nsid) {
                pfix = xmlns_get_ns_prefix(method.nsid);
            }
            uint32 pfixlen = pfix ? xml_strlen(pfix) + 1 : 0;

            buff = m__getMem(pfixlen + xml_strlen(method.elname) 
                             + xml_strlen(RPC_ROOT) + 2);
            if (buff) {
                xmlChar *p = buff;
                p += xml_strcpy(p, RPC_ROOT);
                *p++ = '/';
                if (pfix) {
                    p += xml_strcpy(p, pfix);
                    *p++ = ':';
                    status_t testres = 
                        xml_msg_add_ncid_to_prefix_map(&msg->mhdr,
                                                       msg->rpc_in_attrs,
                                                       method.nsid);
                    if (testres != NO_ERR) {
                        log_error("\nError: add prefix to map failed (%s)",
                                  get_error_string(testres));
                    }
                }
                xml_strcpy(p, method.qname);
            }
        }

        /* passing a NULL buff is okay; it will get checked 
          * The NCX_NT_STRING node type enum is ignored if 
         * the buff pointer is NULL
         */
        agt_record_error(scb, &msg->mhdr, NCX_LAYER_RPC, res, &method, 
                         NCX_NT_NONE, NULL, 
                         (buff) ? NCX_NT_STRING : NCX_NT_NONE, buff);
        if (buff) {
            m__free(buff);
        }
        send_rpc_reply(scb, msg);
        agt_acm_clear_msg_cache(&msg->mhdr);
        free_msg(msg);
        xml_clean_node(&method);
        return;
    }

    /* change the session state */
    scb->state = SES_ST_IN_MSG;

    /* parameter set parse state */
    if (res == NO_ERR) {
        res = agt_rpc_parse_rpc_input(scb, msg, rpcobj, &method);
    }

    /* read in a node which should be the endnode to match 'top' */
    xml_node_t testnode;
    xml_init_node(&testnode);
    if (res == NO_ERR) {
        res = agt_xml_consume_node(scb, &testnode, NCX_LAYER_RPC, &msg->mhdr);
    }
    if (res == NO_ERR) {
        if (LOGDEBUG2) {
            log_debug2("\nagt_rpc: expecting %s end node", 
                       top->qname);
        }
        if (LOGDEBUG3) {
            xml_dump_node(&testnode);
        }
        res = xml_endnode_match(top, &testnode);
    }
    xml_clean_node(&testnode);

    /* check that there is nothing after the <rpc> element */
    if (res == NO_ERR && !xml_docdone(scb->reader)) {

        /* get all the extra nodes and report the errors */
        res = NO_ERR;
        while (res==NO_ERR) {
            /* do not add errors such as unknown namespace */
            res = agt_xml_consume_node(scb, &testnode, NCX_LAYER_NONE, NULL);
            if (res==NO_ERR) {
                res = ERR_NCX_UNKNOWN_ELEMENT;
                char *errstr = (char *)xml_strdup((const xmlChar *)RPC_ROOT);
                agt_record_error(scb, 
                                 &msg->mhdr, 
                                 NCX_LAYER_RPC, 
                                 res, 
                                 &method, 
                                 NCX_NT_NONE, 
                                 NULL, 
                                 (errstr) ? NCX_NT_STRING : NCX_NT_NONE,
                                 errstr);
                if (errstr) {
                    m__free(errstr);
                }
            }
        }

        /* reset the error status */
        res = ERR_NCX_UNKNOWN_ELEMENT;
    } 
    xml_clean_node(&testnode);

    /* check the defaults and any choices if there is clean input */
    if (res == NO_ERR && obj_rpc_has_input(rpcobj)) {
        res = agt_rpc_post_psd_state(scb, msg, res);
    }

    /* execute the validate and invoke callbacks */
    if (res == NO_ERR) {
        res = agt_rpc_invoke_rpc(scb, msg, &method);
    }

    /* make sure the prefix map is correct for report-all-tagged mode
     * OK to skip this if there was an error exit */
    if (res == NO_ERR) {
        res = xml_msg_finish_prefix_map(&msg->mhdr, msg->rpc_in_attrs);
    }

    /* always send an <rpc-reply> element in response to an <rpc> */
    msg->rpc_agt_state = AGT_RPC_PH_REPLY;
    send_rpc_reply(scb, msg);

    /* check if there is a post-reply callback;
     * call even if the RPC failed
     */
    if (cbset && cbset->acb[AGT_RPC_PH_POST_REPLY]) {
        msg->rpc_agt_state = AGT_RPC_PH_POST_REPLY;
        (void)(*cbset->acb[AGT_RPC_PH_POST_REPLY])(scb, msg, &method);
    }

    /* check if there is any auditQ because changes to 
     * the running config were made
     */
    if (msg->rpc_txcb && !dlq_empty(&msg->rpc_txcb->auditQ)) {
        agt_sys_send_sysConfigChange(scb, &msg->rpc_txcb->auditQ);
    }

    /* only reset the session state to idle if was not changed
     * to SES_ST_SHUTDOWN_REQ during this RPC call
     */
    if (scb->state == SES_ST_IN_MSG) {
        scb->state = SES_ST_IDLE;
    }

    /* cleanup and exit */
    xml_clean_node(&method);
    agt_acm_clear_msg_cache(&msg->mhdr);
    free_msg(msg);

    print_errors();
    clear_errors();

} /* agt_rpc_dispatch */


/********************************************************************
* FUNCTION agt_rpc_load_config_file
*
* Dispatch an internal <load-config> request
* used for OP_EDITOP_LOAD to load the running from startup
* and OP_EDITOP_REPLACE to restore running from backup
*
*    - Create a dummy session and RPC message
*    - Call a special agt_ps_parse function to parse the config file
*    - Call the special agt_ncx function to invoke the proper 
*      parmset and application 'validate' callback functions, and 
*      record all the error/warning messages
*    - Call the special ncx_agt function to invoke all the 'apply'
*      callbacks as needed
*    - transfer any error messages to the cfg->load_errQ
*
* INPUTS:
*   filespec == XML config filespec to load
*   cfg == cfg_template_t to fill in
*   isload == TRUE for normal load-config
*             FALSE for restore backup load-config
*   use_sid == session ID to use for the access control
*
* RETURNS:
*     status
*********************************************************************/
status_t
    agt_rpc_load_config_file (const xmlChar *filespec,
                              cfg_template_t  *cfg,
                              boolean isload,
                              ses_id_t  use_sid)
{
#ifdef DEBUG
    if (!filespec || !cfg) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    status_t res = 
        load_config_file(filespec, cfg, isload, use_sid, FALSE, NULL,
                         &cfg->load_errQ);
    cfg_clear_running_dirty_flag();

    return res;

} /* agt_rpc_load_config_file */


/********************************************************************
* FUNCTION agt_rpc_get_config_file
*
* Dispatch an internal <load-config> request
* except skip the INVOKE phase and just remove
* the 'config' node from the input and return it
*
*    - Create a dummy session and RPC message
*    - Call a special agt_ps_parse function to parse the config file
*    - Call the special agt_ncx function to invoke the proper 
*      parmset and application 'validate' callback functions, and 
*      record all the error/warning messages
*    - return the <config> element if no errors
*    - otherwise return all the error messages in a Q
*
* INPUTS:
*   filespec == XML config filespec to get
*   targetcfg == target database to validate against
*   use_sid == session ID to use for the access control
*   errorQ == address of return queue of rpc_err_rec_t structs
*   res == address of return status
*
* OUTPUTS:
*   if any errors, the error structs are transferred to
*   the errorQ (if it is non-NULL).  In this case, the caller
*   must free these data structures with ncx/rpc_err_clean_errQ
* 
*   *res == return status
*
* RETURNS:
*   malloced and filled in struct representing a <config> element
*   NULL if some error, check errorQ and *res
*********************************************************************/
val_value_t *
    agt_rpc_get_config_file (const xmlChar *filespec,
                             cfg_template_t *targetcfg,
                             ses_id_t  use_sid,
                             dlq_hdr_t *errorQ,
                             status_t *res)
{
#ifdef DEBUG
    if (!filespec || !errorQ) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    val_value_t *retval = NULL;
    *res = load_config_file(filespec, targetcfg, FALSE, use_sid,  TRUE,
                            &retval, errorQ);

    return retval;

} /* agt_rpc_get_config_file */


/********************************************************************
* FUNCTION agt_rpc_fill_rpc_error
*
* Fill one <rpc-error> like element using the specified
* namespace and name, which may be different than NETCONF
*
* INPUTS:
*   err == error record to use to fill 'rpcerror'
*   rpcerror == NCX_BT_CONTAINER value struct already
*               initialized.  The val_add_child function
*               will use this parm as the parent.
*               This namespace will be used for all
*               child nodes
*
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_rpc_fill_rpc_error (const rpc_err_rec_t *err,
                            val_value_t *rpcerror)
{
#ifdef DEBUG
    if (!rpcerror || !err) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    /* init local vars */
    status_t res = NO_ERR;
    status_t retres = NO_ERR;

    /* add the <error-type> field */
    val_value_t *leafval = 
        agt_make_leaf(rpcerror->obj, NCX_EL_ERROR_TYPE,
                      ncx_get_layer(err->error_type), &res);
    if (leafval) {
        val_add_child(leafval, rpcerror);
    } else {
        retres = res;
    }

    /* add the <error-tag> field */
    leafval = agt_make_leaf(rpcerror->obj, NCX_EL_ERROR_TAG,
                            err->error_tag, &res);
    if (leafval) {
        val_add_child(leafval, rpcerror);
    } else {
        retres = res;
    }

    /* add the <error-severity> field */
    leafval = agt_make_leaf(rpcerror->obj, NCX_EL_ERROR_SEVERITY,
                            rpc_err_get_severity(err->error_severity),  &res);
    if (leafval) {
        val_add_child(leafval, rpcerror);
    } else {
        retres = res;
    }
                            
    /* generate the <error-app-tag> field */
    leafval = NULL;
    if (err->error_app_tag) {
        leafval = agt_make_leaf(rpcerror->obj, NCX_EL_ERROR_APP_TAG,
                                err->error_app_tag, &res);
        if (leafval) {
            val_add_child(leafval, rpcerror);
        } else {
            retres = res;
        }
    }

    /* generate the <error-path> field */
    if (err->error_path) {
        /* since the error nodes may already be deleted,
         * suppress the warning in xpath1.c about no child node
         * found   */
        boolean savewarn = ncx_warning_enabled(ERR_NCX_NO_XPATH_CHILD);

        if (savewarn) {
            (void)ncx_turn_off_warning(ERR_NCX_NO_XPATH_CHILD);
        }

        leafval = agt_make_leaf(rpcerror->obj, NCX_EL_ERROR_PATH,
                                err->error_path, &res);

        if (leafval) {
            val_add_child(leafval, rpcerror);
        } else {
            retres = res;
        }

        if (savewarn) {
            (void)ncx_turn_on_warning(ERR_NCX_NO_XPATH_CHILD);
        }
    }

    /* generate the <error-message> field */
    if (err->error_message) {
        leafval = agt_make_leaf(rpcerror->obj, NCX_EL_ERROR_MESSAGE,
                                err->error_message, &res);
        if (leafval) {
            val_add_child(leafval, rpcerror);
        } else {
            retres = res;
        }
    }

    /* TBD: add the error info */    

    return retres;

}  /* agt_rpc_fill_rpc_error */


/********************************************************************
* FUNCTION agt_rpc_send_error_reply
*
* Operation failed or was never attempted
* Return an <rpc-reply> with an <rpc-error>
* 
* INPUTS:
*   scb == session control block
*   retres == error number for termination  reason
*
*********************************************************************/
void
    agt_rpc_send_error_reply (ses_cb_t *scb,
                              status_t retres)
{
    status_t res = ses_start_msg(scb);
    if (res != NO_ERR) {
        return;
    }

    ses_start_msg_mode(scb);

    xmlChar *error_path = NULL;
    ncx_layer_t layer;
    if (retres == ERR_NCX_INVALID_FRAMING) {
        layer = NCX_LAYER_TRANSPORT;
    } else {
        layer = NCX_LAYER_RPC;
        error_path = xml_strdup(RPC_ROOT);
        /* ignore malloc error; send as much as possible */
    }

    rpc_err_rec_t *err = 
        agt_rpcerr_gen_error(layer, retres, NULL, NCX_NT_NONE,
                             NULL, error_path);
    if (err == NULL) {
        m__free(error_path);
        error_path = NULL;
    }

    ses_total_stats_t *agttotals = ses_get_total_stats();

    xml_attrs_t attrs;
    xml_init_attrs(&attrs);

    xml_msg_hdr_t mhdr;
    xml_msg_init_hdr(&mhdr);

    res = xml_msg_gen_xmlns_attrs(&mhdr, &attrs, TRUE);
    if (res != NO_ERR) {
        if (err != NULL) {
            rpc_err_free_record(err);
        }
        ses_finish_msg(scb);
        xml_clean_attrs(&attrs);
        xml_msg_clean_hdr(&mhdr);
        return;
    }

    xmlns_id_t ncid = xmlns_nc_id();
    int32 indent = ses_message_indent_count(scb);

    /* generate the <rpc-reply> start tag */
    xml_wr_begin_elem_ex(scb, &mhdr, 0, ncid, NCX_EL_RPC_REPLY, 
                         &attrs, ATTRQ, min(indent, 0), START);

    if (err != NULL) {
        res = agt_rpc_send_rpc_error(scb, &mhdr, err, indent, TRUE, TRUE);
    } else {
        log_error("\nError: could not send error reply for session %u",
                  SES_MY_SID(scb));
    }

    /* generate the <rpc-reply> end tag */
    xml_wr_end_elem(scb, &mhdr, ncid, NCX_EL_RPC_REPLY, min(indent, 0));

    /* finish the message */
    ses_finish_msg(scb);
    ses_stop_msg_mode(scb);

    scb->stats.inBadRpcs++;
    agttotals->stats.inBadRpcs++;
    scb->stats.outRpcErrors++;
    agttotals->stats.outRpcErrors++;

    if (err != NULL) {
        rpc_err_free_record(err);
    }
    xml_clean_attrs(&attrs);
    xml_msg_clean_hdr(&mhdr);

}  /* agt_rpc_send_error_reply */


/********************************************************************
* FUNCTION agt_rpc_send_rpc_error
*
* Send one <rpc-error> element on the specified session
* 
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   err == error record to send
*   indent == starting indent count
*   isfirst == TRUE if first error (JSON only)
*   islast == TRUE if last error (JSON only)
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_rpc_send_rpc_error (ses_cb_t *scb,
                            xml_msg_hdr_t *msg,
                            const rpc_err_rec_t *err,
                            int32 indent,
                            boolean isfirst,
                            boolean islast)
{
    ncx_display_mode_t display_mode = ses_get_out_encoding(scb);
    status_t res = NO_ERR;

    switch (display_mode) {
    case NCX_DISPLAY_MODE_XML:
    case NCX_DISPLAY_MODE_XML_NONS:
        res = send_rpc_error_xml(scb, msg, err, indent);
        break;
    case NCX_DISPLAY_MODE_JSON:
        send_rpc_error_json(scb, msg, err, indent, isfirst, islast);
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    return res;

}  /* agt_rpc_send_rpc_error */


/********************************************************************
* FUNCTION agt_rpc_get_first_backptr
*
* Get the first backptr to the registered and supported
* RPC methods on the server
* 
* RETURNS:
*   pointer to first backptr (node is obj_template_t *)
*********************************************************************/
ncx_backptr_t *
    agt_rpc_get_first_backptr (void)
{
    return ncx_first_backptr(&agt_rpc_methodQ);
}

/********************************************************************
* FUNCTION agt_rpc_get_next_backptr
*
* Get the next backptr to the registered and supported
* RPC methods on the server
*
* INPUTS:
*    curptr == current backptr to use
* RETURNS:
*   pointer to next backptr (node is obj_template_t *)
*********************************************************************/
ncx_backptr_t *
    agt_rpc_get_next_backptr (ncx_backptr_t *curptr)
{
    return ncx_next_backptr(curptr);
}


/********************************************************************
* FUNCTION agt_rpc_check_rpc_invoke
*
* Some RPC node parsed, check if it can be invoked or not
* 
* INPUTS:
*   scb == session control block
*   msg == rpc_msg_t in progress
    rpcobj == RPC object template to use (may be NULL)
*
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_rpc_check_rpc_invoke (ses_cb_t *scb,
                              rpc_msg_t  *msg,
                              obj_template_t *rpcobj)
{
    if (!rpcobj) {
        return ERR_NCX_DEF_NOT_FOUND;
    }

    if (!obj_is_supported(rpcobj)) {
        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    if (scb->username && 
        !agt_acm_rpc_allowed(&msg->mhdr, scb->username, rpcobj)) {
        return ERR_NCX_ACCESS_DENIED;
    }

    return NO_ERR;

} /* agt_rpc_check_rpc_invoke */


/********************************************************************
* FUNCTION agt_rpc_parse_rpc_input
*
* RPC received, parse parameters against rpcio for 'input'
* 
* INPUTS:
*   scb == session control block
*   msg == rpc_msg_t in progress
    rpcobj == RPC object template to use
*   method == method node
*
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_rpc_parse_rpc_input (ses_cb_t *scb,
                             rpc_msg_t  *msg,
                             obj_template_t *rpcobj,
                             xml_node_t  *method)
{
    status_t res = NO_ERR;

    /* check if there is an input parmset specified */
    obj_template_t *obj =
        obj_find_template(obj_get_datadefQ(rpcobj), NULL, YANG_K_INPUT);

    if (obj && obj_get_child_count(obj)) {
        msg->rpc_agt_state = AGT_RPC_PH_PARSE;
        res = agt_val_parse_nc(scb, &msg->mhdr, obj, method, NCX_DC_CONFIG, 
                               msg->rpc_input);

        if (LOGDEBUG3) {
            log_debug3("\nagt_rpc: parse RPC input state\n");
            rpc_err_dump_errors(msg);

            val_dump_value_max(msg->rpc_input, 1, NCX_DEF_INDENT,
                           DUMP_VAL_LOG,
                           NCX_DISPLAY_MODE_MODULE,
                           TRUE, /* with_meta */
                           FALSE); /* configonly */
            log_debug3_append("\n");
        }
    }

    return res;

}  /* agt_rpc_parse_rpc_input */


/********************************************************************
* FUNCTION agt_rpc_post_psd_state
*
* Fixup parmset after parse phase
* Only call if the RPC has an input PS
*
* INPUTS:
*   scb == session control block
*   msg == rpc_msg_t in progress
*   psdres == result from PSD state
*
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_rpc_post_psd_state (ses_cb_t *scb,
                            rpc_msg_t  *msg,
                            status_t  psdres)
{
    status_t  res;

    res = NO_ERR;

    /* at this point the input is completely parsed and
     * the syntax is valid.  Now add any missing defaults
     * to the RPC Parmset. 
     */
    if (psdres == NO_ERR) {
        /*** need to keep errors for a bit so this function
         *** can be called.  Right now, child errors are not
         *** added to the value tree, so val_add_defaults
         *** will add a default where there was an error before
         ***/
        res = val_add_defaults(msg->rpc_input, NULL, NULL, FALSE);
    }

    if (res == NO_ERR) {
        /* check for any false when-stmts on nodes 
         * in the rpc/input section and flag them
         * as unexpected-element errors
         */
        res = agt_val_rpc_xpath_check(scb, msg, &msg->mhdr, msg->rpc_input,
                                      msg->rpc_method);
    }

    if (res == NO_ERR) {
        /* check that the number of instances of the parameters
         * is reasonable and only one member from any choice is
         * present (if applicable)
         *
         * Make sure to check choice before instance because
         * any missing choices would be incorrectly interpreted
         * as multiple missing parameter errors
         */
        res = agt_val_instance_check(scb, &msg->mhdr, msg->rpc_input, 
                                     msg->rpc_input, NCX_LAYER_OPERATION);
    }

    return res;

}  /* agt_rpc_post_psd_state */


/********************************************************************
* FUNCTION agt_rpc_invoke_rpc
*
* Invoke an RPC -- need to call following functions first:
*
*  agt_rpc_check_rpc_invoke
*  agt_rpc_parse_rpc_input (if any input)
*  agt_rpc_post_psd_state (if any input expected)
*
* INPUTS:
*   scb == session control block
*   msg == rpc_msg_t in progress
*   method_node == XML node of the <rpc> start tag (NULL if none)
*
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_rpc_invoke_rpc (ses_cb_t *scb,
                        rpc_msg_t  *msg,
                        xml_node_t *method_node)
{
    obj_template_t *rpcobj = msg->rpc_method;
    if (rpcobj == NULL) {
        return SET_ERROR(ERR_INTERNAL_VAL);
    }

    agt_rpc_cbset_t *cbset = (agt_rpc_cbset_t *)rpcobj->cbset;
    if (cbset == NULL) {
        log_debug("\nagt_rpc: No cbset for RPC '%s', skipping it",
                  obj_get_name(rpcobj));
        return NO_ERR;
    }

        
    xml_node_t mynode;
    boolean clean_node = FALSE;

    if (method_node == NULL) {
        /* need to contruct a method node */
        const xmlChar *name = obj_get_name(rpcobj);

        xml_init_node(&mynode);
        mynode.nodetyp = XML_NT_START;
        mynode.nsid = obj_get_nsid(rpcobj);
        mynode.module = obj_get_mod_name(rpcobj);
        mynode.qname = xml_strdup(name);
        if (mynode.qname == NULL) {
            xml_clean_node(&mynode);
            return ERR_INTERNAL_MEM;
        }
        mynode.elname = name;
        mynode.depth = 1;
        method_node = &mynode;
        clean_node = TRUE;
    }
        
    status_t res = NO_ERR;

    /* validate state */
    if (cbset->acb[AGT_RPC_PH_VALIDATE]) {
        /* input passes the basic YANG schema tests at this point;
         * check if there is a validate callback for
         * referential integrity, locks, resource reservation, etc.
         * Only the top level data node has been checked for instance
         * qualifer usage.  The caller must do data node instance
         * validataion in this VALIDATE callback, as needed
         */
        msg->rpc_agt_state = AGT_RPC_PH_VALIDATE;
        res = (*cbset->acb[AGT_RPC_PH_VALIDATE])(scb, msg, method_node);
        if (res != NO_ERR) {
            /* make sure there is an error recorded in case
             * the validate phase callback did not add one
             */
            if (!rpc_err_any_errors(msg)) {
                agt_record_error(scb, 
                                 &msg->mhdr, 
                                 NCX_LAYER_RPC, 
                                 res, 
                                 method_node, 
                                 NCX_NT_NONE, 
                                 NULL, 
                                 NCX_NT_OBJ,
                                 rpcobj);
            }
        }
    }

    /* there does not always have to be an invoke callback,
     * especially since the return of data can be automated
     * in the send_rpc_reply phase. 
     */
    if ((res==NO_ERR) && cbset->acb[AGT_RPC_PH_INVOKE]) {
        msg->rpc_agt_state = AGT_RPC_PH_INVOKE;
        res = (*cbset->acb[AGT_RPC_PH_INVOKE])(scb, msg, method_node);
        if (res != NO_ERR) {
            /* make sure there is an error recorded in case
             * the invoke phase callback did not add one
             */
            if (!rpc_err_any_errors(msg)) {
                agt_record_error(scb, 
                                 &msg->mhdr, 
                                 NCX_LAYER_RPC, 
                                 res, 
                                 method_node,
                                 NCX_NT_NONE, 
                                 NULL, 
                                 NCX_NT_OBJ,
                                 rpcobj);
            }
        }
    }

    if (clean_node) {
        xml_clean_node(&mynode);
    }

    return res;

}  /* agt_rpc_invoke_rpc */


/* END file agt_rpc.c */
