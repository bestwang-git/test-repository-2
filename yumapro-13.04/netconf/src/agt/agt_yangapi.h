/*
 * Copyright (c) 2012, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
#ifdef WITH_YANGAPI
#ifndef _H_agt_yangapi
#define _H_agt_yangapi
/*  FILE: agt_yangapi.h
*********************************************************************
*                                                                   *
*                         P U R P O S E                             *
*                                                                   *
*********************************************************************

    Yuma REST API Message Handler

*********************************************************************
*                                                                   *
*                   C H A N G E         H I S T O R Y               *
*                                                                   *
*********************************************************************

date             init     comment
----------------------------------------------------------------------
10-apr-12    abb      Begun.
*/

#ifndef _H_ses
#include "ses.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_yangapi
#include "yangapi.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#define YANG_API_MODULE  NCXMOD_YANG_API
#define YANG_API_REVISION NULL
#define YANG_API_ROOT    (const xmlChar *)"yang-api"
#define YANG_API_VERSION (const xmlChar *)"1.0"
#define YANGAPI_WRAPPER_NODE NCX_EL_DATA


/********************************************************************
*                                                                   *
*                            T Y P E S                              *
*                                                                   *
*********************************************************************/


/* module context node with cached schema tree and API template
 * for retrieval (root)
 */
typedef struct agt_yangapi_context_t_ {
    ncx_module_t     *mod;          // yang-api module
    obj_template_t   *obj;          // /yang-api container
    val_value_t      *root;         // template for retrieval
    uint32            cap_changes;  // not used yet
    boolean           enabled;
    xmlns_id_t        mod_nsid;
} agt_yangapi_context_t;

				      
/********************************************************************
*                                                                   *
*                        F U N C T I O N S                          *
*                                                                   *
*********************************************************************/

/********************************************************************
* FUNCTION agt_yangapi_init
*
* Initialize the agt_yangapi module
*
* RETURNS:
*   status
*********************************************************************/
extern status_t 
    agt_yangapi_init (void);


/********************************************************************
* FUNCTION agt_yangapi_cleanup
*
* Cleanup the agt_yangapi module.
*
*********************************************************************/
extern void 
    agt_yangapi_cleanup (void);


/********************************************************************
* FUNCTION agt_yangapi_dispatch
*
* Dispatch an incoming Yuma REST API request
*
* INPUTS:
*   scb == session control block
*********************************************************************/
extern void
    agt_yangapi_dispatch (ses_cb_t  *scb);


/********************************************************************
* FUNCTION agt_yangapi_get_nsid
*
* Get the YANG-API XML namespace ID
*
* RETURNS:
*  YANG-API namespace ID
*********************************************************************/
extern xmlns_id_t
    agt_yangapi_get_nsid (void);


/********************************************************************
* FUNCTION agt_yangapi_get_context
*
* Get the YANG-API Context structure
*
* RETURNS:
*  YANG-API namespace ID
*********************************************************************/
extern agt_yangapi_context_t *
    agt_yangapi_get_context (void);


/********************************************************************
* FUNCTION agt_yangapi_get_config_parm
*
* Get the value of the config parm to use
*
* INPUTS:
*   rcb == yangapi control block to use
*   context_node == XPath context node to check
* RETURNS:
*    TRUE if config=true or FALSE if config=false
*********************************************************************/
extern boolean
    agt_yangapi_get_config_parm (yangapi_cb_t *rcb,
                                 val_value_t *context_node);


/********************************************************************
* FUNCTION agt_yangapi_method_is_read
*
* Check if this is a read method
*
* INPUTS:
*   method == enum to check
*
* RETURNS:
*   TRUE if some sort of read or FALSE if some sort of write
*********************************************************************/
extern boolean
    agt_yangapi_method_is_read (yangapi_method_t method);


/********************************************************************
* FUNCTION agt_yangapi_record_error
*
* Record an rpc-error for YANG-API response translation
* 
* INPUTS:
*   scb == session control block to use
*   msg == rpc_msg_t to use
*   res == error status code
*   errnode == node in API tree or data tree associated with the error
*   badval == error-info string to use
*********************************************************************/
extern void
    agt_yangapi_record_error (ses_cb_t *scb,
                              rpc_msg_t *msg,
                              status_t res,
                              val_value_t *errnode,
                              const xmlChar *badval);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif            /* _H_agt_yangapi */
#endif  // WITH_YANGAPI
