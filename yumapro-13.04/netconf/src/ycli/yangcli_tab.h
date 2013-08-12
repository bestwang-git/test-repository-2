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
#ifndef _H_yangcli_tab
#define _H_yangcli_tab

/*  FILE: yangcli_tab.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

   Tab word completion callback support for libtecla
 
*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
18-apr-09    abb      Begun;

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


/*.......................................................................
 *
 * FUNCTION yangcli_tab_callback (word_complete_cb)
 *
 *   libtecla tab-completion callback function
 *
 * Matches the CplMatchFn typedef
 *
 * From /usr/lib/include/libtecla.h:
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
extern int
    yangcli_tab_callback (WordCompletion *cpl, 
			  void *data,
			  const char *line, 
			  int word_end);


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
extern GlAfterAction
    yangcli_help_callback (GetLine *gl,
                           void *data,
                           int count,
                           size_t curpos,
                           const char *line);


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
extern status_t
    fill_parm_completion (session_cb_t *session_cb,
                          obj_template_t *parmobj,
                          WordCompletion *cpl,
                          completion_state_t *comstate,
                          const char *curval,
                          const char *line,
                          int word_start,
                          int word_end,
                          int parmlen);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_tab */
