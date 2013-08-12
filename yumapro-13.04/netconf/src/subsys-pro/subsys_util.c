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
/*  FILE: subsys-util.c

  Shared functions from the subsystem used by yp-shell

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
24-oct-12    abb      begun; split out from subsystem.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <stdarg.h>
#include <termios.h>

#include "procdefs.h"

#include "ncxconst.h"
#include "send_buff.h"
#include "status.h"
#include "subsystem.h"
#include "subsys_util.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
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
status_t
    get_ssh_parms (subsys_cb_t *cb)
{
    /* get the username */
    cb->user = getenv("USER");
    if (!cb->user) {
        SUBSYS_TRACE1(cb, "ERROR: init_subsys(): Get USER variable failed\n");
        return ERR_NCX_MISSING_PARM;
    }

    /* expecting the connection environment to be OpenSSH
     * get the client address */
    char *con = getenv("SSH_CONNECTION");
    if (!con) {
        SUBSYS_TRACE1(cb, "ERROR: init_subsys(): "
                       "Get SSH_CONNECTION variable failed\n" );
        return ERR_NCX_MISSING_PARM;
    }

    /* get the client addr */
    cb->client_addr = strdup(con);
    if (!cb->client_addr) {
        SUBSYS_TRACE1(cb, "ERROR: init_subsys(): "
                       "strdup(client_addr) failed\n" );
        return ERR_NCX_MISSING_PARM;
    } else {
        malloc_cnt++;
    }
    char *cp = strchr(cb->client_addr, ' ');
    if (!cp) {
        SUBSYS_TRACE1(cb, "ERROR: init_subsys(): "
                       "Malformed SSH_CONNECTION variable\n" );
        return ERR_NCX_INVALID_VALUE;
    } else {
        *cp = 0;
    }

    /* get the server connect port */
    cp = strrchr(con, ' ');
    if (cp && cp[1]) {
        cb->port = strdup(++cp);
    }
    if (!cb->port) {
        SUBSYS_TRACE1(cb, "ERROR: init_subsys(): "
                       "Malformed SSH_CONNECTION variable\n" );
        return ERR_NCX_INVALID_VALUE;
    } else {
        malloc_cnt++;
    }

    return NO_ERR;

}  /* get_ssh_parms */


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
status_t
    start_connection (subsys_cb_t *cb)
{
    /* make a socket to connect to the NCX server */
    cb->ncxsock = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (cb->ncxsock < 0) {
        SUBSYS_TRACE1(cb, "ERROR: init_subsys(): NCX Socket Creation "
                      "failed\n");
        return ERR_NCX_CONNECT_FAILED;
    }

    struct sockaddr_un ncxname;
    ncxname.sun_family = AF_LOCAL;
    strncpy(ncxname.sun_path, NCXSERVER_SOCKNAME, sizeof(ncxname.sun_path));

    /* try to connect to the NCX server */
    int ret = connect(cb->ncxsock, (const struct sockaddr *)&ncxname,
                      SUN_LEN(&ncxname));
    if (ret != 0) {
        SUBSYS_TRACE1(cb, "ERROR: init_subsys(): NCX Socket Connect failed\n");
        return ERR_NCX_OPERATION_FAILED;
    } else {
        SUBSYS_TRACE2(cb, "DEBUG:  init_subsys(): "
                       "NCX Socket Connected on FD: %d \n", cb->ncxsock);
        cb->ncxconnect = TRUE;
    }

#ifdef USE_NONBLOCKING_IO
    /* set non-blocking IO */
    if (fcntl(cb->ncxsock, F_SETFD, O_NONBLOCK)) {
        SUBSYS_TRACE1(cb, "ERROR: init_subsys(): fnctl() failed\n");
    }
#endif

    /* connected to the ncxserver and setup the ENV vars ok */
    return NO_ERR;

} /* start_connection */


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
status_t 
    send_cli_ncxconnect (subsys_cb_t *cb)
{
    status_t  res;
    const char connectmsg[] = 
        "%s\n<ncx-connect xmlns=\"%s\" version=\"%d\" user=\"%s\" "
        "address=\"%s\" magic=\"%s\" transport=\"netconf-cli\" "
        "protocol=\"cli\" />\n%s";

    //memset(msgbuff, 0x0, SUBSYS_BUFFLEN);
    snprintf(cb->msgbuff, SUBSYS_BUFFLEN, connectmsg, 
             (const char *)XML_START_MSG, 
             NCX_URN, NCX_SERVER_VERSION, cb->user, cb->client_addr, 
             NCX_SERVER_MAGIC, NC_SSH_END);

    res = send_buff(cb->ncxsock, cb->msgbuff, strlen(cb->msgbuff));
    return res;

} /* send_cli_ncxconnect */


/********************************************************************
* FUNCTION init_subsys_cb
*
* Initialize the fields of a subsystem control block
* 
* INPUTS:
*    cb == control block to initialize
*********************************************************************/
void
    init_subsys_cb (subsys_cb_t *cb)
{
    cb->request_uri = NULL;
    cb->request_method = NULL;
    cb->content_type = NULL;
    cb->content_length = NULL;
    cb->modified_since = NULL;
    cb->match = NULL;
    cb->http_accept = NULL;
    cb->user = NULL;

    cb->client_addr = NULL;
    cb->port = NULL;
    cb->ncxsock = 0;
    cb->ncxconnect = FALSE;
    cb->proto_id = PROTO_ID_NONE;

    cb->traceLevel = 0;
    cb->errfile = NULL;

    // do not initialize message buffer

} /* init_subsys_cb */


/********************************************************************
* FUNCTION start_subsys_ypshell
*
* Initialize the subsystem, and get it ready to send and receive
* the first message of any kind; special version for yp-shell
* 
* This is the module entry point for yp-shell only
* The subsystem.c module cannot be linked with netconfd-pro
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
status_t
    start_subsys_ypshell (int *retfd)
{
    subsys_cb_t shell_cb;

    init_subsys_cb(&shell_cb);

    *retfd = -1;

    status_t res = get_ssh_parms(&shell_cb);

    if (res == NO_ERR) {
        res = start_connection(&shell_cb);
    }

    if (res == NO_ERR) {
        res = send_cli_ncxconnect(&shell_cb);
    }

    if (res == NO_ERR) {
        *retfd = shell_cb.ncxsock;
    }

    /* inline cleanup_subsys */
    m__free(shell_cb.client_addr);
    shell_cb.client_addr = NULL;
    m__free(shell_cb.port);
    shell_cb.port = NULL;

    /* do not close the socket -- still in use if started OK;
     * will be closed by the ses_free_cb function
     * connected to the ncxserver and setup the ENV vars ok */
    return res;

} /* start_subsys_ypshell */


/* END subsys_util.c */

