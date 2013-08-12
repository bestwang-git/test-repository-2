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
#ifndef _H_yangcli_tab_config
#define _H_yangcli_tab_config

/*  FILE: yangcli_tab_config.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

   Tab word completion callback support for libtecla
   Config mode

*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
20-dec-12    abb      Begun;

*/


#include "libtecla.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*		      F U N C T I O N S 			    *
*								    *
*********************************************************************/

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
extern status_t
    fill_one_config_command (session_cb_t *session_cb,
                             WordCompletion *cpl,
                             completion_state_t *comstate,
                             const char *line,
                             int word_start,
                             int word_end,
                             int cmdlen,
                             obj_template_t *startobj,
                             val_value_t *startval);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_tab_config */
