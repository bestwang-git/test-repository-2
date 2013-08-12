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
#ifndef _H_yangcli_record_test
#define _H_yangcli_record_test

/*  FILE: yangcli_record_test.h
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
13-augr-09    abb      Begun; move from yangcli_cmd.c

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
*                                                                   *
*                        C O N S T A N T S                          *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                            T Y P E S                              *
*                                                                   *
*********************************************************************/

/********************************************************************
*								    *
*		      F U N C T I O N S 			    *
*								    *
*********************************************************************/
/*********************************************************************
 * record_step_reply
 *  
 * INPUTS:
 *   server == server_control_block
 *      
 * Record a new step for the test.
 *  
 * INPUT:   server_cb_t to use
 * RETURNS: status_t: NO_ERR or ERR
 *********************************************************************/
extern status_t
    record_step_reply (server_cb_t *server_cb,
                       response_type_t resp_type,
                       const xmlChar *msg_id,
                       val_value_t *reqmsg,
                       val_value_t *rpydata);

/*********************************************************************
 * new_record_step
 *
 * Record a new step for the test.
 *
 * INPUTS:
 *   server_cb == server control block to use
 *   rpc == template for the local RPC
 *   line == input command line from user
 *   len == line length
 *
 * RETURNS: status_t: NO_ERR or ERR
 *********************************************************************/
extern status_t
     new_record_step (server_cb_t *server,
                     obj_template_t *rpc,
                     const xmlChar *line); 

/********************************************************************
* FUNCTION is_test_recording_on
*
* INPUTS:
*   server == server_control_block
*
* RETURNS:
*   TRUE if recording is on
*   FALSE if recording is off
*********************************************************************/
extern boolean
    is_test_recording_on (server_cb_t *server);

/********************************************************************
 * FUNCTION do_record_test (local RPC)
 * 
 * Get the specified parameter and record the test,
 * based on the parameter
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
    do_record_test (server_cb_t *server_cb,
	     obj_template_t *rpc,
	     const xmlChar *line,
	     uint32  len);

/********************************************************************
* FUNCTION set_cur_record_step_done
*
* INPUTS:
*    server == server control block to use
*    boolean == TRUE/FALSE
* RETURNS:
*   none
*********************************************************************/
extern void
    set_cur_record_step_done (server_cb_t *server, 
                              boolean done_status);

/********************************************************************
* FUNCTION record_test_init
*
* INPUTS:
*    server_cb == server control block to use
* RETURNS:
*   none
*********************************************************************/
extern void
    record_test_init (server_cb_t *server_cb);

/********************************************************************
* FUNCTION record_test_cleanup
*
* Cleanup the yangcli-unit-test test record module
*
* INPUTS:
*   server_cb == server context to use
* RETURNS:
*   none
*********************************************************************/
void
    record_test_cleanup (server_cb_t *server_cb);


/*********************************************************************
* record_new_step_valset
* INPUTS:
*   server_cb_t *server
*   rpc == RPC method that is being called
*   valset == commad line value to get
* RETURNS: status_t: NO_ERR or ERR
 *********************************************************************/
extern status_t
    record_new_step_valset (server_cb_t *server,
                          obj_template_t *rpc,
                          val_value_t *valset);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_record_test */
