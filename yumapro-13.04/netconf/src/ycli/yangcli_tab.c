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
/*  FILE: yangcli_tab.c

   NETCONF YANG-based CLI Tool

   interactive CLI tab completion support 


  normal command mode
  -------------------

   fill_completion_commands --> 

    if match an OBJ_TYP_RPC on the first term --> 

        fill_one_rpc_completion_parms  -->
          if starts-wit '/' then --> fill_one_xpath_completion
  
       
    else -->  fill_one_completion_commands  -->

       fill_one_module_completion_commands  -->

          fill in all the matching top-level commands


*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
18-apr-09    abb      begun; split out from yangcli.c
25-jan-12    abb      Add xpath tab completion provided by Zesi Cai
 
*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
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
#include "ncx.h"
#include "ncx_list.h"
#include "ncxtypes.h"
#include "obj.h"
#include "obj_help.h"
#include "rpc.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "var.h"
#include "xml_util.h"
#include "yangcli.h"
#include "yangcli_alias.h"
#include "yangcli_cmd.h"
#include "yangcli_tab.h"
#include "yangcli_tab_config.h"
#include "yangcli_util.h"
#include "yangconst.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

/* make sure this is not enabled in the checked in version
 * will mess up STDOUT display with debug messages!!!
 */
/* if log-level >= debug2 */
//#define YANGCLI_TAB_DEBUG 1

/* if log-level >= debug */
//#define DEBUG_TRACE 1


#define MAX_HELP_LINE 79

/********************************************************************
*                                                                   *
*                          T Y P E S                                *
*                                                                   *
*********************************************************************/

/* package all parameters for identity walk callback function */
typedef struct cb_parmpack_t_ {
    WordCompletion *cpl;
    completion_state_t *comstate;
    const char *line;
    int word_start;
    int word_end;
    int parmlen;
} cb_parmpack_t;


/********************************************************************
*                                                                   *
*             F O R W A R D   D E C L A R A T I O N S               *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
 * FUNCTION init_comstate
 * 
 * Initialize select comstate variables when tab or help
 * mode is entered
 *
 * INPUTS:
 *    comstate == completion state to init
 *
 * OUTPUTS:
 *   some fields in comstate filled in
 *********************************************************************/
static void
    init_comstate (completion_state_t *comstate)
{
    comstate->config_done = FALSE;
    comstate->keys_done = FALSE;
    comstate->gl_normal_done = FALSE;
    comstate->gl = NULL;

    if (comstate->server_cb && comstate->server_cb->climore) {
        comstate->cmdstate = CMD_STATE_MORE;
    }

} /* init_comstate */


/********************************************************************
 * FUNCTION set_normal_io
 * 
 * Set the output mode to normal IO if not done already
 *
 * INPUTS:
 *    comstate == completion state to init
 *********************************************************************/
static void
    set_normal_io (completion_state_t *comstate)
{
    if (!comstate->gl_normal_done) {
        gl_normal_io(comstate->gl);
        comstate->gl_normal_done = TRUE;
    }
}  /* set_normal_io */


/********************************************************************
 * FUNCTION show_help_lines
 * 
 * Show a help lines for the specified comstate
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    comstate == completion state to init
 *
 * OUTPUTS:
 *   help text printed with log_stdout
 *********************************************************************/
static void
    show_help_lines (session_cb_t *session_cb,
                     completion_state_t *comstate)
{
    if (comstate->help_action == HELP_ACTION_NONE) {
        return;
    }

    ncx_backptr_t *backptr = ncx_first_backptr(&comstate->help_backptrQ);
    if (backptr == NULL) {
        return;
    }

    set_normal_io(comstate);

    boolean done = FALSE;
    uint32 maxnamelen = 0;

    if (comstate->help_action == HELP_ACTION_SHOW_NODE) {
        done = TRUE;
    }

    /* first get the length of the longest command  */
    for (; backptr && !done; backptr = ncx_next_backptr(backptr)) {
        const xmlChar *nodename = NULL;

        switch (comstate->help_backptr_type) {
        case NCX_NT_OBJ:
            {
                obj_template_t *obj = (obj_template_t *)backptr->node;
                nodename = obj_get_name(obj);
                break;
            }
        case NCX_NT_VAL:
            {
                val_value_t *val = (val_value_t *)backptr->node;
                nodename = val->name;
                break;
            }
        default:
            SET_ERROR(ERR_INTERNAL_VAL);
            return;
        }
        uint32 len = xml_strlen(nodename);
        if (len > maxnamelen) {
            maxnamelen = len;
        }
    }

    uint32 maxline = MAX_HELP_LINE; // TBD configure terminal line length
    uint32 helpcol = 0;
    uint32 helplen = 0;

    /* allow for 2 spaces + <longest-name> + 3 spaces +
     * at least 10 chars of help text         */
    if ((maxnamelen + 15) < maxline) {
        helpcol = maxnamelen + 5;
        helplen = maxline - helpcol;
    } /* else just print names since no room for help line */

    /* print the help output according to the requested action */
    switch (comstate->help_action) {
    case HELP_ACTION_SHOW_LINES:
        /* go through all the lines and print 1 per line  */
        backptr = ncx_first_backptr(&comstate->help_backptrQ);
        for (; backptr && !done; backptr = ncx_next_backptr(backptr)) {
            const xmlChar *nodename = NULL;
            const xmlChar *helptext = NULL;
            xmlChar helptextbuff[MAX_HELP_LINE + 1];

            switch (comstate->help_backptr_type) {
            case NCX_NT_OBJ:
                {
                    obj_template_t *obj = (obj_template_t *)backptr->node;
                    nodename = obj_get_name(obj);
                    helptext = obj_get_alt_description(obj);
                    if (helptext == NULL) {
                        helptext = obj_get_description(obj);
                    }
                    break;
                }
            case NCX_NT_VAL:
                {
                    val_value_t *val = (val_value_t *)backptr->node;
                    nodename = val->name;

                    /* TBD: not sure if val will have different text */
                    helptext = obj_get_alt_description(val->obj);
                    if (helptext == NULL) {
                        helptext = obj_get_description(val->obj);
                    }
                    break;
                }
            default:
                SET_ERROR(ERR_INTERNAL_VAL);
                return;
            }

            /* generate the help text line */
            helptextbuff[0] = 0;
            if (helptext) {
                uint32 total = xml_strlen(helptext);
                xmlChar *p = helptextbuff;
                if (total > helplen) {
                    p += xml_strncpy(p, helptext, helplen - 3);
                    xml_strcpy(p, (const xmlChar *)"...");
                } else {
                    xml_strncpy(p, helptext, helplen);
                }

                /* remove any newlines and unprintable chars 
                 * from the help text              */
                p = helptextbuff;
                while (*p) {
                    if (*p == '\n') {
                        *p = ' ';
                    } else if (!isprint((int)*p)) {
                        *p = ' ';
                    }
                    p++;
                }
            }

            /* print the start of the line */
            log_stdout("\n  %s", nodename);

            if (helpcol && helplen) {
                /* print the padding spaces needed to get to helpcol */
                uint32 idx = xml_strlen(nodename) + 2;
                for (; idx < helpcol; idx++) {
                    log_stdout(" ");
                }

                /* print the cleaned help text */
                if (helptextbuff[0]) {
                    log_stdout("%s", (const char *)helptextbuff);
                }
            }
        }
        log_stdout("\n\n");
        break;
    case HELP_ACTION_SHOW_NODE:
        backptr = ncx_first_backptr(&comstate->help_backptrQ);
        if (backptr == NULL) {
            break;  // should not happen!!!
        }

        obj_template_t *obj = NULL;
        val_value_t *val = NULL;

        switch (comstate->help_backptr_type) {
        case NCX_NT_OBJ:
            obj = (obj_template_t *)backptr->node;
            break;
        case NCX_NT_VAL:
            val = (val_value_t *)backptr->node;
            obj = val->obj;
            break;
        default:
            SET_ERROR(ERR_INTERNAL_VAL);
        }

        /* TBD: separate system var for CLI help mode */
        help_mode_t helpmode = (session_cb) ? 
            session_cb->prompt_type : HELP_MODE_NORMAL;

        /* TBD: parm for help mode nest levels */
        uint32 nestlevel = 1;

        uint32 indent = (session_cb && session_cb->defindent >= 0) ? 
            (uint32)session_cb->defindent : NCX_DEF_INDENT;

        /* print help for the specified node */
        obj_dump_template(obj, helpmode, nestlevel, indent);

        if (val) {
            // TBD: print current value
        }

        log_stdout("\n\n");
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }


} /* show_help_lines */


/********************************************************************
 * FUNCTION cpl_add_completion
 *
 *  COPIED FROM /usr/local/include/libtecla.h
 *
 *  Used by libtecla to store one completion possibility
 *  for the current command line in progress
 * 
 *  cpl      WordCompletion *  The argument of the same name that was passed
 *                             to the calling CPL_MATCH_FN() callback function.
 *  line         const char *  The input line, as received by the callback
 *                             function.
 *  word_start          int    The index within line[] of the start of the
 *                             word that is being completed. If an empty
 *                             string is being completed, set this to be
 *                             the same as word_end.
 *  word_end            int    The index within line[] of the character which
 *                             follows the incomplete word, as received by the
 *                             callback function.
 *  suffix       const char *  The appropriately quoted string that could
 *                             be appended to the incomplete token to complete
 *                             it. A copy of this string will be allocated
 *                             internally.
 *  type_suffix  const char *  When listing multiple completions, gl_get_line()
 *                             appends this string to the completion to indicate
 *                             its type to the user. If not pertinent pass "".
 *                             Otherwise pass a literal or static string.
 *  cont_suffix  const char *  If this turns out to be the only completion,
 *                             gl_get_line() will append this string as
 *                             a continuation. For example, the builtin
 *                             file-completion callback registers a directory
 *                             separator here for directory matches, and a
 *                             space otherwise. If the match were a function
 *                             name you might want to append an open
 *                             parenthesis, etc.. If not relevant pass "".
 *                             Otherwise pass a literal or static string.
 * Output:
 *  return              int    0 - OK.
 *                             1 - Error.
 */


/********************************************************************
 * FUNCTION fill_one_parm_completion
 * 
 * fill the command struct for one RPC parameter value
 * check all the parameter values that match, if possible
 *
 * command state is CMD_STATE_FULL or CMD_STATE_GETVAL
 *
 * INPUTS:
 *    cpl == word completion struct to fill in
 *    comstate == completion state to use
 *    line == line passed to callback
 *    parmval == parm value string to check
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    parmlen == length of parameter value already entered
 *
 * OUTPUTS:
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_one_parm_completion (WordCompletion *cpl,
                              completion_state_t *comstate,
                              const char *line,
                              const char *parmval,
                              int word_start,
                              int word_end,
                              int parmlen)
{
    /* check no partial match so need to skip this one */
    if (parmlen > 0 &&
        strncmp(parmval, &line[word_start], parmlen)) {
        /* parameter value start is not the same so skip it */
        return NO_ERR;
    }

    if (cpl) {
        const char *endchar;
        if (comstate->cmdstate == CMD_STATE_FULL) {
            endchar = " ";
        } else {
            endchar = "";
        }

        int retval =
            cpl_add_completion(cpl, line, word_start, word_end,
                               (const char *)&parmval[parmlen],
                               (const char *)"", endchar);
        if (retval != 0) {
            return ERR_NCX_OPERATION_FAILED;
        }
    }

    return NO_ERR;

}  /* fill_one_parm_completion */


/********************************************************************
 * FUNCTION fill_idref_completion
 * 
 * callback to fill the tab completion for a YANG identity
 *
 * command state is CMD_STATE_FULL or CMD_STATE_GETVAL
 *
 * INPUTS:
 *    identity == identity to process
 *    cookie == parmpack_cb_t pointer
 *
 * OUTPUTS:
 *   cpl (inside cookie) filled in if any matching commands found
 *
 * RETURNS:
 *    TRUE if processing should continue
 *    FALSE if identity traversal should terminate
 *********************************************************************/
static boolean
    fill_idref_completion (ncx_identity_t *identity,
                           void *cookie)
{
    cb_parmpack_t *parms = (cb_parmpack_t *)cookie;

    status_t res = fill_one_parm_completion(parms->cpl,
                                            parms->comstate,
                                            parms->line,
                                            (const char *)identity->name,
                                            parms->word_start,
                                            parms->word_end,
                                            parms->parmlen);
    return (res == NO_ERR);

} /* fill_idref_completion */


/********************************************************************
 * FUNCTION fill_xpath_children_completion
 * 
 * fill the command struct for one XPath child node
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    parentObj == object template of parent to check
 *    cpl == word completion struct to fill in
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == command length
 *
 * OUTPUTS:
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_xpath_children_completion (session_cb_t *session_cb,
                                    obj_template_t *parentObj,
                                    WordCompletion *cpl,
                                    const char *line,
                                    int word_start,
                                    int word_end,
                                    int cmdlen)
{
    if (parentObj == NULL) {
        return NO_ERR;
    }

    int word_iter = word_start + 1;
    // line[word_start] == '/'
    word_start++;
    cmdlen--;

    while (word_iter <= word_end) {
        if (line[word_iter] == '/') {
            // The second '/' is found
            // find the top level obj and fill its child completion

            int child_name_len = word_iter - word_start;

            obj_template_t *childObj = 
                find_child_obj_str(session_cb, parentObj, 
                                   (const xmlChar *)&line[word_start],
                                   child_name_len);
            
            if (childObj == NULL) {
                return NO_ERR;
            }

            // put the children path with topObj into the recursive 
            // lookup function

            cmdlen = word_end - word_iter;

            return fill_xpath_children_completion(session_cb, childObj,
                                                  cpl, line, word_iter,
                                                  word_end, cmdlen);
        }
        word_iter++;
    }

    obj_template_t * childObj = obj_first_child_deep(parentObj);
    for (; childObj != NULL; childObj = obj_next_child_deep(childObj)) {
        const xmlChar *pathname = obj_get_name(childObj);
        /* check if there is a partial command name */
        if (cmdlen > 0 &&
            strncmp((const char *)pathname, &line[word_start], cmdlen)) {
            /* command start is not the same so skip it */
            continue;
        }

        if( !obj_is_data_db(childObj)) {
            /* object is either rpc or notification*/
            continue;
        }
        if (cpl) {
            int retval =
                cpl_add_completion(cpl, line, word_start, word_end,
                                   (const char *)&pathname[cmdlen], "", "");
            if (retval != 0) {
                return ERR_NCX_OPERATION_FAILED;
            }
        }
    }
    return NO_ERR;

}  /* fill_xpath_children_completion */


/********************************************************************
 * FUNCTION find_xpath_top_obj
 * 
 * Check all modules for top-level data nodes to save
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *
 * RETURNS:
 *   object template for the found name, or NULL if not found
 *********************************************************************/
static obj_template_t *
    find_xpath_top_obj (session_cb_t *session_cb,
                        const char *line,
                        int word_start,
                        int word_end)
{

    // line[word_end] == '/'
    int cmdlen = (word_end - 1) - word_start;

    obj_template_t *modObj =
        find_top_obj_str(session_cb, (const xmlChar *)&line[word_start],
                         cmdlen);

    return modObj;

} /* find_xpath_top_obj */


/********************************************************************
 * FUNCTION check_save_xpath_completion
 * 
 * Check a module for saving top-level data objects
 *
 * INPUTS:
 *    cpl == word completion struct to fill in
 *    mod == module to check
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == command length
 *
 * OUTPUTS:
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    check_save_xpath_completion (
        WordCompletion *cpl,
        ncx_module_t *mod,
        const char *line,
        int word_start,
        int word_end,
        int cmdlen)
{
    obj_template_t * modObj = ncx_get_first_object(mod);
    for (; modObj != NULL; 
         modObj = ncx_get_next_object(mod, modObj)) {

        if (!obj_is_data_db(modObj)) {
            /* object is either rpc or notification*/
            continue;
        }

        const xmlChar *pathname = obj_get_name(modObj);
        /* check if there is a partial command name */
        if (cmdlen > 0 && strncmp((const char *)pathname, 
                                  &line[word_start], cmdlen)) {
            continue;
        }

#ifdef DEBUG_TRACE
        log_debug("\nFilling from module %s, object %s", 
                  mod->name, pathname);
#endif

        if (cpl) {
            int retval =
                cpl_add_completion(cpl, line, word_start, word_end,
                                   (const char *)&pathname[cmdlen], "", "");
            if (retval != 0) {
                return ERR_NCX_OPERATION_FAILED;
            }
        }
    }
    return NO_ERR;

}  /* check_save_xpath_completion */


/********************************************************************
 * FUNCTION fill_xpath_root_completion
 * 
 * Check all modules for top-level data nodes to save
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    cpl == word completion struct to fill in
 *    comstate == completion state in progress
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == command length
 *
 * OUTPUTS:
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_xpath_root_completion (session_cb_t *session_cb,
                                WordCompletion *cpl,
                                const char *line,
                                int word_start,
                                int word_end,
                                int cmdlen)
{
    status_t res;

    if (use_session_cb(session_cb)) {
        modptr_t *modptr = (modptr_t *)
            dlq_firstEntry(&session_cb->modptrQ);
        for (; modptr != NULL; modptr = (modptr_t *)dlq_nextEntry(modptr)) {
            res = check_save_xpath_completion(cpl, modptr->mod, line,
                                              word_start, word_end, cmdlen);
            if (res != NO_ERR) {
                return res;
            }
        }

        /* check manager loaded commands next */
        for (modptr = (modptr_t *)dlq_firstEntry(get_mgrloadQ());
             modptr != NULL;
             modptr = (modptr_t *)dlq_nextEntry(modptr)) {
            res = check_save_xpath_completion(cpl, modptr->mod, line,
                                              word_start, word_end, cmdlen);
            if (res != NO_ERR) {
                return res;
            }
        }
    } else {
        ncx_module_t * mod = ncx_get_first_session_module();
        for (;mod!=NULL; mod = ncx_get_next_session_module(mod)) {
            res = check_save_xpath_completion(cpl, mod, line,
                                              word_start, word_end, cmdlen);
            if (res != NO_ERR) {
                return res;
            }
        }
    }
    return NO_ERR;

} /* fill_xpath_root_completion */


/********************************************************************
 *
 * FUNCTION fill_one_xpath_completion
 *
 * fill the command struct for one RPC operation
 * check all the xpath that match
 *
 * command state is CMD_STATE_FULL or CMD_STATE_GETVAL
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    cpl == word completion struct to fill in
 *    line == line passed to callback
 *    word_start == start position within line of the
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == length of parameter name already entered
 *              this may not be the same as
 *              word_end - word_start if the cursor was
 *              moved within a long line
 *
 * OUTPUTS:
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_one_xpath_completion (session_cb_t *session_cb,
                               WordCompletion *cpl,
                               const char *line,
                               int word_start,
                               int word_end,
                               int cmdlen)
{
    int word_iter = word_start + 1;

    // line[word_start] == '/'
    word_start++;
    cmdlen--;

    while(word_iter <= word_end) {
        if (line[word_iter] == '/') {
            // The second '/' is found
            obj_template_t *topObj =
                find_xpath_top_obj(session_cb, line, word_start, word_iter);

            cmdlen = word_end - word_iter;

            // put the children path with topObj into the recursive 
            // lookup function
            return fill_xpath_children_completion (session_cb, topObj, cpl,
                                                   line, word_iter,
                                                   word_end, cmdlen);
        }
        word_iter++;
    }

    // The second '/' is not found
    return fill_xpath_root_completion(session_cb, cpl, line, 
                                      word_start, word_end, cmdlen);

}  /* fill_one_xpath_completion */


/********************************************************************
 * FUNCTION fill_one_rpc_completion_parms
 * 
 * fill the command struct for one RPC operation
 * check all the parameters that match
 *
 * command state is CMD_STATE_FULL or CMD_STATE_GETVAL
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    comstate == current command completion state
 *    rpc == rpc operation to use
 *    cpl == word completion struct to fill in
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == length of parameter name already entered
 *              this may not be the same as 
 *              word_end - word_start if the cursor was
 *              moved within a long line
 *
 * OUTPUTS:
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_one_rpc_completion_parms (session_cb_t *session_cb,
                                   completion_state_t *comstate,
                                   obj_template_t *rpc,
                                   WordCompletion *cpl,
                                   const char *line,
                                   int word_start,
                                   int word_end,
                                   int cmdlen)
{
    if(line[word_start] == '/') {
        return fill_one_xpath_completion(session_cb, cpl, line, word_start,
                                         word_end, cmdlen);
    }

    obj_template_t *inputobj = obj_find_child(rpc, NULL, YANG_K_INPUT);
    if (inputobj == NULL || !obj_has_children(inputobj)) {
        /* no input parameters */
        return NO_ERR;
    }

    comstate->help_action = HELP_ACTION_SHOW_LINES;

    obj_template_t *obj;
    for (obj = obj_first_child_deep(inputobj);
         obj != NULL;
         obj = obj_next_child_deep(obj)) {

        const xmlChar *parmname = obj_get_name(obj);

        /* check if there is a partial command name */
        if (cmdlen > 0 &&
            strncmp((const char *)parmname, &line[word_start], cmdlen)) {
            /* command start is not the same so skip it */
            continue;
        }

        ncx_btype_t btyp = obj_get_basetype(obj);
        boolean hasval = (btyp == NCX_BT_EMPTY) ? FALSE : TRUE;

        if (cpl) {
            int retval =
                cpl_add_completion(cpl, line, word_start, word_end,
                                   (const char *)&parmname[cmdlen],
                                   "", (hasval) ? "=" : " ");
            if (retval != 0) {
                return ERR_NCX_OPERATION_FAILED;
            }
        } else if (comstate->help_mode) {
            status_t res = add_obj_backptr(comstate, obj);
            if (res != NO_ERR) {
                return res;
            }
        }
    }

    return NO_ERR;

}  /* fill_one_rpc_completion_parms */


/********************************************************************
 * FUNCTION fill_one_module_completion_commands
 * 
 * fill the command struct for one command string
 * for one module; check all the commands in the module
 *
 * command state is CMD_STATE_FULL
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    mod == module to use
 *    cpl == word completion struct to fill in
 *    comstate == completion state in progress
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == length of command already entered
 *              this may not be the same as 
 *              word_end - word_start if the cursor was
 *              moved within a long line
 *
 * OUTPUTS:
 *   cpl filled in if any matching parameters found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_one_module_completion_commands (session_cb_t *session_cb,
                                         ncx_module_t *mod,
                                         WordCompletion *cpl,
                                         completion_state_t *comstate,
                                         const char *line,
                                         int word_start,
                                         int word_end,
                                         int cmdlen)
{
    ncx_module_t *yangcli_mod = get_yangcli_mod();
    boolean toponly = FALSE;
    if (mod == yangcli_mod && !use_servercb(comstate->server_cb)) {
        toponly = TRUE;
    }

    if (session_cb->config_mode && !comstate->do_command) {
        if (!ncx_mod_has_data_nodes(mod, TRUE, FALSE)) {
            return NO_ERR;
        }
    } else {
        if (!ncx_mod_has_rpcs(mod)) {
            return NO_ERR;
        }
    }

#ifdef DEBUG_TRACE
    log_debug("\nFill one module '%s' toponly=%d mod-yangcli=%d",
              mod->name, (int)toponly,
              (int)(yangcli_mod == get_yangcli_mod()));
#endif

    /* for config mode need to match only 1 word if this
     * first word is complete, otherwise return all that match
     */
    boolean complete_word = ((word_start + cmdlen) < word_end);
    boolean servermode = (get_program_mode() == PROG_MODE_SERVER);

    /* check all the OBJ_TYP_RPC objects in the module
     * or check data nodes if this is config mode    */
    obj_template_t *obj;
    for (obj = ncx_get_first_object(mod);
         obj != NULL;
         obj = ncx_get_next_object(mod, obj)) {

        if (session_cb->config_mode && !comstate->do_command) {
            if (!obj_is_data_db(obj) || !obj_is_config(obj)) {
                continue;
            }
        } else {
            if (!obj_is_rpc(obj)) {
                continue;
            }
        }

        status_t res = NO_ERR;
        const xmlChar *cmdname = obj_get_name(obj);

        /* check for top commands -- commands that can be entered even
         * if no session is active; should be called local vs. remove  */
        if (!session_cb->config_mode && toponly && !is_top_command(cmdname)) {
            continue;
        }

        /* check if this is a config mode 'do' command and skip if
         * not one of the commands allowed in that mode         */
        if (session_cb->config_mode && comstate->do_command &&
            !is_do_command(cmdname)) {
            continue;
        }

        /* check if this is a server program mode command and skip if
         * not one of the commands allowed in that mode         */
        if (servermode && obj_is_rpc(obj) && !is_server_command(cmdname)) {
            continue;
        }

        /* check if there is a partial command name */
        if (cmdlen > 0 &&
            strncmp((const char *)cmdname, &line[word_start], cmdlen)) {
            /* command start is not the same so skip it */
            continue;
        }

        /* found a matching object */
        if (session_cb->config_mode && !comstate->do_command &&
            complete_word) {
            word_start += cmdlen;
            cmdlen = 0;
            res = fill_one_config_command(session_cb, cpl, comstate, line,
                                          word_start, word_end, cmdlen,
                                          obj, NULL);
            return res;
        }

        if (cpl) {
            int retval =
                cpl_add_completion(cpl, line, word_start, word_end,
                                   (const char *)&cmdname[cmdlen],
                                   (const char *)"", (const char *)" ");
            if (retval != 0) {
                return ERR_NCX_OPERATION_FAILED;
            }
        } else if (comstate->help_mode) {
            res = add_obj_backptr(comstate, obj);
            if (res != NO_ERR) {
                return res;
            }
        }
    }

    return NO_ERR;

}  /* fill_one_module_completion_commands */


/********************************************************************
 * FUNCTION fill_all_alias_names
 * 
 * fill the command struct for one alias string
 *
 * command state is CMD_STATE_FULL
 * tab completion is requested for all command names
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    cpl == word completion struct to fill in
 *    comstate == completeion state struct in progress
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == length of command already entered
 *              this may not be the same as 
 *              word_end - word_start if the cursor was
 *              moved within a long line
 * OUTPUTS:
 *   cpl filled in if any matching alias names found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_all_alias_names (session_cb_t *session_cb,
                          WordCompletion *cpl,
                          completion_state_t *comstate,
                          const char *line,
                          int word_start,
                          int word_end,
                          int cmdlen)
{
    status_t res = NO_ERR;

    alias_cb_t *alias = get_first_alias();

    /* TBD: session alias Q is not used yet! */
    (void)session_cb;
    (void)comstate;  // not used yet

    while (alias && res == NO_ERR) {
        res = fill_one_parm_completion(cpl, comstate, line,
                                       (const char *)get_alias_name(alias),
                                       word_start, word_end, cmdlen);

        alias = get_next_alias(alias);
    }

    return res;
        
}  /* fill_all_alias_names */


/********************************************************************
 * FUNCTION fill_one_completion_commands
 * 
 * fill the command struct for one command string
 *
 * command state is CMD_STATE_FULL
 * could be config mode instead of command mode
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    cpl == word completion struct to fill in
 *    comstate == completeion state struct in progress
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == length of command already entered
 *              this may not be the same as 
 *              word_end - word_start if the cursor was
 *              moved within a long line
 *
 * OUTPUTS:
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_one_completion_commands (session_cb_t *session_cb,
                                  WordCompletion *cpl,
                                  completion_state_t *comstate,
                                  const char *line,
                                  int word_start,
                                  int word_end,
                                  int cmdlen)
{
    status_t res = NO_ERR;

    /* default help action will be show lines */
    comstate->help_action = HELP_ACTION_SHOW_LINES;

    /* check if this is a sub-mode of the configure mode and
     * there is already a parent object selected;
     * if the 'do' command mode is active then a normal command
     * is expected, not a config data node name
     */
    if (session_cb->config_mode && session_cb->config_curobj &&
        !comstate->do_command) {
        res = fill_one_config_command(session_cb, cpl, comstate, line,
                                      word_start, word_end, cmdlen,
                                      NULL, NULL);
        return res;
    }

    /* figure out which modules to use */
    if (comstate->cmdmodule) {
        /* if foo:\t entered as the current token, then
         * the comstate->cmdmodule pointer will be set
         * to limit the definitions to that module 
         */
        res = fill_one_module_completion_commands
            (session_cb, comstate->cmdmodule, cpl, comstate, line, word_start,
             word_end, cmdlen);
    } else {
        if (use_session_cb(session_cb)) {
            /* list server commands first */
            modptr_t *modptr;
            for (modptr = (modptr_t *)
                     dlq_firstEntry(&session_cb->modptrQ);
                 modptr != NULL && res == NO_ERR;
                 modptr = (modptr_t *)dlq_nextEntry(modptr)) {

#ifdef DEBUG_TRACE
                log_debug("\nFilling from server_cb module %s", 
                          modptr->mod->name);
#endif

                res = fill_one_module_completion_commands
                    (session_cb, modptr->mod, cpl, comstate, line, word_start,
                     word_end, cmdlen);
                if (comstate->config_done) {
                    return res;
                }
            }

            /* list manager loaded commands next */
            for (modptr = (modptr_t *)
                     dlq_firstEntry(get_mgrloadQ());
                 modptr != NULL && res == NO_ERR;
                 modptr = (modptr_t *)dlq_nextEntry(modptr)) {

#ifdef DEBUG_TRACE
                log_debug("\nFilling from mgrloadQ module %s", 
                          modptr->mod->name);
#endif

                res = fill_one_module_completion_commands
                    (session_cb, modptr->mod, cpl, comstate, line, word_start,
                     word_end, cmdlen);
                if (comstate->config_done) {
                    return res;
                }
            }
        }

#ifdef DEBUG_TRACE
        /* use the yangcli top commands every time */
        log_debug("\nFilling from yangcli module");
#endif

        res = fill_one_module_completion_commands
            (session_cb, get_yangcli_mod(), cpl, comstate, line, word_start,
             word_end, cmdlen);
    }

    if (res == NO_ERR && session_cb->config_mode) {
        /* if we get here in config mode then top-level nodes
         * were added for tab completion; also need to add the
         * top-level config mode commands
         */
        if (comstate->no_command || comstate->do_command ||
            comstate->help_mode) {
            /* do not add any commands at the end since there was
             * starting keyword 'do' or 'no' or not tab mode   */
            ;
        } else {
            res = fill_one_parm_completion(cpl, comstate, line, 
                                           (const char *)NCX_EL_EXIT,
                                           word_start, word_end, cmdlen);
            if (res == NO_ERR) {
                res = fill_one_parm_completion(cpl, comstate, line, 
                                               (const char *)NCX_EL_NO,
                                               word_start, word_end, cmdlen);
            }

            if (res == NO_ERR) {
                res = fill_one_parm_completion(cpl, comstate, line, 
                                               (const char *)NCX_EL_DO,
                                               word_start, word_end, cmdlen);
            }

            if (res == NO_ERR && !dlq_empty(&session_cb->config_editQ)) {
                res = fill_one_parm_completion(cpl, comstate, line, 
                                               (const char *)NCX_EL_APPLY,
                                               word_start, word_end, cmdlen);
            }
        }
    } else if (res == NO_ERR) {
        /* add any aliases */
        res = fill_all_alias_names(session_cb, cpl, comstate, line,
                                   word_start, word_end, cmdlen);
    }
    return res;

}  /* fill_one_completion_commands */


/********************************************************************
 * FUNCTION fill_all_variable_names
 * 
 * fill the command struct for one command string
 *
 * command state is CMD_STATE_FULL
 * tab completion is requested for all matching variable names
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    cpl == word completion struct to fill in
 *    comstate == completeion state struct in progress
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == length of command already entered
 *              this may not be the same as 
 *              word_end - word_start if the cursor was
 *              moved within a long line
 *    islocal == TRUE if a $foo variable was specified
 *               FALSE if a $$foo variable was specified
 *    islhs == TRUE if left-hand-side variable assignment
 *             FALSE if right-hand-side variable reference
 *
 * OUTPUTS:
 *   cpl filled in if any matching variable names found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_all_variable_names (session_cb_t *session_cb,
                             WordCompletion *cpl,
                             completion_state_t *comstate,
                             const char *line,
                             int word_start,
                             int word_end,
                             int cmdlen,
                             boolean islocal,
                             boolean islhs)
{
    runstack_context_t *rcxt = comstate->server_cb->runstack_context;
    ncx_var_t *var = var_get_first(rcxt, islocal);
    status_t res = NO_ERR;

    /* TBD: session variable Q is not used yet! */
    (void)session_cb;

    while (var && res == NO_ERR) {
        if (!islhs || var_get_vartype(var) != VAR_TYP_SYSTEM) {
            res = fill_one_parm_completion(cpl, comstate, line,
                                           (const char *)var_get_name(var),
                                           word_start, word_end, cmdlen);
        }
        var = var_get_next(var);
    }

    if (res == NO_ERR && islocal && !islhs) {
        /* For LHS variable usage, get globals after locals */
        var = var_get_first(rcxt, FALSE);
        while (var && res == NO_ERR) {
            if (!islhs || var_get_vartype(var) != VAR_TYP_SYSTEM) {
                res = fill_one_parm_completion(cpl, comstate, line,
                                               (const char *)var_get_name(var),
                                               word_start, word_end, cmdlen);
            }
            var = var_get_next(var);
        }
    }

    return res;
        
}  /* fill_all_variable_names */



/********************************************************************
 * FUNCTION fill_from_target_template
 * 
 * fill the command struct for one command string
 * 
 * command state is CMD_STATE_FULL
 * tab completion is requested for all matching variable names
 * parameter is a target or source paramewter for a NETCONF operation
 *
 * INPUTS:
 *    cpl == word completion struct to fill in
 *    comstate == completeion state struct in progress
 *    obj == container object to use
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == length of command already entered
 *              this may not be the same as 
 *              word_end - word_start if the cursor was
 *              moved within a long line
 *
 * OUTPUTS:
 *   cpl filled in if any matching variable names found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_from_target_template (WordCompletion *cpl,
                               completion_state_t *comstate,
                               obj_template_t *obj,
                               const char *line,
                               int word_start,
                               int word_end,
                               int cmdlen)
{
    status_t res = NO_ERR;

    obj_template_t *leafobj = obj_first_child_deep(obj);
    for (; leafobj && res == NO_ERR;
         leafobj = obj_next_child_deep(leafobj)) {
        res = fill_one_parm_completion(cpl, comstate, line,
                                       (const char *)obj_get_name(leafobj),
                                       word_start, word_end, cmdlen);
    }

    return res;
        
}  /* fill_from_target_template */


/********************************************************************
 * FUNCTION is_parm_start
 * 
 * check if current backwards sequence is the start of
 * a parameter or not
 *
 * command state is CMD_STATE_FULL or CMD_STATE_GETVAL
 *
 * INPUTS:
 *    line == current line in progress
 *    word_start == lower bound of string to check
 *    word_cur == current internal parser position 
 *
 * RETURN:
 *   TRUE if start of parameter found
 *   FALSE if not found
 *
 *********************************************************************/
static boolean
    is_parm_start (const char *line,
                   int word_start,
                   int word_cur)
{
    if (word_start >= word_cur) {
        return FALSE;
    }

    const char *str = &line[word_cur];
    if (*str == '-') {
        if ((word_cur - 2) >= word_start) {
            /* check for previous char before '-' */
            if (line[word_cur - 2] == '-') {
                if ((word_cur - 3) >= word_start) {
                    /* check for previous char before '--' */
		  if (isspace((int)line[word_cur - 3])) {
                        /* this a real '--' command start */
                        return TRUE;
                    } else {
                        /* dash dash part of some value string */
                        return FALSE;
                    }
                } else {
                    /* start of line is '--' */
                    return TRUE;
                }
            } else if (isspace((int)line[word_cur - 2])) {
                /* this is a real '-' command start */
                return TRUE;
            } else {
                return FALSE;
            }
        } else {
            /* start of line is '-' */
            return TRUE;
        }
    } else {
        /* previous char is not '-' */
        return FALSE;
    }

} /* is_parm_start */


/********************************************************************
 * FUNCTION find_parm_start
 * 
 * go backwards and figure out the previous token
 * from word_end to word_start
 *
 * command state is CMD_STATE_FULL or CMD_STATE_GETVAL
 *
 * INPUTS:
 *    inputobj == RPC operation input node
 *                used for parameter context
 *    cpl == word completion struct to fill in
 *    comstate == completion state struct to use
 *    line == current line in progress
 *    word_start == lower bound of string to check
 *    word_end == current cursor pos in the 'line'
 *              may be in the middle if the user 
 *              entered editing keys; libtecla will 
 *              handle the text insertion if needed
 *   expectparm == address of return expect--parmaeter flag
 *   emptyexit == address of return empty exit flag
 *   parmobj == address of return parameter in progress
 *   tokenstart == address of return start of the current token
 *   
 *
 * OUTPUTS:
 *   *expectparm == TRUE if expecting a parameter
 *                  FALSE if expecting a value
 *   *emptyexit == TRUE if bailing due to no matches possible
 *                 FALSE if there is something to process
 *   *parmobj == found parameter that was entered
 *            == NULL if no parameter expected
 *            == NULL if expected parameter not found
 *               (error returned)
 *  *tokenstart == index within line that the current
 *                 token starts; will be equal to word_end
 *                 if not in the middle of an assignment
 *                 sequence
 *
 * RETURN:
 *   status
 *********************************************************************/
static status_t
    find_parm_start (obj_template_t *inputobj,
                     const char *line,
                     int word_start,
                     int word_end,
                     boolean *expectparm,
                     boolean *emptyexit,
                     obj_template_t **parmobj,
                     int *tokenstart)

{
    *expectparm = FALSE;
    *emptyexit = FALSE;
    *parmobj = NULL;
    *tokenstart = 0;

    const char *equals = NULL;
    const char *seqstart = NULL;
    obj_template_t *childobj = NULL;
    uint32 withequals = 0;
    uint32 matchcount = 0;
    boolean inbetween = FALSE;
    boolean gotdashes = FALSE;

    /* get the last char entered */
    const char *str = &line[word_end - 1];

    /* check starting in between 2 token sequences */
    if (isspace((int)*str)) {
        inbetween = TRUE;

        /* the tab was in between tokens, not inside a token
         * skip all the whitespace backwards
         */
        while (str >= &line[word_start] && isspace((int)*str)) {
            str--;
        }

        if (isspace((int)*str)) {
            /* only found spaces, so this is the line start */
            *expectparm = TRUE;
            *tokenstart = word_end;
            return NO_ERR;
        } /* else found the end of some text */
    } else if (is_parm_start(line, 
                             word_start, 
                             (int)(str - line))) {
        /* got valid -<tab> or --<tab> */
        *expectparm = TRUE;
        *tokenstart = word_end;
        return NO_ERR;
    } else if (*str =='"' || *str == '\'') {
        /* last char was the start or end of a string
         * no completions possible at this point
         */
        *emptyexit = TRUE;
        return NO_ERR;
    }

    /* str is pointing at the last char in the token sequence
     * that needs to be checked; e.g.:
     *    somecommand parm1=3 --parm2=fred<tab>
     *    somecommand parm1<tab>
     *
     * count the equals signs; hack to guess if
     * this is a complete assignment statement
     */
    while (str >= &line[word_start] && !isspace((int)*str)) {
        if (*str == '=') {
            if (equals == NULL) {
                equals = str;
            }
            withequals++;
        }
        str--;
    }

    /* figure out where the backwards search stopped */
    if (isspace((int)*str)) {
        /* found a space entered so see if
         * the string following it looks like
         * the start of a parameter or a value
         */
        seqstart = ++str;
    } else {
        /* str backed up all the way to word_start
         * start the forward analysis from this char
         */
        seqstart = str;
    }

    /* str is now pointing at the preceding token sequence
     * check if the parameter start sequence is next
     */
    if (*str == '-') {
        /* try to find any matching parameters
         * within the rpc/input section
         */
        seqstart++;
        gotdashes = TRUE;
        if (str+1 < &line[word_end]) {
            /* -<some-text ... <tab>  */
            str++;
            if (*str == '-') {
                if (str+1 < &line[word_end]) {
                    /* --<some-text ... <tab>  */
                    seqstart++;
                    str++;
                } else {
                    /* entire line is --<tab> 
                     * return parameter list
                     */
                    *expectparm = TRUE;
                    *tokenstart = word_end;
                    return NO_ERR;

                }
            } /*else  -<some-text ... <tab>  */
        } else {
            /* entire line is -<tab> 
             * return parameter list
             */
            *expectparm = TRUE;
            *tokenstart = word_end;
            return NO_ERR;
        }
    } /* else token does not start with any dashes */

    /* got a sequence start so figure out what to with it */
    if (inbetween) {
        if (withequals == 1) {
            /* this profile fits 
             *   '[-]-<parmname>=<parmval><spaces><tab>'
             * assume that a new parameter is expected
             * and cause all of them to be listed
             */
            *expectparm = TRUE;
            *tokenstart = word_end;
            return NO_ERR;
        } else {
            /* this profile fits 
             *    '[-]-<parmname><spaces><tab>'
             * this entire token needs to be matched to a
             * parameter in the RPC input section
             *
             * first find the end of the parameter name
             */
            str = seqstart;

            while (str < &line[word_end] && 
                   !isspace((int)*str) &&
                   *str != '=') {
                str++;
            }

            /* try to find this parameter name */
            childobj = obj_find_child_str(inputobj, NULL,
                                          (const xmlChar *)seqstart,
                                          (uint32)(str - seqstart));
            if (childobj == NULL) {
                matchcount = 0;

                /* try to match this parameter name */
                childobj = obj_match_child_str(inputobj, NULL,
                                               (const xmlChar *)seqstart,
                                               (uint32)(str - seqstart),
                                               &matchcount);

                if (childobj && matchcount > 1) {
                    /* ambiguous command error
                     * but just return no completions
                     */
                    *emptyexit = TRUE;
                    return NO_ERR;
                }
            }

            /* check find/match child result */
            if (childobj == NULL) {
                /* do not recognize this parameter,
                 * so just return no matches
                 */
                *emptyexit = TRUE;
                return NO_ERR;
            }

            /* else found one matching child object */
            *tokenstart = word_end;

            /* check if the parameter is an empty, which
             * means another parameter is expected, not a value
             */
            if (obj_get_basetype(childobj) == NCX_BT_EMPTY) {
                *expectparm = TRUE;
            } else {
                /* normal leaf parameter; needs a value */
                *parmobj = childobj;
            }
            return NO_ERR;
        }
    }

    /* not in between 2 tokens so tab is 'stuck' to
     * this preceding token; determine if a parameter
     * or a value is expected
     */
    if (withequals == 1) {
        /* this profile fits 
         *   '[--]<parmname>=<parmval><tab>'
         * assume that a parameter value is expected
         * and cause all of them to be listed that
         * match the partial value string
         */
        *tokenstart = (int)((equals+1) - line);

        /* try to find the parameter name */
        childobj = obj_find_child_str(inputobj, NULL,
                                      (const xmlChar *)seqstart,
                                      (uint32)(equals - seqstart));
        if (childobj == NULL) {
            matchcount = 0;
            
            /* try to match this parameter name */
            childobj = obj_match_child_str(inputobj, NULL,
                                           (const xmlChar *)seqstart,
                                           (uint32)(equals - seqstart),
                                           &matchcount);

            if (childobj && matchcount > 1) {
                /* ambiguous command error
                 * but just return no completions
                 */
                *emptyexit = TRUE;
                return NO_ERR;
            }
        }

        /* check find/match child result */
        if (childobj == NULL) {
            /* do not recognize this parameter,
             * so just return no matches
             */
            *emptyexit = TRUE;
        } else {
            /* match the found parameter with a
             * partial value string
             */
            *parmobj = childobj;
        }
        return NO_ERR;
    } else if (gotdashes) {
        /* this profile fits 
         *   '--<parmname><tab>'
         * assume that a parameter name is expected
         * and cause all of them to be listed that
         * match the partial name string
         */
        *tokenstart = (int)(seqstart - line);
        *expectparm = TRUE;
    } else {
        /* this profile fits 
         *   '[don't know]<parmwhat><tab>'
         * assume that a parameter name is expected
         * and cause all of them to be listed that
         * match the partial name string
         *
         * TBD: should check back another token
         * so see if a value or a name is expected
         */
        *tokenstart = (int)(seqstart - line);
        *expectparm = TRUE;  /****/
    }
       
    return NO_ERR;

} /* find_parm_start */


/********************************************************************
 * FUNCTION parse_backwards_parm
 * 
 * go through the command line backwards
 * from word_end to word_start, figuring
 *
 * command state is CMD_STATE_FULL
 *
 * This function only used when there are allowed
 * to be multiple parm=value pairs on the same line
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    cpl == word completion struct to fill in
 *    comstate == completion state struct to use
 *    line == current line in progress
 *    word_start == lower bound of string to check
 *    word_end == current cursor pos in the 'line'
 *              may be in the middle if the user 
 *              entered editing keys; libtecla will 
 *              handle the text insertion if needed
 *
 * OUTPUTS:
 *   parameter or value completions addeed if found
 *
 * RETURN:
 *   status
 *********************************************************************/
static status_t
    parse_backwards_parm (session_cb_t *session_cb,
                          WordCompletion *cpl,
                          completion_state_t *comstate,
                          const char *line,
                          int word_start,
                          int word_end)

{
    if (word_end == 0) {
        return SET_ERROR(ERR_INTERNAL_VAL);
    }

    status_t res = NO_ERR;
    int tokenstart = 0;
    boolean expectparm = FALSE;
    boolean emptyexit = FALSE;
    obj_template_t *rpc = comstate->cmdobj;
    obj_template_t *inputobj = comstate->cmdinput;
    obj_template_t *parmobj = NULL;

    res = find_parm_start(inputobj, line, word_start, word_end, &expectparm,
                          &emptyexit, &parmobj, &tokenstart);
    if (res != NO_ERR || emptyexit) {
        return res;
    }

    if (expectparm) {
        /* add all the parameters, even those that
         * might already be entered (oh well)
         * this is OK for leaf-lists but not leafs
         */

#ifdef YANGCLI_TAB_DEBUG
        log_debug2("\n*** fill one RPC %s parms ***\n", obj_get_name(rpc));
#endif

        res = fill_one_rpc_completion_parms(session_cb, comstate, rpc,
                                            cpl, line, tokenstart, word_end, 
                                            word_end - tokenstart);
    } else if (parmobj) {
        /* have a parameter in progress and the
         * token start is supposed to be the value
         * for this parameter
         */

#ifdef YANGCLI_TAB_DEBUG
        log_debug2("\n*** fill one parm in backwards ***\n");
#endif

        res = fill_parm_completion(session_cb, parmobj, cpl, comstate,
                                   NULL, line, tokenstart, word_end,
                                   word_end - tokenstart);
    } /* else nothing to do */

    return res;

} /* parse_backwards_parm */


/********************************************************************
 * FUNCTION fill_parm_values
 * 
 * go through the command line 
 * from word_start to word_end, figuring
 * out which value is entered
 *
 * command state is CMD_STATE_GETVAL
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    cpl == word completion struct to fill in
 *    comstate == completion state struct to use
 *    line == current line in progress
 *    word_start == lower bound of string to check
 *    word_end == current cursor pos in the 'line'
 *              may be in the middle if the user 
 *              entered editing keys; libtecla will 
 *              handle the text insertion if needed
 *
 * OUTPUTS:
 *   value completions addeed if found
 *
 * RETURN:
 *   status
 *********************************************************************/
static status_t
    fill_parm_values (session_cb_t *session_cb,
                      WordCompletion *cpl,
                      completion_state_t *comstate,
                      const char *line,
                      int word_start,
                      int word_end)
{
    /* skip starting whitespace */
    while (word_start < word_end && isspace((int)line[word_start])) {
        word_start++;
    }

    /*** NEED TO CHECK FOR STRING QUOTES ****/

#ifdef YANGCLI_TAB_DEBUG
    log_debug2("\n*** fill parm values ***\n");
#endif

    status_t res =
        fill_parm_completion(session_cb, comstate->cmdcurparm, cpl, 
                             comstate, NULL, line, word_start, word_end,
                             word_end - word_start);

    return res;

} /* fill_parm_values */


/********************************************************************
 * FUNCTION fill_completion_commands
 * 
 * go through the available commands to see what
 * matches should be returned
 *
 * command state is CMD_STATE_FULL
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    cpl == word completion struct to fill in
 *    comstate == completion state struct to use
 *    line == current line in progress
 *    word_end == current cursor pos in the 'line'
 *              may be in the middle if the user 
 *              entered editing keys; libtecla will 
 *              handle the text insertion if needed
 * OUTPUTS:
 *   comstate
 * RETURN:
 *   status
 *********************************************************************/
static status_t
    fill_completion_commands (session_cb_t *session_cb,
                              WordCompletion *cpl,
                              completion_state_t *comstate,
                              const char *line,
                              int word_end)

{
    status_t res = NO_ERR;
    const char *str = line;
    const char *cmdend = NULL;
    int word_start = 0;

    comstate->no_command = FALSE;
    comstate->do_command = FALSE;

    /* skip starting whitespace */
    while ((str < &line[word_end]) && isspace((int)*str)) {
        str++;
    }

    /* check if any real characters entered yet */
    if (word_end == 0 || str == &line[word_end]) {
        /* found only spaces so far or
         * nothing entered yet 
         */
        res = fill_one_completion_commands(session_cb, cpl, comstate, 
                                           line, word_end, word_end, 0);
        if (res != NO_ERR) {
            cpl_record_error(cpl, get_error_string(res));
        }
        return res;
    }

    /* else found a non-whitespace char in the buffer that
     * is before the current tab position
     * check what kind of command line is in progress
     */
    if (session_cb->config_mode) {
        /* skip the leading 'no' if present */
        if ((str + 2) < &line[word_end] && 
            !strncmp(str, "no", 2) && isspace((int)str[2])) {

            comstate->no_command = TRUE;
        } else if ((str + 2) < &line[word_end] && 
                   !strncmp(str, "do", 2) && isspace((int)str[2])) {

            comstate->do_command = TRUE;
        }

        if (comstate->no_command || comstate->do_command) {
            /* skip this starting keyword */
            str += 2;
    
            /* skip whitespace following 'no' or 'do' keyword */
            while ((str < &line[word_end]) && isspace((int)*str)) {
                str++;
            }

            /* check if any real characters left */
            if (str == &line[word_end]) {
                /* found only spaces; nothing else entered */
                res = fill_one_completion_commands(session_cb, cpl, comstate, 
                                                   line, word_end, word_end, 0);
                if (res != NO_ERR && cpl) {
                    cpl_record_error(cpl, get_error_string(res));
                }
                return res;
            } // else continue below and parse the first word past no
        }  // elso continue below and parse node name or 'exit' command
    }
                
    if (*str == '@' || *str == '$') {
        if (session_cb->config_mode) {
            /* assignment statements not allowed in config mode */
            return NO_ERR;
        }

        /* at-file-assign start sequence OR
         * variable assignment start sequence 
         * Skip over the keyword if it complete or fill in
         * variable names if '$' is seen            */
        comstate->assignstmt = TRUE;
        boolean equaldone = FALSE;
        boolean isvar = (*str == '$');
        boolean islocal = TRUE;
        if (isvar && (&str[1] < &line[word_end]) && (str[1] == '$')) {
            islocal = FALSE;
        }

        /* get the start of the variable name in case partial word entered */
        const char *startvar = NULL;
        if (isvar) {
            if (islocal) {
                startvar = str + 1;
            } else {
                startvar = str + 2;
            }
        }

        /* find end of the variable or file name */
        while ((str < &line[word_end]) && !isspace((int)*str)
               && (*str != '=')) {
            str++;
        }

        /* check where the string search stopped */
        if (isspace((int)*str) || *str == '=') {
            /* stopped past the first word
             * so need to skip further
             */
	  if (isspace((int)*str)) {
                /* find equals sign or word_end */
                while ((str < &line[word_end]) && 
                       isspace((int)*str)) {
                    str++;
                }
            }

            if ((*str == '=') && (str < &line[word_end])) {
                equaldone = TRUE;
                str++;  /* skip equals sign */

                /* skip more whitespace */
                while ((str < &line[word_end]) && 
                       isspace((int)*str)) {
                    str++;
                }
            }

            /* got past the '$foo =' part, now looking for a command */
            if (str < &line[word_end]) {
                /* go to next part and look for
                 * the end of this start command
                 */
                word_start = (int)(str - line);
            } else if (equaldone) {
                res = fill_one_completion_commands(session_cb, cpl, comstate,
                                                   line, word_end, word_end, 0);
                if (res != NO_ERR && cpl) {
                    cpl_record_error(cpl, get_error_string(res));
                }
                return res;
            } else {
                /* still inside the file or variable name */
                return res;
            }
        } else {
            /* word_end is still inside the first word, which is
             * an assignment of a local or global variable or
             * a file assignment statement.  If a variable then
             * try to match the tab completion to the variaqbles
             * present in the specified variable scope    */
            if (startvar) {
                /* make word_start skip the start of variable ($ or $$) */
                word_start = (startvar - line);

                /* get the length of the variable partial name entered */
                int varlen = (&line[word_end] - startvar);

                res = fill_all_variable_names(session_cb, cpl, comstate,
                                              line, word_start, word_end,
                                              varlen, islocal, TRUE);
            } // else TBD look for data file names
            return res;
        }
    } else {
        /* first word starts with a normal char */
        word_start = (int)(str - line);
    }

    /* the word_start var is set to the first char
     * that is supposed to be start command name
     * the 'str' var points to that char
     * check if it is an entire or partial command name
     */
    cmdend = str;
    while (cmdend < &line[word_end] && !isspace((int)*cmdend)) {
        cmdend++;
    }
    int cmdlen = (int)(cmdend - str);

    /* check if still inside the start command */
    if (cmdend == &line[word_end]) {
        /* get all the commands that start with the same
         * characters for length == 'cmdlen'
         */
        res = fill_one_completion_commands(session_cb, cpl, comstate, line,
                                           word_start, word_end, cmdlen);
        if (res != NO_ERR && cpl) {
            cpl_record_error(cpl, get_error_string(res));
        }
        return res;
    }

    /* not inside the start command so get the command that
     * was selected if it exists; at this point the command
     * should be known, so find it
     */
    const char *cmdname = &line[word_start];

#define CMDLEN_BUFFSIZE 128

    xmlChar *usebuff = NULL;
    xmlChar buffer[CMDLEN_BUFFSIZE];

    if (cmdlen >= CMDLEN_BUFFSIZE) {
        /* have to malloc the command name string */
        usebuff = xml_strndup((const xmlChar *)cmdname, (uint32)cmdlen);
        if (usebuff == NULL) {
            res = ERR_INTERNAL_MEM;
            if (cpl != NULL) {
                cpl_record_error(cpl, get_error_string(res));
            }
            return res;
        }
    } else {
        /* command name fits in the buffer */
        xml_strncpy(buffer, (const xmlChar *)cmdname, (uint32)cmdlen);
        usebuff = buffer;
    }

    if (session_cb->config_mode && !comstate->do_command) {
        res = fill_one_completion_commands(session_cb, cpl, comstate, line,
                                           word_start, word_end, cmdlen);
    } else { 
        uint32 retlen = 0;
        ncx_node_t dtyp = NCX_NT_OBJ;
        res = NO_ERR;
        comstate->cmdobj = (obj_template_t *)
            parse_def(comstate->server_cb, &dtyp, usebuff, &retlen, &res);
    }
    if (usebuff != buffer) {
        m__free(usebuff);
        usebuff = NULL;
    }

    if (session_cb->config_mode && !comstate->do_command) {
        /* done processing the config command line at this point */
        return res;
    }

    /* non-config mode */
    if (comstate->cmdobj == NULL) {
        if (cpl != NULL) {
            cpl_record_error(cpl, get_error_string(res));
        }
        return res;
    }

    /* have a command that is valid
     * first skip any whitespace after the
     * command name
     */
    str = cmdend;
    while (str < &line[word_end] && isspace((int)*str)) {
        str++;
    }
    /* set new word_start == word_end */
    word_start = (int)(str - line);

    /* check where E-O-WSP search stopped */
    if (str == &line[word_end]) {
        /* stopped before entering any parameters */
        res = fill_one_rpc_completion_parms(session_cb, comstate, 
                                            comstate->cmdobj, cpl, 
                                            line, word_start, word_end, 0);
        return res;
    }


    /* there is more text entered; check if this rpc
     * really has any input parameters or not
     */
    obj_template_t *inputobj =
        obj_find_child(comstate->cmdobj, NULL, YANG_K_INPUT);
    if (inputobj == NULL ||
        !obj_has_children(inputobj)) {
        /* no input parameters expected */
        return NO_ERR;
    } else {
        comstate->cmdinput = inputobj;
    }

    /* check if any strings entered on the line
     * stopping forward parse at line[word_start]
     */
    cmdend = str;
    while (cmdend < &line[word_end] &&
           *cmdend != '"' && 
           *cmdend != '\'') {
        cmdend++;
    }

    if (cmdend < &line[word_end]) {
        /* found the start of a quoted string
         * look for the end of the last quoted
         * string
         */

        boolean done = FALSE;
        while (!done) {
            /* match == start of quoted string */
            const char *match = cmdend++;
            while (cmdend < &line[word_end] &&
                   *cmdend != *match) {
                cmdend++;
            }
            if (cmdend == &line[word_end]) {
                /* entering a value inside a string
                 * so just return, instead of
                 * guessing the string value
                 */
                return res;
            } else {
                cmdend++;
                while (cmdend < &line[word_end] &&
                       *cmdend != '"' && 
                       *cmdend != '\'') {
                    cmdend++;
                }
                if (cmdend == &line[word_end]) {
                    /* did not find the start of
                     * another string so set the
                     * new word_start and call
                     * the parse_backwards_parm fn
                     */
                    word_start = (int)(cmdend - line);
                    done = TRUE;
                } 

                /* else stopped on another start of 
                 * quoted string so loop again
                 */
            }
        }
    }

    /* got some edited text past the last 
     * quoted string so figure out the
     * parm in progress and check for completions
     */
    res = parse_backwards_parm(session_cb, cpl, comstate, line, word_start,
                               word_end);

    return res;

} /* fill_completion_commands */


/********************************************************************
 * FUNCTION fill_completion_yesno
 * 
 * go through the available commands to see what
 * matches should be returned
 *
 * command state is CMD_STATE_YESNO
 *
 * INPUTS:
 *    cpl == word completion struct to fill in
 *    line == current line in progress
 *    word_end == current cursor pos in the 'line'
 *              may be in the middle if the user 
 *              entered editing keys; libtecla will 
 *              handle the text insertion if needed
 * OUTPUTS:
 *   comstate
 * RETURN:
 *   status
 *********************************************************************/
static status_t
    fill_completion_yesno (WordCompletion *cpl,
                           const char *line,
                           int word_end)
{
    const char *str = line;
    int word_start = 0;

    /* skip starting whitespace */
    while ((str < &line[word_end]) && isspace((int)*str)) {
        str++;
    }

    /* check if any real characters entered yet */
    if (word_end == 0 || str == &line[word_end]) {
        /* found only spaces so far or
         * nothing entered yet 
         */

        if (cpl) {
            int retval =
                cpl_add_completion(cpl, line, word_start, word_end,
                                   (const char *)NCX_EL_YES, 
                                   (const char *)"", (const char *)" ");
            if (retval != 0) {
                return ERR_NCX_OPERATION_FAILED;
            }


            retval = cpl_add_completion(cpl, line, word_start, word_end,
                                        (const char *)NCX_EL_NO,
                                        (const char *)"", (const char *)" ");
            if (retval != 0) {
                return ERR_NCX_OPERATION_FAILED;
            }
        }
    }

    /*** !!!! ELSE NOT GIVING ANY PARTIAL COMPLETIONS YET !!! ***/

    return NO_ERR;

}  /* fill_completion_yesno */


/********************************************************************
 * FUNCTION fill_type_completion
 * 
 * fill the command struct for one leaf or leaf-list type
 * check all the parameter values that match, if possible
 *
 * command state is CMD_STATE_FULL or CMD_STATE_GETVAL
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    typdef == typdef of leaf or leaf-list to complete
 *    cpl == word completion struct to fill in
 *    comstate == completion state record to use
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    parmlen == length of parameter name already entered
 *              this may not be the same as 
 *              word_end - word_start if the cursor was
 *              moved within a long line
 *
 * OUTPUTS:
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_type_completion (session_cb_t *session_cb,
                          typ_def_t *typdef,
                          WordCompletion *cpl,
                          completion_state_t *comstate,
                          const char *line,
                          int word_start,
                          int word_end,
                          int parmlen)
{
    status_t res = ERR_NCX_SKIPPED;

    (void)session_cb;

    /* fill in the parameter value based on its type */
    ncx_btype_t btyp = typ_get_basetype(typdef);

    switch (btyp) {
    case NCX_BT_ENUM:
    case NCX_BT_BITS:
        {
            res = NO_ERR;
            typ_def_t *basetypdef = typ_get_base_typdef(typdef);
            typ_enum_t *typenum = NULL;
            for (typenum = typ_first_enumdef(basetypdef);
                 typenum != NULL && res == NO_ERR;
                 typenum = typ_next_enumdef(typenum)) {

#ifdef YANGCLI_TAB_DEBUG
                log_debug2("\n*** found enubit  %s ***\n", typenum->name);
#endif

                res = fill_one_parm_completion(cpl, comstate, line,
                                               (const char *)typenum->name,
                                               word_start, word_end, parmlen);
            }
            return res;
        }
    case NCX_BT_BOOLEAN:
        res = fill_one_parm_completion(cpl, comstate, line,
                                       (const char *)NCX_EL_TRUE,
                                       word_start, word_end, parmlen);
        if (res == NO_ERR) {
            res = fill_one_parm_completion(cpl, comstate, line,
                                           (const char *)NCX_EL_FALSE,
                                           word_start, word_end, parmlen);
        }
        return res;
    case NCX_BT_INSTANCE_ID:
        break;
    case NCX_BT_IDREF:
        {
            typ_idref_t *idref = idref = typ_get_idref(typdef);

            if (idref == NULL || idref->base == NULL) {
                break;
            }

            cb_parmpack_t parms;
            parms.cpl = cpl;
            parms.comstate = comstate;
            parms.line = line;
            parms.word_start = word_start;
            parms.word_end = word_end;
            parms.parmlen = parmlen;

            ncx_find_all_identities(idref->base, fill_idref_completion,
                                    &parms);
            res = NO_ERR;
        }
        break;
    case NCX_BT_LEAFREF:
        // lookup in the session_cb_config_root
        break;
    case NCX_BT_UNION:
        {
            // lookup in the session_cb_config_root
            boolean fillany = FALSE;
            typ_unionnode_t *un = typ_first_unionnode(typdef);
            for (; un; un = (typ_unionnode_t *)dlq_nextEntry(un)) {
                typ_def_t *untypdef = typ_get_unionnode_ptr(un);
                if (untypdef) {
                    res = fill_type_completion(session_cb, untypdef,
                                               cpl, comstate, line,
                                               word_start, word_end,
                                               parmlen);
                    if (res == NO_ERR) {
                        fillany = TRUE;
                    } else if (res == ERR_NCX_SKIPPED) {
                        ;
                    } else {
                        return res;
                    }
                }
            }
            if (fillany && res == ERR_NCX_SKIPPED) {
                res = NO_ERR;
            }
            break;
        }
    default:
        break;
    }

    return res;

}  /* fill_type_completion */


/*.......................................................................
 *
 * FUNCTION yangcli_tab_callback (word_complete_cb)
 *
 *   libtecla tab-completion callback function
 *
 * Matches the CplMatchFn typedef
 *
 * From libtecla/libtecla.h:
 * 
 * Callback functions declared and prototyped using the following macro
 * are called upon to return an array of possible completion suffixes
 * for the token that precedes a specified location in the given
 * input line. It is up to this function to figure out where the token
 * starts, and to call cpl_add_completion() to register each possible
 * completion before returning.
 *
 * Input:
 *  cpl  WordCompletion *  An opaque pointer to the object that will
 *                         contain the matches. This should be filled
 *                         via zero or more calls to cpl_add_completion().
 *  data           void *  The anonymous 'data' argument that was
 *                         passed to cpl_complete_word() or
 *                         gl_customize_completion()).
 *  line     const char *  The current input line.
 *  word_end        int    The index of the character in line[] which
 *                         follows the end of the token that is being
 *                         completed.
 * Output
 *  return          int    0 - OK.
 *                         1 - Error.
 */
int
    yangcli_tab_callback (WordCompletion *cpl, 
                          void *data,
                          const char *line, 
                          int word_end)
{
#ifdef DEBUG
    if (!cpl || !data || !line) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return 1;
    }
#endif

    int retval = 0;
    status_t res = NO_ERR;

    /* check corner case that never has any completions
     * no matter what state; backslash char only allowed
     * for escaped-EOLN or char-sequence that we can't guess
     */
    if (word_end > 0 && line[word_end] == '\\') {
        return retval;
    }

    completion_state_t *comstate = (completion_state_t *)data;

    init_comstate(comstate);

    session_cb_t *session_cb = NULL;
    if (comstate->server_cb && comstate->server_cb->cur_session_cb) {
        session_cb = comstate->server_cb->cur_session_cb;
    } else {
        return retval;
    }

    switch (comstate->cmdstate) {
    case CMD_STATE_FULL:
        res = fill_completion_commands(session_cb, cpl, comstate, 
                                       line, word_end);
        if (res != NO_ERR) {
            retval = 1;
        }
        break;
    case CMD_STATE_GETVAL:
        res = fill_parm_values(session_cb, cpl, comstate, line, 0, word_end);
        if (res != NO_ERR) {
            retval = 1;
        }
        break;
    case CMD_STATE_YESNO:
        res = fill_completion_yesno(cpl, line, word_end);
        if (res != NO_ERR) {
            retval = 1;
        }
        break;
    case CMD_STATE_MORE:
        /*** NO SUPPORT FOR MORE MODE YET ***/
        break;
    case CMD_STATE_NONE:
        /* command state not initialized */
        SET_ERROR(ERR_INTERNAL_VAL);
        retval = 1;
        break;
    default:
        /* command state garbage value */
        SET_ERROR(ERR_INTERNAL_VAL);
        retval = 1;
    }

    return retval;

} /* yangcli_tab_callback */


/*.......................................................................
 *
 * FUNCTION yangcli_help_callback (word_complete_cb)
 *
 *   libtecla external callback for ctl-? keypress
 *
 * Matches the GlActionFn typedef
 *
 * From libtecla/libtecla.h:
 * 
 * Functions of the following form implement external
 * application-specific action functions, which can then be bound to
 * sequences of terminal keys.
 * 
 * Input:
 *  gl            GetLine *  The line editor resource object.
 *  data             void *  The anonymous 'data' argument that was
 *                           passed to gl_external_action() when the
 *                           callback function was registered.
 *  count             int    A positive repeat count specified by the user,
 *                           or 1 if not specified. Action functions should
 *                           ignore this if repeating the action multiple
 *                           times isn't appropriate. Alternatively they
 *                           can interpret it as a general numeric
 *                           argument.
 *  curpos         size_t    The position of the cursor within the input
 *                           line, expressed as the index of the
 *                           corresponding character within the line[]
 *                           array.
 *  line       const char *  A read-only copy of the current input line.
 * Output
 *  return  GlAfterAction    What should gl_get_line() do when the action
 *                           function returns?
 *                            GLA_ABORT    - Cause gl_get_line() to
 *                                           abort with an error (set
 *                                           errno if you need it).
 *                            GLA_RETURN   - Return the input line as
 *                                           though the user had typed
 *                                           the return key.
 *                            GLA_CONTINUE - Resume waiting for keyboard
 *                                           input.
 */
GlAfterAction
    yangcli_help_callback (GetLine *gl,
                           void *data,
                           int count,
                           size_t curpos,
                           const char *line)
{
#ifdef DEBUG
    if (!gl || !data || !line) {
        SET_ERROR(ERR_INTERNAL_PTR);
        return GLA_CONTINUE;
    }
#endif
    (void)count;

    GlAfterAction retval = GLA_CONTINUE;
    status_t res = NO_ERR;
    completion_state_t *comstate = (completion_state_t *)data;

    init_comstate(comstate);

    session_cb_t *session_cb = NULL;
    if (comstate->server_cb && comstate->server_cb->cur_session_cb) {
        session_cb = comstate->server_cb->cur_session_cb;
    } else {
        return retval;
    }

    init_help_completion_state(comstate);
    comstate->gl = gl;

    switch (comstate->cmdstate) {
    case CMD_STATE_FULL:
        res = fill_completion_commands(session_cb, NULL, comstate, 
                                       line, curpos);
        if (res == NO_ERR) {
            show_help_lines(session_cb, comstate);
        } else {
            retval = GLA_ABORT;
        } 
        break;
    case CMD_STATE_GETVAL:
        /* getval mode has its own help mode and the ? char
         * is used for other escape commands    */
        break;
    case CMD_STATE_YESNO:
        /*** NO SUPPORT FOR YES/NO MODE YET ***/
        break;
    case CMD_STATE_MORE:
        /*** NO SUPPORT FOR MORE MODE YET ***/
        break;
    case CMD_STATE_NONE:
        /* command state not initialized */
        SET_ERROR(ERR_INTERNAL_VAL);
        break;
    default:
        /* command state garbage value */
        SET_ERROR(ERR_INTERNAL_VAL);
    }

    clean_help_completion_state(comstate);

    return retval;

} /* yangcli_help_callback */


/********************************************************************
 * FUNCTION fill_parm_completion
 * 
 * fill the command struct for one RPC parameter value
 * check all the parameter values that match, if possible
 *
 * command state is CMD_STATE_FULL or CMD_STATE_GETVAL
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    parmobj == RPC input parameter template to use
 *    cpl == word completion struct to fill in
 *    comstate == completion state record to use
 *    curval == current value of parmobj (may be NULL)
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    parmlen == length of parameter name already entered
 *              this may not be the same as 
 *              word_end - word_start if the cursor was
 *              moved within a long line
 *
 * OUTPUTS:
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    fill_parm_completion (session_cb_t *session_cb,
                          obj_template_t *parmobj,
                          WordCompletion *cpl,
                          completion_state_t *comstate,
                          const char *curval,
                          const char *line,
                          int word_start,
                          int word_end,
                          int parmlen)
{
#ifdef YANGCLI_TAB_DEBUG
    log_debug2("\n*** fill parm %s ***\n", obj_get_name(parmobj));
#endif

    status_t res = NO_ERR;

    /* check variable name completion */
    if (parmlen && line[word_start] == '$') {
        /* a variable is selected as the value so get all
         * the matching parameter names    */
        parmlen--;
        word_start++;
        boolean islocal = TRUE;
        if (word_start < word_end && line[word_start] == '$') {
            islocal = FALSE;
            parmlen--;
            word_start++;
        }
        res = fill_all_variable_names(session_cb, cpl, comstate,
                                      line, word_start, word_end,
                                      parmlen, islocal, FALSE);
        return res;
    }

    /* check if this is a <target> or <source> NETCONF parameter */
    boolean istarget = obj_is_target_template(parmobj);
    if (istarget) {
        res = fill_from_target_template(cpl, comstate, parmobj, line,
                                        word_start, word_end, parmlen);
        return res;
    }

    /* expecting a simple node, so get the typedef */
    typ_def_t *typdef = obj_get_typdef(parmobj);
    if (typdef == NULL) {
        return NO_ERR;  // !!!
    }

    /* check if there is a current value to show, this is a simple type */
    if (curval && !comstate->help_mode) {
        res = fill_one_parm_completion(cpl, comstate, line, curval,
                                       word_start, word_end, parmlen);
        if (res != NO_ERR) {
            return res;
        }
    }

    if (comstate->help_mode) {
        comstate->help_action = HELP_ACTION_SHOW_NODE;
        res = add_obj_backptr(comstate, parmobj);
        return res;
    }

    res = fill_type_completion(session_cb, typdef, cpl, comstate,
                               line, word_start, word_end, parmlen);
    if (res == ERR_NCX_SKIPPED) {
        /* no current values to show;
         * just use the default if any
         */
        res = NO_ERR;
        const char *defaultstr = (const char *)obj_get_default(parmobj);
        if (defaultstr) {
            res = fill_one_parm_completion(cpl, comstate, line, defaultstr,
                                           word_start, word_end, parmlen);
        }
    }

    return res;

}  /* fill_parm_completion */


/* END yangcli_tab.c */
