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
/*  FILE: agt_cfg.c

   Manage Server configuration edit transactions

   <load-config>

   Special Config Operation: agt_rpc_load_config_file

    <rpc>
      <load-config>
        <config> ....</config>
      </load-config>
    </rpc>

   Used for loading running from startup at boot-time and
   when restoring the running config from backup after the
   confirm-commit is canceled or timed out.

   <edit-config>

   Two sources supported: inline config and URL for a local XML file 

    <rpc>
      <edit-config>
        <config> ....</config>
      </edit-config>
    </rpc>

    <rpc>
      <edit-config>
        <url>file://foo.xml</url>
      </ledit-config>
    </rpc>


   Configuration States:

     
      +-----------+  +---------+  +---------+
      |   INIT    |  |   BAD   |  |   OK    |
      +-----------+  +---------+  +---------+
            V             ^            ^
            |             | fail       | pass
            +-------------+------------+
       root-check              

   If agt_profile.agt_running_error == FALSE:
     - only INIT and OK states are used. BAD state will
       cause the server to terminate

   If agt_profile.agt_running_error == TRUE:
     - server config state will stay BAD until the root-check
       succeeds, then it will transition to GOOD and stay there

   Init State:

   - the startup config is loaded into the running config
     if enabled.
      - the agt_profile.agt_startup_error flag is checked
         - if FALSE, then any errors at this point will cause
           the server to terminate.
         - if TRUE the server will continue after pruning error
           nodes from the config

   - the static and dynamic SIL code is run to give it a chance
     to make sure all factory-preset nodes, especially top-level nodes,
     are present and properly configured

   - the root check is run on the complete running config
      - the agt_profile.agt_running_error flag is checked
         - if FALSE, then any errors at this point will cause
           the server to terminate.
           The config state is set to 'OK'
         - if TRUE the server will continue!!!
           Use this mode carefully!!!
           No edit-config to running or commit operation will
           succeed until the root checks all pass.
           The config state is set to Bad

   OK State:

     - The running config was loaded correctly
     - Normal root test node pruning optimizations are enabled
     - the agt_cfg_commit_editQ for running is empty
     - the agt_cfg_commit_editQ for candidate is empty

   Bad State:

     - The running config was not loaded correctly
     - Normal root test node pruning optimizations are not enabled
     - Full root checks will be done until they all pass, and if so
       the config state is set to OK

 Configuration Edit Actions

   Pre-Apply:  

   The newnode is removed from its source value tree
   and a deleted node put in its place

      +-----------+                    +----------------+
      |  newnode  | <- val_swap_child  | newnode_marker |
      +-----------+                    +----------------+

   If curnode exists and it is a leaf:

      +-----------+                  +---------------+
      |  curnode  | val_clone_ex ->  | curnode_clone |
      +-----------+                  +---------------+
  
   AGT_CFG_EDIT_ACTION_ADD:

   The newnode is added to the proper parent node in the target tree

      +-----------+                    +---------+
      |  newnode  | add_child_clean -> | newnode | 
      +-----------+                    +---------+

   AGT_CFG_EDIT_ACTION_SET:

     Merge leaf or duplicate leaf-list if curnode exists.
     The newnode will be placed back in its source tree
      in the recovery phase.

      +-----------+               +---------+
      |  newnode  | val_merge ->  | curnode | 
      +-----------+               +---------+

    Merge complex type will always pick the current node if
    it exists. The applyhere flag will not be set for merge
    if both newnode and curnode exist.

   AGT_CFG_EDIT_ACTION_MOVE:


   AGT_CFG_EDIT_ACTION_DELETE:


        
   AGT_CFG_EDIT_ACTION_DELETE_DEFAULT:



   Recovery: The newnode_marker is removed from its source value tree
             and discarded, and the newnode parm is put in its place

      +-----------+                   +----------------+
      |  newnode  | val_swap_child -> | newnode_marker |
      +-----------+                   +----------------+

Datastore Validation Result Caching

   After the first good running config is commited
   the xpath_backptr and object dirty caching can be
   used if ncx_use_xpath_backptrs() = TRUE (default)

   After a good config validation, all the co m mit test control
   blocks and XPath control blocks are clean, and their result
   values are valid.

   If a node is edited in the candidate datastore then
   its object node will be marked candidate-dirty (OBJ_FL_CDIRTY)

   If a node is edited in the running datastore then
   its object node will be marked running-dirty (OBJ_FL_RDIRTY)

XPath cache initialization

   When a must-stmt, when-stmt, or NACM data-rule XPath
   expression is created, XPath backptrs are saved in the
   object header for the datastore location-path-stmt node-set
   results encountered in the expression.

   This means that the XPath expression automatically keeps track
   of whether it is valid or not, based on edits to the target
   datastore

Object cache initialization

   When all modules are initially loaded (and individually for
   <load> protocol operation) the config data nodes are checked
   for any must-stmt or when-stmt XPath expressions.
   If so, then an xpath_backptr is added the the referenced
   config data nodes in any XPath expressions for the
   object being added.

   During OP_EDITOP_LOAD, all caching code is skipped since the
   data tree is not complete yet.  The undo record is created
   for 'root', and not used at all.

   During a normal edit, the undo records are used.
   These are created during the APPLY phase and finalized during
   the COMMIT phase.

     add_undo_node --> 

         obj_flag_xpath_backptrs_dirty
           - set object dirty
           - for all must-when exprs: --> xpath_set_backptrs_dirty

    Separate dirty flags are maintained for candidate dirty
    and running dirty

Adding a New Module

   When a module is added via the <load> operation, the
   XPath backptrs are set via the ncx_set_obj_backptrs() function.

   The agt_acm_clean_xpath_cache() function is also called
   to force any NACM data rule XPath results to be freed,
   since they might be invalid due to new objects in the
   data model.

Instance Check Skipping

   The obj->flags OBJ_FL_CDIRTY and OBJ_FL_RDIRTY
   bits are set and cleared when an instance of that node is
   edited.  Only the deepest node possible is affected,
   not all ancestor nodes.

   If the obj_is_dirty() operation is FALSE, then the
   unique_test and choice_agt_check functions are skipped
   during a root-check for the target object.

Unique Tests Not Cached

  The XPath result node-sets for unique-stmt checks are
  not cached and checked for dirty.  These tests are avoided
  if the commit-test XPath result is not dirty.  Since unique-stmt
  is very rare, this code does not usually run in a root check

NACM Data Rule Caching

  If a node in the running config is altered that is referenced
  by a NACM data rule (agt_acm_ietf.c) then evaluating the
  nodeset for whether a data rule applies to a particular instance
  is skipped if the xpath backptrs_cdirty or backptrs_rdirty
  flags are true (depending on the target config).

  The update_data_rule function will not re-evaluate the
  nodeset unless it is changed.

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
22dec11      abb      begun; split out from ncx/rpc ...


*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <time.h>

#include "procdefs.h"
#include "agt.h"
#include "agt_cfg.h"
#include "agt_util.h"
#include "cfg.h"
#include "dlq.h"
#include "log.h"
#include "ncx.h"
#include "ncx_num.h"
#include "ncxconst.h"
#include "obj.h"
#include "op.h"
#include "rpc.h"
#include "rpc_err.h"
#include "status.h"
#include "val.h"
#include "val_util.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/



/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/
static ncx_transaction_id_t agt_cfg_txid;

static const xmlChar *agt_cfg_txid_filespec;

static boolean agt_cfg_init_done = FALSE;

/********************************************************************
* FUNCTION allocate_txid
*
* Allocate a new transaction ID 
*
* SIDE EFFECTS:
*    agt_cfg_txid is updated
* RETURNS:
*    new value of agt_cfg_txid
*********************************************************************/
static ncx_transaction_id_t
    allocate_txid (void)
{
    ++agt_cfg_txid;
    if (agt_cfg_txid == 0) {
        /* skip value zero */
        ++agt_cfg_txid;
    }
    log_debug2("\nAllocated transaction ID '%llu'", 
               (unsigned long long)agt_cfg_txid);

    return agt_cfg_txid;

}  /* allocate_txid */


/********************************************************************
* FUNCTION read_txid_file
*
* Read the transaction ID file and return the current ID value found
*
* INPUTS:
*    txidfile == full filespec of the transaction ID file
*    curid == address of return ID
*
* OUTPUTS:
*    *curid == return current ID (if return NO_ERR
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    read_txid_file (const xmlChar *txidfile,
                    ncx_transaction_id_t *curid)
{

    assert( txidfile && "txidfile is NULL" );
    assert( curid && "curid is NULL" );

    *curid = 0;
    status_t res = NO_ERR;
    FILE *fil = fopen((const char *)txidfile, "r");
    if (!fil) {
        res = errno_to_status();
        log_error("\nError: Open txid file for read failed (%s)", 
                  get_error_string(res));
        return res;
    } 

    char buffer [128];
    if (fgets(buffer, sizeof buffer, fil)) {
        /* expecting just 1 line containing the ASCII encoding of
         * the transaction ID  */
        log_debug4("\nRead transaction ID line '%s'", buffer);

        uint32 len = xml_strlen((const xmlChar *)buffer);
        if (len > 1) {
            /* strip ending newline */
            buffer[len-1] = 0;

            ncx_num_t num;
            ncx_init_num(&num);
            res = ncx_convert_num((xmlChar *)buffer, NCX_NF_DEC, NCX_BT_UINT64,
                                  &num);
            if (res == NO_ERR) {
                agt_cfg_txid = num.ul;
                *curid = num.ul;
                log_debug3("\nGot transaction ID line '%llu'", 
                           (unsigned long long)num.ul);
            } else {
                log_error("\nError: txid is not valid (%s)",
                          get_error_string(res));
            }
            ncx_clean_num(NCX_BT_UINT64, &num);
        } else {
            res = ERR_NCX_INVALID_NUM;
            log_error("\nError: txid is not valid (%s)",
                      get_error_string(res));
        }
    } else {
        res = errno_to_status();
        log_error("\nError: Read txid file failed (%s)", 
                  get_error_string(res));
    }

    fclose(fil);

    return res;

} /* read_txid_file */


/********************************************************************
* FUNCTION write_txid_file
*
* Write the transaction ID file with the supplied value
*
* INPUTS:
*    txidfile == full filespec of the transaction ID file
*    curid == pointer to new txid to write
*
* RETURNS:
*    status
*********************************************************************/
static status_t
    write_txid_file (const xmlChar *txidfile,
                     ncx_transaction_id_t *curid)
{

    assert( txidfile && "txidfile is NULL" );
    assert( curid && "curid is NULL" );

    status_t res = NO_ERR;
    FILE *fil = fopen((const char *)txidfile, "w");
    if (!fil) {
        res = errno_to_status();
        log_error("\nError: Open txid file for write failed (%s)", 
                  get_error_string(res));
        log_error_append("\n   txid file: %s", txidfile);
        return res;
    } 

    uint32 len = 0;
    char buffer [128];
    ncx_num_t num;

    ncx_init_num(&num);
    num.ul = *curid;

    res = ncx_sprintf_num((xmlChar *)buffer, &num, NCX_BT_UINT64, &len);
    if (res == NO_ERR) {
        buffer[len] = '\n';
        buffer[len+1] = 0;
        int ret = fputs(buffer, fil);
        if (ret <= 0) {
            res = errno_to_status();
            log_error("\nError: write txid ID file failed (%s)",
                      get_error_string(res));
        }
    } else {
        log_error("\nError: sprintf txid ID failed (%s)",
                  get_error_string(res));
    }

    ncx_clean_num(NCX_BT_UINT64, &num);
    fclose(fil);
    return res;

} /* write_txid_file */


/********************************************************************
* FUNCTION clean_undorec
*
* Clean all the memory used by the specified agt_cfg_undo_rec_t
* but do not free the struct itself
*
*  !!! The caller must free internal pointers that were malloced
*  !!! instead of copied.  This function does not check them!!!
*  
* INPUTS:
*   undo == agt_cfg_undo_rec_t to clean
*   reuse == TRUE if possible reuse
*            FALSE if this node is being freed right now
*
* RETURNS:
*   none
*********************************************************************/
static void 
    clean_undorec (agt_cfg_undo_rec_t *undo,
                   boolean reuse)
{
    if (undo->free_curnode && undo->curnode) {
        val_free_value(undo->curnode);
    }

    if (undo->curnode_clone) {
        val_free_value(undo->curnode_clone);
    }

    if (undo->newnode_marker) {
        val_free_value(undo->newnode_marker);
    }

    if (undo->curnode_marker) {
        val_free_value(undo->curnode_marker);
    }

    /* really expecting the extra_deleteQ to be empty at this point */
    while (!dlq_empty(&undo->extra_deleteQ)) {
        agt_cfg_nodeptr_t *nodeptr = (agt_cfg_nodeptr_t *)
            dlq_deque(&undo->extra_deleteQ);
        if (nodeptr && nodeptr->node) {
            val_remove_child(nodeptr->node);
            val_free_value(nodeptr->node);
        } else {
            SET_ERROR(ERR_INTERNAL_VAL);
        }
        agt_cfg_free_nodeptr(nodeptr);
    }

    if (reuse) {
        agt_cfg_init_undorec(undo);
    }

} /* clean_undorec */


/***************** E X P O R T E D    F U N C T I O N S  ***********/


/********************************************************************
* FUNCTION agt_cfg_new_transaction
*
* Malloc and initialize agt_cfg_transaction_t struct
*
* INPUTS:
*    cfgid == config ID to use
*    edit_type == database edit type
*    rootcheck == TRUE if root_check needs to be done before commit
*                 during agt_val_apply_write; FALSE if it is done
*                 manually via agt_val_root_check
*    is_validate == TRUE if this is a <validate> operation
*                   the target data nodes will not be altered
*                   at all during the transaction
*                   FALSE if this is some sort of real edit
*    is_rollback == TRUE if this is a confirmed commit timeout
*                   or a cancel-confirmed-commit operation
*                   FALSE if this is some other type of transaction
*    res == address of return status
* OUTPUTS:
*    *res == return status
* RETURNS:
*    malloced transaction struct; need to call agt_cfg_free_transaction
*********************************************************************/
agt_cfg_transaction_t *
    agt_cfg_new_transaction (ncx_cfg_t cfgid,
                             agt_cfg_edit_type_t edit_type,
                             boolean rootcheck,
                             boolean is_validate,
                             boolean is_rollback,
                             status_t *res)
{
    assert( edit_type && "edit_type in NONE" );
    assert( res && "res is NULL" );

    cfg_template_t *cfg = cfg_get_config_id(cfgid);
    if (cfg == NULL) {
        *res = ERR_NCX_CFG_NOT_FOUND;
        return NULL;
    }

    if (cfg->cur_txid != 0) {
        /* a current transaction is already in progress */
        *res = ERR_NCX_NO_ACCESS_STATE;
        return NULL;
    }

    agt_cfg_transaction_t *txcb = m__getObj(agt_cfg_transaction_t);
    if (txcb == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    memset(txcb, 0x0, sizeof(agt_cfg_transaction_t));
	   
    dlq_createSQue(&txcb->undoQ);
    dlq_createSQue(&txcb->auditQ);
    dlq_createSQue(&txcb->deadnodeQ);
    dlq_createSQue(&txcb->deleteQ);

    txcb->txid = allocate_txid();
    (void)time(&txcb->timestamp);

    txcb->cfg_id = cfgid;
    txcb->rootcheck = rootcheck;
    txcb->edit_type = edit_type;
    txcb->is_validate = is_validate;
    txcb->is_rollback = is_rollback;
    txcb->apply_res = ERR_NCX_SKIPPED;
    txcb->commit_res = ERR_NCX_SKIPPED;
    txcb->rollback_res = ERR_NCX_SKIPPED;

    agt_profile_t *profile = agt_get_profile();
    if (profile->agt_config_state == AGT_CFG_STATE_BAD) {
        txcb->start_bad = TRUE;
    }

    cfg->cur_txid = txcb->txid;

    *res = NO_ERR;
    return txcb;

}  /* agt_cfg_new_transaction */


/********************************************************************
* FUNCTION agt_cfg_free_transaction
*
* Clean and free a agt_cfg_transaction_t struct
*
* INPUTS:
*    txcb == transaction struct to free
*********************************************************************/
void
    agt_cfg_free_transaction (agt_cfg_transaction_t *txcb)
{
    if (txcb == NULL) {
        return;
    }

    /* clear current config txid if any */
    if (txcb->txid) {
        cfg_template_t *cfg = cfg_get_config_id(txcb->cfg_id);
        if (cfg) {
            if (cfg->cur_txid == txcb->txid) {
                log_debug3("\nClearing current txid for %s config",
                           cfg->name);
                cfg->cur_txid = 0;
            }
        }
    }

    /* clean undo queue */
    while (!dlq_empty(&txcb->undoQ)) {
        agt_cfg_undo_rec_t *undorec = (agt_cfg_undo_rec_t *)
            dlq_deque(&txcb->undoQ);
        agt_cfg_free_undorec(undorec);
    }

    /* clean audit queue */
    while (!dlq_empty(&txcb->auditQ)) {
        agt_cfg_audit_rec_t *auditrec = (agt_cfg_audit_rec_t *)
            dlq_deque(&txcb->auditQ);
        agt_cfg_free_auditrec(auditrec);
    }

    /* clean dead node queue */
    while (!dlq_empty(&txcb->deadnodeQ)) {
        agt_cfg_nodeptr_t *nodeptr = (agt_cfg_nodeptr_t *)
            dlq_deque(&txcb->deadnodeQ);
        agt_cfg_free_nodeptr(nodeptr);
    }

    /* clean delete node queue */
    ncx_clean_backptrQ(&txcb->deleteQ);

    m__free(txcb);

}  /* agt_cfg_free_transaction */


/********************************************************************
* FUNCTION agt_cfg_init_transactions
*
* Initialize the transaction ID functionality
*
* INPUTS:
*   txidfile == TXID filespec to use
*   foundfile == TRUE if file found; FALSE if this is first write
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_cfg_init_transactions (const xmlChar *txidfile,
                               boolean foundfile)
{
    assert( txidfile && "txidfile is NULL" );

    agt_cfg_txid = CFG_INITIAL_TXID;
    agt_cfg_init_done = TRUE;

    status_t res = NO_ERR;

    /* if we get here profile->agt_startup_txid_file is set
     * now either read the file if it was found or start a new
     * file with an ID of zero if this is the first time set */
    ncx_transaction_id_t txid = CFG_INITIAL_TXID;

    if (foundfile) {
        res = read_txid_file(txidfile, &txid);
    } else {
        log_debug("\nNo initial transaction ID file found;"
                  " Setting running config initial transaction id to '0'");
        res = write_txid_file(txidfile, &txid);
    }

    if (res != NO_ERR) {
        return res;
    }

    /* TBD: get last_modified from saved startup config */
    time_t last_modified = 0;
    tstamp_now(&last_modified);

    /* set all the last transaction ID values for the standard datastores */
    cfg_template_t *cfg = cfg_get_config_id(NCX_CFGID_RUNNING);
    if (cfg != NULL) {
        cfg->last_txid = txid;
        cfg->last_modified = last_modified;
    }

    cfg = cfg_get_config_id(NCX_CFGID_CANDIDATE);
    if (cfg != NULL) {
        cfg->last_txid = txid;
        cfg->last_modified = last_modified;
    }

    cfg = cfg_get_config_id(NCX_CFGID_STARTUP);
    if (cfg != NULL) {
        cfg->last_txid = txid;
        cfg->last_modified = last_modified;
    }

    /* TBD: get etag from saved startup config */
    /* set the global cached values of the TXID file and ID */
    agt_cfg_txid = txid;
    agt_cfg_txid_filespec = txidfile;

    return res;

}  /* agt_cfg_init_transactions */


/********************************************************************
* FUNCTION agt_cfg_cleanup_transactions
*
* Cleanup the transaction ID functionality
*
* INPUTS:
*   txidfile == TXID filespec to use
*********************************************************************/
void
    agt_cfg_cleanup_transactions (const xmlChar *txidfile)
{
    if (txidfile) {
        ncx_transaction_id_t txid = agt_cfg_txid;
        status_t res = write_txid_file(txidfile, &txid);
        if (res != NO_ERR) {
            log_error("\nError: save txid failed (%s)",
                      get_error_string(res));
        }
    }
}


/********************************************************************
* FUNCTION agt_cfg_txid_in_progress
*
* Return the ID of the current transaction ID in progress
*
* INPUTS:
*    cfgid == config ID to check
* RETURNS:
*    txid of transaction in progress or 0 if none
*********************************************************************/
ncx_transaction_id_t
    agt_cfg_txid_in_progress (ncx_cfg_t cfgid)
{
    cfg_template_t *cfg = cfg_get_config_id(cfgid);
    if (cfg) {
        return cfg->cur_txid;
    }
    return 0;

}  /* agt_cfg_txid_in_progress */


/********************************************************************
* FUNCTION agt_cfg_new_undorec
*
* Malloc and initialize a new agt_cfg_undo_rec_t struct
*
* INPUTS:
*   none
* RETURNS:
*   pointer to struct or NULL or memory error
*********************************************************************/
agt_cfg_undo_rec_t *
    agt_cfg_new_undorec (void)
{
    agt_cfg_undo_rec_t *undo;

    undo = m__getObj(agt_cfg_undo_rec_t);
    if (!undo) {
        return NULL;
    }
    agt_cfg_init_undorec(undo);
    return undo;

} /* agt_cfg_new_undorec */


/********************************************************************
* FUNCTION agt_cfg_init_undorec
*
* Initialize a new agt_cfg_undo_rec_t struct
*
* INPUTS:
*   undo == agt_cfg_undo_rec_t memory to initialize
* RETURNS:
*   none
*********************************************************************/
void
    agt_cfg_init_undorec (agt_cfg_undo_rec_t *undo)
{
#ifdef DEBUG
    if (!undo) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    memset(undo, 0x0, sizeof(agt_cfg_undo_rec_t));
    dlq_createSQue(&undo->extra_deleteQ);
    undo->apply_res = ERR_NCX_SKIPPED;
    undo->commit_res = ERR_NCX_SKIPPED;
    undo->rollback_res = ERR_NCX_SKIPPED;

} /* agt_cfg_init_undorec */


/********************************************************************
* FUNCTION agt_cfg_free_undorec
*
* Free all the memory used by the specified agt_cfg_undo_rec_t
*
* INPUTS:
*   undo == agt_cfg_undo_rec_t to clean and delete
* RETURNS:
*   none
*********************************************************************/
void 
    agt_cfg_free_undorec (agt_cfg_undo_rec_t *undo)
{
    if (!undo) {
        return;
    }
    clean_undorec(undo, FALSE);
    m__free(undo);

} /* agt_cfg_free_undorec */


/********************************************************************
* FUNCTION agt_cfg_clean_undorec
*
* Clean all the memory used by the specified agt_cfg_undo_rec_t
* but do not free the struct itself
*
*  !!! The caller must free internal pointers that were malloced
*  !!! instead of copied.  This function does not check them!!!
*  
* INPUTS:
*   undo == agt_cfg_undo_rec_t to clean
* RETURNS:
*   none
*********************************************************************/
void 
    agt_cfg_clean_undorec (agt_cfg_undo_rec_t *undo)
{
    if (!undo) {
        return;
    }
    clean_undorec(undo, TRUE);

} /* agt_cfg_clean_undorec */


/********************************************************************
* FUNCTION agt_cfg_new_auditrec
*
* Malloc and initialize a new agt_cfg_audit_rec_t struct
*
* INPUTS:
*   target == i-i string of edit target
*   editop == edit operation enum
*
* RETURNS:
*   pointer to struct or NULL or memory error
*********************************************************************/
agt_cfg_audit_rec_t *
    agt_cfg_new_auditrec (const xmlChar *target,
                          op_editop_t editop)
{
    agt_cfg_audit_rec_t *auditrec;

#ifdef DEBUG
    if (target == NULL) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    auditrec = m__getObj(agt_cfg_audit_rec_t);
    if (!auditrec) {
        return NULL;
    }
    memset(auditrec, 0x0, sizeof(agt_cfg_audit_rec_t));
    auditrec->target = xml_strdup(target);
    if (auditrec->target == NULL) {
        m__free(auditrec);
        return NULL;
    }
    auditrec->editop = editop;
    return auditrec;

} /* agt_cfg_new_auditrec */


/********************************************************************
* FUNCTION agt_cfg_free_auditrec
*
* Free all the memory used by the specified agt_cfg_audit_rec_t
*
* INPUTS:
*   auditrec == agt_cfg_audit_rec_t to clean and delete
*
* RETURNS:
*   none
*********************************************************************/
void 
    agt_cfg_free_auditrec (agt_cfg_audit_rec_t *auditrec)
{
    if (auditrec == NULL) {
        return;
    }
    if (auditrec->target) {
        m__free(auditrec->target);
    }
    m__free(auditrec);

} /* agt_cfg_free_auditrec */



/********************************************************************
* FUNCTION agt_cfg_new_commit_test
*
* Malloc a agt_cfg_commit_test_t struct
*
* RETURNS:
*   malloced commit test struct or NULL if ERR_INTERNAL_MEM
*********************************************************************/
agt_cfg_commit_test_t *
    agt_cfg_new_commit_test (void)
{
    agt_cfg_commit_test_t *commit_test = m__getObj(agt_cfg_commit_test_t);
    if (commit_test) {
        memset(commit_test, 0x0, sizeof(agt_cfg_commit_test_t));
    }
    return commit_test;

} /* agt_cfg_new_commit_test */


/********************************************************************
* FUNCTION agt_cfg_free_commit_test
*
* Free a previously malloced agt_cfg_commit_test_t struct
*
* INPUTS:
*    commit_test == commit test record to free
*
*********************************************************************/
void
    agt_cfg_free_commit_test (agt_cfg_commit_test_t *commit_test)
{
    if (commit_test == NULL) {
        return;
    }
 
    if (commit_test->objpcb) {
        xpath_free_pcb(commit_test->objpcb);
    }
    if (commit_test->result) {
        xpath_free_result(commit_test->result);
    }
    m__free(commit_test);

} /* agt_cfg_free_commit_test */


/********************************************************************
* FUNCTION agt_cfg_new_nodeptr
*
* Malloc and initialize a new agt_cfg_nodeptr_t struct
*
* INPUTS:
*   node == node to point at
* RETURNS:
*   pointer to struct or NULL or memory error
*********************************************************************/
agt_cfg_nodeptr_t *
    agt_cfg_new_nodeptr (val_value_t *node)
{
    agt_cfg_nodeptr_t *nodeptr;

    nodeptr = m__getObj(agt_cfg_nodeptr_t);
    if (!nodeptr) {
        return NULL;
    }
    memset(nodeptr, 0x0, sizeof(agt_cfg_nodeptr_t));
    nodeptr->node = node;
    return nodeptr;

} /* agt_cfg_new_nodeptr */


/********************************************************************
* FUNCTION agt_cfg_free_nodeptr
*
* Free all the memory used by the specified agt_cfg_nodeptr_t
*
* INPUTS:
*   nodeptr == agt_cfg_nodeptr_t to clean and delete
*********************************************************************/
void 
    agt_cfg_free_nodeptr (agt_cfg_nodeptr_t *nodeptr)
{
    if (!nodeptr) {
        return;
    }
    m__free(nodeptr);

} /* agt_cfg_free_nodeptr */


/********************************************************************
* FUNCTION agt_cfg_set_transaction_commit
*
* Tag this transaction as a commit from candidate to running
*
* INPUTS:
*    txcb == transaction struct to tag
*********************************************************************/
void
    agt_cfg_set_transaction_commit (agt_cfg_transaction_t *txcb)
{
    assert( txcb && "txcb is NULL" );

    txcb->is_commit = TRUE;

} /* agt_cfg_set_transaction_commit */


/* END file agt_cfg.c */

