/*
 * Copyright (c) 2012, YumaWorks. Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
/*  FILE: yangapi.c


   Yuma YANG-API definitions

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
11apr12      abb      begun; 

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/

//#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
//#include <memory.h>
//#include <unistd.h>
//#include <errno.h>
//#include <ctype.h>
#include <assert.h>


#include "procdefs.h"
#include "yangapi.h"
#include "xml_util.h"
#include "xpath.h"

/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                           T Y P E S                               *
*                                                                   *
*********************************************************************/
    

/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
* FUNCTION yangapi_new_param
*
* Create a new YANGAPI paramater
*
* INPUTS:
*   parmname == parameter name
*   parmnamelen == parameter name string length
*   parmval == parameter value
*   parmvallen == parameter value string length
* RETURNS:
*   pointer to initialized param, or NULL if malloc error
*********************************************************************/
yangapi_param_t *
    yangapi_new_param (const xmlChar *parmname,
                       uint32 parmnamelen,
                       const xmlChar *parmval,
                       uint32 parmvallen)
{
    yangapi_param_t *param = m__getObj(yangapi_param_t);
    if (param) {
        memset(param, 0x0, sizeof(yangapi_param_t));
        if (parmname) {
            param->name = xml_strndup(parmname, parmnamelen);
            if (!param->name) {
                yangapi_free_param(param);
                return NULL;
            }
            if (parmval) {
                param->value = xml_strndup(parmval, parmvallen);
                if (!param->value) {
                    yangapi_free_param(param);
                    return NULL;
                }
            }
        }
    }
    return param;

}  /* yangapi_new_param */


/********************************************************************
* FUNCTION yangapi_free_param
*
* Free a YANGAPI parameter
*
* INPUTS:
*   param == Yuma REST-API parameter to free
* RETURNS:
*   none
*********************************************************************/
void
    yangapi_free_param (yangapi_param_t *param)
{
    if (!param) {
        return;
    }
    m__free(param->name);
    m__free(param->value);
    m__free(param);

}  /* yangapi_free_param */


/********************************************************************
* FUNCTION yangapi_new_keyval
*
* Create a new YANGAPI keyval holder
*
* INPUTS:
*   keyval == key valuse string
* RETURNS:
*   pointer to initialized keyval, or NULL if malloc error
*********************************************************************/
yangapi_keyval_t *
    yangapi_new_keyval (const xmlChar *keyval)
{
    yangapi_keyval_t *val = m__getObj(yangapi_keyval_t);
    if (val) {
        memset(val, 0x0, sizeof(yangapi_keyval_t));
        val->value = xml_strdup(keyval);
        if (!val->value) {
            yangapi_free_keyval(val);
            return NULL;
        }
    }
    return val;

}  /* yangapi_new_keyval */


/********************************************************************
* FUNCTION yangapi_free_keyval
*
* Free a YANGAPI keyval
*
* INPUTS:
*   param == Yuma REST-API keyval to free
* RETURNS:
*   none
*********************************************************************/
void
    yangapi_free_keyval (yangapi_keyval_t *keyval)
{
    if (!keyval) {
        return;
    }
    m__free(keyval->value);
    m__free(keyval);

}  /* yangapi_free_keyval */


/********************************************************************
* FUNCTION yangapi_clean_keyvalQ
*
* Free all the YANGAPI keyval
*
* INPUTS:
*   rcb == control block to use
* RETURNS:
*   none
*********************************************************************/
void
    yangapi_clean_keyvalQ (yangapi_cb_t *rcb)
{
    while (!dlq_empty(&rcb->keyvalQ)) {
        yangapi_keyval_t *kv = (yangapi_keyval_t *)
            dlq_deque(&rcb->keyvalQ);
        yangapi_free_keyval(kv);
    }
}  /* yangapi_clean_keyvalQ */



/********************************************************************
* FUNCTION yangapi_new_rcb
*
* Create a new YANGAPI control block
*
* INPUTS:
*   none
* RETURNS:
*   pointer to initialized RCB, or NULL if malloc error
*********************************************************************/
yangapi_cb_t *
    yangapi_new_rcb (void)
{
    yangapi_cb_t *rcb = m__getObj(yangapi_cb_t);
    if (rcb) {
        memset(rcb, 0x0, sizeof(yangapi_cb_t));
        dlq_createSQue(&rcb->paramQ);
        dlq_createSQue(&rcb->keyvalQ);
    }
    return rcb;

}  /* yangapi_new_rcb */


/********************************************************************
* FUNCTION yangapi_free_rcb
*
* Free a YANGAPI control block
*
* INPUTS:
*   rcb == Yuma REST-API control block to free
* RETURNS:
*   none
*********************************************************************/
void
    yangapi_free_rcb (yangapi_cb_t *rcb)
{

    if (rcb == NULL) {
        return;
    }

    while (!dlq_empty(&rcb->paramQ)) {
        yangapi_param_t *param = (yangapi_param_t *)dlq_deque(&rcb->paramQ);
        if (param) {
            yangapi_free_param(param);
        }
    }

    yangapi_clean_keyvalQ(rcb);

    m__free(rcb->accept);
    m__free(rcb->request_method);
    m__free(rcb->request_uri);

    xpath_free_pcb(rcb->request_xpath);
    xpath_free_result(rcb->request_xpath_result);

    xpath_free_pcb(rcb->query_select_xpath);
    xpath_free_result(rcb->query_select_xpath_result);

    xpath_free_pcb(rcb->query_test_xpath);
    xpath_free_result(rcb->query_test_xpath_result);

    m__free(rcb->content_type);
    m__free(rcb->content_length);
    m__free(rcb->if_modified_since);
    m__free(rcb->if_unmodified_since);
    m__free(rcb->if_match);
    m__free(rcb->if_none_match);

    m__free(rcb);

}  /* yangapi_free_rcb */


/* END file yangapi.c */
