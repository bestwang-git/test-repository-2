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
#ifndef _H_yangcli_sessions
#define _H_yangcli_sessions

/*  FILE: yangcli_sessions.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

  implement yangcli sessions command
  
 
*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
17-aug-12    abb      Begun;

*/

#include <xmlstring.h>

#include "obj.h"
#include "status.h"
#include "val.h"
#include "yangcli.h"

#ifdef __cplusplus
extern "C" {
#endif


/********************************************************************
*								    *
*		      F U N C T I O N S 			    *
*								    *
*********************************************************************/


/********************************************************************
 * FUNCTION do_session_cfg (local RPC)
 * 
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
extern status_t
    do_session_cfg (server_cb_t *server_cb,
                    obj_template_t *rpc,
                    const xmlChar *line,
                    uint32  len);


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
extern status_t
    do_sessions_cfg (server_cb_t *server_cb,
                     obj_template_t *rpc,
                     const xmlChar *line,
                     uint32  len);


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
extern status_t
    load_sessions (server_cb_t *server_cb,
                   const xmlChar *fspec,
                   boolean file_error);


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
extern status_t
    save_sessions (server_cb_t *server_cb,
                   const xmlChar *fspec);


/********************************************************************
 * FUNCTION free_session_cfg
 * 
 * Free a session_cfg struct
 *
 * INPUT:
 *   session_cfg == session config struct to free
 *********************************************************************/
extern void
    free_session_cfg (session_cfg_t *session_cfg);


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
 *
 * RETURNS:
 *   malloced server_cfg_t struct; need to free with free_session_cfg
 *********************************************************************/
extern session_cfg_t *
    new_session_cfg (val_value_t *sesval,
                     status_t *res);



/********************************************************************
 * FUNCTION new_session_cfg
 * 
 * Malloc and fill in a new session_cfg struct from the conf file
 * value tree
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
extern session_cfg_t *
    new_session_cfg_cli (const xmlChar *name,
                         const xmlChar *server,
                         const xmlChar *username,
                         const xmlChar *password,
                         const char *publickey,
                         const char *privatekey,
                         uint16 ncport,
                         uint16 protocols,
                         status_t *res);



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
extern status_t
    do_start_session (server_cb_t *server_cb,
                      obj_template_t *rpc,
                      const xmlChar *line,
                      uint32  len);


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
extern status_t
    do_stop_session (server_cb_t *server_cb,
                     obj_template_t *rpc,
                     const xmlChar *line,
                     uint32  len);

/********************************************************************
 * FUNCTION connect_named_session
 *
 * Connect a named session.
 *
 * INPUT:
 *   valset == connection valset to use
 *   server_cb == server cb to clone
 *
 * RETURNS: status.
 *********************************************************************/
extern status_t
    connect_named_session (const val_value_t *valset,
                           server_cb_t *server_cb);

/********************************************************************
 * FUNCTION connect_all_sessions
 *
 * INPUT:
 *   server_cb == server cb to clone
 *
 * RETURNS: status.
 *********************************************************************/
extern status_t
    connect_all_sessions (const val_value_t *valset,
                          server_cb_t *server_cb);

/********************************************************************
 * FUNCTION check_connect_all_sessions
 *
 * INPUT:
 *
 * RETURNS: status.
 *********************************************************************/
extern status_t
    check_connect_all_sessions (server_cb_t *server_cb);


/********************************************************************
 * FUNCTION do_session (local RPC)
 * 
 * session set-current[=session-name]
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
extern status_t
    do_session (server_cb_t *server_cb,
                obj_template_t *rpc,
                const xmlChar *line,
                uint32  len);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_sessions */
