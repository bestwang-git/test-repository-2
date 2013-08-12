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
/*  FILE: yangcli_tab_config.c

   NETCONF YANG-based CLI Tool

   interactive CLI tab completion support 


  config mode path
  ----------------

    fill_completion_commands -> 
      fill_one_completion_commands  ->
        if config_cur_obj  -> 
        else >  fill_one_module_completion_commands
                : adds all top-level matching objects


*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
20-dec-12    abb      begun; split out from yangcli_tab.c
 
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
#include "rpc.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "var.h"
#include "xml_util.h"
#include "yangcli.h"
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

/* if log-level >= debug */
//#define CONFIG_DEBUG_TRACE 1


/********************************************************************
*                                                                   *
*                          T Y P E S                                *
*                                                                   *
*********************************************************************/


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
 * FUNCTION find_top_data_node
 * 
 * Find a 0 or 1 instance data node (all but list or leaf-list)
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    obj == object to find
 *
 * RETURNS:
 *    pointer to found node or NULL if not found
 *********************************************************************/
static val_value_t *
    find_top_data_node (session_cb_t *session_cb,
                        obj_template_t *obj)
{
    if (session_cb->config_tree == NULL) {
        return NULL;
    }

    val_value_t *topval =
        val_find_child(session_cb->config_tree,
                       obj_get_mod_name(obj), obj_get_name(obj));

    return topval;

} /* find_top_data_node */


/********************************************************************
 * FUNCTION skip_whitespace
 * 
 * skip the whitespace chars
 *
 * INPUTS:
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *
 * RETURNS:
 *   number of chars ckipped
 *********************************************************************/
static int
    skip_whitespace (const char *line,
                     int word_start,
                     int word_end)
{
    int cnt = 0;
    const char *teststr = &line[word_start];
    while (teststr < &line[word_end] && *teststr &&
           isspace((int)*teststr)) {
        teststr++;
        cnt++;
    }
    return cnt;
}  /* skip_whitespace */


/********************************************************************
 * FUNCTION fill_one_parent_node
 * 
 * fill the command struct for the specified object
 *
 * Either filling XPath children (not in config mode)
 * or object children in config mode
 *
 * INPUTS:
 *    comstate == completion state to use
 *    parentobj == parent object to check
 *    parentval == back-ptr into session_cb->config_tree to
 *      the node corresponding to parent of obj (may be NULL)
 *    config_mode == TRUE if config mode and only config=true
 *                == FALSE if config true or false OK
 *    cpl == word completion struct to fill in
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
    fill_one_parent_node (completion_state_t *comstate,
                          obj_template_t *parentobj,
                          val_value_t *parentval,
                          boolean config_mode,
                          WordCompletion *cpl,
                          const char *line,
                          int word_start,
                          int word_end,
                          int cmdlen)
{
    if (config_mode && !obj_is_config(parentobj)) {
        return NO_ERR;
    }

    // not using value node right now
    // TBD: check config_tree to evaluate when-stmts
    // and if-feature stmts to remove unimplemented objects
    (void)parentval;

#ifdef CONFIG_DEBUG_TRACE
    if (LOGDEBUG) {
        log_debug("\nFill one parent '%s:%s' node",
                  obj_get_mod_name(parentobj), obj_get_name(parentobj));
    }
#endif

    boolean islist = obj_is_list(parentobj);
    obj_template_t * childobj = obj_first_child_deep(parentobj);
    for (; childobj != NULL; childobj = obj_next_child_deep(childobj)) {
        const xmlChar *pathname = obj_get_name(childobj);
        /* check if there is a partial command name */
        if (cmdlen > 0 &&
            strncmp((const char *)pathname, &line[word_start], cmdlen)) {
            /* command start is not the same so skip it */
            continue;
        }

        if (!obj_is_data_db(childobj)) {
            /* object is either rpc or notification*/
            continue;
        }
        if (config_mode && !obj_is_config(childobj)) {
            continue;
        }
        if (config_mode && islist && obj_is_key(childobj)) {
            continue;
        }

        if (cpl) {
            int retval =
                cpl_add_completion(cpl, line, word_start, word_end,
                                   (const char *)&pathname[cmdlen], "", 
                                   (config_mode) ? " " : "");
            if (retval != 0) {
                return ERR_NCX_OPERATION_FAILED;
            }
        } else if (comstate->help_mode) {
            status_t res = add_obj_backptr(comstate, childobj);
            if (res != NO_ERR) {
                return res;
            }
        }
    }

    return NO_ERR;

}  /* fill_one_parent_node */


/********************************************************************
 * FUNCTION find_config_value_end
 *
 * find the end of the current value
 *
 * INPUTS:
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    instr == address of return in string flag
 *    complete == address of return value complete flag
 *
 * OUTPUTS:
 *   *instr == TRUE if stopped inside a string
 *   *complete == TRUE if a complete value string was found
 *             == FALSE if ended on a partial string that might
 *                need tab completion
 *
 * RETURNS:
 *   number of chars used in line
 *********************************************************************/
static int
    find_config_value_end (const char *line,
                           int word_start,
                           int word_end,
                           boolean *instr,
                           boolean *complete)
{
    *instr = FALSE;
    *complete = TRUE;

    int cnt = 0;

    const char *str = &line[word_start];

    if (*str == '"' || *str == '\'') {
        cnt = 1;

        // find the end of this quoted string
        const char match = (int)*str++;
        while (str < &line[word_end] && *str && *str != match) {
            str++;
            cnt++;
        }

        if (str == &line[word_end]) {
            // stopped inside a partial string
            *instr = TRUE;
            *complete = FALSE;
            return cnt;
        }

        // got complete quoted string; account for closing quote
        return ++cnt;
    }

    // else got an unquoted string
    while (str < &line[word_end] && *str && !isspace((int)*str)) {
        str++;
        cnt++;
    }

    if (str == &line[word_end]) {
        *complete = FALSE;
    }

    return cnt;

}  /* find_config_value_end */


/********************************************************************
 * FUNCTION add_list_key_values
 *
 * fill the list keys for a config mode list command
 *
 * command state is CMD_STATE_FULL in config mode
 * expecting leading whitespace to be skipped already
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    listobj == pointer to list object to get keys for
 *    parenttval == back-ptr into session_cb->config_tree to
 *      the node corresponding to parent of listobj (may be NULL)
 *    cpl == word completion struct to fill in
 *    comstate == completion state record to use
 *    keylist == list of key values already parsed
 *    line == line being processed
 *    word_start == start index of key value
 *    word_end == end of line
 *    keylen == length of key string to use (may be 0 to indicate
 *           all key values are requested)
 * OUTPUTS:
 *   cpl filled in if any matching key values found
 *   in the shadow config tree
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    add_list_key_values (session_cb_t *session_cb,
                         obj_template_t *listobj,
                         val_value_t *parentval,
                         WordCompletion *cpl,
                         completion_state_t *comstate,
                         ncx_list_t *keylist,
                         const char *line,
                         int word_start,
                         int word_end,
                         int keylen)
{
    status_t res = NO_ERR;

    /* get the first list key */
    obj_key_t *key = obj_first_key(listobj);
    if (key == NULL) {
        return NO_ERR;
    }

    /* get the target list key */
    obj_key_t *targkey = key;
    uint32 skipcount = ncx_lmem_count(keylist);
    uint32 i = 0;
    for (; i < skipcount; i++) {
        targkey = obj_next_key(targkey);
        if (targkey == NULL) {
            return NO_ERR;  // should not happen
        }
    }

    /* setup the parent object to look for list entries */
    if (parentval == NULL && obj_is_top(listobj)) {
        parentval = session_cb->config_tree;
    }

    if (parentval == NULL) {
        /* no shadow config tree to check */
        res = fill_parm_completion(session_cb, targkey->keyobj, cpl,
                                   comstate, NULL, line, word_start,
                                   word_end, keylen);
        return res;
    }

    /* find the first instance of this list; child of parentval */
    val_value_t *listval =
        val_find_child(parentval, obj_get_mod_name(listobj),
                       obj_get_name(listobj));

    /* add all the matches for existing list instance values */
    while (listval && res == NO_ERR) {

        val_value_t *keyval = 
            val_find_child(listval, obj_get_mod_name(key->keyobj),
                           obj_get_name(key->keyobj));
        if (keyval == NULL) {
            return NO_ERR;  // should not happen!
        }

        /* check the list of key values already provided
         * to filter and determine which key leaf the
         * tab completion is for     */
        ncx_lmem_t *lmem = ncx_first_lmem(keylist);
        boolean skip = FALSE;
        while (lmem && !skip) {
            const xmlChar *lmemstr = ncx_get_lmem_strval(lmem);
            int32 ret = val_compare_to_string(keyval, lmemstr, &res);
            if (res != NO_ERR || ret) {
                /* skip this listval or end due to error */
                skip = TRUE;
                continue;
            }

            /* setup next key compare */
            key = obj_next_key(key);
            if (key == NULL) {
                return NO_ERR;
            }
            lmem = ncx_next_lmem(lmem);
            if (lmem) {
                keyval = val_find_child(listval, obj_get_mod_name(key->keyobj),
                                        obj_get_name(key->keyobj));
                if (keyval == NULL) {
                    return NO_ERR;  // should not happen!
                }
            }
        }

        if (!skip) {
            xmlChar *keyvalstr = val_make_sprintf_string(keyval);
            if (keyvalstr == NULL) {
                return ERR_INTERNAL_MEM;
            }

            int retval = 0;
            if (keylen) {
                /* compare a partial string to this key value */
                int32 ret =
                    xml_strncmp(keyvalstr,
                                (const xmlChar *)&line[word_start],
                                keylen);
                if (ret == 0) {
                    if (cpl) {
                        retval = 
                            cpl_add_completion(cpl, line, word_start, word_end,
                                               (const char *)&keyvalstr[keylen],
                                               (const char *)"",
                                               (const char *)" ");
                        if (retval != 0) {
                            res = ERR_NCX_OPERATION_FAILED;
                        }
                    }
                }
            } else {
                /* add this entire value string */
                if (cpl) {
                    retval = 
                        cpl_add_completion(cpl, line, word_start, word_end,
                                           (const char *)keyvalstr,
                                           (const char *)"",
                                           (const char *)" ");
                    if (retval != 0) {
                        res = ERR_NCX_OPERATION_FAILED;
                    }
                }
            }
            m__free(keyvalstr);
        }

        /* setup next list entry, if any */
        listval = val_get_next_child(listval);
        if (listval &&
            (val_get_nsid(listval) != obj_get_nsid(listobj) ||
             xml_strcmp(listval->name, obj_get_name(listobj)))) {
            listval = NULL;  // went on to different child object type
        }
    }

    /* add the normal parm completions */
    if (res == NO_ERR) {
        res = fill_parm_completion(session_cb, targkey->keyobj, cpl,
                                   comstate, NULL, line, word_start,
                                   word_end, keylen);
    }

    return res; 

}  /* add_list_key_values */


/********************************************************************
 * FUNCTION find_list_node
 *
 * find the list entry for the specified keys
 * Expecting a full set of list keys to compare
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    listobj == pointer to list object to get keys for
 *    parenttval == back-ptr into session_cb->config_tree to
 *      the node corresponding to parent of listobj (may be NULL)
 *    keylist == list of key values already parsed
 *
 * RETURNS:
 *   pointer to found list entry or NULL if not found
 *********************************************************************/
static val_value_t *
    find_list_node (session_cb_t *session_cb,
                    obj_template_t *listobj,
                    val_value_t *parentval,
                    ncx_list_t *keylist)
{
    /* setup the parent object to look for list entries */
    if (parentval == NULL && obj_is_top(listobj)) {
        parentval = session_cb->config_tree;
    }
    if (parentval == NULL) {
        return NULL;
    }

    status_t res = NO_ERR;

    /* find the first instance of this list; child of parentval */
    val_value_t *listval =
        val_find_child(parentval, obj_get_mod_name(listobj),
                       obj_get_name(listobj));

    /* add all the matches for existing list instance values */
    while (listval) {
        /* get the first list key */
        obj_key_t *key = obj_first_key(listobj);
        if (key == NULL) {
            return NULL;
        }

        /* check the list of key values already provided
         * to find the specified list entry   */
        boolean skip = FALSE;
        ncx_lmem_t *lmem = ncx_first_lmem(keylist);
        for (; lmem && !skip; lmem = ncx_next_lmem(lmem)) {
            const xmlChar *lmemstr = ncx_get_lmem_strval(lmem);

            val_value_t *keyval = 
                val_find_child(listval, obj_get_mod_name(key->keyobj),
                               obj_get_name(key->keyobj));
            if (keyval == NULL) {
                skip = TRUE;
                continue;  // should not happen!
            }

            int32 ret = val_compare_to_string(keyval, lmemstr, &res);
            if (res != NO_ERR || ret) {
                /* skip this listval or end due to error */
                skip = TRUE;
                continue;
            }

            /* setup next key compare */
            key = obj_next_key(key);
            if (key == NULL) {
                skip = TRUE;  // should not happen!
            }
        }

        if (!skip) {
            /* found the matching entry */
            return listval;
        }

        /* setup next list entry, if any */
        listval = val_get_next_child(listval);
        if (listval &&
            (val_get_nsid(listval) != obj_get_nsid(listobj) ||
             xml_strcmp(listval->name, obj_get_name(listobj)))) {
            listval = NULL;  // went on to different child object type
        }
    }

    return NULL;

}  /* find_list_node */


/********************************************************************
 * FUNCTION fill_config_list_keys
 *
 * fill the list keys for a config mode list command
 *
 * command state is CMD_STATE_FULL in config mode
 * expecting leading whitespace to be skipped already
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    comstate == completion state in progress
 *    listobj == pointer to list object to get keys for
 *    parenttval == back-ptr into session_cb->config_tree to
 *      the node corresponding to parent of listobj (may be NULL)
 *    cpl == word completion struct to fill in
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    word_count == address of return word count
 *    listval == address of return list instance
 *
 * OUTPUTS:
 *   *word_count set to number of chars used in line
 *   cpl filled in if any matching commands found
 *   *listval points to the specified child in the config_tree
 *   if a complete set of list keys were entered
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_config_list_keys (session_cb_t *session_cb,
                           completion_state_t *comstate,
                           obj_template_t *listobj,
                           val_value_t *parentval,
                           WordCompletion *cpl,
                           const char *line,
                           int word_start,
                           int word_end,
                           int *word_count,
                           val_value_t **listval)
{
    *word_count = 0;
    *listval = NULL;

    obj_key_t *key = obj_first_key(listobj);
    if (key == NULL) {
        return NO_ERR;
    }

    ncx_list_t keylist;
    ncx_init_list(&keylist, NCX_BT_STRING);

    comstate->curkey = key;
    comstate->keys_done = FALSE;

    while (key) {
        if (word_start == word_end) {
            /* reached point where tab was pressed for an entire key */
            // find all list key values for this key
            add_list_key_values(session_cb, listobj, parentval, cpl,
                                comstate, &keylist, line, word_start,
                                word_end, 0);
            ncx_clean_list(&keylist);
            return NO_ERR;
        }
        boolean instr = FALSE;
        boolean complete = FALSE;
        int cnt = find_config_value_end(line, word_start, word_end,
                                        &instr, &complete);
        *word_count += cnt;

        if (instr) {
            /* nothing to fill inside a quoted string */
            ncx_clean_list(&keylist);
            return NO_ERR;
        }

        if (complete) {
            status_t res =
                ncx_add_strlist((const xmlChar *)&line[word_start],
                                cnt, &keylist);
            if (res != NO_ERR) {
                ncx_clean_list(&keylist);
                return res;
            }

            /* set up next word and next key */
            word_start += cnt;
            key = obj_next_key(key);
            comstate->curkey = key;
            if (key == NULL) {
                comstate->keys_done = TRUE;
            } else {
                cnt = skip_whitespace(line, word_start, word_end);
                *word_count += cnt;
                word_start += cnt;
            }
        } else {
            /* got a partial string; need to list all the value
             * completions for this key 
             */
            // find all list key values that match this partial string
            add_list_key_values(session_cb, listobj, parentval, cpl,
                                comstate, &keylist, line, word_start,
                                word_end, cnt);
            ncx_clean_list(&keylist);
            return NO_ERR;
        }
    }

    /* got all key values specified if we got here */
    *listval = find_list_node(session_cb, listobj, parentval, &keylist);
    ncx_clean_list(&keylist);
    return NO_ERR;

} /* fill_config_list_keys */


/********************************************************************
 * FUNCTION fill_config_value
 *
 * fill a value for a leafy config data node
 * expexting that whitespace before the value has been
 * skipped so word_start is the first char of the value
 * or could be == word_end to indicate tab was pressed here
 *
 * command state is CMD_STATE_FULL in config mode
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    obj == pointer to leaf or leaf-list object to get value for
 *    val == back-ptr into session_cb->config_tree to
 *      the node corresponding to obj (may be NULL)
 *    cpl == word completion struct to fill in
 *    comstate == completion state to use
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    word_count == address of return word count
 *    complete == address of return complete word flag
 * OUTPUTS:
 *   *word_count set to number of chars used in line
 *   *complete == TRUE if complete value found; FALSE if tab
 *                completion was inside value
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_config_value (session_cb_t *session_cb,
                       obj_template_t *obj,
                       val_value_t *val,
                       WordCompletion *cpl,
                       completion_state_t *comstate,
                       const char *line,
                       int word_start,
                       int word_end,
                       int *word_count,
                       boolean *complete)
{
    *word_count = 0;
    *complete = FALSE;

    // no ... leaf is a complete statement, value not allowed
    if (obj_is_leaf(obj) && comstate->no_command) {
        return NO_ERR;
    }

    status_t res = NO_ERR;
    char *buff = NULL;

    if (word_start == word_end) {
        if (val && obj_is_leafy(obj)) {
            buff = (char *)val_make_sprintf_string(val);
        }

        /* reached point where tab was pressed for an entire value
         * find all values for this leaf or leaf-list             */
        res = fill_parm_completion(session_cb, obj, cpl, comstate,
                                   buff, line, word_start, word_end, 0);

        m__free(buff);
        return res;
    }
    
    boolean instr = FALSE;
    int cnt = find_config_value_end(line, word_start, word_end,
                                    &instr, complete);
    *word_count += cnt;

    if (instr || *complete) {
        /* nothing to fill inside a quoted string
         * complete means this value was not the one where tab pressed
         */
    } else {
        if (val && obj_is_leafy(obj)) {
            buff = (char *)val_make_sprintf_string(val);
        }

        /* got a partial string; need to list all the value
         * completions for this leaf or leaf-list
         * find all values that match this partial string      */
        res = fill_parm_completion(session_cb, obj, cpl, comstate,
                                   buff, line, word_start, word_end, cnt);
    }

    return res;

} /* fill_config_value */


/********************************************************************
 * FUNCTION fill_one_config_node
 * 
 * fill the command struct for the specified object
 * Called for config mode to decide how the empty or
 * partial string should be interpreted for value matching
 *
 * INPUTS:
 *    session_cb == session control block to use
 *    comstate == completion state in progress
 *    obj == object to check
 *    objdone == TRUE if this is the current config_curobj
 *                 so any keys have already been entereed
 *            == FALSE if starting from top-level and any keys
 *                after the 'opbj' node should be present
 *    val == back-ptr into session_cb->config_tree to
 *      the node corresponding to obj or parent of obj (may be NULL)
 *    cpl == word completion struct to fill in
 *    line == line passed to callback
 *    word_start == start position within line of the 
 *                  word being completed
 *    word_end == word_end passed to callback
 *    cmdlen == length of command already entered
 *              this may not be the same as 
 *              word_end - word_start if the cursor was
 *              moved within a long line
 * OUTPUTS:
 *   cpl filled in if any matching parameters found
 *
 * RETURNS:
 *   status
 *********************************************************************/
static status_t
    fill_one_config_node (session_cb_t *session_cb,
                          completion_state_t *comstate,
                          obj_template_t *obj,
                          boolean objdone,
                          val_value_t *val,
                          WordCompletion *cpl,
                          const char *line,
                          int word_start,
                          int word_end,
                          int cmdlen)
{
    status_t res = NO_ERR;
    int word_count = 0;
    val_value_t *newlist = NULL;

    if (!objdone && !comstate->keys_done && 
        obj_is_list(obj) && obj_first_key(obj)) {
        /* treat this line input as a list key [partial] value */
        res = fill_config_list_keys(session_cb, comstate, obj, val,
                                    cpl, line, word_start, word_end,
                                    &word_count, &newlist);
    } else if (obj_is_leafy(obj)) {
        /* treat this line input as a leaf or leaf-list [partial] value */
        boolean complete = FALSE;
        res = fill_config_value(session_cb, obj, val, cpl, comstate,
                                line, word_start, word_end, &word_count,
                                &complete);
    } else {
        /* treat this line input as a child node [partial] name */
        res = fill_one_parent_node(comstate, obj, val, TRUE, cpl, line,
                                   word_start, word_end, cmdlen);
    }

    comstate->keys_done = TRUE;
    return res;

}  /* fill_one_config_node */


/********************************************************************
 * FUNCTION fill_one_config_command
 *
 * fill the command struct for one config mode command string
 * This function is called in config mode when it is determined
 * that a complete top-level node name is present so there is expected
 * to be one match (or first match) found
 *
 * no con1 leaf1
 * con1 leaf1 leaf1val
 * con1
 *  leaf1 leaf1val
 * list1 key1 key2
 *  leaf2 leaf2val
 * apply
 * exit
 *
 * command state is CMD_STATE_FULL in config mode
 * Data node names are the command tokens, not RPC names and parms
 *
 * The config mode uses the session_cb->config_curobj pointer
 * as the starting context if it is set; If not then all modules
 * are checked for top-level config data nodes.
 *
 * If startobj is NULL then session_cb->config_curobj is expected
 * to be set.
 *
 * List key values are expected to follow a list node name.
 * List keys are complicated because this code needs to figure
 * out which keys are present and find the subset of entries
 * in the session_cb->config_tree that match the key values
 * or partial values that are entered.
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
 *   startobj == pointer to found object to check (NULL if not used)
 *            If set then cmdlen is ignored!!!
 *   startval == pointer to found value node in the config_tree
 *            If set then cmdlen is ignored!!!
 * OUTPUTS:
 *   cpl filled in if any matching commands found
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    fill_one_config_command (session_cb_t *session_cb,
                             WordCompletion *cpl,
                             completion_state_t *comstate,
                             const char *line,
                             int word_start,
                             int word_end,
                             int cmdlen,
                             obj_template_t *startobj,
                             val_value_t *startval)
{
    if (!session_cb->config_mode) {
        return ERR_NCX_OPERATION_FAILED;
    }

    /* this flags prevents the module code from looking for more
     * commands to match      */
    comstate->config_done = TRUE;

    /* get the context object to start from */
    obj_template_t *useobj = startobj;
    if (startobj == NULL) {
        useobj = session_cb->config_curobj;
        if (useobj == NULL) {
            return SET_ERROR(ERR_INTERNAL_VAL);
        }
    }

    /* get the context value node in the shadow config tree */
    val_value_t *useval = startval;
    if (startval == NULL && startobj == NULL) {
        useval = session_cb->config_curval;
    }

    /* check function recurse flag */
    if (cmdlen < 0) {
        /* we are recursing after a list was finished.
         * Set flag to prevent getting the list keys again
         */
        if (startobj && obj_is_list(startobj)) {
            comstate->keys_done = TRUE;
        }
        cmdlen = 0;
    } else {
        /* this is a call for the first word
         * check if the startval is NULL;
         * get the corresponding node unless this is a list
         */
        if (useval == NULL &&
            !(obj_is_list(useobj) || obj_is_leaf_list(useobj))) {
            useval = find_top_data_node(session_cb, useobj);
        }
    }

    status_t res = NO_ERR;

    if (startobj || cmdlen == 0) {
        word_start += skip_whitespace(line, word_start, word_end);

        if (!line[word_start] || word_start == word_end) {
            /* just whitespace after first word
             * do completion for all matching child nodes */
            res = fill_one_config_node(session_cb, comstate, useobj, 
                                       (startobj == NULL), useval, cpl,
                                       line, word_end, word_end, cmdlen);
            return res;
        }

        /* reset cmdlen to next word length */
        cmdlen = 0;
        const char *tstr = &line[word_start];
        while (tstr < &line[word_end] && *tstr && !isspace((int)*tstr)) {
            tstr++;
            cmdlen++;
        }
    }

    /* for config mode need to match only 1 word if this
     * first word is complete, otherwise return all that match
     */
    boolean complete_word = ((word_start + cmdlen) < word_end);
    boolean has_keys = FALSE;
    if (startobj && !comstate->keys_done) {
        has_keys = (obj_is_list(useobj) && obj_first_key(useobj));
    }

    /* need to parse the first object which is a child node
     * of the current object */
    obj_template_t *chobj = NULL;
    val_value_t *chval = NULL;

    if (has_keys) {
        /* not expecting child node; expecting key value(s) instead */
        ;
    } else if (complete_word) {
        /* expect this first word to match 1 data node */
        chobj = find_child_obj_str(session_cb, useobj,
                                   (const xmlChar *)&line[word_start],
                                   cmdlen);
        if (chobj == NULL) {
            /* nothing found to complete; this is probably an error command */
            return NO_ERR;
        }

        /* set child as the current object to process */
        useobj = chobj;

        /* check if the child node needs to be found in the
         * config shadow tree   */
        if (useval && !(obj_is_list(chobj) || obj_is_leaf_list(chobj))) {
            chval = val_find_child(useval, obj_get_mod_name(chobj),
                                   obj_get_name(chobj));
            useval = chval;
        }

        /* skip command name length */
        word_start += cmdlen;

        /* skip whitespace after complete word */
        word_start += skip_whitespace(line, word_start, word_end);
    } else {
        /* do completion based on parentobj type */
        res = fill_one_config_node(session_cb, comstate, useobj,
                                   (startobj == NULL), useval, cpl,
                                   line, word_start, word_end, cmdlen);
        return res;
    }

    /* got a start node so figure out what type it is and
     * what to expect next   */
    int word_count = 0;
    val_value_t *listval = NULL;
    if (obj_is_list(useobj) && obj_first_key(useobj)) {
        res = fill_config_list_keys(session_cb, comstate, useobj, 
                                    useval, cpl, line,
                                    word_start, word_end,
                                    &word_count, &listval);
        if (res != NO_ERR || (word_start + word_count) == word_end) {
            return res;
        }
        word_start += word_count;
    } else if (obj_is_leafy(useobj)) {
        boolean complete = FALSE;
        res = fill_config_value(session_cb, useobj, useval, cpl, comstate,
                                line, word_start, word_end, &word_count,
                                &complete);
        if (res != NO_ERR || (word_start + word_count) == word_end) {
            return res;
        }
        word_start += word_count;

        if (complete) {
            if (obj_is_leaf(useobj)) {
                /* no more values are allowed */
                return NO_ERR;
            } // else this is a leaf-list; keep going for more values
        } else {
            // stopped inside value for this leaf or leaf-list
            return NO_ERR;
        }
    }

    if (listval) {
        useval = listval;
        useobj = listval->obj;
    }

    /* current object is a container, leaf-list, or list with completed keys */
    res = fill_one_config_command(session_cb, cpl, comstate, line,
                                  word_start, word_end, -1, useobj, useval);

    return res;
}  /* fill_one_config_command */


/* END yangcli_tab_config.c */
