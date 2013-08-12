/*
 * Copyright (c) 2013, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: yangcli_autotest.c

   NETCONF YANG-based CLI Tool

   autotest support

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
05-mar-13    abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
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
#include <assert.h>

#include "libtecla.h"

#include "procdefs.h"
#include "log.h"
#include "mgr.h"
#include "mgr_ses.h"
#include "ncx.h"
#include "ncx_feature.h"
#include "ncx_list.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "op.h"
#include "rpc.h"
#include "rpc_err.h"
#include "status.h"
#include "val_util.h"
#include "var.h"
#include "xmlns.h"
#include "xml_util.h"
#include "xml_val.h"
#include "yangconst.h"
#include "yangcli.h"
#include "yangcli_autotest.h"
#include "yangcli_cmd.h"
#include "yangcli_save.h"
#include "yangcli_util.h"

#ifdef DEBUG
#define YANGCLI_AUTOTEST_DEBUG 1
#endif


/* forward declaration of auto-make object entry point */
static status_t
    auto_create_object (server_cb_t *server_cb,
                        session_cb_t *session_cb,
                        obj_template_t *obj,
                        val_value_t *parentval);



/********************************************************************
* FUNCTION auto_create_leafy
*
* Generate an instance of the specified leafy object
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   obj == leaf or leaf-list obj template to use
*   retres == address of return status
* OUTPUTS:
*   *retres == return status
* RETURNS:
*    malloced leaf or leaf-list value
*********************************************************************/
static val_value_t *
    auto_create_leafy (server_cb_t *server_cb,
                       session_cb_t *session_cb,
                       obj_template_t *obj,
                       status_t *retres)
{
    /* TBD: use variables/expressions in templates */
    (void)server_cb;
    (void)session_cb;

    *retres = NO_ERR;

    val_value_t *leafval = val_new_value();
    if (leafval == NULL) {
        *retres = ERR_INTERNAL_MEM;
        return NULL;
    }
    val_init_from_template(leafval, obj);

    typ_def_t *typdef = obj_get_typdef(obj);
    ncx_btype_t btyp = obj_get_basetype(obj);
    const xmlChar *name = obj_get_name(obj);

    int seed_iter = ++session_cb->autotest_cb->seed_iteration;
    int seed = time(NULL);
    seed = seed * seed_iter;
    seed += (*name << 6);
    srand(seed);
    int randnum = rand();

    switch (btyp) {
    case NCX_BT_NONE:
    case NCX_BT_ANY:
    case NCX_BT_FLOAT64:
    case NCX_BT_CONTAINER:
    case NCX_BT_CHOICE:
    case NCX_BT_CASE:
    case NCX_BT_LIST:
    case NCX_BT_EXTERN:
    case NCX_BT_INTERN:
    case NCX_BT_SLIST:
        *retres = SET_ERROR(ERR_INTERNAL_VAL);
        break;
    case NCX_BT_BITS:
        *retres = ERR_NCX_OPERATION_FAILED;
        break;
    case NCX_BT_ENUM:
        {
            uint32 enumcount = typ_enumdef_count(typdef);
            uint32 enumid = (uint32)(randnum % (int)enumcount);
            typ_enum_t *enumrec = typ_get_enumdef_n(typdef, enumid);
            if (enumrec == NULL) {
                *retres = SET_ERROR(ERR_INTERNAL_VAL);
            } else {
                VAL_ENUM_NAME(leafval) = enumrec->name;
            }
        }
        break;
    case NCX_BT_EMPTY:
        VAL_FLAG(leafval) = (randnum & bit15);
        break;
    case NCX_BT_BOOLEAN:
        VAL_BOOL(leafval) = (randnum & bit17);
        break;
    case NCX_BT_INT8:
        {
            int8 i8 = (int8)(randnum % NCX_MAX_INT8);
            if (randnum & bit20) {
                i8 *= -1;
            }
            VAL_L_INT8(leafval) = i8;
        }
        break;
    case NCX_BT_INT16:
        {
            int16 i16 = (int16)(randnum % NCX_MAX_INT16);
            if (randnum & bit20) {
                i16 *= -1;
            }
            VAL_L_INT16(leafval) = i16;
        }
        break;
    case NCX_BT_INT32:
        {
            int32 i32 = (int32)(randnum % NCX_MAX_INT32);
            if (randnum & bit20) {
                i32 *= -1;
            }
            VAL_INT32(leafval) = i32;
        }
        break;
    case NCX_BT_INT64:
        {
            int64 i64 = (int64)(randnum % NCX_MAX_INT64);
            if (randnum & bit20) {
                i64 *= -1;
            }
            VAL_INT64(leafval) = i64;
        }
        break;
    case NCX_BT_UINT8:
        {
            uint8 u8 = (uint8)(randnum % NCX_MAX_UINT8);
            VAL_L_UINT8(leafval) = u8;
        }
        break;
    case NCX_BT_UINT16:
        {
            uint16 u16 = (uint16)(randnum % NCX_MAX_UINT16);
            VAL_L_UINT16(leafval) = u16;
        }
        break;
    case NCX_BT_UINT32:
        {
            uint32 u32 = (uint32)(randnum % NCX_MAX_UINT32);
            VAL_UINT32(leafval) = u32;
        }
        break;
    case NCX_BT_UINT64:
        {
            uint64 u64 = (uint64)randnum;
            VAL_UINT64(leafval) = u64;
        }
        break;
    case NCX_BT_DECIMAL64:
        {
            uint8 digits = (uint8)typ_get_fraction_digits(typdef);

            /*** !!! TBD: this is probably wrong !!!  ***/
            uint8 lzero = 0;
            int64 decval = (int64)(randnum % NCX_MAX_UINT32);

            VAL_DEC64(leafval) = decval;
            VAL_DEC64_DIGITS(leafval) = digits;
            VAL_DEC64_ZEROES(leafval) = lzero;
        }
        break;
    case NCX_BT_STRING:
        /* this is really hard to generate strings that match
         * patterns!! generate something for now  !!!   */
        if (randnum & bit2) {
            VAL_STR(leafval) = xml_strdup((const xmlChar *)"foo");
        } else if (randnum & bit3) {
            VAL_STR(leafval) = xml_strdup((const xmlChar *)"bar");
        } else if (randnum & bit4) {
            VAL_STR(leafval) = xml_strdup((const xmlChar *)"baz");
        } else if (randnum & bit6) {
            VAL_STR(leafval) = xml_strdup((const xmlChar *)"fred");
        } else if (randnum & bit9) {
            VAL_STR(leafval) = xml_strdup((const xmlChar *)"barney");
        } else if (randnum & bit13) {
            VAL_STR(leafval) = xml_strdup((const xmlChar *)"wilma");
        } else if (randnum & bit17) {
            VAL_STR(leafval) = xml_strdup((const xmlChar *)"betty");
        } else if (randnum & bit21) {
            VAL_STR(leafval) = xml_strdup((const xmlChar *)"bambam");
        } else if (randnum & bit24) {
            VAL_STR(leafval) = xml_strdup((const xmlChar *)"dino");
        } else {
            VAL_STR(leafval) = xml_strdup((const xmlChar *)"none");
        }
        if (VAL_STR(leafval) == NULL) {
            *retres = ERR_INTERNAL_MEM;
        }
        break;
    case NCX_BT_BINARY:
    case NCX_BT_INSTANCE_ID:
    case NCX_BT_UNION:
    case NCX_BT_LEAFREF:
    case NCX_BT_IDREF:
        *retres = ERR_NCX_OPERATION_FAILED;
        break;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (*retres != NO_ERR) {
        val_free_value(leafval);
        leafval = NULL;
    }

    return leafval;

}  /* auto_create_leafy */


/********************************************************************
* FUNCTION auto_create_choice
*
* Generate an instance of the specified list object
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   obj == list obj template to use
*   parentval == parentval to add case objects into
*
* OUTPUTS:
*   parentval childQ may have new nodes added
* RETURNS:
*    status
*********************************************************************/
static status_t
    auto_create_choice (server_cb_t *server_cb,
                        session_cb_t *session_cb,
                        obj_template_t *obj,
                        val_value_t *parentval)
{
    /* TBD: use variables/expressions in templates */
    (void)server_cb;
    (void)session_cb;

    /* TBD: not supprted yet */
    (void)obj;
    (void)parentval;

    return NO_ERR;

}  /* auto_create_choice */


/********************************************************************
* FUNCTION auto_create_leaflist
*
* Generate instances of the specified leaf-list object
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   obj == leaf-list obj template to use
*   parentval == parentval to add case objects into
*
* OUTPUTS:
*   parentval childQ may have new nodes added
* RETURNS:
*    status
*********************************************************************/
static status_t
    auto_create_leaflist (server_cb_t *server_cb,
                          session_cb_t *session_cb,
                          obj_template_t *obj,
                          val_value_t *parentval)
{
    /* TBD: generate N different leafy objects */

    status_t res = NO_ERR;
    val_value_t *val = auto_create_leafy(server_cb, session_cb, obj, &res);
    if (res != NO_ERR) {
        return res;
    }
    val_add_child(val, parentval);
    return NO_ERR;

}  /* auto_create_leaflist */


/********************************************************************
* FUNCTION auto_create_list
*
* Generate an instance of the specified list object
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   obj == list obj template to use
*
* OUTPUTS:
*   *retres == return status
* RETURNS:
*    malloced list value
*********************************************************************/
static val_value_t *
    auto_create_list (server_cb_t *server_cb,
                      session_cb_t *session_cb,
                      obj_template_t *obj,
                      status_t *retres)
{
    *retres = NO_ERR;

    val_value_t *listval = val_new_value();
    if (listval == NULL) {
        *retres = ERR_INTERNAL_MEM;
        return NULL;
    }
    val_init_from_template(listval, obj);

    /* create all the keys */
    obj_key_t *key = obj_first_key(obj);
    for (; key && *retres == NO_ERR; key = obj_next_key(key)) {
        val_value_t *keyval =
            auto_create_leafy(server_cb, session_cb, key->keyobj, retres);
        if (keyval == NULL) {
            val_free_value(listval);
            return NULL;
        }
        val_add_child(keyval, listval);
    }

    if (retres == NO_ERR) {
        *retres = val_gen_index_chain(obj, listval);
    }

    obj_template_t *childobj = obj_first_child(obj);
    for (; childobj && *retres == NO_ERR;
         childobj = obj_next_child(childobj)) {
        if (!obj_is_config(childobj)) {
            continue;
        }
        if (obj_is_key(childobj)) {
            continue;
        }

        *retres = auto_create_object(server_cb, session_cb,
                                     childobj, listval);
    }

    if (*retres != NO_ERR) {
        val_free_value(listval);
        listval = NULL;
    }

    return listval;

}  /* auto_create_list */


/********************************************************************
* FUNCTION auto_create_container
*
* Generate an instance of the specified container object
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   obj == container obj template to use
*
* OUTPUTS:
*   *retres == return status
* RETURNS:
*    malloced container value
*********************************************************************/
static val_value_t *
    auto_create_container (server_cb_t *server_cb,
                           session_cb_t *session_cb,
                           obj_template_t *obj,
                           status_t *retres)
{
    *retres = NO_ERR;

    val_value_t *conval = val_new_value();
    if (conval == NULL) {
        *retres = ERR_INTERNAL_MEM;
        return NULL;
    }
    val_init_from_template(conval, obj);

    obj_template_t *childobj = obj_first_child(obj);
    for (; childobj && *retres == NO_ERR;
         childobj = obj_next_child(childobj)) {
        if (!obj_is_config(childobj)) {
            continue;
        }
        *retres = auto_create_object(server_cb, session_cb, childobj, conval);
    }

    if (*retres != NO_ERR) {
        val_free_value(conval);
        conval = NULL;
    }

    return conval;

}  /* auto_create_container */


/********************************************************************
* FUNCTION auto_create_object
*
* Generate an instance of the specified object
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   obj == obj template to use
*   parentval == parent value to add nodes into
* OUTPUTS:
*   *retres == return status
* RETURNS:
*    malloced list value
*********************************************************************/
static status_t
    auto_create_object (server_cb_t *server_cb,
                        session_cb_t *session_cb,
                        obj_template_t *obj,
                        val_value_t *parentval)
{
    val_value_t *val = NULL;
    status_t res = NO_ERR;

    if (obj_is_list(obj)) {
        val = auto_create_list(server_cb, session_cb, obj, &res);
    } else if (obj_is_leaf_list(obj)) {
        res = auto_create_leaflist(server_cb, session_cb, obj, parentval);
    } else if (obj_is_leaf(obj)) {
        val = auto_create_leafy(server_cb, session_cb, obj, &res);
    } else if (obj_is_choice(obj)) {
        res = auto_create_choice(server_cb, session_cb, obj, parentval);
    } else if (obj_is_container(obj)) {
        val = auto_create_container(server_cb, session_cb, obj, &res);
    } else {
        log_debug("\nautotest: Skipping %s %s:%s",
                  obj_get_typestr(obj), obj_get_mod_name(obj),
                  obj_get_name(obj));
    }
    if (val) {
        val_add_child(val, parentval);
    }
    return res;

}  /* auto_create_object */


/********************************************************************
* FUNCTION finish_auto_test
*
* Finish the auto-test command
*
* INPUTS:
*   session_cb == session control block to use
*   autotest_cb == autotest control block to use
*********************************************************************/
static void
    finish_auto_test (session_cb_t *session_cb,
                      autotest_cb_t *autotest_cb)
{
    session_cb->command_mode = CMD_MODE_NORMAL;
    val_free_value(autotest_cb->editroot);
    autotest_cb->editroot = NULL;
    autotest_cb->state = AUTOTEST_ST_NONE;

    // TBD: do some summary reporting

} /* finish_auto_test */


/********************************************************************
* FUNCTION send_auto_test_edit
*
* Setup an edit and send it to the server
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    send_auto_test_edit (server_cb_t *server_cb,
                         session_cb_t *session_cb)
{
    autotest_cb_t *autotest_cb = session_cb->autotest_cb;
    autotest_cb->cur_iteration++;

    //uint32 maxcount = autotest_cb->count;
    //uint32 curcount = 1;

    status_t res = NO_ERR;
    val_value_t *configval =
        autotest_create_object_payload(server_cb, session_cb,
                                       autotest_cb->target_obj, &res);

    if (res == NO_ERR) {
        /* this will consume the configval memory even if error occurs */
        res = send_edit_config_to_server(server_cb, session_cb, NULL,
                                         configval, TRUE,
                                         session_cb->timeout,
                                         OP_DEFOP_MERGE);
    }

    if (res == NO_ERR) {
        autotest_cb->state = AUTOTEST_ST_WAIT_EDIT;
    } else {
        autotest_cb->res = res;
        finish_auto_test(session_cb, autotest_cb);
    }

    return res;
}  /* send_auto_test_edit */


/********************************************************************
* FUNCTION start_auto_test
*
* Setup the autotest_cb_t struct and start the first edit
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   editroot == constructed path-to-target-val
*        VALUE IS FREED EVEN IF ERROR IS RETURNED!!!!
*   target_val == dummy value in the constructed editroot
*       tree that needs to be replaced with 1 or more real values
*       in each edit
*   target_obj == object template to use for root leaf and leaf-lists
*   count == max internal list count
*   iterations = edit iterations to so
* RETURNS:
*   status
*********************************************************************/
static status_t
    start_auto_test (server_cb_t *server_cb,
                     session_cb_t *session_cb,
                     val_value_t *editroot,
                     val_value_t *target_val,
                     obj_template_t *target_obj,
                     uint32 count,
                     uint32 iterations)
{
    if (session_cb->default_target == NULL) {
        log_error("\nError: server does not support editing");
        val_free_value(editroot);
        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    autotest_cb_t *autotest_cb = session_cb->autotest_cb;
    if (autotest_cb) {
        if (autotest_cb->state != AUTOTEST_ST_NONE) {
            log_error("\nError: auto-test already in progress");
            val_free_value(editroot);
            return ERR_NCX_OPERATION_FAILED;
        }
    } else {
        autotest_cb = m__getObj(autotest_cb_t);
        if (autotest_cb == NULL) {
            val_free_value(editroot);
            return ERR_INTERNAL_MEM;
        }
        memset(autotest_cb, 0x0, sizeof(autotest_cb_t));
        session_cb->autotest_cb = autotest_cb;
    }

    if (autotest_cb->editroot) {
        val_free_value(autotest_cb->editroot);
    }
    autotest_cb->editroot = editroot;
    autotest_cb->target_val = target_val;
    autotest_cb->target_obj = target_obj;
    autotest_cb->count = count;

    if (count > 1 &&
        !(obj_is_list(target_val->obj) || obj_is_leaf_list(target_val->obj))) {
        log_info("\nChanging count from %u to 1 for %s %s:%s",
                 count, obj_get_typestr(target_val->obj),
                 obj_get_mod_name(target_val->obj),
                 obj_get_name(target_val->obj));
        autotest_cb->count = 1;
    }

    autotest_cb->iterations = iterations;
    autotest_cb->cur_iteration = 0;
    autotest_cb->seed_iteration = 0;
    autotest_cb->state = AUTOTEST_ST_STARTED;
    autotest_cb->res = NO_ERR;

    session_cb->command_mode = CMD_MODE_AUTOTEST;

    status_t res = send_auto_test_edit(server_cb, session_cb);
    
    return res;

}  /* start_auto_test */


/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION autotest_create_object
*
* Generate an instance of the specified object
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   obj == obj template to use
*   
* RETURNS:
*    status
*********************************************************************/
status_t
    autotest_create_object (server_cb_t *server_cb,
                            session_cb_t *session_cb,
                            obj_template_t *obj,
                            val_value_t *parentval)
{
    assert(server_cb && "server_cb is NULL!");
    assert(session_cb && "session_cb is NULL!");
    assert(obj && "obj is NULL!");
    assert(parentval && "parentval is NULL!");

    status_t res =
        auto_create_object(server_cb, session_cb, obj, parentval);
    
    return res;

}  /* autotest_create_object */


/********************************************************************
* FUNCTION autotest_create_object_payload
*
* Generate an instance of the specified top-level object
*  as a <config> payload
*
* INPUTS:
*   server_cb == server context to use
*   session_cb == session control block to use
*   obj == obj template to use
*   retres == address of return status
*
* RETURNS:
*    malloced <config> element with N random instances of
*    the specified top-level object
*********************************************************************/
val_value_t *
    autotest_create_object_payload (server_cb_t *server_cb,
                                    session_cb_t *session_cb,
                                    obj_template_t *obj,
                                    status_t *retres)
{
    assert(server_cb && "server_cb is NULL!");
    assert(session_cb && "session_cb is NULL!");
    assert(obj && "obj is NULL!");
    assert(retres && "retres is NULL!");

    val_value_t *rootval = val_make_config_root();
    if (rootval == NULL) {
        *retres = ERR_INTERNAL_MEM;
        return NULL;
    }

    autotest_cb_t *autotest_cb = session_cb->autotest_cb;
    if (autotest_cb->target_val == autotest_cb->editroot) {
        *retres = auto_create_object(server_cb, session_cb, obj, rootval);
    } else {
        val_value_t *insert_val = autotest_cb->target_val->parent;
        val_value_t *save_val = autotest_cb->target_val;
        val_remove_child(save_val);
        val_value_t *new_val = val_clone(autotest_cb->editroot);
        val_add_child(save_val, insert_val);
        if (new_val == NULL) {
            *retres = ERR_INTERNAL_MEM;
            return NULL;
        }
        val_add_child(new_val, rootval);
        insert_val = save_val = val_get_first_child(new_val);
        if (insert_val) {
            while (save_val) {
                save_val = val_get_first_child(save_val);
                if (save_val) {
                    insert_val = save_val;
                }
            }
        } else {
            insert_val = new_val;
        }
        *retres = auto_create_object(server_cb, session_cb, obj, insert_val);
    }

    if (*retres != NO_ERR) {
        val_free_value(rootval);
        rootval = NULL;
    }
    return rootval;

}  /* autotest_create_object_payload */


/********************************************************************
 * FUNCTION do_auto_test (local RPC)
 * 
 * auto-test
 *      session-name=named session to use (current session if NULL)
 *      count=N         (entries per create attempt)
 *      iterations=N    (number of commit iterations)
 *      target=/path/to/object (mandatory target object to create)
 *      
 * Run editing tests on the specified config path object
 *
 * INPUTS:
 *    server_cb == server control block to use
 *    rpc == RPC method for the autotest command
 *    line == CLI input in progress
 *    len == offset into line buffer to start parsing
 *
 * RETURNS:
 *   status
 *********************************************************************/
status_t
    do_auto_test (server_cb_t *server_cb,
                  obj_template_t *rpc,
                  const xmlChar *line,
                  uint32  len)
{
    status_t res = NO_ERR;
    //boolean imode = interactive_mode();
    val_value_t *valset = get_valset(server_cb, rpc, &line[len], &res);
    if (valset == NULL) {
        return res;
    }

    const xmlChar *target_str = NULL;
    obj_template_t *target_obj = NULL;
    val_value_t *target_val = NULL;
    val_value_t *root = NULL;
    val_value_t *parm = val_find_child(valset, YANGCLI_MOD, NCX_EL_TARGET);
    if (parm) {
        target_str = VAL_STR(parm);
        root = get_instanceid_parm(target_str, TRUE, TRUE,
                                   &target_obj, &target_val, &res);
    } else {
        return ERR_NCX_MISSING_PARM;
    }

    session_cb_t *session_cb = server_cb->cur_session_cb;
    const xmlChar *session_name = NULL;
    uint32 count = 1;
    uint32 iterations = 1;

    if (res == NO_ERR) {
        parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_SESSION_NAME);
        if (parm) {
            session_name = VAL_STR(parm);
            session_cb = find_session_cb(server_cb, session_name);
        }
        if (session_cb == NULL) {
            res = ERR_NCX_DEF_NOT_FOUND;
            log_error("\nError: session '%s' not found", session_name);
        } else if (!session_connected(session_cb)) {
            res = ERR_NCX_OPERATION_FAILED;
            log_error("\nError: session '%s' not connected", session_name);
        }
    }

    if (res == NO_ERR) {
        parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_COUNT);
        if (parm) {
            count = VAL_UINT32(parm);
        }
        parm = val_find_child(valset, YANGCLI_MOD, YANGCLI_ITERATIONS);
        if (parm) {
            iterations = VAL_UINT32(parm);
        }

        /* pass off root live memory here */
        res = start_auto_test(server_cb, session_cb, root, target_val,
                              target_obj, count, iterations);
    }

    val_free_value(valset);

    if (res != NO_ERR) {
        log_error("\nError: auto-test failed (%s)\n",
                  get_error_string(res));
    }

    return res;

}  /* do_auto_test */


/********************************************************************
* FUNCTION autotest_handle_rpc_reply
* 
* Handle the current <edit-comfig>, <commit>, or <copy-config<>
* response from the server and either start the next iteration
* or finish autotest mode
*
* INPUTS:
*   server_cb == server session control block to use
*   session_cb == session control block to use
*   scb == session control block to use
*   reply == data node from the <rpc-reply> PDU
*   anyerrors == TRUE if <rpc-error> detected instead of <data>
*             == FALSE if no <rpc-error> elements detected
*
* RETURNS:
*    status
*********************************************************************/
status_t
    autotest_handle_rpc_reply (server_cb_t *server_cb,
                               session_cb_t *session_cb,
                               val_value_t *reply,
                               boolean anyerrors)
{
    assert(server_cb && "server_cb is NULL!");
    assert(reply && "reply is NULL!");

    status_t res = NO_ERR;
    autotest_cb_t *autotest_cb = session_cb->autotest_cb;

    if (anyerrors) {
        if (LOGINFO) {
            val_dump_value_max(reply, 0, session_cb->defindent,
                               DUMP_VAL_LOG, session_cb->display_mode,
                               FALSE, FALSE);
        }
        res = ERR_NCX_OPERATION_FAILED;
    } else {
        switch (autotest_cb->state) {
        case AUTOTEST_ST_WAIT_EDIT:
            res = do_save_ex(server_cb, session_cb);
            if (res == NO_ERR) {
                autotest_cb->state = AUTOTEST_ST_WAIT_SAVE;
            }
            if (res == ERR_NCX_SKIPPED) {
                res = NO_ERR;
            } else {
                break;
            }
            // if skipped then fall through
        case AUTOTEST_ST_WAIT_SAVE:
            if (autotest_cb->cur_iteration == autotest_cb->iterations) {
                res = finish_save(server_cb, session_cb);
                finish_auto_test(session_cb, autotest_cb);
            } else {
                res = send_auto_test_edit(server_cb, session_cb);
            }
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
    }


    if (res != NO_ERR) {
        autotest_cancel(session_cb);
    }

    return res;

}  /* autotest_handle_rpc_reply */


/********************************************************************
* FUNCTION autotest_cancel
*
* Stop the autotest with an error
*
* INPUTS:
*   session_cb == session control block to use
*   autotest_cb == autotest control block to use
*********************************************************************/
void
    autotest_cancel (session_cb_t *session_cb)
{
    assert(session_cb);

    autotest_cb_t *autotest_cb = session_cb->autotest_cb;
    assert(autotest_cb);

    session_cb->command_mode = CMD_MODE_NORMAL;
    val_free_value(autotest_cb->editroot);
    autotest_cb->editroot = NULL;
    autotest_cb->state = AUTOTEST_ST_NONE;

} /* autotest_cancel */


/* END yangcli_autotest.c */
