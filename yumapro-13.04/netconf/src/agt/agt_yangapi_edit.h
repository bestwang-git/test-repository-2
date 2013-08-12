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
#ifndef _H_agt_yangapi_edit
#define _H_agt_yangapi_edit
/*  FILE: agt_yangapi_edit.h
*********************************************************************
*                                                                   *
*                         P U R P O S E                             *
*                                                                   *
*********************************************************************

    Yuma REST API Edit Handler

*********************************************************************
*                                                                   *
*                   C H A N G E         H I S T O R Y               *
*                                                                   *
*********************************************************************

date             init     comment
----------------------------------------------------------------------
17-mar-13    abb      Begun.; split from agt_yangapi.c
*/

#ifndef _H_ses
#include "ses.h"
#endif

#ifndef _H_rpc
#include "rpc.h"
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
* FUNCTION agt_yangapi_edit_request
*
* Perform an internal <edit-config> and <commit> if needed
* Also write result to NV-storage if separate :startup config
*
* INPUTS:
*   scb == session to use
*   rcb == yangapi control block to use
*   msg == response message in progress
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    agt_yangapi_edit_request (ses_cb_t *scb,
                              yangapi_cb_t *rcb,
                              rpc_msg_t *msg);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif            /* _H_agt_yangapi */
#endif  // WITH_YANGAPI
