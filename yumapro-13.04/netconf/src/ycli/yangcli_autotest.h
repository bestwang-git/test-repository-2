/*
 * Copyright (c) 2013, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
#ifndef _H_yangcli_autotest
#define _H_yangcli_autotest

/*  FILE: yangcli_autotest.h
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
05-mar-13    abb      Begun

*/

#include <xmlstring.h>

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val
#include "val.h"
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
* FUNCTION autotest_create_object
*
* Generate an instance of the specified object
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   obj == obj template to use
*   
* RETURNS:
*    status
*********************************************************************/
extern status_t
    autotest_create_object (server_cb_t *server_cb,
                            session_cb_t *session_cb,
                            obj_template_t *obj,
                            val_value_t *parentval);


/********************************************************************
* FUNCTION autotest_create_object_payload
*
* Generate an instance of the specified top-level object
*  as a <config> payload
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   obj == obj template to use
*   retres == address of return status
*
* RETURNS:
*    malloced <config> element with N random instances of
*    the specified top-level object
*********************************************************************/
extern val_value_t *
    autotest_create_object_payload (server_cb_t *server_cb,
                                    session_cb_t *session_cb,
                                    obj_template_t *obj,
                                    status_t *retres);


/********************************************************************
 * FUNCTION do_auto_test (local RPC)
 * 
 * auto-test target=/path/to/object
 *      max-instances=N
 *      session-name=named session to use (current session if NULL)
 *
 * Run editing tests on the specified config path object
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the autotest command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
extern status_t
    do_auto_test (server_cb_t *server_cb,
                  obj_template_t *rpc,
                  const xmlChar *line,
                  uint32  len);



/********************************************************************
* FUNCTION autotest_handle_rpc_reply
* 
* Handle the current <edit-comfig>, <commit>, or <copy-config<>
* response from the server and either start the next iteration
* or finish autotest mode
*
* INPUTS:
*   server_cb == server session control block to use
*   session_cb == session control block to use
*   scb == session control block to use
*   reply == data node from the <rpc-reply> PDU
*   anyerrors == TRUE if <rpc-error> detected instead of <data>
*             == FALSE if no <rpc-error> elements detected
*
* RETURNS:
*    status
*********************************************************************/
extern status_t
    autotest_handle_rpc_reply (server_cb_t *server_cb,
                               session_cb_t *session_cb,
                               val_value_t *reply,
                               boolean anyerrors);

/********************************************************************
* FUNCTION autotest_cancel
*
* Stop the autotest with an error
*
* INPUTS:
*   session_cb == session control block to use
*   autotest_cb == autotest control block to use
*********************************************************************/
extern void
    autotest_cancel (session_cb_t *session_cb);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_autotest */
