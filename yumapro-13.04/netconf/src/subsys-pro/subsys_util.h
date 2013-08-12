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
#ifndef _H_subsys_util
#define _H_subsys_util

/*  FILE: subsys_util.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

   thin client IO handler for YumaPro server   (utility functions)

*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
24-oct-12    abb      Begun; split from subsystem.c

*/

#ifndef _H_subsystem
#include "subsystem.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

/* ncxserver magic cookie hack
 * this must match the definition in agt/agt_connect.c
 * It is purposely different than the string in Yuma
 */
#define NCX_SERVER_MAGIC \
  "x56o8937ab1erfgertgertb99r9tgb9rtyb99rtbwdesd9902k40vfrevef0Opal1t2p"


/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/

/* one subsystem control block for session */
typedef struct subsys_cb_t_ {

    const char *request_uri;
    const char *request_method;
    const char *content_type;
    const char *content_length;
    const char *modified_since;
    const char *unmodified_since;
    const char *match;
    const char *none_match;
    const char *http_accept;
    const char *user;

    subsys_stdin_fn_t stdin_fn;
    subsys_stdout_fn_t stdout_fn;
    int32 content_len;

    char *client_addr;
    char *port;
    int ncxsock;
    boolean ncxconnect;
    proto_id_t proto_id;

    int traceLevel;
    FILE *errfile;
    char msgbuff[SUBSYS_BUFFLEN];

} subsys_cb_t;


/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/

/********************************************************************
* FUNCTION get_ssh_parms
*
* Get the SSH environment parameters
* 
* INPUTS:
*  cb == control vlock to use
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    get_ssh_parms (subsys_cb_t *cb);


/********************************************************************
* FUNCTION start_connection
*
* Start the connection to the server
* 
* INPUTS:
*  cb == control vlock to use
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    start_connection (subsys_cb_t *cb);


/********************************************************************
* FUNCTION send_cli_ncxconnect
*
* Send the <ncx-connect> message to the ncxserver for CLI protocol
* 
* INPUTS:
*  cb == control vlock to use
*
* RETURNS:
*   status
*********************************************************************/
extern status_t 
    send_cli_ncxconnect (subsys_cb_t *cb);


/********************************************************************
* FUNCTION init_subsys_cb
*
* Initialize the fields of a subsystem control block
* 
* INPUTS:
*    cb == control block to initialize
*********************************************************************/
extern void
    init_subsys_cb (subsys_cb_t *cb);

/********************************************************************
* FUNCTION start_subsys_ypshell
*
* Initialize the subsystem, and get it ready to send and receive
* the first message of any kind; special version for yp-shell
* 
* INPUTS:
*  retfd == address of return file desciptor number assigned to
*           the socket that was created
* OUTPUTS:
*  *retfd == the file descriptor number used for the socket
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    start_subsys_ypshell (int *retfd);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_subsys_util */
