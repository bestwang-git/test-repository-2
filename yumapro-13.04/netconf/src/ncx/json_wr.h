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
#ifndef _H_json_wr
#define _H_json_wr

/*  FILE: json_wr.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    JSON Write functions

*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
01-sep-11    abb      Begun;

*/

#include <stdio.h>
#include <xmlstring.h>

#ifndef _H_ses
#include "ses.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_val_util
#include "val_util.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*			 C O N S T A N T S			    *
*								    *
*********************************************************************/
#define JSON_WR_YANG_NULL (const xmlChar *)"[null]"

/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/

/* type parser used in 3 separate modes */
typedef enum json_wr_tag_t_ {
    JSON_WR_TAG_NONE,
    JSON_WR_TAG_FIRST,
    JSON_WR_TAG_MIDDLE,
    JSON_WR_TAG_LAST
} json_wr_tag_t;


/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/


/********************************************************************
* FUNCTION json_wr_start_object
* 
* generate start of a JSON object
*
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   modname == module name of JSON object (NULL to omit)
*   name == name of JSON object
*   startindent == start indent amount if indent enabled
*   
*********************************************************************/
extern void
    json_wr_start_object (ses_cb_t *scb,
                          xml_msg_hdr_t *msg,
                          const xmlChar *modname,
                          const xmlChar *name,
                          int32  startindent);


/********************************************************************
* FUNCTION json_wr_start_object_ex
* 
* generate start of a JSON object
*
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   modname == module name of JSON object (NULL to omit)
*   name == name of JSON object
*   startindent == start indent amount if indent enabled
*   starbrace == TRUE to print start brace
*********************************************************************/
extern void
    json_wr_start_object_ex (ses_cb_t *scb,
                             xml_msg_hdr_t *msg,
                             const xmlChar *modname,
                             const xmlChar *name,
                             int32  startindent,
                             boolean startbrace);


/********************************************************************
* FUNCTION json_wr_end_object
* 
* generate end of a JSON object
*
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   startindent == start indent amount if indent enabled
*
*********************************************************************/
extern void
    json_wr_end_object (ses_cb_t *scb,
                        xml_msg_hdr_t *msg,
                        int32  startindent);


/********************************************************************
* FUNCTION json_wr_start_array
* 
* generate start of a JSON array
*
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   startindent == start indent amount if indent enabled
*   
*********************************************************************/
extern void
    json_wr_start_array (ses_cb_t *scb,
                         xml_msg_hdr_t *msg,
                         int32  startindent);


/********************************************************************
* FUNCTION json_wr_end_array
* 
* generate end of a JSON array
*
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   startindent == start indent amount if indent enabled
*
*********************************************************************/
extern void
    json_wr_end_array (ses_cb_t *scb,
                       xml_msg_hdr_t *msg,
                       int32  startindent);


/********************************************************************
* FUNCTION json_wr_simval_line
* 
* generate 1 line for a simple value within a container or array
*
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   nsid == active namespace ID
*   namestr == complete name string 
*      - has module name already if needed
*      - NULL to skip (inside an array)
*   valstr == value string to write (NULL for empty types)
*   btyp == basetype of the value before translated to string
*   startindent == start indent amount if indent enabled
*   isfirst == TRUE if this is the first line; FALSE if not
*********************************************************************/
extern void
    json_wr_simval_line (ses_cb_t *scb,
                         xml_msg_hdr_t *msg,
                         xmlns_id_t nsid,
                         const xmlChar *namestr,
                         const xmlChar *valstr,
                         ncx_btype_t btyp,
                         int32  startindent,
                         boolean isfirst);


/********************************************************************
* FUNCTION json_wr_max_check_val
* 
* generate entire val_value_t *w/filter)
* Write an entire val_value_t out as XML, including the top level
* Using an optional testfn to filter output
* Maximum parameters exposed
*
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   parent_nsid == parent namespace ID; used if val->parent NULL
*   val == value to write
*   startindent == start indent amount if indent enabled
*   testcb == callback function to use, NULL if not used
*   isfirst == TRUE if this is the first (top) val printed
*   islast == TRUE if this is the last (top) val printed
*   isfirstchild == TRUE if this is the first child of a parent node
*                == FALSE if this is the 2nd - Nth value of an array
*   isfirstsibling == TRUE if this is the first value of an array
*                == FALSE if this is the 2nd - Nth value of an array
*   force_lastsibling == TRUE to force this as the last sibling
*        TRUE or FALSE to check the sibling of 'val'
*   force_lastsib_value == TRUE to force this as the last sibling
*        FALSE to force this as not the last sibling; ignore if
*        force_lastsibling is FALSE
*   force_array_obj == TRUE to treat select leafs and containers
*       as leaf-lists and lists which normally have only 1 instance
*       in their actual context, but select removes the context
* RETURNS:
*   status
*********************************************************************/
extern status_t
    json_wr_max_check_val (ses_cb_t *scb,
                           xml_msg_hdr_t *msg,
                           xmlns_id_t parent_nsid,
                           val_value_t *val,
                           int32  startindent,
                           val_nodetest_fn_t testfn,
                           boolean isfirst,
                           boolean islast,
                           boolean isfirstchild,
                           boolean isfirstsibling,
                           boolean force_lastsibling,
                           boolean force_lastsib_value,
                           boolean force_array_obj);


/********************************************************************
* FUNCTION json_wr_full_check_val
* 
* generate entire val_value_t *w/filter)
* Write an entire val_value_t out as XML, including the top level
* Using an optional testfn to filter output
*
* INPUTS:
*   scb == session control block
*   msg == xml_msg_hdr_t in progress
*   parent_nsid == parent namespace ID; used if val->parent NULL
*   val == value to write
*   startindent == start indent amount if indent enabled
*   testcb == callback function to use, NULL if not used
*   
* RETURNS:
*   status
*********************************************************************/
extern status_t
    json_wr_full_check_val (ses_cb_t *scb,
                            xml_msg_hdr_t *msg,
                            xmlns_id_t parent_nsid,
                            val_value_t *val,
                            int32  startindent,
                            val_nodetest_fn_t testfn);


/********************************************************************
* FUNCTION json_wr_check_open_file
* 
* Write the specified value to an open FILE in JSON format
*
* INPUTS:
*    fp == open FILE control block
*    val == value for output
*    startindent == starting indent point
*    indent == indent amount (0..9 spaces)
*    testfn == callback test function to use
*
* RETURNS:
*    status
*********************************************************************/
extern status_t
    json_wr_check_open_file (FILE *fp, 
                             val_value_t *val,
                             int32 startindent,
                             int32  indent,
                             val_nodetest_fn_t testfn);


/********************************************************************
* FUNCTION json_wr_check_file
* 
* Write the specified value to a FILE in JSON format
*
* INPUTS:
*    filespec == exact path of filename to open
*    val == value for output
*    startindent == starting indent point
*    indent == indent amount (0..9 spaces)
*    testfn == callback test function to use
*
* RETURNS:
*    status
*********************************************************************/
extern status_t
    json_wr_check_file (const xmlChar *filespec, 
                        val_value_t *val,
                        int32 startindent,
                        int32  indent,
                        val_nodetest_fn_t testfn);


/********************************************************************
* FUNCTION json_wr_file
* 
* Write the specified value to a FILE in JSON format
*
* INPUTS:
*    filespec == exact path of filename to open
*    val == value for output
*    startindent == starting indent point
*    indent == indent amount (0..9 spaces)
*
* RETURNS:
*    status
*********************************************************************/
extern status_t
    json_wr_file (const xmlChar *filespec,
                  val_value_t *val,
                  int32 startindent,
                  int32 indent);

/********************************************************************
* FUNCTION json_wr_output_null
* 
* generate a null value; needed for printing empty <data> element
* which is done as a wrapper, not a real value node
* INPUTS:
*   scb == session control block
*   startindent == start indent amount if indent enabled
*
*********************************************************************/
extern void
    json_wr_output_null (ses_cb_t *scb,
                         int32 startindent);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_json_wr */

