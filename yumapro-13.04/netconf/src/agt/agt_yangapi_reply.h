/*
 * Copyright (c) 2012 - 2013, YumaWorks, Inc., All Rights Reserved.
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.    
 */
#ifdef WITH_YANGAPI
#ifndef _H_agt_yangapi_reply
#define _H_agt_yangapi_reply
/*  FILE: agt_yangapi_reply.h
*********************************************************************
*                                                                   *
*                         P U R P O S E                             *
*                                                                   *
*********************************************************************

    YANG-API Reply Handler

*********************************************************************
*                                                                   *
*                   C H A N G E         H I S T O R Y               *
*                                                                   *
*********************************************************************

date             init     comment
----------------------------------------------------------------------
17-mar-13    abb      Begun.
*/

#ifndef _H_rpc
#include "rpc.h"
#endif

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
*                            T Y P E S                              *
*                                                                   *
*********************************************************************/

				      
/********************************************************************
*                                                                   *
*                        F U N C T I O N S                          *
*                                                                   *
*********************************************************************/

/********************************************************************
* FUNCTION agt_yangapi_reply_send
*
* Operation succeeded or failed
* Return a REST API HTTP message
* 
* INPUTS:
*   scb == session control block
*   rcb == yangapi control block to use
*   msg == rpc_msg_t in progress
*   res == general request status
* RETURNS:
*   status
*********************************************************************/
extern status_t
    agt_yangapi_reply_send (ses_cb_t *scb,
                            yangapi_cb_t *rcb,
                            rpc_msg_t *msg,
                            status_t res);



#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif            /* _H_agt_yangapi_reply */
#endif  // WITH_YANGAPI
