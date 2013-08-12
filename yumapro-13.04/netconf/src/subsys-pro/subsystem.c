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
/*  FILE: netconf-subsystem.c

   netconf-subsystem     : default usage for SSH
   netconf-subsystem -f file | -filename file  : set the trace file
   netconf-subsystem -t level | -trace level  : set the trace level
   netconf-subsystem -p proto | -protocol proto  : set the protocol

   This file is only used for the stand-alone programs

   * netconf-subsystem-pro
   * yp-rawshell   (not supported in server!!)

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
14-jan-07    abb      begun;
03-mar-11    abb      get rid of usleeps and replace with
                      design that checks for EAGAIN

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

#define MAX_READ_TRIES 1000


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

uint32 malloc_cnt;
uint32 free_cnt;

static subsys_cb_t subsys_cb;

static struct termios save_termios;
static int term_saved;


/******************************************************************
 * FUNCTION tty_raw
 *
 * Setup the terminal in IO mode 
 * Adapted from http://www.lafn.org/~dave/linux/terminalIO.html
 *
 * INPUTS:
 *   fd == file descriptor for the TTY to change
 * RETURNS:
 *   0 if NO_ERR; -1 if ERROR
 ******************************************************************/
static int tty_raw (int fd) 
{
    struct termios  buf;

    /* get the original state */
    if (tcgetattr(fd, &save_termios) < 0) {
        return -1;
    }

    buf = save_termios;

    /* echo off, canonical mode off, extended input
       processing off, signal chars off */
    buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* no SIGINT on BREAK, CR-toNL off, input parity
       check off, don't strip the 8th bit on input,
       ouput flow control off */
    buf.c_iflag &= ~(BRKINT | ICRNL | ISTRIP | IXON);

    /* clear size bits, parity checking off */
    buf.c_cflag &= ~(CSIZE | PARENB);

    /* set 8 bits/char */
    buf.c_cflag |= CS8;

    /* output processing off */
    buf.c_oflag &= ~(OPOST);
    buf.c_cc[VMIN] = 1;  /* 1 byte at a time */
    buf.c_cc[VTIME] = 0; /* no timer on input */

    if (tcsetattr(fd, TCSAFLUSH, &buf) < 0) {
        return -1;
    }

    term_saved = 1;

    return 0;
} /* tty_raw */


/******************************************************************
 * FUNCTION tty_reset
 *
 * Reset the original terminal in IO mode 
 * Adapted from http://www.lafn.org/~dave/linux/terminalIO.html
 *
 * INPUTS:
 *   fd == file descriptor for the TTY to change
 ******************************************************************/
static int tty_reset (int fd) {
    if (term_saved) {
        if (tcsetattr(fd, TCSAFLUSH, &save_termios) < 0) {
            return -1;
        }
    }
    return 0;
}  /* tty_reset */


/******************************************************************
 * FUNCTION configure_cli_parms
 *
 * Configure debug logging. This function evaluates command line
 * arguments to configure debug logging.
 *
 * Also configure the protocol in use
 ******************************************************************/
static void configure_cli_parms(subsys_cb_t *cb,
                                int argc, 
                                char **argv)
{
    int arg_idx = 1;
    char  defname[21];
    strncpy(defname, "/tmp/subsys-err.log", 20);

    char * err_filename = defname;

    while ( arg_idx < argc-1 ) {
        if ( !strcmp( argv[arg_idx], "-filename" ) ||
             !strcmp( argv[arg_idx], "-f" ) ) {
            err_filename = argv[++arg_idx];
            if ( !cb->traceLevel )
            {
                cb->traceLevel = 1;
            }
        }
        else if ( !strcmp( argv[arg_idx], "-trace" ) ||
                  !strcmp( argv[arg_idx], "-t" ) ) {
            cb->traceLevel = atoi( argv[++arg_idx] );
        }
        else if ( !strcmp( argv[arg_idx], "-protocol" ) ||
                  !strcmp( argv[arg_idx], "-p" ) ) {
            char *protocol_name = argv[++arg_idx];
            if ( !strcmp( protocol_name, "restapi" ) ) {
                cb->proto_id = PROTO_ID_YANGAPI;
            } else if ( !strcmp( protocol_name, "cli" ) ) {
                cb->proto_id = PROTO_ID_CLI;
            }
        }

        ++arg_idx;
    }

    if ( cb->traceLevel ) {
        cb->errfile = fopen( err_filename, "a" );
        if (cb->proto_id == PROTO_ID_YANGAPI) {
            SUBSYS_TRACE1(cb, "\n*** New YANG-API Session Started ***\n" );
        } else if (cb->proto_id == PROTO_ID_CLI) {
            SUBSYS_TRACE1(cb, "\n*** New CLI Session Started ***\n" );
        } else {
            SUBSYS_TRACE1(cb, "\n*** New NETCONF Session Started ***\n" );
        }
    }
}  /* configure_cli_parms */


/********************************************************************
* FUNCTION init_subsys
*
* Initialize the subsystem, and get it ready to send and receive
* the first message of any kind
* 
* RETURNS:
*   status
*********************************************************************/
static status_t
    init_subsys (void)
{
    status_t res = NO_ERR;

    subsys_cb_t *cb = &subsys_cb;

    if (cb->proto_id == PROTO_ID_YANGAPI) {
        /* expecting the connection environment to be FastCGI
         * get mandatory client address */
        char *con = getenv("REMOTE_ADDR");
        if (!con) {
            SUBSYS_TRACE1(cb, "ERROR: init_subsys(): "
                          "Get REMOTE_ADDR variable failed\n" );
            return ERR_INTERNAL_VAL;
        }

        cb->client_addr = strdup(con);
        if (!cb->client_addr) {
            SUBSYS_TRACE1(cb, "ERROR: init_subsys(): "
                           "strdup(client_addr) failed\n" );
            return ERR_INTERNAL_MEM;
        } else {
            malloc_cnt++;
        }

        /* get mandatory request URI
         *
         * need to use REQUEST_URI instead of PATH_INFO
         * because that env-var is already translated and
         * '//' sequences are lost in translation
         * Do not bother with QUERY_STRING since that text
         * is also in REQUEST_URI
         */
        cb->request_uri = getenv("REQUEST_URI");
        if (!cb->request_uri) {
            SUBSYS_TRACE1(cb, "ERROR: init_subsys(): "
                           "Get REQUEST_URI variable failed\n" );
            return ERR_INTERNAL_VAL;
        }

        /* mandatory HTTP method */
        cb->request_method = getenv("REQUEST_METHOD");
        if (!cb->request_method) {
            SUBSYS_TRACE1(cb, "ERROR: init_subsys(): "
                           "Get REQUEST_METHOD variable failed\n" );
            return ERR_INTERNAL_VAL;
        }

        /* get the username */
        cb->user = getenv("REMOTE_USER");
        if (!cb->user) {
            SUBSYS_TRACE1(cb, "ERROR: init_subsys(): Get REMOTE_USER "
                           "variable failed\n");
            return ERR_INTERNAL_VAL;
        }

        /* optional content type */
        cb->content_type = getenv("CONTENT_TYPE");
        if (!cb->content_type) {
            cb->content_type = "none";
        }

        /* optional content length */
        cb->content_length = getenv("CONTENT_LENGTH");
        if (!cb->content_length) {
            cb->content_length = "0";
        }

        /* optional If-Modified-Since */
        cb->modified_since = getenv("HTTP_IF_MODIFIED_SINCE");
        if (!cb->modified_since) {
            cb->modified_since = "none";
        }

        /* optional If-Unmodified-Since */
        cb->unmodified_since = getenv("HTTP_IF_UNMODIFIED_SINCE");
        if (!cb->unmodified_since) {
            cb->unmodified_since = "none";
        }

        /* optional If-Match */
        cb->match = getenv("HTTP_IF_MATCH");
        if (!cb->match) {
            cb->match = "none";
        }

        /* optional If-None-Match */
        cb->none_match = getenv("HTTP_IF_NONE_MATCH");
        if (!cb->none_match) {
            cb->none_match = "none";
        }

        /* optional Accept */
        cb->http_accept = getenv("HTTP_ACCEPT");
        if (!cb->http_accept) {
            cb->http_accept = "none";
        }
    } else {
        res = get_ssh_parms(cb);
    }

    if (res == NO_ERR) {
        res = start_connection(cb);
    }

    return res;

} /* init_subsys */


/********************************************************************
* FUNCTION cleanup_subsys
*
* Cleanup the subsystem
* 
*********************************************************************/
static void
    cleanup_subsys (subsys_cb_t *cb)
{
    m__free(cb->client_addr);
    cb->client_addr = NULL;

    m__free(cb->port);
    cb->port = NULL;

    if (cb->ncxconnect) {
        close(cb->ncxsock);
        cb->ncxconnect = FALSE;
    }

    if (cb->errfile) {
        fclose(cb->errfile);
        cb->errfile = NULL;
    }

} /* cleanup_subsys */


/********************************************************************
* FUNCTION send_nc_ncxconnect
*
* Send the <ncx-connect> message to the ncxserver for SSH or LOCAL
* transport for NETCONF protocol
* 
* RETURNS:
*   status
*********************************************************************/
static status_t send_nc_ncxconnect (void)
{
    subsys_cb_t *cb = &subsys_cb;
    const char connectmsg[] = 
        "%s\n<ncx-connect xmlns=\"%s\" version=\"%d\" user=\"%s\" "
        "address=\"%s\" magic=\"%s\" protocol=\"netconf\" "
        "transport=\"ssh\" port=\"%s\" />\n%s";

    snprintf(cb->msgbuff, SUBSYS_BUFFLEN, connectmsg,
             (const char *)XML_START_MSG, 
             NCX_URN, NCX_SERVER_VERSION, cb->user, cb->client_addr, 
             NCX_SERVER_MAGIC, cb->port, NC_SSH_END);

    status_t res = send_buff(cb->ncxsock, cb->msgbuff, 
                             strlen(cb->msgbuff));
    return res;

} /* send_nc_ncxconnect */


/********************************************************************
* FUNCTION send_yangapi_ncxconnect
*
* Send the <ncx-connect> message to the ncxserver for YANG-API protocol
* 
* RETURNS:
*   status
*********************************************************************/
static status_t send_yangapi_ncxconnect (void)
{
    subsys_cb_t *cb = &subsys_cb;
    const char connectmsg[] =
        "%s\n<ncx-connect xmlns=\"%s\" version=\"%d\" user=\"%s\" "
        "address=\"%s\" magic=\"%s\" transport=\"netconf-http\" "
        "protocol=\"restapi\" method=\"%s\" uri=\"%s\" type=\"%s\" "
        "length=\"%s\" since=\"%s\" accept=\"%s\" match=\"%s\" "
        "nomatch=\"%s\" nosince=\"%s\" />\n%s";

    const char *str = cb->request_uri;
    int qcnt = 0;
    int acnt = 0;
    int len = 0;
    while (*str) {
        if (*str == '"') {
            qcnt++;
        } else if (*str == '&') {
            acnt++;
        }
        str++;
        len++;
    }

    /* make a guess at way more than the buffer used at 512 */
    if ((len + strlen(cb->user) + 512) > SUBSYS_BUFFLEN) {
        SUBSYS_TRACE1(cb, "ERROR: buffer would overflow\n" );
        return ERR_BUFF_OVFL;
    }
        
    if (qcnt || acnt) {
        char *newbuff = malloc(len+(qcnt*5)+(acnt*4)+1);
        if (!newbuff) {
            return ERR_INTERNAL_MEM;
        }
        char *newstr = newbuff;
        str = cb->request_uri;
        while (*str) {
            if (*str == '"') {
                *newstr++ = '&';
                *newstr++ = 'q';
                *newstr++ = 'u';
                *newstr++ = 'o';
                *newstr++ = 't';
                *newstr++ = ';';
            } else if (*str == '&') {
                *newstr++ = '&';
                *newstr++ = 'a';
                *newstr++ = 'm';
                *newstr++ = 'p';
                *newstr++ = ';';
            } else {
                *newstr++ = *str;
            }
            str++;
        }
        *newstr = 0;
        snprintf(cb->msgbuff, SUBSYS_BUFFLEN, connectmsg,
                 (const char *)XML_START_MSG,
                 NCX_URN, NCX_SERVER_VERSION, cb->user, cb->client_addr,
                 NCX_SERVER_MAGIC, cb->request_method,
                 newbuff, cb->content_type, cb->content_length,
                 cb->modified_since, cb->http_accept, cb->match,
                 cb->none_match, cb->unmodified_since, NC_SSH_END);
        free(newbuff);
    } else {
        snprintf(cb->msgbuff, SUBSYS_BUFFLEN, connectmsg,
                 (const char *)XML_START_MSG,
                 NCX_URN, NCX_SERVER_VERSION, cb->user, cb->client_addr,
                 NCX_SERVER_MAGIC, cb->request_method,
                 cb->request_uri, cb->content_type, cb->content_length,
                 cb->modified_since, cb->http_accept, cb->match,
                 cb->none_match, cb->unmodified_since, NC_SSH_END);
    }

    if (cb->traceLevel >= 2) {
        SUBSYS_TRACE2(cb, "DEBUG:  init_subsys(): "
                      "Sending YANG-API connect (%zu):\n\n%s\n", 
                      strlen(cb->msgbuff), cb->msgbuff);
    }

    status_t res = send_buff(cb->ncxsock, cb->msgbuff, strlen(cb->msgbuff));
    return res;

} /* send_yangapi_ncxconnect */


/********************************************************************
* FUNCTION send_webui_ncxconnect
*
* Send the <ncx-connect> message to the ncxserver for WEBui protocol
* 
* RETURNS:
*   status
*********************************************************************/
static status_t send_webui_ncxconnect (void)
{
    subsys_cb_t *cb = &subsys_cb;
    const char connectmsg[] = 
        "%s\n<ncx-connect xmlns=\"%s\" version=\"%d\" user=\"%s\" "
        "address=\"%s\" magic=\"%s\" transport=\"netconf-webui\" "
        "protocol=\"webui\" />\n%s";

    // TBD : not complete

    snprintf(cb->msgbuff, SUBSYS_BUFFLEN, connectmsg,
             (const char *)XML_START_MSG, 
             NCX_URN, NCX_SERVER_VERSION, cb->user, cb->client_addr, 
             NCX_SERVER_MAGIC, NC_SSH_END);

    status_t  res = send_buff(cb->ncxsock, cb->msgbuff, strlen(cb->msgbuff));
    return res;

} /* send_webui_ncxconnect */


/********************************************************************
* FUNCTION do_read
*
* Read from a FD
* 
* INPUTS:
*              
* RETURNS:
*   return byte count
*********************************************************************/
static ssize_t
    do_read (int readfd, 
             char *readbuff, 
             size_t readcnt, 
             status_t *retres)
{
    subsys_cb_t *cb = &subsys_cb;
    boolean   readdone = FALSE;
    ssize_t   retcnt = 0;

    *retres = NO_ERR;

    while (!readdone && *retres == NO_ERR) {
        retcnt = read(readfd, readbuff, readcnt);
        if (retcnt < 0) {
            if (errno != EAGAIN) {
                SUBSYS_TRACE1(cb, "ERROR: do_read(): read of FD(%d): "
                               "failed with error: %s\n", 
                               readfd, strerror( errno  ) );
                *retres = ERR_NCX_READ_FAILED;
                continue;
            }
        } else if (retcnt == 0) {
            SUBSYS_TRACE1(cb, "INFO: do_read(): closed connection\n");
            *retres = ERR_NCX_EOF;
            readdone = TRUE;
            continue;
        } else {
            /* retcnt is the number of bytes read */
            readdone = TRUE;
            SUBSYS_TRACE3(cb, "DEBUG: do_read: OK (%zu)\n", retcnt);
        }
    }  /*end readdone loop */

    return retcnt;

}  /* do_read */


/********************************************************************
* FUNCTION io_loop
*
* Handle the IO for the program
* 
* INPUTS:
*              
* RETURNS:
*   status
*********************************************************************/
static status_t
    io_loop (void)
{
    subsys_cb_t *cb = &subsys_cb;
    status_t  res = NO_ERR;
    boolean done = FALSE;
    ssize_t retcnt = 0;
    int readleft = cb->content_len;
    boolean use_stdin = (readleft < 0);

    fd_set fds;
    FD_ZERO(&fds);

    while (!done) {
        FD_SET(cb->ncxsock, &fds);
        if (use_stdin) {
            FD_SET(STDIN_FILENO, &fds);
        }

        boolean select_called = FALSE;
        if (use_stdin || readleft <= 0) {
            SUBSYS_TRACE3(cb, "DEBUG: io_loop: about to call select\n");
            int ret = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
            select_called = TRUE;
            if (ret < 0) {
                if ( errno != EINTR ) {
                    SUBSYS_TRACE1(cb, "ERROR: io_loop(): select() "
                                  "failed with error: %s\n",
                                  strerror( errno ) );
                    res = ERR_NCX_OPERATION_FAILED;
                    done = TRUE;
                }
                continue;
            } else if (ret == 0) {
                SUBSYS_TRACE1(cb, "ERROR: io_loop(): select() "
                              "returned 0, exiting...\n" );
                res = NO_ERR;
                done = TRUE;
                continue;
            } /* else some IO to process */
        }

        /* check any input from client
         * if content_len was > 0 then all that content will be
         * read and transferred to the server before starting
         * to look for input from the server
         */
        if ((!use_stdin && readleft > 0) || FD_ISSET(STDIN_FILENO, &fds)) {
            SUBSYS_TRACE2(cb, "DEBUG: read STDIN\n");
            if (cb->stdin_fn != NULL) {
                int32 maxread = 0;
                if (readleft > 0) {
                    maxread = min(readleft, SUBSYS_BUFFLEN);
                } else {
                    maxread = SUBSYS_BUFFLEN;
                }
                SUBSYS_TRACE3(cb, "DEBUG: calling stdin_fn max:%d\n", maxread);
                int32 cnt = (*cb->stdin_fn)(cb->msgbuff, maxread);
                if (cnt < 0) {
                    res = ERR_NCX_READ_FAILED;
                } else if (cnt == 0) {
                    res = ERR_NCX_EOF;
                } else {
                    retcnt = (ssize_t)cnt;
                    res = NO_ERR;
                    if (readleft > 0) {
                        readleft -= cnt;
                    }
                }
            } else {
                /* get buff from openssh */
                retcnt = do_read(STDIN_FILENO, cb->msgbuff, 
                                 (size_t)SUBSYS_BUFFLEN, &res);
            }
            if (res == ERR_NCX_EOF && retcnt <= 0) {
                res = NO_ERR;
                if (cb->proto_id == PROTO_ID_YANGAPI) {
                    SUBSYS_TRACE1(cb, "INFO: io_loop(): "
                                  "no more input from HTTP client\n");
                } else {
                    done = TRUE;
                    continue;
                }
            } else if (res == ERR_NCX_SKIPPED) {
                res = NO_ERR;
            } else if ((res == NO_ERR || res == ERR_NCX_EOF) && retcnt > 0) {
                if (cb->traceLevel >= 2) {
                    SUBSYS_TRACE2(cb, "DEBUG: io_loop: send to NCXSOCK (%zu)\n",
                                  retcnt);
                }
                if (cb->traceLevel >= 3) {
                    cb->msgbuff[retcnt] = 0;
                    SUBSYS_TRACE3(cb, "DEBUG: io_loop(): "
                                  "Sending buff\n\n%s\n", cb->msgbuff);
                }

                /* The lastbuff flag is used only if the stdin_fn is
                 * also used.  The message body in a POST request is
                 * sent to the server as a separate message with
                 * NETCONF v1.0 framing;  The response message 
                 * coming back from the server does not have any framing
                 */
                boolean lastbuff = FALSE;
                if (cb->stdin_fn != NULL) {
                    lastbuff = (readleft == 0);
                }
                if (lastbuff && 
                    ((retcnt + NC_SSH_END_LEN) < SUBSYS_BUFFLEN)) {
                    /* add the NETCONF v1.0 EOM marker */
                    strcpy(&cb->msgbuff[retcnt], NC_SSH_END);
                    retcnt += NC_SSH_END_LEN;
                    lastbuff = FALSE;
                }

                /* send the received buffer to the ncxserver */
                res = send_buff(cb->ncxsock, cb->msgbuff, (size_t)retcnt);
                if (res != NO_ERR) {
                    SUBSYS_TRACE1(cb, "ERROR: io_loop(): "
                                  "send_buff() to ncxserver "
                                   "failed with %s\n", strerror( errno ) );
                    done = TRUE;
                    continue;
                }

                if (lastbuff) {
                    /* could not fit the EOM marker */
                    res = send_buff(cb->ncxsock, NC_SSH_END, NC_SSH_END_LEN);
                    if (res != NO_ERR) {
                        SUBSYS_TRACE1(cb, "ERROR: io_loop(): "
                                      "send_buff() to ncxserver "
                                      "failed with %s\n", strerror( errno ) );
                        done = TRUE;
                        continue;
                    }
                }
            }
        }  /* if STDIN needs to be read */

        /* check any input from the YP server */
        if (select_called && FD_ISSET(cb->ncxsock, &fds)) {
            res = NO_ERR;

            SUBSYS_TRACE2(cb, "DEBUG: read NCXSOCK\n");

            retcnt = do_read(cb->ncxsock, cb->msgbuff, 
                             (size_t)SUBSYS_BUFFLEN, &res);
            if (res == ERR_NCX_EOF) {
                res = NO_ERR;
                done = TRUE;
                continue;
            } else if (res == ERR_NCX_SKIPPED) {
                res = NO_ERR;
            } else if (res == NO_ERR && retcnt > 0) {
                /* send this buffer to STDOUT */
                if (cb->traceLevel >= 2) {
                    SUBSYS_TRACE2(cb, "DEBUG: io_loop: send to STDOUT (%zu)\n",
                                  retcnt);
                }
                if (cb->traceLevel >= 3) {
                    cb->msgbuff[retcnt] = 0;
                    SUBSYS_TRACE3(cb, "DEBUG: io_loop(): "
                                  "Sending buff\n\n%s\n", cb->msgbuff);
                }

                if (cb->stdout_fn != NULL) {
                    res = (*cb->stdout_fn)(cb->msgbuff, retcnt);
                } else {
                    res = send_buff(STDOUT_FILENO, cb->msgbuff, 
                                    (size_t)retcnt);
                }
                if (res != NO_ERR) {
                    SUBSYS_TRACE1(cb, "ERROR: io_loop(): send_buff() to client "
                                   "failed with %s\n", strerror( errno ) );
                    done = TRUE;
                    continue;
                }
            }
        }
    }

    return res;

} /* io_loop */


/********************************************************************
* FUNCTION run_subsystem
*
* STDIN is input from the SSH client (sent to YP server)
* STDOUT is output to the SSH client (rcvd from YP server)
* 
* INPUTS:
*   protocol_id == default protocol to use if no -p CLI entered
*   trace_level == default trace level to use if no -t CLI entered
*   argc == argument count passed to program
*   argv == argument list passed to program
*
* RETURNS:
*   0 if NO_ERR
*   1 if error connecting or logging into ncxserver
*********************************************************************/
int run_subsystem (proto_id_t protocol_id,
                   int trace_level,
                   int argc, 
                   char **argv)
{
    return run_subsystem_ex(protocol_id, trace_level, argc, argv,
                            NULL, NULL, NULL, -1);
}


/********************************************************************
* FUNCTION run_subsystem_ex
*
* STDIN is input from the client (sent to YP server)
* STDOUT is output to the client (rcvd from YP server)
* 
* INPUTS:
*   protocol_id == default protocol to use if no -p CLI entered
*   trace_level == default trace level to use if no -t CLI entered
*   argc == argument count passed to program
*   argv == argument list passed to program
*   envp == environment list passed to program
*   stdin_fn == function to call for reading a buffer from STDIN
*             == NULL to use internal default function
*   stdout_fn == function to call for writing a buffer to STDOUT
*             == NULL to use internal default function
*   stdin_len == content length if limited or known in advance
*             == -1 if content_length not known or used
* RETURNS:
*   0 if NO_ERR
*   1 if error connecting or logging into ncxserver
*********************************************************************/
int run_subsystem_ex (proto_id_t protocol_id,
                      int trace_level,
                      int argc, 
                      char **argv, 
                      char **envp,
                      subsys_stdin_fn_t stdin_fn,
                      subsys_stdout_fn_t stdout_fn,
                      int32 stdin_len)
{
    subsys_cb_t *cb = &subsys_cb;
    const char *msg = "operation failed";

    malloc_cnt = 0;
    free_cnt = 0;
    term_saved = 0;

    init_subsys_cb(cb);
    cb->proto_id = protocol_id;
    cb->traceLevel = trace_level;
    cb->stdin_fn = stdin_fn;
    cb->stdout_fn = stdout_fn;
    cb->content_len = stdin_len;
    configure_cli_parms(cb, argc, argv);

    if (cb->traceLevel >= 1) {
        if (envp) {
            char **env;
            SUBSYS_TRACE1(cb, "\nprint all envp vars");
            for (env = envp; *env != 0; env++) {
                char* thisEnv = *env;
                SUBSYS_TRACE1(cb, "\nenv: %s", thisEnv);
            }
        }
        SUBSYS_TRACE1(cb, "\ntraceLevel %d\n", cb->traceLevel);
        SUBSYS_TRACE1(cb, "content_len %d\n", cb->content_len);
    }

    status_t res = init_subsys();
    if (res != NO_ERR) {
        msg = "init_subsys failed";
    }

    if (res == NO_ERR) {
        switch (cb->proto_id) {
        case PROTO_ID_NETCONF:
            res = send_nc_ncxconnect();
            if (res != NO_ERR) {
                msg = "send NETCONF ncx-connect failed";
            }
            break;
        case PROTO_ID_YANGAPI:
            res = send_yangapi_ncxconnect();
            if (res != NO_ERR) {
                msg = "send YANG-API ncx-connect failed";
            }
            break;
        case PROTO_ID_CLI:
            res = send_cli_ncxconnect(cb);
            if (res == NO_ERR) {
                SUBSYS_TRACE3(cb, "\nsetting tty_raw\n");
                int ret = tty_raw(STDIN_FILENO);
                if (ret) {
                    msg = "set tty_raw failed";
                    res = ERR_NCX_OPERATION_FAILED;
                } else {
                    SUBSYS_TRACE3(cb, "\nset tty_raw OK\n");
                }
            } else {
                msg = "send CLI ncx-connect failed";
            }
            break;
        case PROTO_ID_WEBUI:
            res = send_webui_ncxconnect();
            if (res != NO_ERR) {
                msg = "send WEBui ncx-connect failed";
            }
            break;
        default:
            msg = "unknown protocol";
            res = ERR_NCX_INVALID_VALUE;
        }
    }

    if (res == NO_ERR) {
        SUBSYS_TRACE3(cb, "DEBUG: starting io_loop()\n");
        res = io_loop();
        if (res != NO_ERR) {
            SUBSYS_TRACE1(cb, "ERROR: io_loop(): exited with error\n");
        } else {
            SUBSYS_TRACE2(cb, "INFO: io_loop(): exited OK\n" );
        }
    } else {
        SUBSYS_TRACE1(cb, "ERROR: run_subsystem setup failed (%s)\n", msg);
    }

    if (cb->proto_id == PROTO_ID_CLI) {
        tty_reset(STDIN_FILENO);
    }

    cleanup_subsys(cb);

    if (res != NO_ERR) {
        return 1;
    } else {
        return 0;
    }

} /* run_subsystem */

/* END subsystem.c */
 
