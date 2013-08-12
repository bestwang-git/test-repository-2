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
/*  FILE: yangcli_util.c

   Utilities for NETCONF YANG-based CLI Tool

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
01-jun-08    abb      begun; started from ncxcli.c
27-mar-09    abb      split out from yangcli.c

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
#include "cap.h"
#include "log.h"
#include "mgr.h"
#include "mgr_ses.h"
#include "ncx.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "runstack.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "var.h"
#include "xmlns.h"
#include "xml_util.h"
#include "xml_val.h"
#include "xpath.h"
#include "xpath_yang.h"
#include "yangconst.h"
#include "yangcli.h"
#include "yangcli_config.h"
#include "yangcli_util.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#define NAME_BUFF_SIZE  256


/********************************************************************
*                                                                   *
*                          T Y P E S                                *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
 * FUNCTION check_find_top_obj
 * 
 * Check a module for a specified object (full or partial name)
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    mod == module to check
 *    namestr == name string to find
 *
 * RETURNS:
 *   pointer to object if found, NULL if not found
 *********************************************************************/
static obj_template_t * 
    check_find_top_obj (session_cb_t *session_cb,
                        ncx_module_t *mod,
                        const xmlChar *namestr)
{
    status_t res = NO_ERR;
    obj_template_t *modObj = 
        obj_find_template_top_ex(mod, NULL, /*modname = any mod name */
                                 namestr, /* objname */
                                 session_cb->match_names,
                                 session_cb->alt_names,
                                 TRUE, /* dataonly */
                                 &res);

    if (res != NO_ERR && res != ERR_NCX_DEF_NOT_FOUND) {
        log_error("\nError: could not retrieve top object name (%s)",
                  get_error_string(res));
    }

    return modObj;

}  /* check_find_top_obj */



/********************************************************************
 * FUNCTION check_find_top_obj_str
 * 
 * Check a module for a specified object (full or partial name)
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    mod == module to check
 *    namestr == name string to find
 *    namestrlen == length of namestr to use
 * RETURNS:
 *   pointer to object if found, NULL if not found
 *********************************************************************/
static obj_template_t * 
    check_find_top_obj_str (session_cb_t *session_cb,
                            ncx_module_t *mod,
                            const xmlChar *namestr,
                            int namestrlen)
{
    xmlChar *usebuff = NULL;
    xmlChar buff[NAME_BUFF_SIZE];

    if (namestrlen >= NAME_BUFF_SIZE) {
        usebuff = m__getMem(namestrlen+1);
        if (usebuff == NULL) {
            log_error("\nError: malloc for name string failed");
            return NULL;
        }
    } else {
        usebuff = buff;
    }

    xml_strncpy(usebuff, namestr, namestrlen);

    status_t res = NO_ERR;
    obj_template_t *modObj = 
        obj_find_template_top_ex(mod, NULL, /*modname = any mod name */
                                 usebuff, /* objname */
                                 session_cb->match_names,
                                 session_cb->alt_names,
                                 TRUE, /* dataonly */
                                 &res);

    if (res != NO_ERR && res != ERR_NCX_DEF_NOT_FOUND) {
        log_error("\nError: could not retrieve top object name (%s)",
                  get_error_string(res));
    }

    if (usebuff != buff) {
        m__free(usebuff);
    }

    return modObj;

}  /* check_find_top_obj_str */


/********************************************************************
 * FUNCTION insert_help_backptr
 * 
 * Insert a help back pointer in order in the help_backptrQ
 *
 * INPUTS:
 *    comstate == completion state to use
 *    newnode == new backptr node to add
 *    nodetyp == node type of node
 * RETURNS:
 *   status; ERR_NCX_SKIPPED if entry exists
 *********************************************************************/
static status_t
    insert_help_backptr (completion_state_t *comstate,
                         ncx_backptr_t *newnode,
                         ncx_node_t nodetyp)
{
    obj_template_t *newobj = NULL;
    val_value_t *newval = NULL;

    if (nodetyp == NCX_NT_OBJ) {
        if (comstate->help_backptr_type == NCX_NT_NONE) {
            comstate->help_backptr_type = NCX_NT_OBJ;
        } else if (comstate->help_backptr_type != NCX_NT_OBJ) {
            return ERR_NCX_WRONG_TYPE;
        }
        newobj = (obj_template_t *)newnode->node;
    } else if (nodetyp == NCX_NT_VAL) {
        if (comstate->help_backptr_type == NCX_NT_NONE) {
            comstate->help_backptr_type = NCX_NT_VAL;
        } else if (comstate->help_backptr_type != NCX_NT_VAL) {
            return ERR_NCX_WRONG_TYPE;
        }
        newval = (val_value_t *)newnode->node;
    } else {
        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    ncx_backptr_t *backptr =
        ncx_first_backptr(&comstate->help_backptrQ);
    for (; backptr; backptr = ncx_next_backptr(backptr)) {
        if (nodetyp == NCX_NT_OBJ) {
            obj_template_t *curobj = (obj_template_t *)backptr->node;

            /* compare object names */
            int ret = xml_strcmp(obj_get_name(newobj), obj_get_name(curobj));
            if (ret < 0) {
                dlq_insertAhead(newnode, backptr);
                return NO_ERR;
            } else if (ret == 0) {
                /* compare object module names */
                int ret2 = xml_strcmp(obj_get_mod_name(newobj),
                                      obj_get_mod_name(curobj));
                if (ret2 < 0) {
                    dlq_insertAhead(newnode, backptr);
                    return NO_ERR;
                } else if (ret2 == 0) {
                    return ERR_NCX_SKIPPED;
                }
            }
        } else if (nodetyp == NCX_NT_VAL) {
            val_value_t *curval = (val_value_t *)backptr->node;

            /* compare value names */
            int ret = xml_strcmp(newval->name, curval->name);
            if (ret < 0) {
                dlq_insertAhead(newnode, backptr);
                return NO_ERR;
            } else if (ret == 0) {
                /* compare value module names */
                int ret2 = xml_strcmp(val_get_mod_name(newval),
                                      val_get_mod_name(curval));
                if (ret2 < 0) {
                    dlq_insertAhead(newnode, backptr);
                    return NO_ERR;
                } else if (ret2 == 0) {
                    return ERR_NCX_SKIPPED;
                }
            }
        }
    }

    /* new last entry */
    dlq_enque(newnode, &comstate->help_backptrQ);
    return NO_ERR;

} /* insert_help_backptr */


/********************************************************************
 * FUNCTION get_first_connected
 * 
 * Return pointer to first connected session_cb
 *
 * INPUTS:
 *    server_cb == server control block to use
 *********************************************************************/
static session_cb_t *
    get_first_connected (server_cb_t *server_cb)
{
    session_cb_t *session_cb =
        (session_cb_t *)dlq_firstEntry(&server_cb->session_cbQ);
    for (; session_cb != NULL;
         session_cb = (session_cb_t *)dlq_nextEntry(session_cb)) {

        if (session_connected(session_cb)) {
            return session_cb;
        }
    }
    return NULL;

}  /* get_first_connected */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION is_top_command
* 
* Check if command name is a top command
* Must be full name
*
* Add all commands here that can be called even if not connected
* to a server!
*
* INPUTS:
*   rpcname == command name to check
*
* RETURNS:
*   TRUE if this is a top command
*   FALSE if not
*********************************************************************/
boolean
    is_top_command (const xmlChar *rpcname)
{
    assert(rpcname && "rpcname is NULL!");

    if (!xml_strcmp(rpcname, YANGCLI_ALIAS)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_ALIASES)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_CD)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_CONNECT)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_EVAL)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_EVENTLOG)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_ELIF)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_ELSE)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_END)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_FILL)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_HELP)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_IF)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_HISTORY)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_LIST)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_LOG_ERROR)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_LOG_WARN)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_LOG_INFO)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_LOG_DEBUG)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_MGRLOAD)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_PWD)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_QUIT)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_RECORD_TEST)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_RECALL)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_RUN)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_SESSION)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_SESSION_CFG)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_SESSIONS_CFG)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_SHOW)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_START_SESSION)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_STOP_SESSION)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_TEST_SUITE)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_WHILE)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_UNSET)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_USERVARS)) {
        ;
    } else {
        return FALSE;
    }
    return TRUE;

}  /* is_top_command */


/********************************************************************
* FUNCTION is_do_command
* 
* Check if command name can be used with the 'do' command
* Must be full name
*
* Add all commands here that can only be called in config mode
* after the 'do' command prefix; e.g.
*
*   do show var testvar
*   do history
*
* INPUTS:
*   rpcname == command name to check
*
* RETURNS:
*   TRUE if this is OK for the 'do' command
*   FALSE if not
*********************************************************************/
boolean
    is_do_command (const xmlChar *rpcname)
{
    assert(rpcname && "rpcname is NULL!");

    if (!xml_strcmp(rpcname, YANGCLI_ALIAS)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_ALIASES)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_CD)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_EVENTLOG)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_FILL)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_HELP)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_HISTORY)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_LIST)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_MGRLOAD)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_PWD)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_QUIT)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_RECALL)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_SESSION_CFG)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_SESSIONS_CFG)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_SHOW)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_UNSET)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_USERVARS)) {
        ;
    } else {
        return FALSE;
    }
    return TRUE;

}  /* is_do_command */


/********************************************************************
* FUNCTION is_server_command
* 
* Check if command name is available to yp-shell
* 
* Add all commands here that can be called by yp-shell
*
* INPUTS:
*   rpcname == command name to check; Must be full name
*
* RETURNS:
*   TRUE if this is a server command
*   FALSE if not
*********************************************************************/
boolean
    is_server_command (const xmlChar *rpcname)
{
    assert(rpcname && "rpcname is NULL!");

    if (!xml_strcmp(rpcname, YANGCLI_ALIAS)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_ALIASES)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_CONFIG)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_EXIT)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_HELP)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_HISTORY)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_QUIT)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_RECALL)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_SHOW)) {
        ;
    } else if (!xml_strcmp(rpcname, YANGCLI_UNSET)) {
        ;
    } else {
        return FALSE;
    }
    return TRUE;

}  /* is_server_command */


/********************************************************************
* FUNCTION new_modptr
* 
*  Malloc and init a new module pointer block
* 
* INPUTS:
*    mod == module to cache in this struct
*    malloced == TRUE if mod is malloced
*                FALSE if mod is a back-ptr
*    feature_list == feature list from capability
*    deviation_list = deviations list from capability
*
* RETURNS:
*   malloced modptr_t struct or NULL of malloc failed
*********************************************************************/
modptr_t *
    new_modptr (ncx_module_t *mod,
                ncx_list_t *feature_list,
                ncx_list_t *deviation_list)
{
    assert(mod && "mod is NULL!");

    modptr_t *modptr = m__getObj(modptr_t);
    if (!modptr) {
        return NULL;
    }
    memset(modptr, 0x0, sizeof(modptr_t));
    modptr->mod = mod;
    modptr->feature_list = feature_list;
    modptr->deviation_list = deviation_list;

    return modptr;

}  /* new_modptr */


/********************************************************************
* FUNCTION free_modptr
* 
*  Clean and free a module pointer block
* 
* INPUTS:
*    modptr == mod pointer block to free
*              MUST BE REMOVED FROM ANY Q FIRST
*
*********************************************************************/
void
    free_modptr (modptr_t *modptr)
{
    if (!modptr) {
        return;
    }

    m__free(modptr);

}  /* free_modptr */


/********************************************************************
* FUNCTION find_modptr
* 
*  Find a specified module name
* 
* INPUTS:
*    modptrQ == Q of modptr_t to check
*    modname == module name to find
*    revision == module revision (may be NULL)
*
* RETURNS:
*   pointer to found entry or NULL if not found
*
*********************************************************************/
modptr_t *
    find_modptr (dlq_hdr_t *modptrQ,
                 const xmlChar *modname,
                 const xmlChar *revision)
{
    assert(modptrQ && "modptrQ is NULL!");
    assert(modname && "modname is NULL!");

    modptr_t  *modptr;
    for (modptr = (modptr_t *)dlq_firstEntry(modptrQ);
         modptr != NULL;
         modptr = (modptr_t *)dlq_nextEntry(modptr)) {

        if (xml_strcmp(modptr->mod->name, modname)) {
            continue;
        }

        if (revision && 
            modptr->mod->version &&
            !xml_strcmp(modptr->mod->version, revision)) {
            return modptr;
        }
        if (revision == NULL) {
            return modptr;
        }
    }
    return NULL;
    
}  /* find_modptr */

/********************************************************************
* FUNCTION clear_server_cb_session
* 
*  Clean the current session data from an server control block
* 
* INPUTS:
*    server_cb == server control block to use
*    session_cb == session control block to use for clearing
*                the session data
*********************************************************************/
void
    clear_server_cb_session (server_cb_t *server_cb,
                             session_cb_t *session_cb)
{
    assert(server_cb && "server_cb is NULL!");
    assert(session_cb && "session_cb is NULL!");

    /* cleanup config mode if it active */
    if (session_cb->config_mode) {
        force_exit_config_mode(session_cb);
    }

    /* get rid of the val->obj pointers that reference
     * server-specific object trees that have been freed
     * already by mgr_ses_free_session
     * !!! TBD: add session to make sure not all variables cleared
     */
    runstack_session_cleanup(server_cb->runstack_context);

    clean_session_cb_conn(session_cb);


    //val_free_value(server_cb->connect_valset);
    //server_cb->connect_valset = NULL;


}  /* clear_server_cb_session */


/********************************************************************
* FUNCTION server_drop_session
*
* INPUTS:
*    server_cb == server control block to use
*    session_cb == session control block to use for clearing
*                the session data
*
* NOTE: It is called if  
* (session_cb->returncode == MGR_IO_RC_DROPPED ||
* session_cb->returncode == MGR_IO_RC_DROPPED_NOW) {
*
*********************************************************************/
void
    server_drop_session (server_cb_t *server_cb, 
                         session_cb_t *session_cb)
{
    assert(server_cb && "server_cb is NULL!");
    assert(session_cb && "session_cb is NULL!");

    if (session_cb->returncode == MGR_IO_RC_DROPPED ||
        session_cb->returncode == MGR_IO_RC_DROPPED_NOW) {

        if (LOGDEBUG2) {
            const xmlChar *name = NULL;
            if (session_cb->session_cfg) {
                name = session_cb->session_cfg->name;
            } else {
                name = NCX_EL_DEFAULT;
            }
            log_debug2("\nDropping and clearing session '%s'", name);
        }

        clear_server_cb_session(server_cb, session_cb);

        if (server_cb->cur_session_cb == NULL ||
            server_cb->cur_session_cb == session_cb) {
            set_new_cur_session(server_cb);
        }
    }

    update_connect_all_in_progress (FALSE);

} /* server_drop_session */


/********************************************************************
* FUNCTION is_top
* 
* Check the state and determine if the top or conn
* mode is active
* 
* INPUTS:
*   server state to use
*
* RETURNS:
*  TRUE if this is TOP mode
*  FALSE if this is CONN mode (or associated states)
*********************************************************************/
boolean
    is_top (mgr_io_state_t state)
{
    switch (state) {
    case MGR_IO_ST_INIT:
    case MGR_IO_ST_IDLE:
        return TRUE;
    case MGR_IO_ST_CONNECT:
    case MGR_IO_ST_CONN_START:
    case MGR_IO_ST_SHUT:
    case MGR_IO_ST_CONN_IDLE:
    case MGR_IO_ST_CONN_RPYWAIT:
    case MGR_IO_ST_CONN_CANCELWAIT:
    case MGR_IO_ST_CONN_CLOSEWAIT:
    case MGR_IO_ST_CONN_SHUT:
        return FALSE;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return FALSE;
    }

}  /* is_top */


/********************************************************************
* FUNCTION use_servercb
* 
* Check if the server_cb should be used for modules right now
*
* INPUTS:
*   server_cb == server control block to check
*
* RETURNS:
*   TRUE to use server_cb
*   FALSE if not
*********************************************************************/
boolean
    use_servercb (server_cb_t *server_cb)
{
    if (server_cb && server_cb->cur_session_cb &&
        session_connected(server_cb->cur_session_cb)) {
        return TRUE;
    }

    return FALSE;
}  /* use_servercb */


/********************************************************************
* FUNCTION use_session_cb
* 
* Check if the session_cb should be used for modules right now
*
* INPUTS:
*   session_cb == session control block to check
*
* RETURNS:
*   TRUE to use session_cb
*   FALSE if not
*********************************************************************/
boolean
    use_session_cb (session_cb_t *session_cb)
{
    if (!session_cb || is_top(session_cb->state)) {
        return FALSE;
    } else if (dlq_empty(&session_cb->modptrQ)) {
        return FALSE;
    }
    return TRUE;
}  /* use_session_cb */


/********************************************************************
* FUNCTION find_module
* 
*  Check the server_cb for the specified module; if not found
*  then try ncx_find_module
* 
* INPUTS:
*    server_cb == control block to free
*    modname == module name
*
* RETURNS:
*   pointer to the requested module
*      using the registered 'current' version
*   NULL if not found
*********************************************************************/
ncx_module_t *
    find_module (server_cb_t *server_cb,
                 const xmlChar *modname)
{
    assert(modname && "modname is NULL!");

    session_cb_t *session_cb = server_cb->cur_session_cb;
    modptr_t      *modptr;

    if (use_session_cb(session_cb)) {
        for (modptr = (modptr_t *)dlq_firstEntry(&session_cb->modptrQ);
             modptr != NULL;
             modptr = (modptr_t *)dlq_nextEntry(modptr)) {

            if (!xml_strcmp(modptr->mod->name, modname)) {
                return modptr->mod;
            }
        }
    }

    ncx_module_t *mod = ncx_find_module(modname, NULL);

    return mod;

}  /* find_module */


/********************************************************************
* FUNCTION get_strparm
* 
* Get the specified string parm from the parmset and then
* make a strdup of the value
*
* INPUTS:
*   valset == value set to check if not NULL
*   modname == module defining parmname
*   parmname  == name of parm to get
*
* RETURNS:
*   pointer to string !!! THIS IS A MALLOCED COPY !!!
*********************************************************************/
xmlChar *
    get_strparm (val_value_t *valset,
                 const xmlChar *modname,
                 const xmlChar *parmname)
{
    assert(valset && "valset is NULL!");
    assert(parmname && "parmname is NULL!");

    xmlChar *str = NULL;
    val_value_t *parm = findparm(valset, modname, parmname);
    if (parm) {
        str = xml_strdup(VAL_STR(parm));
        if (!str) {
            log_error("\nyangcli-pro: Out of Memory error");
        }
    }
    return str;

}  /* get_strparm */


/********************************************************************
* FUNCTION findparm
* 
* Get the specified string parm from the parmset and then
* make a strdup of the value
*
* INPUTS:
*   valset == value set to search
*   modname == optional module name defining the parameter to find
*   parmname  == name of parm to get, or partial name to get
*
* RETURNS:
*   pointer to val_value_t if found
*********************************************************************/
val_value_t *
    findparm (val_value_t *valset,
              const xmlChar *modname,
              const xmlChar *parmname)
{
    assert(parmname && "parmname is NULL!");

    if (!valset) {
        return NULL;
    }

    return val_find_child(valset, modname, parmname);

}  /* findparm */


/********************************************************************
* FUNCTION add_clone_parm
* 
*  Create a parm 
* 
* INPUTS:
*   val == value to clone and add
*   valset == value set to add parm into
*
* RETURNS:
*    status
*********************************************************************/
status_t
    add_clone_parm (const val_value_t *val,
                    val_value_t *valset)
{
    assert(val && "val is NULL!");

    if (!valset) {
        // in case the connect_valset is not present
        return NO_ERR;
    }

    val_value_t *parm = val_clone(val);
    if (!parm) {
        log_error("\nyangcli-pro: val_clone failed");
        return ERR_INTERNAL_MEM;
    } else {
        val_add_child(parm, valset);
    }
    return NO_ERR;

}  /* add_clone_parm */


/********************************************************************
* FUNCTION is_yangcli_ns
* 
*  Check the namespace and make sure this is an YANGCLI command
* 
* INPUTS:
*   ns == namespace ID to check
*
* RETURNS:
*  TRUE if this is the YANGCLI namespace ID
*********************************************************************/
boolean
    is_yangcli_ns (xmlns_id_t ns)
{
    const xmlChar *modname;

    modname = xmlns_get_module(ns);
    if (modname && !xml_strcmp(modname, YANGCLI_MOD)) {
        return TRUE;
    } else {
        return FALSE;
    }

}  /* is_yangcli_ns */


/********************************************************************
 * FUNCTION clear_result
 * 
 * clear out the pending result info
 *
 * INPUTS:
 *   server_cb == server control block to use
 *
 *********************************************************************/
void
    clear_result (server_cb_t *server_cb)

{
    assert(server_cb && "server_cb is NULL!");

    if (server_cb->local_result) {
        val_free_value(server_cb->local_result);
        server_cb->local_result = NULL;
    }
    if (server_cb->result_name) {
        m__free(server_cb->result_name);
        server_cb->result_name = NULL;
    }
    if (server_cb->result_filename) {
        m__free(server_cb->result_filename);
        server_cb->result_filename = NULL;
    }
    server_cb->result_vartype = VAR_TYP_NONE;
    server_cb->result_format = RF_NONE;

}  /* clear_result */


/********************************************************************
* FUNCTION check_filespec
* 
* Check the filespec string for a file assignment statement
* Save it if it si good
*
* INPUTS:
*    server_cb == server control block to use
*    filespec == string to check
*    varname == variable name to use in log_error
*              if this is complex form
*
* OUTPUTS:
*    server_cb->result_filename will get set if NO_ERR
*
* RETURNS:
*   status
*********************************************************************/
status_t
    check_filespec (server_cb_t *server_cb,
                    const xmlChar *filespec,
                    const xmlChar *varname)
{
    xmlChar       *newstr;
    const xmlChar *teststr;
    status_t       res;

    assert(server_cb && "server_cb is NULL!");
    assert(filespec && "filespec is NULL!");

    if (!*filespec) {
        if (varname) {
            log_error("\nError: file assignment variable '%s' "
                      "is empty string", varname);
        } else {
            log_error("\nError: file assignment filespec "
                      "is empty string");
        }
        return ERR_NCX_INVALID_VALUE;
    }

    /* variable must be a string with only
     * valid filespec chars in it; no spaces
     * are allowed; too many security holes
     * if arbitrary strings are allowed here
     */
    if (val_need_quotes(filespec)) {
        if (varname) {
            log_error("\nError: file assignment variable '%s' "
                      "contains whitespace (%s)", 
                      varname, filespec);
        } else {
            log_error("\nError: file assignment filespec '%s' "
                      "contains whitespace", filespec);
        }
        return ERR_NCX_INVALID_VALUE;
    }

    /* check for acceptable chars */
    res = NO_ERR;
    newstr = ncx_get_source_ex(filespec, FALSE, &res);
    if (newstr == NULL || res != NO_ERR) {
        log_error("\nError: get source for '%s' failed (%s)",
                  filespec, get_error_string(res));
        if (newstr != NULL) {
            m__free(newstr);
        }
        return res;
    }

    teststr = newstr;
    while (*teststr) {
        if (*teststr == NCXMOD_PSCHAR ||
            *teststr == '.' ||
#ifdef WINDOWS
            *teststr == ':' ||
#endif
            ncx_valid_name_ch(*teststr)) {
            teststr++;
        } else {
            if (varname) {
                log_error("\nError: file assignment variable '%s' "
                          "contains invalid filespec (%s)", 
                          varname, filespec);
            } else {
                log_error("\nError: file assignment filespec '%s' "
                          "contains invalid filespec", filespec);
            }
            m__free(newstr);
            return ERR_NCX_INVALID_VALUE;
        }
    }

    /* toss out the old value, if any */
    if (server_cb->result_filename) {
        m__free(server_cb->result_filename);
    }

    /* save the filename, may still be an invalid fspec
     * pass off newstr memory here
     */
    server_cb->result_filename = newstr;
    if (!server_cb->result_filename) {
        return ERR_INTERNAL_MEM;
    }
    return NO_ERR;

}  /* check_filespec */


/********************************************************************
 * FUNCTION get_instanceid_parm
 * 
 * Validate an instance identifier parameter
 * Return the target object
 * Return a value struct from root containing
 * all the predicate assignments in the stance identifier
 *
 * INPUTS:
 *    target == XPath expression for the instance-identifier
 *    schemainst == TRUE if ncx:schema-instance string
 *                  FALSE if instance-identifier
 *    configonly == TRUE if there should be an error given
 *                  if the target does not point to a config node
 *                  FALSE if the target can be config false
 *    targobj == address of return target object for this expr
 *    targval == address of return pointer to target value
 *               node within the value subtree returned
 *    retres == address of return status
 *
 * OUTPUTS:
 *    *targobj == the object template for the target
 *    *targval == the target node within the returned subtree
 *                from root
 *    *retres == return status for the operation
 *
 * RETURNS:
 *   If NO_ERR:
 *     malloced value node representing the instance-identifier
 *     from root to the targobj
 *  else:
 *    NULL, check *retres
 *********************************************************************/
val_value_t *
    get_instanceid_parm (const xmlChar *target,
                         boolean schemainst,
                         boolean configonly,
                         obj_template_t **targobj,
                         val_value_t **targval,
                         status_t *retres)
{
#ifdef DEBUG
    if (!target || !targobj || !targval || !retres) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    *targobj = NULL;
    *targval = NULL;
    *retres = NO_ERR;

    /* get a parser block for the instance-id */
    xpath_pcb_t *xpathpcb = xpath_new_pcb(target, NULL);
    if (!xpathpcb) {
        log_error("\nError: malloc failed");
        *retres = ERR_INTERNAL_MEM;
        return NULL;
    }

    /* initial parse into a token chain
     * this is only for parsing leafref paths! 
     */
    status_t res =
        xpath_yang_parse_path(NULL, NULL, (schemainst) ?
                              XP_SRC_SCHEMA_INSTANCEID : XP_SRC_INSTANCEID,
                              xpathpcb);
    if (res != NO_ERR) {
        log_error("\nError: parse XPath target '%s' failed",
                  xpathpcb->exprstr);
        xpath_free_pcb(xpathpcb);
        *retres = res;
        return NULL;
    }

    /* validate against the object tree */
    res = xpath_yang_validate_path(NULL,  ncx_get_gen_root(),
                                   xpathpcb, schemainst, targobj);
    if (res != NO_ERR) {
        log_error("\nError: validate XPath target '%s' failed",
                  xpathpcb->exprstr);
        xpath_free_pcb(xpathpcb);
        *retres = res;
        return NULL;
    }

    /* check if the target is a config node or not
     * TBD: check the baddata parm and possibly allow
     * the target of a write to be a config=false for
     * server testing purposes
     */
    if (configonly && !obj_get_config_flag(*targobj)) {
        log_error("\nError: XPath target '%s' is not a config=true node",
                  xpathpcb->exprstr);
        xpath_free_pcb(xpathpcb);
        *retres = ERR_NCX_ACCESS_READ_ONLY;
        return NULL;
    }
    
    /* have a valid target object, so follow the
     * parser chain and build a value subtree
     * from the XPath expression
     */
    val_value_t *retval =
        xpath_yang_make_instanceid_val(xpathpcb, &res, targval);

    xpath_free_pcb(xpathpcb);
    *retres = res;

    return retval;

} /* get_instanceid_parm */


/********************************************************************
* FUNCTION get_file_result_format
* 
* Check the filespec string for a file assignment statement
* to see if it is text, XML, or JSON
*
* INPUTS:
*    filespec == string to check
*
* RETURNS:
*   result format enumeration; RF_NONE if some error
*********************************************************************/
result_format_t
    get_file_result_format (const xmlChar *filespec)
{
    const xmlChar *teststr;
    uint32         len;

#ifdef DEBUG
    if (!filespec) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return RF_NONE;
    }
#endif

    len = xml_strlen(filespec);
    if (len < 5) {
        return RF_TEXT;
    }

    teststr = &filespec[len-1];

    while (teststr > filespec && *teststr != '.') {
        teststr--;
    }

    if (teststr == filespec) {
        return RF_TEXT;
    }

    teststr++;

    if (!xml_strcmp(teststr, NCX_EL_XML)) {
        return RF_XML;
    }

    if (!xml_strcmp(teststr, NCX_EL_JSON)) {
        return RF_JSON;
    }

    if (!xml_strcmp(teststr, NCX_EL_YANG)) {
        return RF_TEXT;
    }

    if (!xml_strcmp(teststr, NCX_EL_TXT)) {
        return RF_TEXT;
    }

    if (!xml_strcmp(teststr, NCX_EL_TEXT)) {
        return RF_TEXT;
    }

    if (!xml_strcmp(teststr, NCX_EL_LOG)) {
        return RF_TEXT;
    }

    return RF_TEXT;  // default to text instead of error!

}  /* get_file_result_format */


/********************************************************************
* FUNCTION interactive_mode
* 
*  Check if the program is in interactive mode
* 
* RETURNS:
*   TRUE if insteractive mode, FALSE if batch mode
*********************************************************************/
boolean
    interactive_mode (void)
{
    return get_batchmode() ? FALSE : TRUE;

}  /* interactive_mode */


/********************************************************************
 * FUNCTION first_init_completion_state
 * 
 * first time init the completion_state struct for a new command
 *
 * INPUTS:
 *    completion_state == record to initialize
 *    server_cb == server control block to use
 *    cmdstate ==initial  calling state
 *********************************************************************/
void
    first_init_completion_state (completion_state_t *completion_state)
{
#ifdef DEBUG
    if (!completion_state) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    memset(completion_state, 0x0, sizeof(completion_state_t));
    dlq_createSQue(&completion_state->help_backptrQ);

}  /* first_init_completion_state */


/********************************************************************
 * FUNCTION init_completion_state
 * 
 * init the completion_state struct for a new command
 *
 * INPUTS:
 *    completion_state == record to initialize
 *    server_cb == server control block to use
 *    cmdstate ==initial  calling state
 *********************************************************************/
void
    init_completion_state (completion_state_t *completion_state,
                           server_cb_t *server_cb,
                           command_state_t  cmdstate)
{
#ifdef DEBUG
    if (!completion_state) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    while (!dlq_empty(&completion_state->help_backptrQ)) {
        ncx_backptr_t *backptr =
            (ncx_backptr_t *)dlq_deque(&completion_state->help_backptrQ);
        ncx_free_backptr(backptr);
    }

    completion_state->cmdobj = NULL;
    completion_state->cmdinput = NULL;
    completion_state->cmdcurparm = NULL;
    completion_state->curkey = NULL;
    completion_state->server_cb = server_cb;
    completion_state->cmdmodule = NULL;
    completion_state->gl = NULL;
    completion_state->help_backptr_type = NCX_NT_NONE;
    completion_state->help_action = HELP_ACTION_NONE;
    completion_state->cmdstate = cmdstate;
    completion_state->assignstmt = FALSE;
    completion_state->config_done = FALSE;
    completion_state->keys_done = FALSE;
    completion_state->no_command = FALSE;
    completion_state->do_command = FALSE;
    completion_state->help_mode = FALSE;
    completion_state->gl_normal_done = FALSE;

    if (cmdstate == CMD_STATE_GETVAL) {
        unregister_help_action(server_cb);
        completion_state->gl_unregister_done = TRUE;
    } else if (cmdstate == CMD_STATE_FULL) {
        if (completion_state->gl_unregister_done) {
            register_help_action(server_cb);
            completion_state->gl_unregister_done = FALSE;
        }
    }
        
}  /* init_completion_state */


/********************************************************************
 * FUNCTION init_config_completion_state
 * 
 * init the completion_state struct for a new command in config mode
 *
 * INPUTS:
 *    completion_state == record to initialize
 *********************************************************************/
void
    init_config_completion_state (completion_state_t *completion_state)
{
#ifdef DEBUG
    if (!completion_state) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    while (!dlq_empty(&completion_state->help_backptrQ)) {
        ncx_backptr_t *backptr =
            (ncx_backptr_t *)dlq_deque(&completion_state->help_backptrQ);
        ncx_free_backptr(backptr);
    }

    if (completion_state->gl_unregister_done) {
        register_help_action(completion_state->server_cb);
        completion_state->gl_unregister_done = FALSE;
    }
        
}  /* init_config_completion_state */


/********************************************************************
 * FUNCTION init_help_completion_state
 * 
 * init the completion_state struct for a new command
 * for the help key mode
 *
 * INPUTS:
 *    completion_state == record to initialize
 *********************************************************************/
void
    init_help_completion_state (completion_state_t *completion_state)
{
#ifdef DEBUG
    if (!completion_state) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    while (!dlq_empty(&completion_state->help_backptrQ)) {
        ncx_backptr_t *backptr =
            (ncx_backptr_t *)dlq_deque(&completion_state->help_backptrQ);
        ncx_free_backptr(backptr);
    }

    completion_state->help_mode = TRUE;
    completion_state->help_backptr_type = NCX_NT_NONE;
    completion_state->help_action = HELP_ACTION_NONE;
    completion_state->gl_normal_done = FALSE;

}  /* init_help_completion_state */


/********************************************************************
 * FUNCTION clean_help_completion_state
 * 
 * Cleanup after the help key mode
 *
 * INPUTS:
 *    completion_state == record to cleanup
 *********************************************************************/
void
    clean_help_completion_state (completion_state_t *completion_state)
{
#ifdef DEBUG
    if (!completion_state) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    while (!dlq_empty(&completion_state->help_backptrQ)) {
        ncx_backptr_t *backptr =
            (ncx_backptr_t *)dlq_deque(&completion_state->help_backptrQ);
        ncx_free_backptr(backptr);
    }

    completion_state->help_mode = FALSE;

}  /* clean_help_completion_state */


/********************************************************************
 * FUNCTION set_completion_state
 * 
 * set the completion_state struct for a new mode or sub-command
 *
 * INPUTS:
 *    completion_state == record to set
 *    rpc == rpc operation in progress (may be NULL)
 *    parm == parameter being filled in
 *    cmdstate ==current calling state
 *********************************************************************/
void
    set_completion_state (completion_state_t *completion_state,
                          obj_template_t *rpc,
                          obj_template_t *parm,
                          command_state_t  cmdstate)
{
#ifdef DEBUG
    if (!completion_state) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    completion_state->cmdstate = cmdstate;
    completion_state->cmdobj = rpc;
    if (rpc) {
        completion_state->cmdinput =
            obj_find_child(rpc, NULL, YANG_K_INPUT);
    } else {
        completion_state->cmdinput = NULL;
    }
    completion_state->cmdcurparm = parm;

    if (cmdstate == CMD_STATE_GETVAL) {
        if (!completion_state->gl_unregister_done) {
            unregister_help_action(completion_state->server_cb);
            completion_state->gl_unregister_done = TRUE;
        }
    } else {
        if (completion_state->gl_unregister_done) {
            register_help_action(completion_state->server_cb);
            completion_state->gl_unregister_done = FALSE;
        }
    }

}  /* set_completion_state */


/********************************************************************
 * FUNCTION set_completion_state_curparm
 * 
 * set the current parameter in the completion_state struct
 *
 * INPUTS:
 *    completion_state == record to set
 *    parm == parameter being filled in
 *********************************************************************/
void
    set_completion_state_curparm (completion_state_t *completion_state,
                                  obj_template_t *parm)
{
#ifdef DEBUG
    if (!completion_state) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return;
    }
#endif

    completion_state->cmdcurparm = parm;

}  /* set_completion_state_curparm */


/********************************************************************
 * FUNCTION set_completion_state_config_mode
 * 
 * set the current parameter in the completion_state struct
 *
 * INPUTS:
 *    completion_state == record to set
 *********************************************************************/
void
    set_completion_state_config_mode (completion_state_t *completion_state)
{
    assert(completion_state && "completion_state is NULL!");

    completion_state->cmdobj = NULL;
    completion_state->cmdinput = NULL;
    completion_state->cmdcurparm = NULL;

    // leave server_cb set to current value
    completion_state->cmdmodule = NULL;
    completion_state->cmdstate = CMD_STATE_FULL;
    completion_state->assignstmt = FALSE;

}  /* set_completion_state_config_mode */


/********************************************************************
 * FUNCTION clean_completion_state
 * 
 * clean the completion_state struct
 *
 * INPUTS:
 *    completion_state == record to initialize
 *********************************************************************/
void
    clean_completion_state (completion_state_t *completion_state)

{
    if (!completion_state) {
        return;
    }

    while (!dlq_empty(&completion_state->help_backptrQ)) {
        ncx_backptr_t *backptr =
            (ncx_backptr_t *)dlq_deque(&completion_state->help_backptrQ);
        ncx_free_backptr(backptr);
    }
    memset(completion_state, 0x0, sizeof(completion_state_t));

} /* clean_completion_state */


/********************************************************************
* FUNCTION xpath_getvar_fn
 *
 * see ncx/xpath.h -- matches xpath_getvar_fn_t template
 *
 * Callback function for retrieval of a variable binding
 * 
 * INPUTS:
 *   pcb   == XPath parser control block in use
 *   varname == variable name requested
 *   res == address of return status
 *
 * OUTPUTS:
 *  *res == return status
 *
 * RETURNS:
 *    pointer to the ncx_var_t data structure
 *    for the specified varbind
*********************************************************************/
ncx_var_t *
    xpath_getvar_fn (struct xpath_pcb_t_ *pcb,
                     const xmlChar *varname,
                     status_t *res)
{
    ncx_var_t           *retvar;
    runstack_context_t  *rcxt;

#ifdef DEBUG
    if (varname == NULL || res == NULL) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    /* if the runstack context is not set then the default
     * context will be used
     */
    rcxt = (runstack_context_t *)pcb->cookie;
    retvar = var_find(rcxt, varname, 0);
    if (retvar == NULL) {
        *res = ERR_NCX_DEF_NOT_FOUND;
    } else {
        *res = NO_ERR;
    }

    return retvar;

}  /* xpath_getvar_fn */


/********************************************************************
* FUNCTION get_netconf_mod
* 
*  Get the netconf module
*
* INPUTS:
*   server_cb == server control block to use
* 
* RETURNS:
*    netconf module
*********************************************************************/
ncx_module_t *
    get_netconf_mod (server_cb_t *server_cb)
{
#ifdef DEBUG
    if (server_cb == NULL) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    ncx_module_t *mod = find_module(server_cb, NCXMOD_YUMA_NETCONF);
    return mod;

}  /* get_netconf_mod */


/********************************************************************
* FUNCTION get_notif_mod
* 
*  Get the netconf notifications module
*
* INPUTS:
*   server_cb == server control block to use
* 
* RETURNS:
*    notifications module
*********************************************************************/
ncx_module_t *
    get_notif_mod (server_cb_t *server_cb)
{
#ifdef DEBUG
    if (server_cb == NULL) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    ncx_module_t *mod = find_module(server_cb, NCN_MODULE);
    return mod;

}  /* get_notif_mod */


/********************************************************************
* FUNCTION clone_old_parm
* 
*  Clone a parameter value from the 'old' value set
*  if it exists there, and add it to the 'new' value set
*  only if the new value set does not have this parm
*
* The old and new pvalue sets must be complex types 
*  NCX_BT_LIST, NCX_BT_CONTAINER, or NCX_BT_ANYXML
*
* INPUTS:
*   oldvalset == value set to copy from
*   newvalset == value set to copy into
*   parm == object template to find and copy
*
* RETURNS:
*  status
*********************************************************************/
status_t
    clone_old_parm (val_value_t *oldvalset,
                    val_value_t *newvalset,
                    obj_template_t *parm)
{
    val_value_t  *findval, *newval;

#ifdef DEBUG
    if (oldvalset == NULL || newvalset == NULL || parm == NULL) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
    if (!typ_has_children(oldvalset->btyp)) {
        return ERR_NCX_INVALID_VALUE;
    }
    if (!typ_has_children(newvalset->btyp)) {
        return ERR_NCX_INVALID_VALUE;
    }
#endif

    findval = val_find_child(newvalset,
                             obj_get_mod_name(parm),
                             obj_get_name(parm));
    if (findval != NULL) {
        return NO_ERR;
    }

    findval = val_find_child(oldvalset,
                             obj_get_mod_name(parm),
                             obj_get_name(parm));
    if (findval == NULL) {
        return NO_ERR;
    }

    newval = val_clone(findval);
    if (newval == NULL) {
        return ERR_INTERNAL_MEM;
    }
    val_add_child(newval, newvalset);
    return NO_ERR;

}  /* clone_old_parm */


/********************************************************************
* FUNCTION new_rawline
* 
*  Malloc and init a new raw line struct
* 
* INPUTS:
*    line == line value to copy
*
* RETURNS:
*   malloced rawline_t struct or NULL of malloc failed
*********************************************************************/
rawline_t *
    new_rawline (const xmlChar *line)
{
#ifdef DEBUG
    if (!line) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return NULL;
    }
#endif

    rawline_t  *rawline = m__getObj(rawline_t);
    if (!rawline) {
        return NULL;
    }
    memset(rawline, 0x0, sizeof(rawline_t));
    rawline->line = xml_strdup(line);
    if (rawline->line == NULL) {
        m__free(rawline);
        return NULL;
    }
    (void)xml_trim_string(rawline->line);

    return rawline;

}  /* new_rawline */


/********************************************************************
* FUNCTION free_rawline
* 
*  Clean and free a rawline struct
* 
* INPUTS:
*    rawline == rawline block to free
*              MUST BE REMOVED FROM ANY Q FIRST
*
*********************************************************************/
void
    free_rawline (rawline_t *rawline)
{
    if (!rawline) {
        return;
    }

    m__free(rawline->line);
    m__free(rawline);

}  /* free_rawline */


/********************************************************************
* FUNCTION send_request_to_server
* 
*  Send a pre-filled in request to the server
*  The request data is consumed by this function!!!
*
* INPUTS:
*    session_cb == session control block to use
*    rpc == object template for RPC to invoke
*    reqdata == PDU data to use (will be freed later if return NO_ERR
*
* RETURNS:
*   status;i caller MUST NOT free reqdata; it is freed 
*    even if error returned
*********************************************************************/
status_t
    send_request_to_server (session_cb_t *session_cb,
                            obj_template_t *rpc,
                            val_value_t *reqdata)
{
    assert(session_cb && "session_cb is NULL!");
    assert(rpc && "rpc is NULL!");
    assert(reqdata && "reqdata is NULL!");

    status_t res = NO_ERR;
    mgr_rpc_req_t *req = NULL;

    ses_cb_t *scb = mgr_ses_get_scb(session_cb->mysid);
    if (!scb) {
        res = ERR_NCX_OPERATION_FAILED;
        log_error("\nError: session has been dropped by the server\n");
    } else {
        req = mgr_rpc_new_request(scb);
        if (!req) {
            res = ERR_INTERNAL_MEM;
            log_error("\nError allocating a new RPC request\n");
        } else {
            req->data = reqdata;
            req->rpc = rpc;
            req->timeout = session_cb->timeout;
        }
    }
        
    /* if all OK, send the RPC request */
    if (res == NO_ERR) {
        if (LOGDEBUG2) {
            log_debug2("\nabout to send RPC request with reqdata:");
            val_dump_value_max(reqdata, 0, session_cb->defindent,
                               DUMP_VAL_LOG, session_cb->display_mode,
                               FALSE, FALSE);
        } else if (LOGDEBUG) {
            log_debug("\nabout to send <%s> RPC request", obj_get_name(rpc));
        }

        /* the request will be stored if this returns NO_ERR */
        res = mgr_rpc_send_request(scb, req, yangcli_reply_handler);
        if (res != NO_ERR) {
            log_warn("\nWarning: send <%s> request failed for session %u\n",
                     obj_get_name(rpc), session_cb->mysid);
        }
    }

    /* cleanup and set next state */
    if (res != NO_ERR) {
        if (req) {
            mgr_rpc_free_request(req);
        } else if (reqdata) {
            val_free_value(reqdata);
        }
    } else {
        session_cb->state = MGR_IO_ST_CONN_RPYWAIT;
    }

    return res;

} /* send_request_to_server */


/********************************************************************
 * FUNCTION find_top_obj
 * 
 * Check all modules for top-level data nodes to save
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    namestr == namestr to match
 *
 * RETURNS:
 *   object template for the found name, or NULL if not found
 *********************************************************************/
obj_template_t *
    find_top_obj (session_cb_t *session_cb,
                  const xmlChar *namestr)
{
    obj_template_t *modObj;

    if (use_session_cb(session_cb)) {
        modptr_t *modptr;
        for (modptr = (modptr_t *)dlq_firstEntry(&session_cb->modptrQ);
             modptr != NULL;
             modptr = (modptr_t *)dlq_nextEntry(modptr)) {

            modObj = check_find_top_obj(session_cb, modptr->mod, namestr);
            if (modObj != NULL) {
                return modObj;
            }
        }

        /* check manager loaded commands next */
        for (modptr = (modptr_t *)dlq_firstEntry(get_mgrloadQ());
             modptr != NULL;
             modptr = (modptr_t *)dlq_nextEntry(modptr)) {

            modObj = check_find_top_obj(session_cb, modptr->mod, namestr);
            if (modObj != NULL) {
                return modObj;
            }
        }
    } else {
        ncx_module_t * mod = ncx_get_first_session_module();
        for (; mod != NULL; mod = ncx_get_next_session_module(mod)) {

            modObj = check_find_top_obj(session_cb, mod, namestr);
            if (modObj != NULL) {
                return modObj;
            }
        }
    }
    return NULL;

} /* find_top_obj */


/********************************************************************
 * FUNCTION find_top_obj_str
 * 
 * Check all modules for top-level data nodes to save
 * not using a z-terminated string
 * INPUTS:
 *    session_cb == session control block to use
 *    namestr == namestr to match
 *    namestrlen == length of namestr to use
 * RETURNS:
 *   object template for the found name, or NULL if not found
 *********************************************************************/
obj_template_t *
    find_top_obj_str (session_cb_t *session_cb,
                      const xmlChar *namestr,
                      int namestrlen)
{
    obj_template_t *modObj;

    if (use_session_cb(session_cb)) {
        modptr_t *modptr;
        for (modptr = (modptr_t *)dlq_firstEntry(&session_cb->modptrQ);
             modptr != NULL;
             modptr = (modptr_t *)dlq_nextEntry(modptr)) {

            modObj = check_find_top_obj_str(session_cb, modptr->mod,
                                            namestr, namestrlen);
            if (modObj != NULL) {
                return modObj;
            }
        }

        /* check manager loaded commands next */
        for (modptr = (modptr_t *)dlq_firstEntry(get_mgrloadQ());
             modptr != NULL;
             modptr = (modptr_t *)dlq_nextEntry(modptr)) {

            modObj = check_find_top_obj_str(session_cb, modptr->mod,
                                            namestr, namestrlen);
            if (modObj != NULL) {
                return modObj;
            }
        }
    } else {
        ncx_module_t * mod = ncx_get_first_session_module();
        for (; mod != NULL; mod = ncx_get_next_session_module(mod)) {

            modObj = check_find_top_obj_str(session_cb, mod, namestr,
                                            namestrlen);
            if (modObj != NULL) {
                return modObj;
            }
        }
    }
    return NULL;

} /* find_top_obj_str */


/********************************************************************
 * FUNCTION find_child_obj
 * 
 * Check the child nodes of the specified object
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    curobj == parent object to use
 *    namestr == namestr to match
 *
 * RETURNS:
 *   object template for the found name, or NULL if not found
 *********************************************************************/
obj_template_t *
    find_child_obj (session_cb_t *session_cb,
                    obj_template_t *curobj,
                    const xmlChar *namestr)
{
    status_t res = NO_ERR;
    obj_template_t *chobj =
        obj_find_child_ex(curobj,
                          NULL, /* modname = matchall */
                          namestr, /* objname */
                          session_cb->match_names,
                          session_cb->alt_names,
                          TRUE,  /* dataonly */
                          &res);

    if (res != NO_ERR && res != ERR_NCX_DEF_NOT_FOUND) {
        log_error("\nError: could not retrieve child name (%s)",
                  get_error_string(res));
    }

    return chobj;

} /* find_child_obj */


/********************************************************************
 * FUNCTION find_child_obj_str
 * 
 * Check the child nodes of the specified object
 * Does not expect z-terminated string
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    curobj == parent object to use
 *    namestr == namestr to match
 *    namestrlen
 *
 * RETURNS:
 *   object template for the found name, or NULL if not found
 *********************************************************************/
obj_template_t *
    find_child_obj_str (session_cb_t *session_cb,
                        obj_template_t *curobj,
                        const xmlChar *namestr,
                        int namestrlen)
{
    xmlChar *usebuff = NULL;
    xmlChar buff[NAME_BUFF_SIZE];

    if (namestrlen >= NAME_BUFF_SIZE) {
        usebuff = m__getMem(namestrlen+1);
        if (usebuff == NULL) {
            log_error("\nError: malloc for name string failed");
            return NULL;
        }
    } else {
        usebuff = buff;
    }

    xml_strncpy(usebuff, namestr, namestrlen);

    status_t res = NO_ERR;
    obj_template_t *chobj =
        obj_find_child_ex(curobj,
                          NULL, /* modname = matchall */
                          usebuff, /* objname */
                          session_cb->match_names,
                          session_cb->alt_names,
                          TRUE,  /* dataonly */
                          &res);

    if (res != NO_ERR && res != ERR_NCX_DEF_NOT_FOUND) {
        log_error("\nError: could not retrieve child name (%s)",
                  get_error_string(res));
    }

    if (usebuff != buff) {
        m__free(usebuff);
    }

    return chobj;

} /* find_child_obj_str */


/********************************************************************
* FUNCTION cvt_config_edit_mode_enum
* 
*  Convert a config-edit-type enum to a string
*
* INPUTS:
*    editmode == enum to convert
* RETURNS:
*   string version of enum
*********************************************************************/
const xmlChar *
    cvt_config_edit_mode_enum (config_edit_mode_t editmode)
{
    switch (editmode) {
    case CFG_EDITMODE_LINE:
        return YANGCLI_LINE;
    case CFG_EDITMODE_LEVEL:
        return YANGCLI_LEVEL;
    case CFG_EDITMODE_MANUAL:
        return YANGCLI_MANUAL;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return EMPTY_STRING;
    }

}  /* cvt_config_edit_mode_enum */


/********************************************************************
* FUNCTION cvt_config_edit_mode_str
* 
*  Convert a config-edit-type string to an enum
*
* INPUTS:
*    editmode == string to convert
* RETURNS:
*   enum version of enum
*********************************************************************/
config_edit_mode_t
    cvt_config_edit_mode_str (const xmlChar *editmode)
{
    if (!xml_strcmp(editmode, YANGCLI_LINE)) {
        return CFG_EDITMODE_LINE;
    } else if (!xml_strcmp(editmode, YANGCLI_LEVEL)) {
        return CFG_EDITMODE_LEVEL;
    } else if (!xml_strcmp(editmode, YANGCLI_MANUAL)) {
        return CFG_EDITMODE_MANUAL;
    } else {
        return CFG_EDITMODE_NONE;
    }
}  /* cvt_config_edit_mode_str */


/********************************************************************
* FUNCTION notifications_supported
* 
* Check if the NETCONF server supports notifications
*
* INPUTS:
*   scb == low-level session control block to use
*
* RETURNS:
*    TRUE if notifications and interleave should be supported
*********************************************************************/
boolean
    notifications_supported (ses_cb_t *scb)
{
    const mgr_scb_t *mscb = (const mgr_scb_t *)scb->mgrcb;
    if (mscb == NULL) {
        return FALSE;  // should not happen
    }

    if (!cap_std_set(&mscb->caplist, CAP_STDID_NOTIFICATION)) {
        return FALSE;
    }

    if (!cap_std_set(&mscb->caplist, CAP_STDID_INTERLEAVE)) {
        return FALSE;
    }
    return TRUE;

}  /* notifications_supported */


/********************************************************************
 * FUNCTION add_obj_backptr
 * 
 * Add a help mode backptr for the specified object
 *
 * INPUTS:
 *    comstate == completion state to use
 *    obj == object template to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    add_obj_backptr (completion_state_t *comstate,
                     obj_template_t *obj)
{
    /* add help back-ptr for this object */
    ncx_backptr_t *backptr = ncx_new_backptr(obj);
    if (backptr == NULL) {
        return ERR_NCX_OPERATION_FAILED;
    }

    status_t res = insert_help_backptr(comstate, backptr, NCX_NT_OBJ);
    if (res == ERR_NCX_SKIPPED) {
        /* duplicate should not really happen; ignore just in case */
        ncx_free_backptr(backptr);
        res = NO_ERR;
    } else if (res != NO_ERR) {
        ncx_free_backptr(backptr);
    }

    return res;

}  /* add_obj_backptr */


#ifdef NOT_USED_YET
/********************************************************************
 * FUNCTION add_val_backptr
 * 
 * Add a help mode backptr for the specified value node
 *
 * INPUTS:
 *    comstate == completion state to use
 *    val == value node to use
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    add_val_backptr (completion_state_t *comstate,
                     val_value_t *val)
{
    ncx_backptr_t *backptr = ncx_new_backptr(val);
    if (backptr == NULL) {
        return ERR_NCX_OPERATION_FAILED;
    }

    status_t res = insert_help_backptr(comstate, backptr, NCX_NT_VAL);
    if (res == ERR_NCX_SKIPPED) {
        /* duplicate should not really happen; ignore just in case */
        ncx_free_backptr(backptr);
        res = NO_ERR;
    } else if (res != NO_ERR) {
        ncx_free_backptr(backptr);
    }

    return res;

}  /* add_val_backptr */
#endif   // NOT_USED_YET


/********************************************************************
* FUNCTION set_new_cur_session
* 
* Pick a new current session
*
* INPUTS:
*   server_cb == server control block to use
* 
* OUTPUTS:
*   server_cb->cur_session_cb is set to new current session
*********************************************************************/
void
    set_new_cur_session (server_cb_t *server_cb)
{
    assert(server_cb);

    log_debug2("\nSetting new default session to ");
    server_cb->cur_session_cb = get_first_connected(server_cb);
    if (server_cb->cur_session_cb == NULL) {
        server_cb->cur_session_cb =
            find_session_cb(server_cb, NCX_EL_DEFAULT);
        log_debug2_append(" default");
    } else if (LOGDEBUG2) {
        const xmlChar *name =
            get_session_name(server_cb->cur_session_cb);
        log_debug2_append(" %s", name);
    }

}  /* set_new_cur_session */

/********************************************************************
* FUNCTION removeChar
*
* INPUTS: string: where the char is to be removed.
*         letter: the specified char to be removed
* OUTPUTS: string updated with the specified char removed.
*
*********************************************************************/
void 
    removeChar(char *str, char letter) 
{    for( unsigned int i = 0; i < strlen( str ); i++ ) {
        if( str[i] == letter ) {
            strcpy( str + i, str + i + 1 );
        }
    }

}/* removeChar */

/* END yangcli_util.c */
