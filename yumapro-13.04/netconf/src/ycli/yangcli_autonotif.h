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
#ifndef _H_yangcli_autonotif
#define _H_yangcli_autonotif

/*  FILE: yangcli_autonotif.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

   NETCONF YANG-based CLI Tool

   autonotif mode support
  
 
*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
27-dec-12    abb      Begun

*/

#include <xmlstring.h>

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
* FUNCTION start_autonotif_mode
* 
* Send a <create-subscription> operation to the server, only
* if the server indicates it supports notifications and interleave
*
* INPUTS:
*   server_cb == server control block to use
*   session_cb == session control block to use
*   stream_name == name of notification stream to use (NULL = NETCONF)
*
* RETURNS:
*    status
*********************************************************************/
extern status_t
    start_autonotif_mode (server_cb_t *server_cb,
                          session_cb_t *session_cb,
                          const xmlChar *stream_name);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_autonotif */
