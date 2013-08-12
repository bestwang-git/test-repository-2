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
#ifndef _H_yangcli_notif
#define _H_yangcli_notif

/*  FILE: yangcli_notif.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

   NETCONF YANG-based CLI Tool

   notification handler
 
*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
07-dec-12    abb      Begun

*/

#include <xmlstring.h>

#ifndef _H_obj
#include "obj.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_yangcli
#include "yangcli.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/********************************************************************
*								    *
*			     T Y P E S				    *
*								    *
*********************************************************************/


/********************************************************************
 * FUNCTION TEMPLATE yangcli_notif_cbfn_t
 * 
 * callback function for a notification event handler
 *
 * INPUTS:
 *   session_cb == session control block that received the notif
 *   modname == module defining the notification
 *   event_name == notification event name
 *   event_time == notification timestamp
 *   event == pointer to value 
 *
 *********************************************************************/

typedef void
    (*yangcli_notif_cbfn_t) (session_cb_t *session_cb,
                             const xmlChar *modname,
                             const xmlChar *event_name,
                             const xmlChar *event_time,
                             val_value_t *event);

                                          
/********************************************************************
*								    *
*		      F U N C T I O N S 			    *
*								    *
*********************************************************************/

/********************************************************************
 * FUNCTION yangcli_notification_handler
 * 
 * matches callback template mgr_not_cbfn_t
 *
 * INPUTS:
 *   scb == session receiving RPC reply
 *   msg == notification msg that was parsed
 *   consumed == address of return consumed message flag
 *
 *  OUTPUTS:
 *     *consumed == TRUE if msg has been consumed so
 *                  it will not be freed by mgr_not_dispatch
 *               == FALSE if msg has been not consumed so
 *                  it will be freed by mgr_not_dispatch
 *********************************************************************/
extern void
    yangcli_notification_handler (ses_cb_t *scb,
                                  mgr_not_msg_t *msg,
                                  boolean *consumed);


/********************************************************************
 * FUNCTION register_notif_event_handler
 * 
 * Register an event callback function
 *
 * INPUTS:
 *   modname == module defining the notification
 *   event == notification event name
 *   cbfn == callback function to register
 *
 * RETURNS:
 *   status: error if duplicate; only 1 handler per event type
 *           allowed at this time
 *********************************************************************/
extern status_t
    register_notif_event_handler (const xmlChar *modname,
                                  const xmlChar *event,
                                  yangcli_notif_cbfn_t cbfn);


/********************************************************************
 * FUNCTION unregister_notif_event_handler
 * 
 * Unregister an event callback function
 *
 * INPUTS:
 *   modname == module defining the notification
 *   event == notification event name
 *   cbfn == callback function to unregister
 *
 *********************************************************************/
extern void
    unregister_notif_event_handler (const xmlChar *modname,
                                    const xmlChar *event,
                                    yangcli_notif_cbfn_t cbfn);


/********************************************************************
 * FUNCTION yangcli_notif_init
 * 
 * Init this module
 *
 *********************************************************************/
extern void
    yangcli_notif_init (void);


/********************************************************************
 * FUNCTION yangcli_notif_cleanup
 * 
 * Cleanup this module
 *
 *********************************************************************/
extern void
    yangcli_notif_cleanup (void);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_yangcli_notif */
