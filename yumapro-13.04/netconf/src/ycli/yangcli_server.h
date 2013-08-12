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
#ifndef _H_yangcli_server
#define _H_yangcli_server

/*  FILE: yangcli_server.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

 yp-shell connection support module
  
 
*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
24-oct-12    abb      Begun; moved from yangcli.c

*/

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
 * FUNCTION yangcli_server_connect
 * 
 * INPUTS:
 *   server_cb == server control block to use
 *
 * OUTPUTS:
 *   connect_valset parms may be set 
 *   create_session may be called
 *
 * RETURNS:
 *   status
 *********************************************************************/
extern status_t
    yangcli_server_connect (server_cb_t *server_cb);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_server */
