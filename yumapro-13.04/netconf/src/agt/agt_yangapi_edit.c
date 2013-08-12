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
/*  FILE: agt_yangapi_edit.c

   YumaPro REST API Edit Handler for YANG-API protocol
 
   See draft-bierman-netconf-yang-api-01.txt for details.

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
//#define AGT_YANGAPI_EDIT_DEBUG 1
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
* FUNCTION setup_edit_request_editvars
*
* Use the edit-config information to construct an
* internal editvars struct so agt_val will process the
* edit request;
*
*  normally done by agt_val_parse.c: parse_metadata_nc
*
* INPUTS:
*   scb == session to use
*   rcb == yangapi control block to use
*   msg == response message in progress
*   targval == pointer to target value that needs the
*              editvars struct
*
* OUTPUTS:
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    setup_edit_request_editvars (ses_cb_t *scb,
                                 yangapi_cb_t *rcb,
                                 rpc_msg_t *msg,
                                 val_value_t *targval)
{
    status_t res = val_new_editvars(targval);
    if (res != NO_ERR) {
        return res;
    }

    targval->editop = rcb->editop;
    targval->editvars->operset = TRUE;
    targval->editvars->insertop = rcb->query_insert;

    if (rcb->query_point) {
        targval->editvars->insertstr = xml_strdup(rcb->query_point);
        if (targval->editvars->insertstr == NULL) {
            return ERR_INTERNAL_MEM;
        }

        targval->editvars->insert_mode = VAL_INS_MODE_POINT;

        agt_profile_t *profile = agt_get_profile();

        /* convert the UrlPath string to an Xpath string */
        xmlChar *pointurl =
            xpath_convert_url_to_path(rcb->query_point,
                                      profile->agt_match_names,
                                      profile->agt_alt_names,
                                      FALSE,      /* wildcards */
                                      TRUE,       /* withkeys */
                                      &res);
        if (pointurl == NULL || res != NO_ERR) {
            if (res == ERR_NCX_DEF_NOT_FOUND) {
                res = ERR_NCX_RESOURCE_UNKNOWN;
            }
            agt_yangapi_record_error(scb, msg, res, NULL, rcb->query_point);
            m__free(pointurl);
            return res;
        }

        targval->editvars->insertxpcb = xpath_new_pcb(NULL, NULL);
        if (targval->editvars->insertxpcb == NULL) {
            res = ERR_INTERNAL_MEM;
            agt_yangapi_record_error(scb, msg, res, NULL, NULL);
            m__free(pointurl);
            return res;
        }

        /* pass off pointurl memory here */
        targval->editvars->insertxpcb->exprstr = pointurl;
    }

    return NO_ERR;

}  /* setup_edit_request_editvars */


/********************************************************************
* FUNCTION setup_edit_request_config
*
* Use the rcb information to construct a <config> node
* that can be processed by the agt_val database editing code.
* Setup target and edit_model as well
*
* INPUTS:
*   scb == session to use
*   rcb == yangapi control block to use
*   msg == response message in progress
*   retval == address of return configval
*
* OUTPUTS:
*   *retval = <config> node setup for the edit
*   rcb->editop set -- used for debugging
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    setup_edit_request_config (ses_cb_t *scb,
                               yangapi_cb_t *rcb,
                               rpc_msg_t *msg,
                               val_value_t **retval)
{
    *retval = NULL;
    op_editop_t editop = OP_EDITOP_NONE;
    status_t res = NO_ERR;

    /* figure out what edit operation to use */
    switch (rcb->method) {
    case YANGAPI_METHOD_POST:
        editop = OP_EDITOP_CREATE;
        break;
    case YANGAPI_METHOD_PUT:
        editop = OP_EDITOP_REPLACE;
        break;
    case YANGAPI_METHOD_PATCH:
        editop = OP_EDITOP_MERGE;
        break;
    case YANGAPI_METHOD_DELETE:
        editop = OP_EDITOP_DELETE;
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (res == NO_ERR && msg->rpc_input == NULL) {
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (res != NO_ERR) {
        agt_yangapi_record_error(scb, msg, res, NULL, NULL);
        return res;
    }

    /* set the editop */
    rcb->editop = editop;

    /* create a <config> wrapper container value */
    val_value_t *wrapperval = val_new_value();
    if (wrapperval == NULL) {
        res = ERR_INTERNAL_MEM;
        agt_yangapi_record_error(scb, msg, res, NULL, NULL);
        return res;
    }

    val_value_t *targval = cfg_get_root(NCX_CFGID_RUNNING);
    val_init_from_template(wrapperval, targval->obj);
    targval = NULL;

    /* create a path-from-root value tree for the edit operation */
    val_value_t *targroot =
        xpath_yang_make_instanceid_val(rcb->request_xpath, &res, &targval);
    if (res != NO_ERR) {
        val_free_value(wrapperval);
        agt_yangapi_record_error(scb, msg, res, NULL, NULL);
        return res;
    }

    /* need to replace the targval with the rpc_input val */
    if (targroot == targval) {
        if (rcb->content_len == 0 ||
            (rcb->method == YANGAPI_METHOD_DELETE && 
             obj_is_list(targroot->obj))) {
            // use node with keys built-in; ignore msg->rpc_input 
        } else {
            /* just use rpc_input instead of the contructed node */
            val_free_value(targroot);
            targval = targroot = msg->rpc_input;
            msg->rpc_input = NULL;
        }
    } else if (rcb->content_len) {
        /* targroot is a separate object, swap the targval */
        val_swap_child(msg->rpc_input, targval);
        val_free_value(targval);
        targval = msg->rpc_input;
        msg->rpc_input = NULL;
    }

    /* make the edit payload a child of the <config> node */
    val_add_child(targroot, wrapperval);
    targroot = NULL;

    /* add the editvars struct so agt_val will edit the config OK */
    res = setup_edit_request_editvars(scb, rcb, msg, targval);

    if (res != NO_ERR) {
        val_free_value(wrapperval);
        agt_yangapi_record_error(scb, msg, res, NULL, NULL);
        return res;
    }

    if (LOGDEBUG3) {
        int32 indent = ses_indent_count(scb);
        if (indent < 1) {
            indent = 1;
        }
        log_debug3("\n\nagt_yangapi: <config> content:\n");
        val_dump_value_max(wrapperval, indent, indent * 2,
                           DUMP_VAL_LOG,
                           NCX_DISPLAY_MODE_MODULE,
                           TRUE, /* with_meta */
                           FALSE); /* configonly */
        log_debug3_append("\n");
    }
                       
    *retval = wrapperval;
    return NO_ERR;

}  /* setup_edit_request_config */


/********************************************************************
 * FUNCTION check_conditional_method
 *
 * Check if there are any condition headers in the request
 * If so then check then all against the method and target resource
 *
 * INPUTS:
 *   rcb == YANG-API control block to use
 *
 * RETURNS:
 *   TRUE if condition passes or no conditions or does not apply to
 *   the method and launchpoint
 *   FALSE if there is a condition that is not met that causes
 *   the request to be rejected
 *********************************************************************/
static boolean
    check_conditional_method (yangapi_cb_t *rcb)
{
    if (agt_yangapi_method_is_read(rcb->method)) {
        return TRUE;
    }

    if (rcb->if_match == NULL && rcb->if_modified_since == NULL &&
        rcb->if_none_match == NULL && rcb->if_unmodified_since == NULL) {
        return TRUE;
    }

    val_value_t *testnode = NULL;

    switch (rcb->request_launchpt) {
    case YANGAPI_LAUNCHPT_NEW_DATA:
        return FALSE;
    case YANGAPI_LAUNCHPT_DATA:
        testnode = rcb->request_target;
        break;
    case YANGAPI_LAUNCHPT_DATASTORE:
        testnode = cfg_get_root(NCX_CFGID_RUNNING);
        break;
    default:
        return TRUE;
    }

    if (testnode) {
        boolean test_match = FALSE;
        if (rcb->if_match) {
            test_match = agt_match_etag(testnode, rcb->if_match);
            if (!test_match) {
                return FALSE;
            }
        } else if (rcb->if_none_match) {
            test_match = agt_match_etag(testnode, rcb->if_none_match);
            if (test_match) {
                return FALSE;
            }
        }

        if (rcb->if_modified_since || rcb->if_unmodified_since) {
            test_match = agt_modified_since(testnode, &rcb->query_tstamp);
            if (rcb->if_modified_since) {
                if (!test_match) {
                    return FALSE;
                }
            } else {
                if (test_match) {
                    return FALSE;
                }
            }
        }
    }

    return TRUE;

}  /* check_conditional_method */


/************** E X T E R N A L   F U N C T I O N S  ***************/


/********************************************************************
* FUNCTION agt_yangapi_edit_request
*
* Perform an internal <edit-config> and <commit> if needed
* Also write result to NV-storage if separate :startup config
*
* INPUTS:
*   scb == session to use
*   rcb == yangapi control block to use
*   msg == response message in progress
*
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_yangapi_edit_request (ses_cb_t *scb,
                              yangapi_cb_t *rcb,
                              rpc_msg_t *msg)
{
    /* get the <config> element for the edit operation */
    val_value_t *configval = NULL;
    status_t res = setup_edit_request_config(scb, rcb, msg, &configval);
    if (res != NO_ERR) {
        return res;
    }

    agt_profile_t *profile = agt_get_profile();

    /* preference vars */
    boolean use_backup = TRUE;

    /* target tracking vars */
    ncx_cfg_t target_cfgid;
    boolean use_rootcheck;
    boolean use_candidate;
    switch (profile->agt_targ) {
    case NCX_AGT_TARG_CANDIDATE:
        target_cfgid = NCX_CFGID_CANDIDATE;
        use_rootcheck = FALSE;
        use_candidate = TRUE;
        break;
    case NCX_AGT_TARG_RUNNING:
        target_cfgid = NCX_CFGID_RUNNING;
        use_rootcheck = TRUE;
        use_candidate = FALSE;
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
        agt_yangapi_record_error(scb, msg, res, NULL, NULL);
        val_free_value(configval);
        return res;
    }

    cfg_template_t *running = cfg_get_config_id(NCX_CFGID_RUNNING);
    cfg_template_t *target = running;
    cfg_template_t *candidate = NULL;
    if (use_candidate) {
        candidate = cfg_get_config_id(NCX_CFGID_CANDIDATE);
        target = candidate;
    }

    /* check if any global locks in progress */
    res = cfg_ok_to_write(running, SES_MY_SID(scb));
    if (res == NO_ERR && use_candidate) {
        res = cfg_ok_to_write(candidate, SES_MY_SID(scb));
    }

    boolean errdone = FALSE;
    boolean save_startup = FALSE;
    boolean save_startup_done = FALSE;
    if (res == NO_ERR) {
        switch (profile->agt_start) {
        case NCX_AGT_START_MIRROR:
            save_startup = TRUE;
            break;
        case NCX_AGT_START_DISTINCT:
            // TBD: add CLI param to force manual update
            save_startup = TRUE;
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
    }

    if (res != NO_ERR) {
        agt_yangapi_record_error(scb, msg, res, NULL, NULL);
        val_free_value(configval);
        return res;
    }

    op_editop_t defop = OP_EDITOP_NONE;
    op_errop_t  errop = OP_ERROP_STOP;
    //op_testop_t testop = OP_TESTOP_SET;

    /* allocate a new transaction control block */                   
    msg->rpc_txcb = agt_cfg_new_transaction(target_cfgid,
                                            AGT_CFG_EDIT_TYPE_PARTIAL,
                                            use_rootcheck,
                                            FALSE, /* is_validate */
                                            FALSE, /* is_rollback */
                                            &res);
    if (msg->rpc_txcb == NULL || res != NO_ERR) {
        if (res == NO_ERR) {
            res = ERR_NCX_OPERATION_FAILED;
        }
        agt_yangapi_record_error(scb, msg, res, NULL, NULL);
        val_free_value(configval);
        return res;
    }

    msg->rpc_err_option = errop;  // not really used?

    /* first set the config node to canonical order */
    val_set_canonical_order(configval);

    /* validate the <config> element (wrt/ embedded operation
     * attributes) against the existing data model.
     * <rpc-error> records will be added as needed 
     */
    res = agt_val_validate_write(scb, msg, target, configval, defop);

    if (res == NO_ERR) {
        if (!check_conditional_method(rcb)) {
            res = ERR_NCX_PRECONDITION_FAILED;
            // TBD: figure out which pre-condition failed if many present
            const xmlChar *badval = NULL;
            if (rcb->if_match) {
                badval = rcb->if_match;
            } else if (rcb->if_modified_since) {
                badval = rcb->if_modified_since;
            }
            agt_yangapi_record_error(scb, msg, res, NULL, badval);
            errdone = TRUE;
        }
    }

    if (res == NO_ERR) {
        /* apply the <config> into the target config */
        res = agt_val_apply_write(scb, msg, target, configval, defop);
        if (res != NO_ERR) {
            errdone = TRUE;
        }
    }

    agt_cfg_free_transaction(msg->rpc_txcb);
    msg->rpc_txcb = NULL;

    if (res == NO_ERR && target_cfgid == NCX_CFGID_CANDIDATE) {
        /* got the edit OK into candidate, now <commit> the edit
         * allocate a new transaction control block 
         */
        msg->rpc_txcb = 
            agt_cfg_new_transaction(NCX_CFGID_RUNNING, 
                                    AGT_CFG_EDIT_TYPE_FULL,
                                    FALSE, /* rootcheck */
                                    FALSE, /* is_validate */
                                    FALSE, /* is_rollback */
                                    &res);
        if (msg->rpc_txcb == NULL || res != NO_ERR) {
            if (res == NO_ERR) {
                res = ERR_NCX_OPERATION_FAILED;
            }
        } else {
            /******** COMMIT VALIDATE PHASE **********/

            /* tag this transaction as a commit attempt */
            agt_cfg_set_transaction_commit(msg->rpc_txcb);

            /* check if this session allowed to perform all the
             * edits in the commit request   */
            msg->rpc_txcb->commitcheck = TRUE;
            res = agt_val_check_commit_edits(scb, msg, candidate, running);
            if (res != NO_ERR) {
                errdone = TRUE;
            }

            /* do not need to delete dead nodes in candidate because
             * it is already done at the end of every edit */
            if (res == NO_ERR ) {
                res = agt_val_root_check(scb, &msg->mhdr, msg->rpc_txcb,
                                         candidate->root);
                if (res != NO_ERR) {
                    errdone = TRUE;
                }
            }
            msg->rpc_txcb->commitcheck = FALSE;

            /******** COMMIT INVOKE PHASE **********/
            if (res == NO_ERR) {
                xmlChar *backup_source = NULL;
                res = agt_ncx_internal_commit(scb, msg, use_backup,
                                              save_startup,
                                              SES_MY_SID(scb),
                                              &backup_source, &errdone);
                if (save_startup) {
                    save_startup_done = TRUE;
                }
                m__free(backup_source);
            }
        }
    }

    if (res == NO_ERR && save_startup && !save_startup_done) {
        /* save the edit into NV-storage */
        res = agt_ncx_cfg_save(target, FALSE);
    }

    if (res != NO_ERR && !errdone) {
        agt_yangapi_record_error(scb, msg, res, NULL, NULL);
    }

    val_free_value(configval);

    return res;

}  /* agt_yangapi_edit_request */


/* END file agt_yangapi_edit.c */
#endif   // WITH_YANGAPI

