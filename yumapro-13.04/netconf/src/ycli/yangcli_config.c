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
/*  FILE: yangcli_config.c

   NETCONF YANG-based CLI Tool

   config command
   exit command

   configure mode command processing

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
16-nov-12    abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libssh2.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "libtecla.h"

#include "procdefs.h"
#include "log.h"
#include "mgr.h"
#include "mgr_ses.h"
#include "ncx.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "var.h"
#include "xmlns.h"
#include "xml_util.h"
#include "xml_val.h"
#include "yangconst.h"
#include "yangcli.h"
#include "yangcli_cmd.h"
#include "yangcli_config.h"
#include "yangcli_util.h"


/********************************************************************
*                                                                   *
*                          T Y P E S                                *
*                                                                   *
*********************************************************************/


/********************************************************************
 * FUNCTION get_del_op
 * 
 * Check if remove operation allowed, or just delete
 *
 * INPUTS:
 *    session_cb == session control block to use
 * RETURNS:
 *    delete operation to use (OP_EDITOP_DELETE or OP_EDITOP_REMOVE)
 *********************************************************************/
static op_editop_t 
    get_del_op (session_cb_t *session_cb)
{
    ses_cb_t *scb = mgr_ses_get_scb(session_cb->mysid);
    if (scb == NULL) {
        return OP_EDITOP_DELETE;
    }

    switch (ses_get_protocol(scb)) {
    case NCX_PROTO_NETCONF10:
        return OP_EDITOP_DELETE;
    case NCX_PROTO_NETCONF11:
        return OP_EDITOP_REMOVE;
    default:
        return OP_EDITOP_DELETE;
    }

}  /* get_del_op */


/********************************************************************
 * FUNCTION add_enode
 * 
 * Add an edit node to the edit tree 
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    newval == node to add
 *********************************************************************/
static void
    add_enode (session_cb_t *session_cb,
               val_value_t *newval)
{
    if (session_cb->config_ecurval) {
        val_add_child_sorted(newval, session_cb->config_ecurval);
        session_cb->config_ecurval = newval;
    } else {
        assert(!session_cb->config_etree);
        session_cb->config_etree = newval;
        session_cb->config_ecurval = newval;
    }
    if (session_cb->config_firstnew == NULL) {
        session_cb->config_firstnew = newval;
    }

}  /* add_enode */


/********************************************************************
 * FUNCTION add_edit
 * (config mode input received)
 *  Add an edit to the server config_editQ
 *
 * INPUTS:
 *    session_cb == session control block to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_edit (session_cb_t *session_cb)
{
    if (session_cb->config_etree) {
        op_editop_t delop = get_del_op(session_cb);
        op_editop_t op = (session_cb->config_no_active) ?
            delop : OP_EDITOP_MERGE;
        val_value_t *clone = val_clone(session_cb->config_etree);
        if (clone == NULL) {
            return ERR_INTERNAL_MEM;
        }
        config_edit_t *edit = new_config_edit(op, clone);
        if (edit == NULL) {
            val_free_value(clone);
            return ERR_INTERNAL_MEM;
        }
        session_cb->config_edit_dirty = TRUE;
        dlq_enque(edit, &session_cb->config_editQ);
    }
    return NO_ERR;

}  /* add_edit */


/********************************************************************
 * FUNCTION check_edit
 * (config mode input received)
 *  Check an edit to make sure there is really a change
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    edit == edit to check
 * RETURNS:
 *   TRUE if change detected; FALSE if no change detected
 *********************************************************************/
static boolean
    check_edit (session_cb_t *session_cb,
                config_edit_t *edit)
{
    if (edit->edit_payload == NULL) {
        SET_ERROR(ERR_INTERNAL_VAL);
        return FALSE;
    }

    boolean retval = TRUE;
    if (session_cb->config_tree) {
        val_value_t *topval =
            val_first_child_match(session_cb->config_tree,
                                  edit->edit_payload);
        if (topval) {
            /* some sort of edit; compare for merge or delete */
            int32 ret =
                val_compare_for_edit(edit->edit_payload, topval,
                                     (edit->edit_op == OP_EDITOP_MERGE));
            retval = (ret != 0);
        } else {
            if (edit->edit_op != OP_EDITOP_MERGE) {
                /* some sort of delete on non-existent node */
                retval = FALSE;
            } /* else merge on non-existent node is OK */
        }

        if (retval && val_all_np_containers(edit->edit_payload)) {
            retval = FALSE;
        }
    }

    return retval;

}  /* check_edit */


/********************************************************************
 * FUNCTION clear_one_level
 * Clear the current level for the ecurptr
 * The exit command was given so backing up top the
 * parent of the config_ecurptr
 *
 * INPUTS:
 *    session_cb == session control block to use
 *********************************************************************/
static void
    clear_one_level (session_cb_t *session_cb)
{
    val_value_t *target = session_cb->config_ecurval;
    if (target == NULL) {
        return;
    }

    val_value_t *parent = target->parent;

    if (session_cb->config_estartval == target) {
        session_cb->config_estartval = NULL;
    }

    if (parent) {
        val_remove_child(target);
    }
    val_free_value(target);

    session_cb->config_ecurval = parent;
    if (parent == NULL) {
        session_cb->config_etree = NULL;
        session_cb->config_estartval = NULL;
    }

}  /* clear_one_level */


/********************************************************************
 * FUNCTION clear_current_level
 * Clear the current level without changing it
 *
 * INPUTS:
 *    session_cb == session control block to use
 *********************************************************************/
static void
    clear_current_level (session_cb_t *session_cb)
{
    val_value_t *target = session_cb->config_ecurval;
    if (target == NULL) {
        return;
    }

    if (session_cb->config_estartval == NULL) {
        /* clear everything out */
        val_free_value(session_cb->config_etree);
        session_cb->config_etree = NULL;
        session_cb->config_ecurval = NULL;
        return;
    }

    boolean islist = obj_is_list(target->obj);
    val_value_t *chval = val_get_first_child(target);
    val_value_t *nextval = NULL;
    for (; chval; chval = nextval) {
        nextval = val_get_next_child(chval);
        if (islist && obj_is_key(chval->obj)) {
            continue;
        }
        val_remove_child(chval);
        val_free_value(chval);
    }

}  /* clear_current_level */


/********************************************************************
 * FUNCTION find_data_node
 *
 * find the corresponding data node in the specified tree
 *
 * INPUTS:
 *    targval == target value to check
 *    targroot == root node of the targval
 *    rootval == root of tree to find new pointer
 *********************************************************************/
static val_value_t *
    find_data_node (val_value_t *targval,
                    val_value_t *targroot,
                    val_value_t *rootval)
{
    val_value_t *retval = NULL;
    if (targval->parent && targval != targroot) {
        retval = find_data_node(targval->parent, targroot, rootval);
    } else {
        return val_first_child_match(rootval, targval);
    }
    if (retval) {
        retval = val_first_child_match(retval, targval);
    }
    return retval;

}  /* find_data_node */


/********************************************************************
 * FUNCTION start_config_mode
 * 
 * Setup the configuration mode
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    session_cb == session control block to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    start_config_mode (server_cb_t *server_cb,
                       session_cb_t *session_cb)
{
    session_cb->config_mode = TRUE;
    session_cb->config_edit_dirty = FALSE;
    session_cb->config_alt_names = session_cb->alt_names;
    session_cb->alt_names = YANGCLI_DEF_CONFIG_ALT_NAMES;
    session_cb->config_match_names = session_cb->match_names;
    session_cb->match_names = YANGCLI_DEF_CONFIG_MATCH_NAMES;
    set_completion_state_config_mode(&server_cb->completion_state);
    return NO_ERR;

} /* start_config_mode */


/********************************************************************
 * FUNCTION exit_config_mode
 * 
 * Exit the configuration mode
 *
 * INPUTS:
 *    session_cb == session control block to use
 *
 *********************************************************************/
static void
    exit_config_mode (session_cb_t *session_cb)
{
    session_cb->config_mode = FALSE;
    session_cb->config_no_active = FALSE;
    session_cb->config_edit_dirty = FALSE;
    session_cb->alt_names = session_cb->config_alt_names;
    session_cb->match_names = session_cb->config_match_names;
    m__free(session_cb->config_path);
    session_cb->config_path = NULL;
    session_cb->config_curval = NULL;
    session_cb->config_curobj = NULL;
    session_cb->config_curkey = NULL;
    val_free_value(session_cb->config_etree);
    session_cb->config_etree = NULL;
    session_cb->config_ecurval = NULL;
    while (!dlq_empty(&session_cb->config_editQ)) {
        config_edit_t *edit = (config_edit_t *)
            dlq_deque(&session_cb->config_editQ);
        free_config_edit(edit);
    }

} /* exit_config_mode */


/********************************************************************
 * FUNCTION parse_value
 * (config mode input received)
 *  Parse the next word using the comstate and session_cb state
 *  Expecting this word to represent a leaf or leaf-list value
 *
 * e.g.,
 *  interface eth0 mtu 1500
 *      ^      ^    ^    ^
 *    node    key node  value
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    tkc == token chain in progress
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    parse_value (session_cb_t *session_cb,
                 tk_chain_t *tkc)
{
    obj_template_t *obj = session_cb->config_curobj;
    if (obj == NULL) {
        return SET_ERROR(ERR_INTERNAL_VAL);
    }

    const xmlChar *usestr = NULL;
    status_t res = NO_ERR;
    boolean isleaf = obj_is_leaf(obj);
    boolean isempty =
        (obj_is_leafy(obj) && obj_get_basetype(obj) == NCX_BT_EMPTY);

    if (!session_cb->config_no_active && isempty) {
        /* do not get the next token; treat next token as a sibling
         * of this empty leaf or leaf-list           */
        usestr = EMPTY_STRING;
    } else {
        /* get the next token to use */
        res = TK_ADV(tkc);

        if (session_cb->config_no_active) {
            /* got no [command] */
            if (isleaf || isempty) {
                if (res == NO_ERR) {
                    log_error("\nError: Not expecting value string for '%s' "
                              "in 'no' command\n", obj_get_name(obj));
                    return ERR_NCX_EXTRA_NODE;
                }
                if (isleaf) {
                    // node already added in parse_node
                    return NO_ERR;
                }
            } else if (res != NO_ERR) {
                log_error("\nError: Expecting value string for node '%s'\n",
                          obj_get_name(obj));
                return res;
            }
        } else if (res != NO_ERR) {
            log_error("\nError: Expecting value string for node '%s'\n",
                      obj_get_name(obj));
            return res;
        } else {
            usestr = TK_CUR_VAL(tkc);
        }
    }

    /* parse the value as a leaf value */
    /*** do something different if TK_CUR_MOD(tkc) is not NULL ***/
    val_value_t *leafval = val_make_simval_obj(obj, usestr, &res);
    if (res != NO_ERR) {
        log_error("\nError: Invalid value for '%s' leaf%s\n",
                  obj_get_name(obj),
                  obj_is_leaf(obj) ? EMPTY_STRING : (const xmlChar *)"-list");
        val_free_value(leafval);
        return res;
    }

    add_enode(session_cb, leafval);

    return NO_ERR;

}  /* parse_value */


/********************************************************************
 * FUNCTION parse_node
 * (config mode input received)
 *  Parse the next word using the comstate and session_cb state
 *  Expecting this word to represent a datastore node, not a key or value
 *
 * e.g.,
 *  interface eth0 mtu 1500
 *      ^      ^    ^    ^
 *    node    key node  value
 *
 * INPUTS:
 *    tkc == token chain in progress
 *    session_cb == session control block to use
 *    done == address of return done flag
 *    gotexit == address of return gotexit flag
 *    gotapply == address of return gotapply flag
 *    gotdo == address of return do command flag
 *
 * OUTPUTS:
 *    *done == TRUE if parsing done; no more tokens;
 *             ignore unless return NO_ERR
 *    *gotexit == TRUE if got 'exit' command
 *             ignore unless return NO_ERR and *done == TRUE
 *    *gotapply == TRUE if got 'apply' command
 *             ignore unless return NO_ERR and *done == TRUE
 *    *gotdo == TRUE if got 'do' command
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    parse_node (tk_chain_t *tkc,
                session_cb_t *session_cb,
                boolean *done,
                boolean *gotexit,
                boolean *gotapply,
                boolean *gotdo)
{
    *done = FALSE;
    *gotexit = FALSE;
    *gotapply = FALSE;
    *gotdo = FALSE;

    /* get the next token to use */
    status_t res = TK_ADV(tkc);
    if (res != NO_ERR) {
        if (res == ERR_NCX_EOF) {
            *done = TRUE;
            return NO_ERR;
        } else {
            log_error("\nError: Expecting node identifier\n");
            return res;
        }
    }

    boolean first = tk_cur_is_first(tkc);

    /* check if the 'do' form of command was given */
    if (first && TK_CUR_TYP(tkc) == TK_TT_TSTRING &&
        !xml_strcmp(TK_CUR_VAL(tkc), NCX_EL_DO) &&
        tk_next_typ(tkc) != TK_TT_NONE) {

        *gotdo = TRUE;
        *done = TRUE;
        return NO_ERR;
    }

    /* check if the 'no' form of command was given */
    if (first && TK_CUR_TYP(tkc) == TK_TT_TSTRING &&
        !xml_strcmp(TK_CUR_VAL(tkc), NCX_EL_NO) &&
        tk_next_typ(tkc) != TK_TT_NONE) {

        /* get the next token to use as the first identifier */
        res = TK_ADV(tkc);
        if (res != NO_ERR) {
            log_error("\nError: Expecting identifier token after "
                      "'no' keyword\n");
            return res;
        }
        first = FALSE;
        session_cb->config_no_active = TRUE;
    }

    /* check if the current token is an identifier */
    if (!TK_CUR_ID(tkc)) {
        log_error("\nError: expecting a node identifier\n");
        res = ERR_NCX_WRONG_TKTYPE;
        return res;
    }

    /* check if the exit command was given */
    if (first && TK_CUR_TYP(tkc) == TK_TT_TSTRING &&
        !xml_strcmp(TK_CUR_VAL(tkc), NCX_EL_EXIT) &&
        tk_next_typ(tkc) == TK_TT_NONE) {
        *done = TRUE;
        *gotexit = TRUE;
        return NO_ERR;
    }

    /* check if the apply command was given */
    if (first && TK_CUR_TYP(tkc) == TK_TT_TSTRING &&
        !xml_strcmp(TK_CUR_VAL(tkc), NCX_EL_APPLY) &&
        tk_next_typ(tkc) == TK_TT_NONE) {
        *done = TRUE;
        *gotapply = TRUE;
        return NO_ERR;
    }

    obj_template_t *startobj = session_cb->config_curobj;
    obj_template_t *topobj = NULL;

    if (startobj == NULL) {
        /* getting top-level config object */
        topobj = find_top_obj(session_cb, TK_CUR_VAL(tkc));
        if (topobj && !(obj_is_data_db(topobj) && obj_is_config(topobj))) {
            topobj = NULL;
        }
    } else {
        /* getting child node config object */
        topobj = find_child_obj(session_cb, startobj, TK_CUR_VAL(tkc));
        if (topobj && !obj_is_config(topobj)) {
            topobj = NULL;
        }
    }
    
    if (topobj == NULL) {
        log_error("\nError: No config object found matching '%s'\n",
                  TK_CUR_VAL(tkc));
        return ERR_NCX_UNKNOWN_OBJECT;
    }

    /* got a top object (relative to the current context) */
    session_cb->config_curobj = topobj;

    /* make a template object for the edit operation for non-terminals */
    val_value_t *newval = NULL;
    if (!obj_is_leafy(topobj)) {
        newval = val_new_value();
        if (newval == NULL) {
            log_error("\nError: malloc failed for new value\n");
            return ERR_INTERNAL_MEM;
        }
        val_init_from_template(newval, topobj);
        add_enode(session_cb, newval);
    } else if (session_cb->config_no_active && obj_is_leaf(topobj)) {
        /* no ... foo-leaf */
        newval = xml_val_new_flag(obj_get_name(topobj), obj_get_nsid(topobj));
        if (newval == NULL) {
            log_error("\nError: malloc failed for new value\n");
            return ERR_INTERNAL_MEM;
        }
        add_enode(session_cb, newval);
    }

    if (obj_is_list(topobj)) {
        boolean anykeys = FALSE;
        obj_key_t *newkey = obj_first_key(topobj);
        while (newkey) {
            anykeys = TRUE;

            /* get the next token to use as the key value */
            res = TK_ADV(tkc);
            if (res != NO_ERR) {
                log_error("\nError: Expecting value for '%s' key leaf\n",
                          obj_get_name(newkey->keyobj));
                return res;
            }

            /* parse the value as a key value */
            /*** do something different if TK_CUR_MOD(tkc) is not NULL ***/
            val_value_t *keyval =
                val_make_simval_obj(newkey->keyobj, TK_CUR_VAL(tkc), &res);
            if (res != NO_ERR) {
                log_error("\nError: Invalid value for '%s' key leaf\n",
                          obj_get_name(newkey->keyobj));
                val_free_value(keyval);
                return res;
            }

            /* save keyval */
            if (newval) {
                val_add_child(keyval, newval);
            } else {
                val_free_value(keyval);
            }

            /* set up the next key in the list */
            newkey = obj_next_key(newkey);
        }
        if (anykeys) {
            res = val_gen_index_chain(topobj, newval);
        }
    }

    if (res == NO_ERR && obj_is_leafy(topobj)) {
        /* expecting a value node to follow or done if 'no' command */
        *done = TRUE;
    }

    return res;

}  /* parse_node */


/********************************************************************
 * FUNCTION process_apply
 * (config mode input received)
 *  Handle the apply command and check if there are edits
 *  to apply to the server.  If so apply the edits.
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    session_cb == session control block to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    process_apply (server_cb_t *server_cb,
                   session_cb_t *session_cb)
{
    if (dlq_empty(&session_cb->config_editQ)) {
        if (LOGDEBUG2) {
            log_debug2("\nSkipping apply, no edits");
        }
        session_cb->config_edit_dirty = FALSE;
        return NO_ERR;
    }

    /* make a dummy config root -- it will not be used; only the child
     * nodes added to this container will be added to the <config>
     * parameter in the <edit-config> operation
     */
    val_value_t *configval = xml_val_new_root(NCX_EL_CONFIG, xmlns_nc_id());
    if (configval == NULL) {
        log_error("\nError: malloc failed");
        return ERR_INTERNAL_MEM;
    }
 
    status_t res = NO_ERR;
    boolean anyedits = FALSE;
    uint32 editcnt = dlq_count(&session_cb->config_editQ);
    
    while (!dlq_empty(&session_cb->config_editQ)) {
        config_edit_t *edit = (config_edit_t *)
            dlq_deque(&session_cb->config_editQ);

        /* compare the edit to the shadow config to see if it
         * represents any change or not   */
        boolean ischange = check_edit(session_cb, edit);

        if (ischange) {
            /** TBD: add to tree and collapse all edits!!! */
            val_add_child(edit->edit_payload, configval);
            edit->edit_payload = NULL;
            anyedits = TRUE;
        } else if (LOGDEBUG3) {
            log_debug3("\nSkipping edit due to no change:\n");
            val_dump_value(edit->edit_payload, 0);
        }
        free_config_edit(edit);
    }

    if (!anyedits) {
        val_free_value(configval);
        return NO_ERR;
    }

    if (server_cb->program_mode == PROG_MODE_SERVER) {
        if (LOGDEBUG) {
            if (editcnt == 1) {
                log_debug("\nApplying 1 edit\n");
            } else {
                log_debug("\nApplying %u edits\n", editcnt);
            }
        }
    } else {
        if (LOGINFO) {
            const xmlChar *sesname = (session_cb->session_cfg)
                ? session_cb->session_cfg->name : NCX_EL_DEFAULT;

            if (editcnt == 1) {
                log_info("\nApplying 1 edit to session '%s'\n", sesname);
            } else {
                log_info("\nApplying %u edits to session '%s'\n", editcnt,
                         sesname);
            }
        }
    }

    session_cb->command_mode = CMD_MODE_CONF_APPLY;
    session_cb->config_edit_dirty = FALSE;

    res = send_edit_config_to_server(server_cb, session_cb, NULL,
                                     configval, TRUE, session_cb->timeout,
                                     OP_DEFOP_MERGE);
    /* configval consumed no matter what! */

    return res;

}  /* process_apply */


/********************************************************************
 * FUNCTION process_exit
 * (config mode input received)
 *  Handle the exit command based on the current mode
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    session_cb == session control block to use
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    process_exit (server_cb_t *server_cb,
                  session_cb_t *session_cb)
{
    /*** TBD: add warning about edits pending ***/

    status_t res = NO_ERR;

    /* exit to previous level or root or out of config mode */
    if (session_cb->config_curobj) {
        /* check if edits are applied when the mode is exited */
        if (session_cb->config_edit_mode == CFG_EDITMODE_LEVEL) {
            if (session_cb->config_etree) {
                /* there may be an edit to apply */
                if (!session_cb->config_edit_dirty ||
                    dlq_count(&session_cb->config_editQ) == 0) {
                    
                    /* entered mode and then exited without adding any
                     * edits so try adding the container or list    */
                    res = add_edit(session_cb);
                }
                //clear_one_level(session_cb);
            }
            if (res == NO_ERR) {
                res = process_apply(server_cb, session_cb);
            }
        }

        /* get parent of current object */
        session_cb->config_curobj = 
            obj_get_real_parent(session_cb->config_curobj);

        if (!session_cb->config_curobj || 
            obj_is_root(session_cb->config_curobj)) {

            /* exit to root-level, stay in config mode */
            session_cb->config_curobj = NULL;
            session_cb->config_ecurval = NULL;
            val_free_value(session_cb->config_etree);
            session_cb->config_etree = NULL;
        } else if (session_cb->config_ecurval) {
            clear_one_level(session_cb);
        }
    } else {
        /* no current object so exit config mode */
        exit_config_mode(session_cb);
    }

    return res;

}  /* process_exit */


/********************************************************************
 * FUNCTION process_line_done
 * (config mode input received)
 *  Handle the command entered because it is done being parsed
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    session_cb == session control block to use
 *    startobj == object pointer at start of command
 *    gotleafys == TRUE if leaf or leaf-list value parsed
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    process_line_done (server_cb_t *server_cb,
                       session_cb_t *session_cb,
                       obj_template_t *startobj,
                       boolean gotleafys)
{
    status_t res = NO_ERR;
    val_value_t *targval = session_cb->config_ecurval;
    if (session_cb->config_no_active) {
        if (targval) {
            op_editop_t delop = get_del_op(session_cb);
            res = val_add_one_operation_attr(targval, delop);
            session_cb->config_edit_dirty = TRUE;
        }
    }

    boolean newmode = (!session_cb->config_no_active &&
                       !gotleafys && session_cb->config_curobj &&
                       (startobj != session_cb->config_curobj));

    if (res == NO_ERR) {
        if (session_cb->config_edit_mode == CFG_EDITMODE_LINE) {
            /* apply to server as each line is finished */
            res = add_edit(session_cb);
            clear_current_level(session_cb);
            if (res == NO_ERR) {
                res = process_apply(server_cb, session_cb);
            }
            /*
            if (gotleafys) {
                session_cb->config_curobj = startobj;
            }
            */
        } else if (newmode) {
            /* entering new container or list mode */
            session_cb->config_edit_dirty = TRUE;
            if (LOGDEBUG) {
                log_debug("\nEntering config mode for %s %s\n",
                          obj_get_typestr(session_cb->config_curobj),
                          obj_get_name(session_cb->config_curobj));
            }
        } else {
            res = add_edit(session_cb);
            clear_current_level(session_cb);
            if (res == NO_ERR && startobj == NULL) {
                /* apply top-level command right now */
                res = process_apply(server_cb, session_cb);

                /* clean out current top-level value here */
                val_free_value(session_cb->config_etree);
                session_cb->config_etree = NULL;
                session_cb->config_ecurval = NULL;
            }
            if (gotleafys || session_cb->config_no_active) {
                session_cb->config_curobj = startobj;
            }
        }
    }

    session_cb->config_no_active = FALSE;

    return res;

}  /* process_line_done */


/********************************************************************
 * FUNCTION process_do_command
 * (config mode input received)
 *  Handle the do command escape
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    session_cb == session control block to use
 *    tkc == token chain in process
 *    line == command line received
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    process_do_command (server_cb_t *server_cb,
                        session_cb_t *session_cb,
                        tk_chain_t *tkc,
                        const xmlChar *line)
{
    status_t res = TK_ADV(tkc);
    if (res != NO_ERR) {
        log_error("\nError: expected a command name\n");
        return res;
    }

    if (TK_CUR_TYP(tkc) != TK_TT_TSTRING) {
        log_error("\nError: expected a command name\n");
        return ERR_NCX_WRONG_TKTYPE;
    }

    res = run_do_command(server_cb, session_cb, &line[2]);

    set_completion_state_config_mode(&server_cb->completion_state);

    return res;

}  /* process_do_command */


/********************************************************************
 * FUNCTION set_config_path
 *
 *  Change or set the config path prompt
 *
 * INPUTS:
 *    session_cb == session control block to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    set_config_path (session_cb_t *session_cb)
{
    status_t res = NO_ERR;

    /* update the prompt path string */
    m__free(session_cb->config_path);
    session_cb->config_path = NULL;

    if (session_cb->config_ecurval) {
        ncx_instfmt_t format = NCX_IFMT_CLI2;
        if (session_cb->prompt_type == HELP_MODE_FULL) {
            format = NCX_IFMT_CLI;
        }
        res = val_gen_instance_id(NULL, session_cb->config_ecurval,
                                  format, &session_cb->config_path);
    } else if (session_cb->config_curobj) {
        if (session_cb->prompt_type == HELP_MODE_FULL) {
            res = obj_gen_object_id_xpath(session_cb->config_curobj,
                                          &session_cb->config_path);
        } else {
            res = obj_gen_object_id(session_cb->config_curobj,
                                    &session_cb->config_path);
        }
    }

    return res;

}  /* set_config_path */


/*************** E X T E R N A L    F U N C T I O N S  *************/


/********************************************************************
 * FUNCTION do_config (local RPC)
 * 
 * config term
 *
 * Enter the configuration mode
 *
 * INPUTS:
 * server_cb == server control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_config (server_cb_t *server_cb,
               obj_template_t *rpc,
               const xmlChar *line,
               uint32  len)
{
    status_t res = NO_ERR;
    val_value_t *valset = get_valset(server_cb, rpc, &line[len], &res);

    if (valset && res == NO_ERR) {
        /* check if the 'brief' flag is set first */
        val_value_t *parm = 
            val_find_child(valset, YANGCLI_MOD, YANGCLI_TERM);
        if (!parm || parm->res != NO_ERR) {
            log_error("\nError: 'terminal' parameter invalid\n");
            if (parm) {
                res = parm->res;
            } else {
                res = ERR_NCX_MISSING_PARM;
            }
        } else {
            session_cb_t *session_cb = server_cb->cur_session_cb;
            if (session_cb->config_mode) {
                log_error("\nError: configure mode already active\n");
                res = ERR_NCX_IN_USE;
            } else {
                res = start_config_mode(server_cb, session_cb);
            }
        }
    }

    if (valset) {
        val_free_value(valset);
    }

    return res;

}  /* do_config */


/********************************************************************
 * FUNCTION do_exit (local RPC)
 * 
 * exit
 *
 * Exit the configuration mode
 *
 * INPUTS:
 * server_cb == server control block to use
 *    rpc == RPC method for the show command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_exit (server_cb_t *server_cb,
             obj_template_t *rpc,
             const xmlChar *line,
             uint32  len)
{
    status_t res = NO_ERR;
    val_value_t *valset = get_valset(server_cb, rpc, &line[len], &res);

    if (valset && res == NO_ERR) {

        ;
    }

    if (res == NO_ERR || res == ERR_NCX_SKIPPED) {
        session_cb_t *session_cb = server_cb->cur_session_cb;
        if (!session_cb->config_mode) {
            log_error("\nError: configure mode is not active\n");
            res = ERR_NCX_OPERATION_FAILED;
        } else {
            exit_config_mode(session_cb);
        }
    }

    if (valset) {
        val_free_value(valset);
    }

    return res;

}  /* do_exit */


/********************************************************************
 * FUNCTION handle_config_input
 * (config mode input received)
 * 
 * e.g.,
 *  nacm
 *  interface eth0
 *  interface eth0 mtu 1500
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    session_cb == session control block to use
 *    line == CLI input in progress; this line is passed to the
 *           tk_parse functions which expects non-const ptr
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    handle_config_input (server_cb_t *server_cb,
                         session_cb_t *session_cb,
                         xmlChar *line)
{
    if (LOGDEBUG2) {
        log_debug2("\nconfig mode input for line '%s'\n", line);
    }

    /* get a token chain and parse the line of input into tokens */
    tk_chain_t *tkc = tk_new_chain();
    if (tkc == NULL) {
        log_error("\nError: could not malloc token chain\n");
        return ERR_INTERNAL_MEM;
    }
    tk_setup_chain_cli(tkc, line);
    status_t res = tk_tokenize_input(tkc, NULL);
    if (res != NO_ERR) {
        tk_free_chain(tkc);
        return res;
    }

    /* check the number of tokens parsed */
    uint32 tkcount = tk_token_count(tkc);
    if (LOGDEBUG2) {
        log_debug2("\nconfig token count: %u", tkcount);
    }
    if (tkcount == 0) {
        tk_free_chain(tkc);
        return NO_ERR;
    }

    obj_template_t *startobj = session_cb->config_curobj;
    val_value_t *startval = session_cb->config_curval;
    val_value_t *startroot = session_cb->config_etree;
    val_value_t *starteval = session_cb->config_ecurval;

    session_cb->config_no_active = FALSE;
    session_cb->config_estartval = starteval;
    session_cb->config_firstnew = NULL;

    boolean gotleafys = FALSE;
    boolean gotexit = FALSE;
    boolean gotapply = FALSE;
    boolean gotdo = FALSE;
    boolean done = FALSE;
    while (!done && res == NO_ERR) {
        /* parse nodes and possibly keys until a value node or the end
         * of the token chain is reached    */
        res = parse_node(tkc, session_cb, &done, &gotexit, &gotapply, &gotdo);
        obj_template_t *obj = session_cb->config_curobj;
        if (res == NO_ERR && done && !gotdo && !gotexit && !gotapply && obj &&
            obj_is_leafy(obj)) {
            /* get the next node as a value node */
            res = parse_value(session_cb, tkc);
            gotleafys = TRUE;
            if (tk_next_typ(tkc) != TK_TT_NONE) {
                res = ERR_NCX_EXTRA_NODE;
                log_error("\nError: unexpected input at end of line\n");
            }
            if (res == NO_ERR && !session_cb->config_no_active) {
                if (obj->parent && !obj_is_root(obj->parent)) {
                    session_cb->config_curobj = obj->parent;
                } else {
                    session_cb->config_curobj = NULL;
                }
                /* reset to parent of the leaf just parsed */
                session_cb->config_ecurval = session_cb->config_ecurval->parent;
            }
        }
    }
    
    /* check exit conditions */
    if (res != NO_ERR || done || gotexit || gotapply || gotdo) {
        if (res == NO_ERR && gotexit) {
            res = process_exit(server_cb, session_cb);
            if (res == NO_ERR) {
                res = set_config_path(session_cb);
            }
        } else if (res == NO_ERR && gotapply) {
            if (session_cb->config_curobj &&
                dlq_empty(&session_cb->config_editQ) &&
                session_cb->config_edit_mode != CFG_EDITMODE_LINE) {
                /* add an edit just because the user entered a
                 * sub-mode and then invoked apply
                 */
                res = add_edit(session_cb);
            }
            clear_current_level(session_cb);
            if (res == NO_ERR) {
                res = process_apply(server_cb, session_cb);
            }
        } else if (res == NO_ERR && gotdo) {
            res = process_do_command(server_cb, session_cb, tkc, line);
        } else if (res == NO_ERR && done) {
            res = process_line_done(server_cb, session_cb, startobj, gotleafys);
            if (res == NO_ERR && session_cb->config_curobj &&
                session_cb->config_curobj != startobj) {
                res = set_config_path(session_cb);
            }
        }
        if (res != NO_ERR) {
            /* reset current node pointers */
            session_cb->config_curobj = startobj;
            session_cb->config_curval = startval;
            if (startroot == NULL) {
                val_free_value(session_cb->config_etree);
                session_cb->config_etree = NULL;
                session_cb->config_ecurval = NULL;
                session_cb->config_firstnew = NULL;
            } else {
                if (session_cb->config_firstnew) {
                    if (session_cb->config_firstnew->parent) {
                        val_remove_child(session_cb->config_firstnew);
                    }
                    val_free_value(session_cb->config_firstnew);
                    session_cb->config_firstnew = NULL;
                }
                session_cb->config_etree = startroot;
                session_cb->config_ecurval = starteval;
            }
            (void)set_config_path(session_cb);
        }
    }

    tk_free_chain(tkc);
    return res;

}  /* handle_config_input */


/********************************************************************
 * FUNCTION force_exit_config_mode
 * (session was dropped -- exit config mode)
 * 
 * INPUTS:
 *    session_cb == session control block to use
 *********************************************************************/
void
    force_exit_config_mode (session_cb_t *session_cb)
{
    const xmlChar *nam = NCX_EL_DEFAULT;
    if (session_cb->session_cfg)  {
        nam = session_cb->session_cfg->name;
    }
    log_info("\nForced exit from configure mode for session '%s'\n", nam);
    exit_config_mode(session_cb);

} /* force_exit_config_mode */


/********************************************************************
 * FUNCTION config_check_transfer
 * autoconfig update is happening for this session
 * need to check if config mode and if the config_curval
 * pointer needs to be updated
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    rootval == <data> root from <get-config> reply
 *********************************************************************/
void
    config_check_transfer (session_cb_t *session_cb,
                           val_value_t *rootval)
{
    /* check if config mode active */
    if (!session_cb->config_mode) {
        return;
    }

    /* check if config_curval not set */
    if (!session_cb->config_curval) {
        return;
    }

    if (LOGDEBUG) {
        const xmlChar *nam = NCX_EL_DEFAULT;
        if (session_cb->session_cfg)  {
            nam = session_cb->session_cfg->name;
        }
        log_info("\nChecking config_curptr transfer for session '%s'\n", nam);
    }

    session_cb->config_curval =
        find_data_node(session_cb->config_curval,
                        session_cb->config_tree, rootval);

} /* config_check_transfer */



/* END yangcli_config.c */
