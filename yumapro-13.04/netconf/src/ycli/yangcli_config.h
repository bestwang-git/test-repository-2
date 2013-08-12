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
#ifndef _H_yangcli_config
#define _H_yangcli_config

/*  FILE: yangcli_config.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

  
 
*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
16-nov-12    abb      Begun

*/

#include <xmlstring.h>

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_yangcli
#include "yangcli.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*		      F U N C T I O N S 			    *
*								    *
*********************************************************************/


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
extern status_t
    do_config (server_cb_t *server_cb,
               obj_template_t *rpc,
               const xmlChar *line,
               uint32  len);


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
extern status_t
    do_exit (server_cb_t *server_cb,
             obj_template_t *rpc,
             const xmlChar *line,
             uint32  len);


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
extern status_t
    handle_config_input (server_cb_t *server_cb,
                         session_cb_t *session_cb,
                         xmlChar *line);

/********************************************************************
 * FUNCTION force_exit_config_mode
 * (session was dropped -- exit config mode)
 * 
 * INPUTS:
 *    session_cb == session control block to use
 *********************************************************************/
extern void
    force_exit_config_mode (session_cb_t *session_cb);


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
extern void
    config_check_transfer (session_cb_t *session_cb,
                           val_value_t *rootval);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_config */
