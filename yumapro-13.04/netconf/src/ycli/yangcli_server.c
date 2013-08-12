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
/*  FILE: yangcli_server.c

   NETCONF YANG-based CLI Tool in yp-shell mode

   See ./README for details

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
24-oct-12    abb      begun; started from yangcli.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <assert.h>
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

#include "libtecla.h"

#include "procdefs.h"
#include "log.h"
#include "status.h"
#include "yangcli.h"
#include "yangcli_cmd.h"
#include "yangcli_server.h"
#include "yangcli_util.h"


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
status_t
    yangcli_server_connect (server_cb_t *server_cb)
{
#ifdef DEBUG
    if (server_cb == NULL) {
        return SET_ERROR(ERR_INTERNAL_PTR);
    }
#endif

    session_cb_t *session_cb = server_cb->cur_session_cb;

    log_debug("\nyangcli_connect: start local session");

    /* check if all params present yet */
    status_t res = create_session(server_cb, session_cb, NULL);

    return res;

}  /* yangcli_server_connect */


/* END yangcli_server.c */
