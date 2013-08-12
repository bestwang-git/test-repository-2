/*
 * Copyright (c) 2012, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: yangcli_sessions.c

   Client side session manager

   sessions command

   session command

   ~/.yumopro/.yangcli_pro_sessions.conf file


*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
27-aug-12    abb      begun;

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include "procdefs.h"
#include "conf.h"
#include "dlq.h"
#include "log.h"
#include "mgr_load.h"
#include "mgr.h"
#include "mgr_ses.h"
#include "mgr_ses.h"
#include "ncx.h"
#include "ncxconst.h"
#include "ncx_list.h"
#include "status.h"
#include "xml_util.h"
#include "xml_wr.h"
#include "yangcli.h"
#include "yangcli_cmd.h"
#include "yangcli_sessions.h"
#include "yangcli_util.h"


/********************************************************************
 * FUNCTION get_top_obj
 * 
 * Get the saved-sessions top object
 *
 * RETURNS:
 *   pointer to object template for top object 'saved-sessions'
 *********************************************************************/
static obj_template_t *
    get_top_obj (void)
{
    ncx_module_t *mod = get_yangcli_mod();
    if (mod == NULL) {
        return NULL;
    }

    obj_template_t *ssobj = 
        obj_find_template_top(mod, YANGCLI_MOD, 
                              YANGCLI_SAVED_SESSIONS);
    return ssobj;
}  /* get_top_obj */


/********************************************************************
 * FUNCTION make_child_leaf
 * 
 * Make a leaf for the specified session_cfg child
 * and return it
 *
 * INPUTS:
 *  sesobj == session parent object
 *  childname == child leaf name of sesobj parent
 *  valstr == string value to set
 *  res == address of return status
 * OUTPUTS:
 *  *res == return status
 * RETURNS:
 *   pointer to malloced value node; must free with val_free_value
 *********************************************************************/
static val_value_t *
    make_child_leaf (obj_template_t *sesobj,
                     const xmlChar *childname,
                     const xmlChar *valstr,
                     status_t *res)
{
    /* key session/name */
    obj_template_t *chobj = 
        obj_find_child(sesobj, YANGCLI_MOD, childname);
    if (chobj == NULL) {
        log_error("\nError: saved session missing '%s' element",
                  childname);
        *res = ERR_NCX_MISSING_PARM;
        return NULL;
    }
    return val_make_simval_obj(chobj, valstr, res);

}  /* make_child_leaf */


/********************************************************************
 * FUNCTION clean_session_cfgQ
 * 
 * Delete all session_cfg structs for a server context
 *
 * INPUTS:
 *    server_cb == server control block to use
 *********************************************************************/
static void clean_session_cfgQ (server_cb_t *server_cb)
{

    
    while (!dlq_empty(&server_cb->session_cfgQ)) {
        session_cfg_t *session_cfg =
            (session_cfg_t *)dlq_deque(&server_cb->session_cfgQ);
        free_session_cfg(session_cfg);
    }

}  /* clean_session_cfgQ */


/********************************************************************
 * FUNCTION show_session_cfg
 * 
 * Show the saved session
 *
 * INPUT:
 *   server_cb == server control block to use
 *   session_cfg == session_cfg to show
 *   mode == help mode
 *
 *********************************************************************/
static void
    show_session_cfg (server_cb_t *server_cb,
                      session_cfg_t *session_cfg,
                      help_mode_t mode)
{
    boolean isconnected = FALSE;
    const xmlChar *connected = EMPTY_STRING;
    session_cb_t *sescb = find_session_cb(server_cb, session_cfg->name);
    if (sescb && session_connected(sescb)) {
        connected = (const xmlChar *)"*";
        isconnected = TRUE;
    }

    switch (mode) {
    case HELP_MODE_BRIEF:
        log_info("\nSession '%s': %s@%s %s",
                 session_cfg->name, session_cfg->username,
                 session_cfg->server_addr, connected);
        break;
    case HELP_MODE_NORMAL:
        log_info("\nSession '%s':"
                 "\n   user: %s"
                 "\n   server: %s"
                 "\n   connected: %s\n",
                 session_cfg->name, session_cfg->username,
                 session_cfg->server_addr,
                 (isconnected) ? NCX_EL_TRUE : NCX_EL_FALSE);
        break;
    case HELP_MODE_FULL:
        log_info("\nSession '%s':"
                 "\n   user: %s"
                 "\n   server: %s"
                 "\n   connected: %s"
                 "\n   port: %u"
                 "\n   protocols: %s"
                 "\n   password: %s"
                 "\n   public-key: %s"
                 "\n   private-key: %s\n",
                 session_cfg->name, session_cfg->username,
                 session_cfg->server_addr,
                 (isconnected) ? NCX_EL_TRUE : NCX_EL_FALSE,
                 session_cfg->server_port,
                 ncx_get_protocols_enabled(session_cfg->server_protocols),
                 session_cfg->password ? session_cfg->password 
                 : EMPTY_STRING,
                 session_cfg->public_key ? session_cfg->public_key : "",
                 session_cfg->private_key ? session_cfg->private_key : "");
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

}  /* show_session_cfg */


/********************************************************************
 * FUNCTION show_sessions_cfg
 * 
 * Show the saved sessions in memory
 *
 * INPUT:
 *   server_cb == server control block to use
 *   mode == help mode
 *
 *********************************************************************/
static void
    show_sessions_cfg (server_cb_t *server_cb,
                       help_mode_t mode)
{

    session_cfg_t *session_cfg = (session_cfg_t *)
        dlq_firstEntry(&server_cb->session_cfgQ);
    if (session_cfg) {
        const xmlChar *source = server_cb->session_cfg_file;
        if (!server_cb->session_cfg_file_opened) {
            source = YANGCLI_MEMORY;
        } else if (source == NULL) {
            source = get_sessions_file();
        }
        log_info("\nSaved sessions source: '%s'", source);

        for (; session_cfg != NULL;
             session_cfg = (session_cfg_t *)dlq_nextEntry(session_cfg)) {

            show_session_cfg(server_cb, session_cfg, mode);
        }
    } else {
        log_info("\nNo saved sessions found in memory");
    }

}  /* show_sessions_cfg */


/********************************************************************
 * FUNCTION new_session_cfg_val
 * 
 * Create a val_value_t representation of 1 session_cfg struct
 *
 * INPUT:
 *   session_cfg == session config struct to use
 *   sesobj == object template to match session_cfg_t struct
 *   res == address of return status
 *
 * OUTPUTS:
 *   *res == return status

 * RETURNS:
 *   malloced val_value_t tree; need to free with val_free_value
 *********************************************************************/
static val_value_t *
    new_session_cfg_val (session_cfg_t *session_cfg,
                         obj_template_t *sesobj,
                         status_t *res)
{
    /* go through all the child nodes of the session element
     * and create a complete session_cfg_t struct
     */
    val_value_t *sesval = val_new_value();
    if (sesval == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    val_init_from_template(sesval, sesobj);

    
    /* key session/name */
    val_value_t *chval = make_child_leaf(sesobj, NCX_EL_NAME, 
                                         session_cfg->name, res);
    if (chval == NULL) {
        val_free_value(sesval);
        return NULL;
    }
    val_add_child(chval, sesval);

    /* generate the index chain in the indexQ */
    *res = val_gen_index_chain(sesobj, sesval);
    if (*res != NO_ERR) {
        val_free_value(sesval);
        return NULL;
    }

    /* mandatory leaf session/user */
    chval = make_child_leaf(sesobj, NCX_EL_USER,
                            session_cfg->username, res);
    if (chval == NULL) {
        val_free_value(sesval);
        return NULL;
    }
    val_add_child(chval, sesval);

    /* optional leaf session/password */
    if (session_cfg->password) {
        chval = make_child_leaf(sesobj, NCX_EL_PASSWORD,
                                session_cfg->password, res);
        if (chval == NULL) {
            val_free_value(sesval);
            return NULL;
        }
        val_add_child(chval, sesval);
    }

    /* optional leaf session/public-key */
    if (session_cfg->public_key) {
        chval = make_child_leaf(sesobj, YANGCLI_PUBLIC_KEY,
                                (const xmlChar *)session_cfg->public_key,
                                res);
        if (chval == NULL) {
            val_free_value(sesval);
            return NULL;
        }
        val_add_child(chval, sesval);
    }

    /* optional leaf session/private-key */
    if (session_cfg->private_key) {
        chval = make_child_leaf(sesobj, YANGCLI_PRIVATE_KEY,
                                (const xmlChar *)session_cfg->private_key,
                                res);
        if (chval == NULL) {
            val_free_value(sesval);
            return NULL;
        }
        val_add_child(chval, sesval);
    }

    /* mandatory leaf session/server */
    chval = make_child_leaf(sesobj, NCX_EL_SERVER,
                            session_cfg->server_addr, res);
    if (chval == NULL) {
        val_free_value(sesval);
        return NULL;
    }
    val_add_child(chval, sesval);


    /* optional leaf session/ncport; add set at all */
    if (session_cfg->server_port) {
        char buff[NCX_MAX_NUMLEN+1];
        sprintf(buff, "%u", session_cfg->server_port);

        chval = make_child_leaf(sesobj, YANGCLI_NCPORT, 
                                (const xmlChar *)buff, res);
        if (chval == NULL) {
            val_free_value(sesval);
            return NULL;
        }
        val_add_child(chval, sesval);
    }

    /* optional leaf session/protocols; add if set at all */
    if (session_cfg->server_protocols) {
        uint16 flags = session_cfg->server_protocols;
        chval = make_child_leaf(sesobj, NCX_EL_PROTOCOLS,
                                ncx_get_protocols_enabled(flags), res);
        if (chval == NULL) {
            val_free_value(sesval);
            return NULL;
        }
        val_add_child(chval, sesval);
    }

    /* optional container of raw lines session/start-commands */
    if (!dlq_empty(&session_cfg->rawlineQ)) {
        /* get the container child node */
        obj_template_t *conobj = 
            obj_find_child(sesobj, YANGCLI_MOD, YANGCLI_START_COMMANDS);

        /* create the start-commands container */
        val_value_t *conval = val_new_value();
        if (conval == NULL) {
            *res = ERR_INTERNAL_MEM;
            val_free_value(sesval);
            return NULL;
        }
        val_init_from_template(conval, conobj);
        val_add_child(conval, sesval);

        xmlns_id_t nsid = ncx_get_mod_nsid(get_yangcli_mod());
        rawline_t *rawline = 
            (rawline_t *)dlq_firstEntry(&session_cfg->rawlineQ);
        for (; rawline != NULL; 
             rawline = (rawline_t *)dlq_nextEntry(rawline)) {
            chval = val_make_string(nsid, NCX_EL_STRING,
                                    rawline->line);
            if (chval == NULL) {
                *res = ERR_INTERNAL_MEM;
                val_free_value(sesval);
                return NULL;
            }
            val_add_child(chval, conval);
        }
    }

    *res = NO_ERR;
    return sesval;

}  /* new_session_cfg_val */


/********************************************************************
 * FUNCTION new_session_cfg_clone
 * 
 * Malloc and fill in a new session_cfg struct from the conf file
 * value tree; Clone from existing record
 *
 * INPUT:
 *   name == new session name; empty to use default
 *   curcfg == server cfg to clone
 *   res == address of return status
 *
 * OUTPUTS:
 *   *res == return status

 * RETURNS:
 *   malloced server_cfg_t struct; need to free with free_session_cfg
 *********************************************************************/
static session_cfg_t *
    new_session_cfg_clone (const xmlChar *name,
                           session_cfg_t *curcfg,
                           status_t *res)
{
    /* go through all the child nodes of the session element
     * and create a complete session_cfg_t struct
     */
    session_cfg_t *session_cfg = m__getObj(session_cfg_t);
    if (session_cfg == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    memset(session_cfg, 0x0, sizeof(session_cfg_t));
    dlq_createSQue(&session_cfg->rawlineQ);

    /* key session/name */
    if (name == NULL) {
        if (curcfg->name) {
            session_cfg->name = xml_strdup(curcfg->name);
        } else {
            session_cfg->name = xml_strdup(NCX_EL_DEFAULT);
        }
    } else {
        session_cfg->name = xml_strdup(name);
    }
    if (session_cfg->name == NULL) {
        *res = ERR_INTERNAL_MEM;
        free_session_cfg(session_cfg);
        return NULL;
    }

    /* mandatory leaf session/user */
    session_cfg->username = xml_strdup(curcfg->username);
    if (session_cfg->username == NULL) {
        *res = ERR_INTERNAL_MEM;
        free_session_cfg(session_cfg);
        return NULL;
    }

    /* optional leaf session/password */
    if (curcfg->password) {
        session_cfg->password = xml_strdup(curcfg->password);
        if (session_cfg->password == NULL) {
            *res = ERR_INTERNAL_MEM;
            free_session_cfg(session_cfg);
            return NULL;
        }
    }

    /* optional leaf session/public-key */
    if (curcfg->public_key) {
        session_cfg->public_key = 
            (char *)xml_strdup((const xmlChar *)curcfg->public_key);
        if (session_cfg->public_key == NULL) {
            *res = ERR_INTERNAL_MEM;
            free_session_cfg(session_cfg);
            return NULL;
        }
    }

    /* optional leaf session/private-key */
    if (curcfg->private_key) {
        session_cfg->private_key = 
            (char *)xml_strdup((const xmlChar *)curcfg->private_key);
        if (session_cfg->private_key == NULL) {
            *res = ERR_INTERNAL_MEM;
            free_session_cfg(session_cfg);
            return NULL;
        }
    }

    /* mandatory leaf session/server */
    session_cfg->server_addr = xml_strdup(curcfg->server_addr);
    if (session_cfg->server_addr == NULL) {
        *res = ERR_INTERNAL_MEM;
        free_session_cfg(session_cfg);
        return NULL;
    }

    /* optional leaf session/ncport */
    session_cfg->server_port = curcfg->server_port;

    /* optional leaf session/protocols */
    session_cfg->server_protocols = curcfg->server_protocols;

    *res = NO_ERR;
    return session_cfg;

}  /* new_session_cfg_clone */


/********************************************************************
 * FUNCTION handle_start_session
 * 
 * start-session name=session-name
 *
 * Handle the start-session command
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    name == session name tp start
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    handle_start_session (server_cb_t *server_cb,
                          const xmlChar *name)
{
    uint32 count = session_connected_count(server_cb);
    if (count >= YANGCLI_MAX_SESSIONS) {
        log_error("\nError: Maximum number of open sessions reached");
        return ERR_NCX_RESOURCE_DENIED;
    }
        
    session_cfg_t *session_cfg = find_session_cfg(server_cb, name);
    if (session_cfg == NULL) {
        log_error("\nError: saved session '%s' not found\n", name);
        return ERR_NCX_INVALID_VALUE;
    }

    session_cb_t *session_cb = find_session_cb(server_cb, name);
    if (session_cb) {
        if (session_connected(session_cb)) {
            log_error("\nError: saved session '%s' already connected\n",
                      name);
            return ERR_NCX_RESOURCE_DENIED;
        }
    } else {
        session_cb = add_session_cb(server_cb, session_cfg);
        if (session_cb == NULL) {
            log_error("\nError: new session could not be allocated\n");
            return ERR_NCX_RESOURCE_DENIED;
        }
    }

    return create_session(server_cb, session_cb, session_cfg);

}   /* handle_start_session */


/********************************************************************
 * FUNCTION handle_stop_session
 * 
 * stop-session name=session-name
 *
 * Handle the stop-session command
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    name == session name tp stop
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    handle_stop_session (server_cb_t *server_cb,
                         const xmlChar *name)
{
    session_cb_t *session_cb = find_session_cb(server_cb, name);
    if (session_cb == NULL) {
        log_error("\nError: session '%s' not found\n", name);
        return ERR_NCX_INVALID_VALUE;
    }

    if (!session_connected(session_cb)) {
        log_error("\nError: session '%s' not connected\n", name);
        return ERR_NCX_OPERATION_FAILED;
    }

    return send_close_session_to_server(server_cb, session_cb);

}   /* handle_stop_session */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
 * FUNCTION do_session_cfg (local RPC)
 * 
 * session-cfg delete
 * session-cfg save[=filespec]
 * session-cfg set-current[=session-name]
 * session-cfg show[=session-name]
 *
 * Handle the session-cfg command
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the sessions command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_session_cfg (server_cb_t *server_cb,
                    obj_template_t *rpc,
                    const xmlChar *line,
                    uint32  len)
{
    status_t res = NO_ERR;

    val_value_t *valset = 
        get_valset(server_cb, rpc, &line[len], &res);

    if (res == NO_ERR && valset) {
        session_cb_t *session_cb = server_cb->cur_session_cb;
        const xmlChar *parmval = NULL;
        uint32 count = dlq_count(&server_cb->session_cfgQ);
        boolean done = FALSE;
        boolean delete = TRUE;

        /* get the 1 of N 'session-action' choice */            

        /* session-cfg delete */
        val_value_t *parm = val_find_child(valset, YANGCLI_MOD, 
                                           YANGCLI_DELETE);
        if (parm && parm->res == NO_ERR) {
            parmval = VAL_STR(parm);
            if (count) {
                session_cfg_t *session_cfg = 
                    find_session_cfg(server_cb, parmval);
                if (session_cfg) {
                    session_cb_t *sescb = find_session_cb(server_cb, parmval);
                    if (sescb != NULL) {
                        if (session_connected(sescb)) {
                            log_error("\nError: cannot delete session '%s' "
                                  "because it is active", parmval);
                            delete = FALSE;
                        } else {
                            delete_one_session_cb (server_cb, sescb);
                        } 
                    } 

                    if (delete == TRUE) {
                        log_info("\nDeleting saved session '%s' ",
                                 parmval);
                         dlq_remove(session_cfg);
                         free_session_cfg(session_cfg);
                         update_yangcli_param_change_flag (SESSIONS_FILE, TRUE);
                     }
                   
               } else {
                    log_error("\nError: saved session '%s' not found",
                              parmval);
               }
            } else {
                log_info("\nNo saved sessions found");
            }
            done = TRUE;
        }

        /* session-cfg save */
        if (!done) {
            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_SAVE);
            if (parm) {
                parmval = VAL_STR(parm);
                boolean swapped = FALSE;
                session_cfg_t *curcfg = 
                    find_session_cfg(server_cb, parmval);

                /* check current session config set */
                if (session_cb->session_cfg == NULL) {
                    log_error("\nError: cannot save current session "
                              "because it has not been active");
                } else {
                    if (curcfg) {
                        log_info("\nReplacing saved session '%s' "
                                 "with current session", parmval);
                    } else {
                        log_info("\nSaving current session as '%s'",
                                 parmval);
                    }

                    update_yangcli_param_change_flag (SESSIONS_FILE, TRUE);

                    /* add the current session as a saved session */
                    session_cfg_t *newcfg = 
                        new_session_cfg_clone(parmval,
                                              session_cb->session_cfg,
                                              &res);
                    if (newcfg) {
                        if (curcfg) {
                            /* replace current entry */
                            dlq_swap(newcfg, curcfg);
                            swapped = TRUE;
                        } else {
                            // TBD: add sorted; just add to end for now
                            dlq_enque(newcfg, &server_cb->session_cfgQ);
                        }
                    } else {
                        log_error("\nError: save session '%s' failed (%s)",
                                  parmval, get_error_string(res));
                    }
                }

                if (curcfg && swapped) {
                    free_session_cfg(curcfg);
                }

                done = TRUE;
            }
        }

        /* session-cfg show */
        if (!done) {
            // not really needed since default is show
            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_SHOW);
            help_mode_t mode = HELP_MODE_NORMAL;
            session_cfg_t *session_cfg = NULL;
            parmval = (parm) ? VAL_STR(parm) : NULL;
            if (parmval && *parmval == 0) {
                parmval = NULL;
                session_cfg = server_cb->cur_session_cb->session_cfg;
            } else {
                session_cfg = find_session_cfg(server_cb, parmval);
            }
            if (session_cfg == NULL) {
                if (parmval) {
                    log_error("\nError: saved session '%s' not found",
                              parmval);
                } else {
                    log_error("\nNo default saved session found");
                }
            } else {
                /* check if the 'brief' or 'full' flags are set first */
                val_value_t *showparm = 
                    val_find_child(valset, YANGCLI_MOD, YANGCLI_BRIEF);
                if (showparm && showparm->res == NO_ERR) {
                    mode = HELP_MODE_BRIEF;
                } else {
                    showparm = val_find_child(valset, YANGCLI_MOD, 
                                              YANGCLI_FULL);
                    if (showparm && showparm->res == NO_ERR) {
                        mode = HELP_MODE_FULL;
                    }
                }
                show_session_cfg(server_cb, session_cfg, mode);
            }
            done = TRUE;
        }
        log_info_append("\n");
    }

    if (valset) {
        val_free_value(valset);
    }

    return res;

}   /* do_session_cfg */


/********************************************************************
 * FUNCTION do_sessions_cfg (local RPC)
 * 
 * sessions-cfg clear
 * sessions-cfg load[=filespec]
 * sessions-cfg save[=filespec]
 *
 * Handle the sessions-cfg command
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the sessions command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_sessions_cfg (server_cb_t *server_cb,
                     obj_template_t *rpc,
                     const xmlChar *line,
                     uint32  len)
{
    status_t res = NO_ERR;

    val_value_t *valset = 
        get_valset(server_cb, rpc, &line[len], &res);

    if (res == NO_ERR && valset) {
        //session_cb_t *session_cb = server_cb->cur_session_cb;
        const xmlChar *parmval = NULL;
        uint32 count = dlq_count(&server_cb->session_cfgQ);
        boolean done = FALSE;

        /* get the 1 of N 'session-action' choice */            

        /* sessions clear */
        val_value_t *parm = val_find_child(valset, YANGCLI_MOD, 
                                           YANGCLI_CLEAR);
        if (parm && parm->res == NO_ERR) {
            if (count) {
                if (server_connected(server_cb)) {
                    log_error("\nError: session(s) active, "
                              "cannot clear all sessions\n");
                } else {
                    log_info("\nDeleting %u saved sessions\n", count);
                    //clean_session_cbQ(server_cb, FALSE);
                    clean_session_cfgQ(server_cb);
                    update_yangcli_param_change_flag (SESSIONS_FILE, TRUE);
                }
            } else {
                log_info("\nNo saved sessions found\n");
            }
            done = TRUE;
        }

        /* sessions-cfg load */
        if (!done) {
            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_LOAD);
            if (parm) {
                if (count) {
                    if (server_connected(server_cb)) {
                        log_error("\nError: session(s) active, "
                                  "cannot reload sessions\n");
                    } else {
                        if (count == 1) {
                            log_info("\nDeleting 1 saved session\n");
                        } else {
                        log_info("\nDeleting %u saved sessions\n", count);
                        }
                        //clean_session_cbQ(server_cb, FALSE);
                        clean_session_cfgQ(server_cb);
                    }
                }
                parmval = VAL_STR(parm);
                if (*parmval == 0) {
                    parmval = get_sessions_file();
                }
                
                boolean file_error = !val_set_by_default(parm);
                res = load_sessions(server_cb, parmval, file_error);
                if (res == NO_ERR) {
                    log_info("\nLoaded saved sessions OK from '%s'\n",
                             parmval);
                } else {
                    log_error("\nLoad sessions from '%s' "
                              "failed (%s)\n", parmval, 
                              get_error_string(res));
                }
                done = TRUE;
            }
        }

        /* sessions-cfg save */
        if (!done) {
            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_SAVE);
            if (parm) {
                parmval = VAL_STR(parm);
                if (*parmval == 0) {
                    parmval = get_sessions_file();
                }

                res = save_sessions(server_cb, parmval);
                if (res == NO_ERR) {
                    if (count) {
                        log_info("\nSaved %u sessions OK to '%s'\n",
                                 count, parmval);
                    } else {
                        log_info("\nSaved empty sessions file OK to '%s'\n",
                                 parmval);
                    }
                    if (server_cb->session_cfg_file) {
                        if (xml_strcmp(server_cb->session_cfg_file,
                                       parmval)) {
                            m__free(server_cb->session_cfg_file);
                            server_cb->session_cfg_file = xml_strdup(parmval);
                        }
                    } else {
                        server_cb->session_cfg_file = xml_strdup(parmval);
                    }
                    if (server_cb->session_cfg_file == NULL) {
                        res = ERR_INTERNAL_MEM;
                    }
                } else if (res != ERR_NCX_CANCELED) {
                    log_error("\nSave sessions to '%s' "
                              "failed (%s)\n", parmval,
                              get_error_string(res));
                }
                done = TRUE;
            }
        }

        /* sessions-cfg show */
        if (!done) {
            /* this is the default, so don't really need to get this parm */
            parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_SHOW);
            help_mode_t mode = HELP_MODE_NORMAL;

            /* check if the 'brief' or 'full' flags are set first */
            val_value_t *showparm = 
                val_find_child(valset, YANGCLI_MOD, YANGCLI_BRIEF);
            if (showparm && showparm->res == NO_ERR) {
                mode = HELP_MODE_BRIEF;
            } else {
                showparm = val_find_child(valset, YANGCLI_MOD, 
                                          YANGCLI_FULL);
                if (showparm && showparm->res == NO_ERR) {
                    mode = HELP_MODE_FULL;
                }
            }
            show_sessions_cfg(server_cb, mode);
            done = TRUE;
        }

        log_info_append("\n");
    }

    if (valset) {
        val_free_value(valset);
    }

    return res;

}   /* do_sessions_cfg */


/********************************************************************
 * FUNCTION load_sessions
 * 
 * Load the user variables from the specified filespec
 *
 * INPUT:
 *   server_cb == server control block to use
 *   fspec == input filespec to use (NULL == default)
 *   file_error = TRUE if missing file is an error
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    load_sessions (server_cb_t *server_cb,
                   const xmlChar *fspec,
                   boolean file_error)
{
    if (fspec == NULL) {
        fspec = get_sessions_file();
    }

    obj_template_t *ssobj = get_top_obj();
    if (ssobj == NULL) {
        log_error("\nError: yangcli-pro.yang is missing or "
                  "wrong version; no saved sessions loaded");
        return ERR_NCX_OPERATION_FAILED;
    }
    status_t res = NO_ERR;
    val_value_t *ssval = val_new_value();
    if (ssval == NULL) {
        return ERR_INTERNAL_MEM;
    }
    val_init_from_template(ssval, ssobj);

    server_cb->session_cfg_file_opened = FALSE;
    const xmlChar *banner = (const xmlChar *)"saved sessions";
    res = conf_parse_val_from_filespec_ex(fspec, banner, ssval, FALSE, 
                                          file_error,
                                          &server_cb->session_cfg_file_opened);
    if (res != NO_ERR || !server_cb->session_cfg_file_opened) {
        val_free_value(ssval);
        return res;
    }

    /* copy each val_value_t tree into the Q of session_cfg structs */
    val_value_t *sessionval = val_get_first_child(ssval);
    for (; res == NO_ERR && sessionval != NULL;
         sessionval = val_get_next_child(sessionval)) {

        session_cfg_t *session_cfg = new_session_cfg(sessionval, &res);
        if (res != NO_ERR) {
            free_session_cfg(session_cfg);
        } else {
            session_cfg_t *testcfg = find_session_cfg(server_cb,
                                                      session_cfg->name);
            if (testcfg || !xml_strcmp(session_cfg->name, NCX_EL_DEFAULT)) {
                log_error("\nError: skipping saved session '%s'; "
                          "name already used", session_cfg->name);
                free_session_cfg(session_cfg);
            } else {
                log_debug("\nAdding saved session '%s'",
                          session_cfg->name);
                dlq_enque(session_cfg, &server_cb->session_cfgQ);
            }
        }
    }

    /* not saving this value tree; the save_sessions function
     * will save the Q of session_cfg structs, not edit this tree
     */
    val_free_value(ssval);
    
    if (res == NO_ERR) {
        xmlChar  *fullspec = ncx_get_source(fspec, &res);
        if (res == NO_ERR) {
            res = update_def_yangcli_file_mtime (SESSIONS_FILE, fullspec);
        }
        if (fullspec) {
            m__free(fullspec);
        }
    }

    return res;

}  /* load_sessions */


/********************************************************************
 * FUNCTION save_sessions
 * 
 * Save the uservares to the specified filespec
 *
 * INPUT:
 *   server_cb == server control block to use
 *   fspec == output filespec to use  (NULL == default)
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    save_sessions (server_cb_t *server_cb,
                   const xmlChar *fspec)
{
    if (fspec == NULL) {
        fspec = get_sessions_file();
    }

    status_t res = NO_ERR;
    xmlChar *fullspec = ncx_get_source(fspec, &res);
    if (res == NO_ERR) {
        res = check_for_saving_def_yangcli_file (SESSIONS_FILE, fullspec);
        if (res != NO_ERR) {
            m__free(fullspec);
            return res;
        }
    } 

    obj_template_t *ssobj = get_top_obj();
    if (ssobj == NULL) {
        log_error("\nError: yangcli-pro.yang is missing or "
                  "wrong version; no sessions saved");
        m__free(fullspec);
        return ERR_NCX_OPERATION_FAILED;
    }

    obj_template_t *sesobj = obj_find_child(ssobj, YANGCLI_MOD,
                                            YANGCLI_SESSION);
    val_value_t *ssval = val_new_value();
    if (ssval == NULL) {
        return ERR_INTERNAL_MEM;
    }
    val_init_from_template(ssval, ssobj);

    /* copy each val_value_t tree into the Q of session_cfg structs */
    session_cfg_t *session_cfg = (session_cfg_t *)
        dlq_firstEntry(&server_cb->session_cfgQ);
    for (; res == NO_ERR && session_cfg != NULL;
         session_cfg = (session_cfg_t *)dlq_nextEntry(session_cfg)) {

        val_value_t *sesval = new_session_cfg_val(session_cfg, 
                                                  sesobj, &res);
        if (res != NO_ERR) {
            val_free_value(sesval);
        } else {
            val_add_child(sesval, ssval);
        }
    }

    if (res == NO_ERR) {
        /* write the value tree as a config file */
        res = log_alt_open_ex((const char *)fullspec, TRUE);
        if (res != NO_ERR) {
            log_error("\nError: sessions file '%s' could "
                      "not be opened (%s)",
                      fullspec, get_error_string(res));
        } else {
            val_dump_value_full(ssval,
                                0,   /* startindent */
                                get_defindent(),
                                DUMP_VAL_ALT_LOG, /* dumpmode */
                                NCX_DISPLAY_MODE_PLAIN,
                                FALSE,    /* withmeta */
                                FALSE,   /* configonly */
                                TRUE);   /* conf_mode */
             log_alt_close();

             res = update_def_yangcli_file_mtime (SESSIONS_FILE, fullspec);
        }
    }

    /* not saving this value tree; the save_sessions function
     * will save the Q of session_cfg structs, not edit this tree
     */
    val_free_value(ssval);

    m__free(fullspec);
    return res;


}  /* save_sessions */


/********************************************************************
 * FUNCTION free_session_cfg
 * 
 * Free a session_cfg struct
 *
 * INPUT:
 *   session_cfg == session config struct to free
 *********************************************************************/
void
    free_session_cfg (session_cfg_t *session_cfg)

{
    if (session_cfg == NULL) {
        return;
    }
    m__free(session_cfg->name);
    m__free(session_cfg->username);
    m__free(session_cfg->password);
    m__free(session_cfg->public_key);
    m__free(session_cfg->private_key);
    m__free(session_cfg->server_addr);

    while (!dlq_empty(&session_cfg->rawlineQ)) {
        rawline_t *rawline = (rawline_t *)
            dlq_deque(&session_cfg->rawlineQ);
        free_rawline(rawline);
    }
    m__free(session_cfg);

}  /* free_session_cfg */


/********************************************************************
 * FUNCTION new_session_cfg
 * 
 * Malloc and fill in a new session_cfg struct from the conf file
 * value tree
 *
 * INPUT:
 *   sesval == session value sub-tree to use
 *   res == address of return status
 *
 * OUTPUTS:
 *   *res == return status

 * RETURNS:
 *   malloced server_cfg_t struct; need to free with free_session_cfg
 *********************************************************************/
session_cfg_t *
    new_session_cfg (val_value_t *sesval,
                     status_t *res)
{
    /* go through all the child nodes of the session element
     * and create a complete session_cfg_t struct
     */
    session_cfg_t *session_cfg = m__getObj(session_cfg_t);
    if (session_cfg == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    memset(session_cfg, 0x0, sizeof(session_cfg_t));
    dlq_createSQue(&session_cfg->rawlineQ);

    /* key session/name */
    val_value_t *chval = val_find_child(sesval, YANGCLI_MOD, NCX_EL_NAME);
    if (chval == NULL) {
        log_error("\nError: saved session missing 'name' element");
        *res = ERR_NCX_MISSING_PARM;
        free_session_cfg(session_cfg);
        return NULL;
    }
    session_cfg->name = xml_strdup(VAL_STR(chval));
    if (session_cfg->name == NULL) {
        *res = ERR_INTERNAL_MEM;
        free_session_cfg(session_cfg);
        return NULL;
    }

    /* mandatory leaf session/user */
    chval = val_find_child(sesval, YANGCLI_MOD, NCX_EL_USER);
    if (chval == NULL) {
        log_error("\nError: saved session missing 'user' element");
        *res = ERR_NCX_MISSING_PARM;
        free_session_cfg(session_cfg);
        return NULL;
    }
    session_cfg->username = xml_strdup(VAL_STR(chval));
    if (session_cfg->username == NULL) {
        *res = ERR_INTERNAL_MEM;
        free_session_cfg(session_cfg);
        return NULL;
    }

    /* optional leaf session/password */
    chval = val_find_child(sesval, YANGCLI_MOD, NCX_EL_PASSWORD);
    if (chval != NULL) {
        session_cfg->password = xml_strdup(VAL_STR(chval));
        if (session_cfg->password == NULL) {
            *res = ERR_INTERNAL_MEM;
            free_session_cfg(session_cfg);
            return NULL;
        }
    }

    /* optional leaf session/public-key */
    chval = val_find_child(sesval, YANGCLI_MOD, YANGCLI_PUBLIC_KEY);
    if (chval && chval->res == NO_ERR) {
        session_cfg->public_key = (char *)xml_strdup(VAL_STR(chval));
        if (session_cfg->public_key == NULL) {
            *res = ERR_INTERNAL_MEM;
            free_session_cfg(session_cfg);
            return NULL;
        }
    }

    /* optional leaf session/private-key */
    chval = val_find_child(sesval, YANGCLI_MOD, YANGCLI_PRIVATE_KEY);
    if (chval && chval->res == NO_ERR) {
        session_cfg->private_key = (char *)xml_strdup(VAL_STR(chval));
        if (session_cfg->private_key == NULL) {
            *res = ERR_INTERNAL_MEM;
            free_session_cfg(session_cfg);
            return NULL;
        }
    }

    /* mandatory leaf session/server */
    chval = val_find_child(sesval, YANGCLI_MOD, NCX_EL_SERVER);
    if (chval == NULL) {
        log_error("\nError: saved session missing 'server' element");
        *res = ERR_NCX_MISSING_PARM;
        free_session_cfg(session_cfg);
        return NULL;
    }
    session_cfg->server_addr = xml_strdup(VAL_STR(chval));
    if (session_cfg->server_addr == NULL) {
        *res = ERR_INTERNAL_MEM;
        free_session_cfg(session_cfg);
        return NULL;
    }

    /* optional leaf session/ncport */
    chval = val_find_child(sesval, YANGCLI_MOD, YANGCLI_NCPORT);
    if (chval && chval->res == NO_ERR) {
        session_cfg->server_port = VAL_UINT16(chval);
    }

    /* optional leaf session/protocols */
    chval = val_find_child(sesval, YANGCLI_MOD, NCX_EL_PROTOCOLS);
    if (chval && chval->res == NO_ERR) {
        if (ncx_string_in_list(NCX_EL_NETCONF10, &(VAL_BITS(chval)))) {
            ncx_set_val_protocol_enabled(NCX_PROTO_NETCONF10,
                                         &session_cfg->server_protocols);
        }
        if (ncx_string_in_list(NCX_EL_NETCONF11,  &(VAL_BITS(chval)))) {
            ncx_set_val_protocol_enabled(NCX_PROTO_NETCONF11,
                                         &session_cfg->server_protocols);
        }
    }

    /* optional container of raw lines session/start-commands */
    chval = val_find_child(sesval, YANGCLI_MOD, YANGCLI_START_COMMANDS);
    if (chval && chval->res == NO_ERR) {
        val_value_t *strval = val_get_first_child(chval);
        for (; strval != NULL; strval = val_get_next_child(strval)) {
            rawline_t *rawline = new_rawline(VAL_STR(strval));
            if (rawline == NULL) {
                *res = ERR_INTERNAL_MEM;
                free_session_cfg(session_cfg);
                return NULL;
            }
            dlq_enque(rawline, &session_cfg->rawlineQ);
        }
    }

    *res = NO_ERR;
    return session_cfg;

}  /* new_session_cfg */


/********************************************************************
 * FUNCTION new_session_cfg_cli
 * 
 * Malloc and fill in a new session_cfg struct from the conf file
 * value tree; CLI version
 *
 * INPUT:
 *   name == session name (NULL to get default)
 *   server == server address or DNS name
 *   username == SSH username
 *   password == password (may be NULL)
 *   publickey == public key filespec (may be NULL)
 *   privatekey == private key filespec (may be NULL)
 *   ncport == TCP port # to use (d:830)
 *   protocols == NETCONF protocol versions to try
 *   res == address of return status
 *
 * OUTPUTS:
 *   *res == return status

 * RETURNS:
 *   malloced server_cfg_t struct; need to free with free_session_cfg
 *********************************************************************/
session_cfg_t *
    new_session_cfg_cli (const xmlChar *name,
                         const xmlChar *server,
                         const xmlChar *username,
                         const xmlChar *password,
                         const char *publickey,
                         const char *privatekey,
                         uint16 ncport,
                         uint16 protocols,
                         status_t *res)
{
    /* go through all the child nodes of the session element
     * and create a complete session_cfg_t struct
     */
    session_cfg_t *session_cfg = m__getObj(session_cfg_t);
    if (session_cfg == NULL) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    memset(session_cfg, 0x0, sizeof(session_cfg_t));
    dlq_createSQue(&session_cfg->rawlineQ);

    /* key session/name */
    if (name == NULL) {
        session_cfg->name = xml_strdup(NCX_EL_DEFAULT);
    } else {
        session_cfg->name = xml_strdup(name);
    }
    if (session_cfg->name == NULL) {
        *res = ERR_INTERNAL_MEM;
        free_session_cfg(session_cfg);
        return NULL;
    }

    /* mandatory leaf session/user */
    session_cfg->username = xml_strdup(username);
    if (session_cfg->username == NULL) {
        *res = ERR_INTERNAL_MEM;
        free_session_cfg(session_cfg);
        return NULL;
    }

    /* optional leaf session/password */
    if (password) {
        session_cfg->password = xml_strdup(password);
        if (session_cfg->password == NULL) {
            *res = ERR_INTERNAL_MEM;
            free_session_cfg(session_cfg);
            return NULL;
        }
    }

    /* optional leaf session/public-key */
    if (publickey) {
        session_cfg->public_key = 
            (char *)xml_strdup((const xmlChar *)publickey);
        if (session_cfg->public_key == NULL) {
            *res = ERR_INTERNAL_MEM;
            free_session_cfg(session_cfg);
            return NULL;
        }
    }

    /* optional leaf session/private-key */
    if (privatekey) {
        session_cfg->private_key = 
            (char *)xml_strdup((const xmlChar *)privatekey);
        if (session_cfg->private_key == NULL) {
            *res = ERR_INTERNAL_MEM;
            free_session_cfg(session_cfg);
            return NULL;
        }
    }

    /* mandatory leaf session/server */
    session_cfg->server_addr = xml_strdup(server);
    if (session_cfg->server_addr == NULL) {
        *res = ERR_INTERNAL_MEM;
        free_session_cfg(session_cfg);
        return NULL;
    }

    /* optional leaf session/ncport */
    session_cfg->server_port = ncport;

    /* optional leaf session/protocols */
    session_cfg->server_protocols = protocols;

    *res = NO_ERR;
    return session_cfg;

}  /* new_session_cfg_cli */



/********************************************************************
 * FUNCTION do_start_session (local RPC)
 * 
 * start-session name=session-name
 *
 * Handle the start-session command
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the sessions command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_start_session (server_cb_t *server_cb,
                      obj_template_t *rpc,
                      const xmlChar *line,
                      uint32  len)
{
    status_t res = NO_ERR;

    val_value_t *valset = 
        get_valset(server_cb, rpc, &line[len], &res);

    const xmlChar *name = NULL;
    if (res == NO_ERR && valset) {
        val_value_t *parm = 
            val_find_child(valset, YANGCLI_MOD, NCX_EL_NAME);
        if (parm && parm->res == NO_ERR) {
            name = VAL_STR(parm);
        }
    }

    if (name) {
        res = handle_start_session(server_cb, name);
    }

    if (valset) {
        val_free_value(valset);
    }

    return res;

}   /* do_start_session */


/********************************************************************
 * FUNCTION do_stop_session (local RPC)
 * 
 * stop-session name=session-name
 *
 * Handle the stop-session command
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the sessions command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_stop_session (server_cb_t *server_cb,
                     obj_template_t *rpc,
                     const xmlChar *line,
                     uint32  len)
{
    status_t res = NO_ERR;

    val_value_t *valset = 
        get_valset(server_cb, rpc, &line[len], &res);

    const xmlChar *name = NULL;
    if (res == NO_ERR && valset) {
        val_value_t *parm = 
            val_find_child(valset, YANGCLI_MOD, NCX_EL_NAME);
        if (parm && parm->res == NO_ERR) {
            name = VAL_STR(parm);
        }
    }

    if (name) {
        res = handle_stop_session(server_cb, name);
    }

    if (valset) {
        val_free_value(valset);
    }

    return res;

}   /* do_stop_session */


/********************************************************************
 * FUNCTION do_session (local RPC)
 * 
 * session set-current=session-name
 *
 * Handle the session command
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the sessions command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_session (server_cb_t *server_cb,
                obj_template_t *rpc,
                const xmlChar *line,
                uint32  len)
{
    status_t res = NO_ERR;
    val_value_t *valset = get_valset(server_cb, rpc, &line[len], &res);

    if (res == NO_ERR && valset) {
        /* session set-current */
        val_value_t *parm = 
            val_find_child(valset, YANGCLI_MOD, YANGCLI_SET_CURRENT);
        if (parm) {
            const xmlChar *parmval = VAL_STR(parm);
            session_cb_t *sescb = find_session_cb(server_cb, parmval);
            if (sescb == NULL) {
                res = ERR_NCX_DEF_NOT_FOUND;
                log_error("\nError: active session '%s' not found",
                          parmval);
            } else {
                if (!session_connected(sescb)) {
                    log_info("\nNote: session '%s' is not connected",
                             parmval);
                }
                set_cur_session_cb(server_cb, sescb);
                log_info("\nSession '%s' is now active\n",  parmval);
            }
        }
    }

    if (valset) {
        val_free_value(valset);
    }

    return res;

}   /* do_session */

/********************************************************************
 * FUNCTION check_connect_all_sessions
 *
 * INPUT:
 * RETURNS: status.
 *********************************************************************/
status_t
    check_connect_all_sessions (server_cb_t *server_cb)
{
   /* Check if connect_all command in progress */
   if ( !check_connect_all_in_progress()) {
      return NO_ERR;
   }

   /* Continue connect session  */

   session_cfg_t* session_cfg;
   session_cb_t *session_cb;
   status_t res = NO_ERR;

   session_cfg = (session_cfg_t *)
           dlq_firstEntry(&server_cb->session_cfgQ);

   for (; session_cfg != NULL;
           session_cfg = (session_cfg_t *)dlq_nextEntry(session_cfg)) {
           session_cb = find_session_cb(server_cb, session_cfg->name);

           if ((session_cfg && !session_cb) ||
                  (session_cb && !session_connected(session_cb))) {
              res = handle_start_session(server_cb, session_cfg->name);
              break;
           }
    }

    /* Update in_progress flag to indicate that all sessions are connected */
    if (!session_cfg) {
       update_connect_all_in_progress (FALSE);
    }

    return res;

} /* check_connect_all_sessions */


/********************************************************************
 * FUNCTION connect_all_sessions
 *
 * connect_all_sessions 
 *
 * INPUT:
 *   valset == connection param
 *   server_cb == server cb to use
 *
 * RETURNS: status.
 * 
 *********************************************************************/
status_t
 connect_all_sessions (const val_value_t *valset,
                       server_cb_t *server_cb)
{
    status_t res = NO_ERR;
    
    if (!valset) {
        /* there were no parameters entered; just connect keyword */
       return ERR_NCX_SKIPPED;
    }

    /* Check if connect-all specified */
    val_value_t *name_val =
        val_find_child(valset, YANGCLI_MOD, YANGCLI_CONNECT_ALL);
    if (!name_val) {
        return ERR_NCX_SKIPPED;
    }

    // check_any_params_entered (valset, server_cb);

    val_value_t *user_val = val_find_child(valset,
                         YANGCLI_MOD, YANGCLI_USER);

    val_value_t *passwd_val = val_find_child(valset,
                         YANGCLI_MOD, YANGCLI_PASSWORD);

    val_value_t *public_key_val = val_find_child(valset,
                         YANGCLI_MOD, YANGCLI_PUBLIC_KEY);

    val_value_t *private_key_val = val_find_child(valset,
                       YANGCLI_MOD, YANGCLI_PRIVATE_KEY);

    val_value_t *server_val = val_find_child(valset,
                         YANGCLI_MOD, YANGCLI_SERVER);

    val_value_t *port_val = val_find_child(valset,
                         YANGCLI_MOD, YANGCLI_NCPORT);

    val_value_t *protocol_val = val_find_child(valset,
                        YANGCLI_MOD, YANGCLI_PROTOCOLS);

    xmlChar *user = NULL;
    xmlChar *passwd = NULL;
    xmlChar *server = NULL;
    char *public_key = NULL;
    char *private_key = NULL;
    uint16   port = 0;
    uint16  protocol = 0;
    session_cfg_t *cfg = NULL;
    session_cfg_t *newcfg = NULL;
    session_cb_t *session_cb = NULL;

    if (passwd_val) {
        passwd = xml_strdup(VAL_STR(passwd_val));
    }
    if (user_val) {
        user = xml_strdup(VAL_STR(user_val));
    }
    if (server_val) {
        server = xml_strdup(VAL_STR(server_val));
    }
    if (public_key_val) {
        public_key = (char *)xml_strdup(VAL_STR(public_key_val));
    } 
    if (private_key_val) {
        private_key = (char *)xml_strdup(VAL_STR(private_key_val));
    } 
    if (port_val) {
        port = VAL_UINT16(port_val);
    }
    if (protocol_val) {
        protocol = VAL_UINT16(protocol_val);
    }

    // Update the saved sessionsi' params if there is param entered.
    
    if (user || passwd || server || public_key ||
         (port>0) || private_key || (protocol>0)) {

       cfg = (session_cfg_t *)
           dlq_firstEntry(&server_cb->session_cfgQ);
       for (; cfg != NULL;
           cfg = (session_cfg_t *)dlq_nextEntry(cfg)) {
           session_cb = find_session_cb(server_cb, cfg->name);
           if (session_cb) {
              if (session_connected(session_cb)) {
                  continue;
              } else {
                  delete_one_session_cb (server_cb, session_cb);
              }
           }

          if (!user){
              user = xml_strdup(cfg->username);
          }
          if (!passwd){
              passwd = xml_strdup(cfg->password);
          }
          if (!server){
              server = xml_strdup(cfg->server_addr);
          }
          if (!public_key){
              public_key = (char *)xml_strdup((const xmlChar *)cfg->public_key);
          }
          if (port==0){
              port = cfg->server_port;
          }
          if (!private_key){
              private_key = (char *)xml_strdup((const xmlChar *)cfg->private_key);
          }
          if (protocol ==0) {
              protocol = cfg->server_protocols ;
          }

          newcfg = new_session_cfg_cli(cfg->name, server, user, passwd,
                   public_key, private_key, port,  protocol, &res);
          if (newcfg) {
             dlq_swap(newcfg, cfg);
             free_session_cfg(cfg);
             cfg = newcfg;
           } else {
              res = ERR_NCX_RESOURCE_DENIED;
              break;
           }
       }
    }

    if (res == NO_ERR) {
        update_connect_all_in_progress (TRUE);
        res = check_connect_all_sessions (server_cb);
    } 
        
    /* Cleanup: Free local allocated memory. */
     m__free(server);
     m__free(user);
     m__free(passwd);
     m__free(public_key);
     m__free(private_key);
    
     return res;

} /* connect_all_sessions */

/********************************************************************
 * FUNCTION connect_named_session
 *
 * Connect a named session.
 *
 * INPUT:
 *   valset == valset of connection params.
 *   server_cb == server cb to clone
 *
 * RETURNS: status.
 *********************************************************************/
status_t
    connect_named_session (const val_value_t *valset,
                            server_cb_t *server_cb)
{
    status_t res = NO_ERR;
    xmlChar *name = NULL;
    val_value_t *name_val = NULL;

    if (!valset) {
        /* there was no valset entered, just connect keyword */
       return ERR_NCX_SKIPPED;
    }

    /* Check if session-name specified */
    name_val = val_find_child(valset,
            YANGCLI_MOD, YANGCLI_SESSION_NAME);
    if (!name_val) {
        return ERR_NCX_SKIPPED;
    }

    /* Check if this is a default session */
    name =xml_strdup(VAL_STR(name_val));
    if (!xml_strcmp(name, YANGCLI_DEF_SERVER)) {
        m__free(name);
        return ERR_NCX_SKIPPED;
    }

    /* Start to connect a none default session */

    val_value_t *user_val = NULL;
    val_value_t *passwd_val = NULL;
    val_value_t *public_key_val = NULL;
    val_value_t *private_key_val = NULL;
    val_value_t *server_val = NULL;
    val_value_t *port_val = NULL;
    val_value_t *protocol_val = NULL;
    xmlChar *user = NULL;
    xmlChar *passwd = NULL;
    xmlChar *server = NULL;
    char *public_key = NULL;
    char *private_key = NULL;
    uint16   port = 0;
    uint16  protocol = 0;
    session_cfg_t *cfg = NULL;
    session_cfg_t *newcfg = NULL;
    session_cfg_t *curcfg = NULL;

    session_cb_t *session_cb = find_session_cb(server_cb, name);
    if (session_cb) {
        if (session_connected(session_cb)) {
            log_error("\nError: saved session '%s' already connected\n",
                      name);
            m__free(name);
            return ERR_NCX_RESOURCE_DENIED;
        }
    }

    user_val = val_find_child(valset,
                         YANGCLI_MOD, YANGCLI_USER);

    passwd_val = val_find_child(valset,
                         YANGCLI_MOD, YANGCLI_PASSWORD);

    public_key_val = val_find_child(valset,
                         YANGCLI_MOD, YANGCLI_PUBLIC_KEY);

    private_key_val = val_find_child(valset,
                       YANGCLI_MOD, YANGCLI_PRIVATE_KEY);

    server_val = val_find_child(valset,
                         YANGCLI_MOD, YANGCLI_SERVER);

    port_val = val_find_child(valset,
                         YANGCLI_MOD, YANGCLI_NCPORT);

    protocol_val = val_find_child(valset, 
                        YANGCLI_MOD, YANGCLI_PROTOCOLS);

    /* Check if session-name has been used. */
    curcfg = find_session_cfg(server_cb, name);
    if (curcfg) {
        cfg = curcfg;
        log_info("\nsession '%s' is found in session_cfgQ \n", name);
    } else {
        log_info("\nsession '%s' Check if there is an active session.\n", name);
        cfg = server_cb->cur_session_cb->session_cfg;

        /* Need to borrow any missing param from other active session. */
        /* if cli of (connect command) does not have full params.      */

        if ( (!user_val || !passwd_val || !server_val || !public_key_val ||
            (port_val==0) || !private_key_val || (protocol_val==0)) 
             && (!cfg) ) {
            log_info("\nNote:There is no current active session.");
            log_error("\nError:Cannot connect a new session '%s', "
                      "incomplete params. \n",name);
            m__free(name);
            return ERR_NCX_OPERATION_FAILED;
        }
     }

    /* 1: Get all the params from the connect command entered by user. */
    /* 2: If user did not provide all the params,  fill missing
       one from the saved session */
    /* 3: If no saved session configured, get params from the current
       active session.*/

    if (passwd_val) {
        passwd = xml_strdup(VAL_STR(passwd_val));
    } else {
        passwd = xml_strdup(cfg->password);
    }

    if (user_val) {
        user = xml_strdup(VAL_STR(user_val));
     }else {
        user = xml_strdup(cfg->username);
     }

     if (server_val) {
         server = xml_strdup(VAL_STR(server_val));
     }else {
         server = xml_strdup(cfg->server_addr);
     }

     if (public_key_val) {
         public_key = (char *)xml_strdup(VAL_STR(public_key_val));
     } else {
         public_key = (char *)xml_strdup((const xmlChar *)cfg->public_key);
     }

     if (private_key_val) {
          private_key = (char *)xml_strdup(VAL_STR(private_key_val));
      } else {
          private_key = (char *)xml_strdup((const xmlChar *)cfg->private_key);
      }

     if (port_val) {
          port = VAL_UINT16(port_val);
     }else {
          port = cfg->server_port;
     }

     if (protocol_val) {
          protocol = VAL_UINT16(protocol_val);
      }else {
          protocol = cfg->server_protocols ;
     }

     /* Create a new session cfg struct */
     newcfg = new_session_cfg_cli(name, server, user, passwd,
              public_key, private_key, port,  protocol, &res);

     if (newcfg) {
         if (curcfg) {
             dlq_swap(newcfg, curcfg);
             free_session_cfg(curcfg);
         } else {
             dlq_enque(newcfg, &server_cb->session_cfgQ);
         }
     } else {
          res = ERR_NCX_RESOURCE_DENIED;
     }

     if (session_cb) {
        ses_cb_t *scb = mgr_ses_get_scb(session_cb->mysid);
        if (!scb) {
            delete_one_session_cb (server_cb, session_cb);
        }
     }

     if (res == NO_ERR)  {
         res = handle_start_session(server_cb, name);
     }

     /* Cleanup: Free local allocated memory. */
     m__free(name);
     m__free(server);
     m__free(user);
     m__free(passwd);
     m__free(public_key);
     m__free(private_key);

     return res;

} /* connect_named_session */ 

/* END yangcli_sessions.c */
