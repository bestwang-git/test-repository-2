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
#ifndef _H_subsystem
#define _H_subsystem

/*  FILE: subsystem.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

   thin client IO handler for YumaPro server    

*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
06-oct-12    abb      Begun; split from netconf_subsystem.c

*/

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

// max 8K <ncx-connect> message
#define SUBSYS_BUFFLEN  32000

#define SUBSYS_TRACE1(cb, fmt, ...) if (cb->traceLevel && cb->errfile) \
                                { \
                                    fprintf(cb->errfile, fmt, ##__VA_ARGS__); \
                                    fflush(cb->errfile); \
                                }

#define SUBSYS_TRACE2(cb, fmt, ...) if (cb->traceLevel > 1 && cb->errfile) \
                                { \
                                    fprintf(cb->errfile, fmt, ##__VA_ARGS__); \
                                    fflush(cb->errfile); \
                                }

#define SUBSYS_TRACE3(cb, fmt, ...) if (cb->traceLevel > 2 && cb->errfile) \
                                { \
                                    fprintf(cb->errfile, fmt, ##__VA_ARGS__); \
                                    fflush(cb->errfile); \
                                }

/* copied from ncx/xml_util.h to avoid inluding that file
 * and all the files it includes
 */
#define XML_START_MSG ((const xmlChar *)\
		       "<?xml version=\"1.0\" encoding=\"UTF-8\"?>")


/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/

/* the type of protocol starting the subsystem */
typedef enum proto_id_t_ {
    PROTO_ID_NONE,
    PROTO_ID_NETCONF,
    PROTO_ID_YANGAPI,
    PROTO_ID_CLI,
    PROTO_ID_WEBUI
} proto_id_t;


/********************************************************************
* FUNCTION TEMPLATE subsys_stdout_fn_t
*
* INPUTS:
*   buff == buffer to write to STDOUT
*   bufflen == number of bytes to write
* RETURNS:
*   status
*/
typedef status_t
    (*subsys_stdout_fn_t)(const char *buff,
                          size_t bufflen);


/********************************************************************
* FUNCTION TEMPLATE subsys_stdin_fn_t
*
* INPUTS:
*   buff == buffer to read from STDIN
*   bufflen == max number of bytes to read
* RETURNS:
*   -1 if some error; 0 if EOF reached; >0 to indicate
*    number of bytes read
*/
typedef ssize_t
    (*subsys_stdin_fn_t)(char *buff,
                         size_t bufflen);


/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/


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
extern int run_subsystem (proto_id_t protocol_id,
                          int trace_level,
                          int argc,
                          char **argv);


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
extern int
    run_subsystem_ex (proto_id_t protocol_id,
                      int trace_level,
                      int argc, 
                      char **argv, 
                      char **envp,
                      subsys_stdin_fn_t stdin_fn,
                      subsys_stdout_fn_t stdout_fn,
                      int32 stdin_len);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_subsystem */
