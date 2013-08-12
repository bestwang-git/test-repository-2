/*
 * YANG-API 
   Adapted from echo.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include "fcgi_config.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>


#ifdef DEBUG
//#define DEBUG_TRACE 1
#endif

#ifdef DEBUG_TRACE
#ifdef _WIN32
#include <process.h>
#else
extern char **environ;
#endif
#endif

#include "fcgi_stdio.h" /* fcgi library; put it first*/

#include "procdefs.h"
#include "ncxconst.h"
#include "subsystem.h"
#include "subsys_util.h"

/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#define OK                 0
#define INPUT_ERROR        1
#define OUTPUT_ERROR       2
#define MALLOC_ERROR       3
#define MISSING_PARM       4
#define FILE_ERROR1        5
#define FILE_ERROR2        6
#define FILE_ERROR3        7
#define FILE_ERROR4        8
#define UNSUPPORTED_METHOD 9
#define SUBSYS_FAILED     10

#define CONTENT_TYPE_XML   "text/xml"
#define CONTENT_TYPE_JSON  "application/json"


/********************************************************************
*                                                                   *
*                          T Y P E S                                *
*                                                                   *
*********************************************************************/

typedef struct yang_api_profile_t_ {

    /* per-session vars */
    const char *username;                      /* REST API username */

    /* per-request vars */
    int   content_length;                   /* input content length */
    const char *content_type;                    /* input MIME type */
    const char *method;               /* HTTP Requestor method name */
    const char *path_info;         /* rest or URL after script name */

} yang_api_profile_t;


#ifdef DEBUG_TRACE
static void PrintEnv(const char *label, char **envp)
{
    printf("%s:<br>\n<pre>\n", label);
    for ( ; *envp != NULL; envp++) {
        printf("%s\n", *envp);
    }
    printf("</pre><p>\n");
}
#endif


/******************************************************************
 * FUNCTION send_yangapi_buff
 *
 * Send a buffer of bytes to the FCGI STDOUT
 *
 * INPUTS:
 *     buff == buffer to writer
 *     bufflen == number of bytes  in buffer
 * RETURNS:
 *   status
 ******************************************************************/
static status_t
    send_yangapi_buff (const char *buff,
                       size_t bufflen)
{
    size_t idx = 0;
    for (; idx < bufflen; idx++) {
        putchar(*buff++);
    }
    return NO_ERR;

} /* send_yangapi_buff */


/******************************************************************
 * FUNCTION read_yangapi_buff
 *
 * Read a buffer of bytes from the FCGI STDIN
 *
 * INPUTS:
 *     buff == buffer to writer
 *     bufflen == max number of bytes to read
  * RETURNS:
 *   -1 if some error; 0 if EOF reached; >0 to indicate
 *    number of bytes read
 ******************************************************************/
static ssize_t
    read_yangapi_buff (char *buff, 
                       size_t bufflen)
{
    size_t idx = 0;

    for (; idx < bufflen; idx++) {
        int ch = getchar();
        if (ch == EOF) {
            return idx;
        } else {
            *buff++ = (xmlChar)ch;
        }
    }
    return idx;

} /* read_yangapi_buff */


/********************************************************************
* FUNCTION yang_api_init
* 
* Setup global vars before accepting any requests
*
* INPUTS:
*   profile == rest gateway profile to init
*
* RETURNS:
*    status: 0 == OK
*********************************************************************/
static int yang_api_init (yang_api_profile_t *profile)
{
    memset(profile, 0x0, sizeof(yang_api_profile_t));

    return OK;

}  /* yang_api_init */


/********************************************************************
* FUNCTION yang_api_cleanup
* 
* Cleanup all vars before exit
*
* INPUTS:
*   gp == rest gateway profile to cleanup
*
*********************************************************************/
static void yang_api_cleanup (yang_api_profile_t *gp)
{
    memset(gp, 0x0, sizeof(yang_api_profile_t));

}  /* yang_api_cleanup */


/********************************************************************
* FUNCTION cleanup_request
* 
* Cleanup all vars after request is done
*
* INPUTS:
*   gp == YANG-API profile to cleanup
*
*********************************************************************/
static void cleanup_request (yang_api_profile_t *gp)
{

    gp->content_length = 0;
    gp->content_type = NULL;
    gp->method = NULL;
    gp->path_info = NULL;

}  /* cleanup_request */


/********************************************************************
* FUNCTION save_environment_vars
* 
*  Save pointers to the relevant FastCGI env
*
* INPUTS:
*    profile == profile parameters to use
*
* RETURNS:
*    status: 0 == OK
*********************************************************************/
static int save_environment_vars (yang_api_profile_t *profile)
{
    char *str = getenv("CONTENT_LENGTH");

    if (str != NULL) {
        profile->content_length = strtol(str, NULL, 10);
#ifdef DEBUG_TRACE
        printf("Got CONTENT_LENGTH (%s)\n", str);
#endif
    }
    else {
        profile->content_length = 0;
    }

    if (profile->content_length > 0) {
        profile->content_type = getenv("CONTENT_TYPE");
        if (profile->content_type == NULL) {
#ifdef DEBUG_TRACE
            printf("Missing CONTENT_TYPE\n");
#endif
            return MISSING_PARM;
        } else {
#ifdef DEBUG_TRACE
            printf("Got CONTENT_TYPE (%s)\n", profile->content_type);
#endif
        }
    }

    profile->method = getenv("REQUEST_METHOD");
    if (profile->method == NULL) {
#ifdef DEBUG_TRACE
            printf("Missing REQUEST_METHOD\n");
#endif
        return MISSING_PARM;
    }

    profile->path_info = getenv("PATH_INFO");

    profile->username = getenv("REMOTE_USER");
    if (profile->username == NULL) {
        // TEMP!!!
        profile->username = "yang-api";
#ifdef DEBUG_TRACE
        printf("Missing REMOTE_USER\n");
#endif

        //return MISSING_PARM;
    } else {
#ifdef DEBUG_TRACE
        printf("Got REMOTE_USER (%s)\n", profile->username);
#endif
    }

    /* check if the username is well-formed */
    const char *tempstr = profile->username;
    if (*tempstr == '-') {
        return MISSING_PARM;
    }
    while (*tempstr) {
        if (isspace(*tempstr) || !isprint(*tempstr)) {
            return MISSING_PARM;
        }
        tempstr++;
    }
#ifdef DEBUG_TRACE
    printf("REMOTE_USER ok\n");
#endif

    return OK;

}  /* save_environment_vars */


/********************************************************************
* FUNCTION main
*
* STDIN is input from the HTTP server through FastCGI wrapper 
*   (sent to ncxserver)
* STDOUT is output to the HTTP server (rcvd from ncxserver)
* 
* RETURNS:
*   0 if NO_ERR
*   1 if error connecting or logging into ncxserver
*********************************************************************/
int main (int argc, char **argv, char **envp)
{
    int status = 0;
    yang_api_profile_t  yang_api_profile;

    /* setup global vars used across all requests */
    status = yang_api_init(&yang_api_profile);

    if (status != OK) {
        printf("Content-type: text/html\r\n\r\n"
               "<html><head><title>YANG-API echo</title></head>"
               "<body><h1>YANG-API init failed</h1></body></html>\n");
    }

    /* temp: exit on all errors for now */
    while (status == OK && FCGI_Accept() >= 0) {
#ifdef DEBUG_TRACE
        printf("Content-type: text/html\r\n\r\n"
               "<html><head><title>YANG-API echo</title></head>"
               "<body><h1>YANG-API echo</h1>\n<pre>\n");
        PrintEnv("Request environment", environ);
#endif

        status = save_environment_vars(&yang_api_profile);
        if (status != OK) {
            continue;
        }

        status = setenv("REMOTE_USER", yang_api_profile.username, 0);
        if (status != 0) {
            continue;
        }

#ifdef DEBUG_TRACE
        printf("start invoke netconf-subsystem-pro\n");
#endif

        status = run_subsystem_ex(PROTO_ID_YANGAPI, 3, argc, argv, envp,
                                  read_yangapi_buff, send_yangapi_buff,
                                  yang_api_profile.content_length);
        if (status != OK) {
            continue;
        }

#ifdef DEBUG_TRACE
        printf("\n</pre></body></html>\n");
#endif

        cleanup_request(&yang_api_profile);
    } /* while */

    yang_api_cleanup(&yang_api_profile);

    return status;

}  /* main */
