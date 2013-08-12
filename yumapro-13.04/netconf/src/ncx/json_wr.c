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
/*  FILE: json_wr.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
27aug11      abb      begun; start from clone of xml_wr.c

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>

#include "procdefs.h"
#include "dlq.h"
#include "ncx.h"
#include "ncx_num.h"
#include "ncxconst.h"
#include "obj.h"
#include "ses.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "xmlns.h"
#include "xml_msg.h"
#include "json_wr.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#ifdef DEBUG
#define JSON_WR_DEBUG  1
#endif


/********************************************************************
* FUNCTION write_json_string_value
* 
* Write a simple value as a JSON string
*
* INPUTS:
*   scb == session control block
*   val == value to write
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    write_json_string_value (ses_cb_t *scb,
                             val_value_t *val)
{
    boolean isnum = typ_is_number(val->btyp);
    xmlChar *valstr = val_make_sprintf_string(val);
    if (valstr) {
        if (!isnum) {
            ses_putchar(scb, '"');
        }
        ses_putjstr(scb, valstr, -1);
        if (!isnum) {
            ses_putchar(scb, '"');
        }
        m__free(valstr);
        return NO_ERR;
    }
    return ERR_INTERNAL_MEM;

} /* write_json_string_value */


/********************************************************************
* FUNCTION write_terminal_node
* 
* Write leaf or leaf-list value
*
* INPUTS:
*   scb == session control block
*   out == value to write
*   force_no_name == TRUE to skip leaf name; needed for select nodesets
*       that have leafs of the same name in them
*********************************************************************/
static void
    write_terminal_node (ses_cb_t *scb,
                         val_value_t *out,
                         boolean force_no_name)
{
    /* write the name of the node 
     * TBD: put module name when forced */
    if (!force_no_name && obj_is_leaf(out->obj)) {
        ses_putchar(scb, '"');
        ses_putjstr(scb, out->name, -1);
        ses_putchar(scb, '"');
        ses_putchar(scb, ':');
    }

    switch (out->btyp) {
    case NCX_BT_EXTERN:
        ses_putchar(scb, '"');
        val_write_extern(scb, out);
        ses_putchar(scb, '"');
        break;
    case NCX_BT_INTERN:
        ses_putchar(scb, '"');
        val_write_intern(scb, out);
        ses_putchar(scb, '"');
        break;
    case NCX_BT_ENUM:
        ses_putchar(scb, '"');
        if (VAL_ENUM_NAME(out)) {
            ses_putjstr(scb, VAL_ENUM_NAME(out), -1);
        }
        ses_putchar(scb, '"');
        break;
    case NCX_BT_EMPTY:
        if (out->v.boo) {
            ses_putchar(scb, '[');
            ses_putjstr(scb, NCX_EL_NULL, -1);
            ses_putchar(scb, ']');
        } // else skip this value! should already be checked
        break;
    case NCX_BT_BOOLEAN:
        if (out->v.boo) {
            ses_putjstr(scb, NCX_EL_TRUE, -1);
        } else {
            ses_putjstr(scb, NCX_EL_FALSE, -1);
        }
        break;
    case NCX_BT_IDREF:
        {
            const xmlChar *modname = val_get_mod_name(out);
            const xmlChar *name = VAL_IDREF_NAME(out);
            if (modname && name) {
                ses_putchar(scb, '"');
                ses_putjstr(scb, modname, -1);
                ses_putchar(scb, ':');
                ses_putjstr(scb, name, -1);
                ses_putchar(scb, '"');
            } else if (name) {
                ses_putchar(scb, '"');
                ses_putjstr(scb, name, -1);
                ses_putchar(scb, '"');
            }
        }
        break;
    default:
        write_json_string_value(scb, out);
    }

} /* write_terminal_node */


/************  E X T E R N A L    F U N C T I O N S    **************/


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
void
    json_wr_start_object (ses_cb_t *scb,
                          xml_msg_hdr_t *msg,
                          const xmlChar *modname,
                          const xmlChar *name,
                          int32  startindent)
{
    assert(scb && "scb is NULL!");
    assert(msg && "msg is NULL!");
    assert(name && "name is NULL!");

    (void)msg;

    int32 indent_amount = ses_message_indent_count(scb);
    int32 indent = ses_new_indent_count(TRUE, startindent, indent_amount);

    ses_putchar(scb, '{');
    ses_indent(scb, indent);    
    ses_putchar(scb, '"');
    if (modname) {
        ses_putstr(scb, modname);
        ses_putchar(scb, ':');
    }
    ses_putstr(scb, name);
    ses_putchar(scb, '"');
    ses_putchar(scb, ':');
    if (indent >= 0) {
        ses_putchar(scb, ' ');
    }

}  /* json_wr_start_object */


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
void
    json_wr_start_object_ex (ses_cb_t *scb,
                             xml_msg_hdr_t *msg,
                             const xmlChar *modname,
                             const xmlChar *name,
                             int32  startindent,
                             boolean startbrace)
{
    assert(scb && "scb is NULL!");
    assert(msg && "msg is NULL!");
    assert(name && "name is NULL!");

    (void)msg;

    if (startbrace) {
        ses_putchar(scb, '{');
    }

    int32 indent_amount = ses_message_indent_count(scb);
    int32 indent = startindent;

    if (startbrace) {
        indent = ses_new_indent_count(TRUE, startindent, indent_amount);
    }

    ses_indent(scb, indent);    
    ses_putchar(scb, '"');
    if (modname) {
        ses_putstr(scb, modname);
        ses_putchar(scb, ':');
    }
    ses_putstr(scb, name);
    ses_putchar(scb, '"');
    ses_putchar(scb, ':');
    if (indent >= 0) {
        ses_putchar(scb, ' ');
    }
    if (!startbrace) {
        ses_putchar(scb, '{');
    }

}  /* json_wr_start_object_ex */


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
void
    json_wr_end_object (ses_cb_t *scb,
                        xml_msg_hdr_t *msg,
                        int32  startindent)
{
    assert(scb && "scb is NULL!");
    assert(msg && "msg is NULL!");
    (void)msg;

    ses_indent(scb, startindent);
    ses_putchar(scb, '}');

}  /* json_wr_end_object */


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
void
    json_wr_start_array (ses_cb_t *scb,
                         xml_msg_hdr_t *msg,
                         int32  startindent)
{
    assert(scb && "scb is NULL!");
    assert(msg && "msg is NULL!");
    (void)msg;

    ses_indent(scb, startindent);
    ses_putchar(scb, '[');

}  /* json_wr_start_array */


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
void
    json_wr_end_array (ses_cb_t *scb,
                       xml_msg_hdr_t *msg,
                       int32  startindent)
{
    assert(scb && "scb is NULL!");
    assert(msg && "msg is NULL!");
    (void)msg;

    ses_indent(scb, startindent);
    ses_putchar(scb, ']');

}  /* json_wr_end_array */


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
void
    json_wr_simval_line (ses_cb_t *scb,
                         xml_msg_hdr_t *msg,
                         xmlns_id_t nsid,
                         const xmlChar *namestr,
                         const xmlChar *valstr,
                         ncx_btype_t btyp,
                         int32  startindent,
                         boolean isfirst)
{
    assert(scb && "scb is NULL!");
    assert(msg && "msg is NULL!");
    assert(namestr && "namestr is NULL!");
    assert(valstr && "valstr is NULL!");

    (void)nsid;

    if (!isfirst) {
        ses_putchar(scb, ',');
    }

    ses_indent(scb, startindent);

    if (namestr) {
        ses_putchar(scb, '"');
        ses_putjstr(scb, namestr, -1);
        ses_putchar(scb, '"');
        ses_putchar(scb, ':');
    }

    switch (btyp) {
    case NCX_BT_EXTERN:
    case NCX_BT_INTERN:
    case NCX_BT_ENUM:
        ses_putchar(scb, '"');
        ses_putjstr(scb, valstr, -1);
        ses_putchar(scb, '"');
        break;
    case NCX_BT_EMPTY:
        ses_putchar(scb, '[');
        ses_putjstr(scb, NCX_EL_NULL, -1);
        ses_putchar(scb, ']');
        break;
    case NCX_BT_BOOLEAN:
        ses_putjstr(scb, valstr, -1);
        break;
    default:
        {
            boolean isnum = typ_is_number(btyp);
            if (!isnum) {
                ses_putchar(scb, '"');
            }
            ses_putjstr(scb, valstr, -1);
            if (!isnum) {
                ses_putchar(scb, '"');
            }
        }
    }

}  /* json_wr_simval_line */


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
status_t
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
                           boolean force_array_obj)
{
    assert(scb && "scb is NULL!");
    assert(msg && "msg is NULL!");
    assert(val && "val is NULL!");

    (void)parent_nsid;

    boolean malloced = FALSE;
    status_t res = NO_ERR;
    val_value_t *out =
        val_get_value(scb, msg, val, testfn, TRUE, &malloced, &res);
    if (!out || res != NO_ERR) {
        if (res == ERR_NCX_SKIPPED) {
            res = NO_ERR;
        }
        if (malloced) {
            val_free_value(out);
        }
        return res;
    }

    boolean is_array_obj =
        (obj_is_list(val->obj) || obj_is_leaf_list(val->obj));
    if (!is_array_obj) {
        is_array_obj = force_array_obj;
    }

    int32 indent_amount = ses_message_indent_count(scb);
    int32 indent = startindent;
    boolean out_is_simple = typ_is_simple(out->btyp);

    if (!isfirstchild) {
        ses_putchar(scb, ',');
    }

    if (isfirst) {
        ses_putchar(scb, '{');
        indent = ses_new_indent_count(TRUE, startindent, indent_amount);
    } else if (force_lastsibling && !(isfirstsibling && isfirstchild)) {
        indent = ses_new_indent_count(TRUE, startindent, indent_amount);
    }

    /* check if this is an external file to send */
    if (!is_array_obj &&
        (out->btyp == NCX_BT_EXTERN || out->btyp == NCX_BT_INTERN ||
         out_is_simple)) {
        ses_indent(scb, indent);
        write_terminal_node(scb, out, FALSE);
    } else {
        /* render a complex type; either an object or an array */
        if (isfirstsibling) {
            ses_indent(scb, indent);
            ses_putchar(scb, '"');
            ses_putjstr(scb, out->name, -1);
            ses_putchar(scb, '"');
            ses_putchar(scb, ':');
            if (indent > 0) {
                ses_putchar(scb, ' ');
            }
            if (is_array_obj) {
                ses_putchar(scb, '[');
            }
        }

        if (is_array_obj) {
            indent = ses_new_indent_count(TRUE, indent, indent_amount);
            ses_indent(scb, indent);
        }

        if (is_array_obj && out_is_simple) {
            write_terminal_node(scb, out, force_array_obj);
        } else {
            ses_putchar(scb, '{');
            indent = ses_new_indent_count(TRUE, indent, indent_amount);

            val_value_t *lastch = NULL;
            val_value_t *nextch = NULL;
        
            val_value_t *chval = val_get_first_child(out); 
            for (; chval != NULL && res == NO_ERR; chval = nextch) {

                /* JSON ignores XML namespaces, so foo:a and bar:a
                 * are both encoded in the same array
                 */

                boolean firstchild = (lastch == NULL);
                boolean firstsibling = 
                    (!lastch || xml_strcmp(lastch->name, chval->name));

                lastch = chval;
                nextch = val_get_next_child(chval);

                res = json_wr_max_check_val(scb, msg, val_get_nsid(out),
                                            chval, indent, testfn,
                                            FALSE, FALSE, firstchild,
                                            firstsibling, FALSE, FALSE, FALSE);
                                       
                if (res == ERR_NCX_SKIPPED) {
                    res = NO_ERR;
                }
            }

            indent = ses_new_indent_count(FALSE, indent, indent_amount);
            ses_indent(scb, indent);
            ses_putchar(scb, '}');
        }

        if (is_array_obj) {
            boolean doarr = FALSE;
            val_value_t *peeknext = val_get_next_child(val);
            if (force_lastsibling) {
                if (force_lastsib_value) {
                    doarr = TRUE;
                }
            } else if (!peeknext || xml_strcmp(val->name, peeknext->name)) {
                doarr = TRUE;
            }
            if (doarr) {
                indent = ses_new_indent_count(FALSE, indent, indent_amount);
                ses_indent(scb, indent);
                ses_putchar(scb, ']');
            }
        }
    }

    boolean doend = isfirst;
    if (force_lastsibling) {
        if (!force_lastsib_value) {
            doend = FALSE;
        } else {
            doend = islast;
        }
    }
    if (doend) {
        indent = ses_new_indent_count(FALSE, indent, indent_amount);
        ses_indent(scb, indent);
        ses_putchar(scb, '}');
    }

    if (malloced) {
        val_free_value(out);
    }

    return res;

}  /* json_wr_max_check_val */


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
status_t
    json_wr_full_check_val (ses_cb_t *scb,
                            xml_msg_hdr_t *msg,
                            xmlns_id_t parent_nsid,
                            val_value_t *val,
                            int32  startindent,
                            val_nodetest_fn_t testfn)
{
    return json_wr_max_check_val(scb, msg, parent_nsid, val, startindent, 
                                 testfn, TRUE, TRUE, TRUE, TRUE, FALSE, 
                                 FALSE, FALSE);

}  /* json_wr_full_check_val */


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
status_t
    json_wr_check_open_file (FILE *fp, 
                             val_value_t *val,
                             int32 startindent,
                             int32  indent,
                             val_nodetest_fn_t testfn)
{
    assert(fp && "fp is NULL!");
    assert(val && "val is NULL!");

    rpc_msg_t *msg = NULL;
    status_t res = NO_ERR;

    xml_attrs_t myattrs;

    indent = min(indent, 9);

    xml_init_attrs(&myattrs);

    /* get a dummy session control block */
    ses_cb_t *scb = ses_new_dummy_scb();
    if (!scb) {
        res = ERR_INTERNAL_MEM;
    } else {
        scb->fp = fp;
        scb->indent = indent;
    }

    /* get a dummy output message */
    if (res == NO_ERR) {
        msg = rpc_new_out_msg();
        if (!msg) {
            res = ERR_INTERNAL_MEM;
        } else {
            /* hack -- need a queue because there is no top
             * element which this usually shadows
             */
            msg->rpc_in_attrs = &myattrs;
        }
    }

    if (res == NO_ERR) {
        xmlns_id_t parent_nsid = 0;
        if (val->parent) {
            parent_nsid = val_get_nsid(val->parent);
        }

        /* write the tree in JSON format */
        res = json_wr_full_check_val(scb, &msg->mhdr, parent_nsid,
                                     val, startindent, testfn);
        ses_finish_msg(scb);
    }

    /* clean up and exit */
    if (msg) {
        rpc_free_msg(msg);
    }
    if (scb) {
        scb->fp = NULL;   /* do not close the file */
        ses_free_scb(scb);
    }

    xml_clean_attrs(&myattrs);
    return res;

} /* json_wr_check_open_file */


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
status_t
    json_wr_check_file (const xmlChar *filespec, 
                        val_value_t *val,
                        int32 startindent,
                        int32  indent,
                        val_nodetest_fn_t testfn)
{
    assert(filespec && "filespec is NULL!");
    assert(val && "val is NULL!");

    FILE *fp = fopen((const char *)filespec, "w");
    if (!fp) {
        log_error("\nError: Cannot open XML file '%s'", filespec);
        return ERR_FIL_OPEN;
    }

    status_t res =
        json_wr_check_open_file(fp, val, startindent, indent, testfn);

    fclose(fp);

    return res;

} /* json_wr_check_file */


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
status_t
    json_wr_file (const xmlChar *filespec,
                  val_value_t *val,
                  int32 startindent,
                  int32 indent)
{
    return json_wr_check_file(filespec, val, startindent, indent, NULL);

} /* json_wr_file */


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
void
    json_wr_output_null (ses_cb_t *scb,
                         int32 startindent)
{
    assert(scb && "scb is NULL!");
    ses_indent(scb, startindent);
    ses_putstr(scb, JSON_WR_YANG_NULL);

}  /* json_wr_output_null */


/* END file json_wr.c */
