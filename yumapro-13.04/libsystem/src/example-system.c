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
/*  FILE: example-system.c

  Example External System Library

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/

#include <xmlstring.h>

#include "procdefs.h"
#include "agt.h"
#include "agt_acm.h"
#include "agt_acm_extern.h"
#include "agt_util.h"
#include "dlq.h"
#include "example-system.h"
#include "log.h"
#include "log_vendor.h"
#include "log_vendor_extern.h"
#include "ncx.h"
#include "ncxtypes.h"
#include "obj.h"
#include "ses.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "xml_util.h"



/****************  Example External NACM Hooks *******************/


/********************************************************************
* FUNCTION acm_extern_rpc
*
* Check if the specified user is allowed to invoke an RPC
* 
* INPUTS:
*   msg == XML header in incoming message in progress
*   user == user name string
*   rpcobj == obj_template_t for the RPC method to check
*
* RETURNS:
*   TRUE if user allowed invoke this RPC; FALSE otherwise
*********************************************************************/
static boolean 
    acm_extern_rpc (xml_msg_hdr_t *msg,
                    const xmlChar *user,
                    const obj_template_t *rpcobj)
{
    (void)msg;
    (void)user;
    (void)rpcobj;
    log_debug("\nacm_extern_rpc: return OK\n");
    return TRUE;
}


/********************************************************************
* FUNCTION acm_extern_notif
*
* Check if the specified user is allowed to receive
* a notification event
* 
* INPUTS:
*   user == user name string
*   notifobj == obj_template_t for the notification event to check
*
* RETURNS:
*   TRUE if user allowed receive this notification event;
*   FALSE otherwise
*********************************************************************/
static boolean 
    acm_extern_notif (const xmlChar *user,
                      const obj_template_t *notifobj)
{
    (void)user;
    (void)notifobj;
    log_debug("\nacm_extern_notif: return OK\n");
    return TRUE;
}


/********************************************************************
* FUNCTION acm_extern_write_fn
*
* Check if the specified user is allowed to access a value node
* The val->obj template will be checked against the val->editop
* requested access and the user's configured max-access
* 
* INPUTS:
*   msg == XML header from incoming message in progress
*   newval  == val_value_t in progress to check
*                (may be NULL, if curval set)
*   curval  == val_value_t in progress to check
*                (may be NULL, if newval set)
*   val  == val_value_t in progress to check
*   editop == requested CRUD operation
*
* RETURNS:
*   TRUE if user allowed this level of access to the value node
*********************************************************************/
static boolean 
    acm_extern_write (xml_msg_hdr_t *msg,
                      const xmlChar *user,
                      const val_value_t *newval,
                      const val_value_t *curval,
                      op_editop_t editop)
{
    (void)msg;
    (void)user;
    (void)newval;
    (void)curval;
    (void)editop;
    log_debug("\nacm_extern_write: return OK\n");
    return TRUE;
}


/********************************************************************
* FUNCTION acm_extern_read_fn
*
* Check if the specified user is allowed to read a value node
* 
* INPUTS:
*   msg == XML header from incoming message in progress
*   user == user name string
*   val  == val_value_t in progress to check
*
* RETURNS:
*   TRUE if user allowed read access to the value node
*********************************************************************/
static boolean 
    acm_extern_read (xml_msg_hdr_t *msg,
                     const xmlChar *user,
                     const val_value_t *val)
{
    (void)msg;
    (void)user;
    (void)val;
    log_debug("\nacm_extern_read: return OK\n");
    return TRUE;
}


/********************************************************************
* FUNCTION log_vendor_init1_fn
*
* Vendor init function to support log output (optional)
*
*********************************************************************/
static void
    log_vendor_init1_fn ( boolean pre_cli )
{
    // Action TBD. This function is available for required vendor init.
    if (pre_cli) {
      ;
    } else {
      ;
    }
}

/********************************************************************
* FUNCTION log_vendor_send_fn
*
* Vendor function to consume log output (mandatory)
* 
* INPUTS:
*   app == "Facility" (Yangcli or Netconfd)
*   level == Logging level (error, warn, info, debug, ..., debug4)
*   fstr == format string (like printf)
*   args == variable argument list
*
* RETURNS:
*   void
*********************************************************************/
static void
    log_vendor_send_fn (log_debug_app_t app, log_debug_t level,
			const char *fstr, va_list args)
{
    (void)app;
    (void)level;
    (void)fstr;
    (void)args;
    return;
}

/****************  Required System Library Hooks *******************/

/* system init server profile callback
 *
 * Initialize the server profile if needed
 *
 * INPUTS:
 *  profile == server profile to change if needed
 */
void yp_system_init_profile (agt_profile_t *profile)
{
    (void) profile;
    log_debug("\nyp_system init profile\n");
    /* example: use an external ACM module */
    //profile->agt_acm_model = AGT_ACM_MODEL_EXTERNAL;
}


/* system init1 callback
 * init1 system call
 * this callback is invoked twice; before and after CLI processing
 * INPUTS:
 * pre_cli == TRUE if this call is before the CLI parameters
 *            have been read
 *            FALSE if this call is after the CLI parameters
 *            have been read
 * RETURNS:
 *  status; error will abort startup
 */
status_t yp_system_init1 (boolean pre_cli)
{
    log_debug("\nyp_system init1\n");

    if (pre_cli) {
	// example -- register vendor callback to consume logging output.
        // Note that output will not be re-directed to the vendor stream
        // until AFTER --log-vendor is parsed by CLI processing or log_vendor
        // is parsed by config file processing.
        log_vendor_init1_fn(pre_cli); /* Optional */
	log_vendor_extern_register_send_fn(log_vendor_send_fn);
    } else {
        // example -- external NACM callbacks
        // load module for external module
        // with ncxmod_load_module

        // register the external ACM callbacks
        // this will have no affect unless the
        // yp_system_init_profile fn sets the
        // agt_acm_model to AGT_ACM_MODEL_EXTERNAL
        agt_acm_extern_register_callbacks(acm_extern_rpc,
                                          acm_extern_notif,
                                          acm_extern_write,
                                          acm_extern_read);
	// example -- register vendor callback to consume logging output.
        log_vendor_init1_fn(pre_cli); /* Optional */
    }
    return NO_ERR;
}


/* system init2 callback
 * init2 system call
 * this callback is invoked twice; before and after 
 * load_running_config processing
 *
 * INPUTS:
 * pre_load == TRUE if this call is before the running config
 *            has been loaded
 *            FALSE if this call is after the running config
 *            has been loaded
 * RETURNS:
 *  status; error will abort startup
 */

status_t yp_system_init2 (boolean pre_load)
{
    log_debug("\nyp_system init2\n");

    if (pre_load) {
        ;
    } else {
        ;
    }
    return NO_ERR;
}


/* system cleanup callback
 * this callback is invoked once during agt_cleanup
 */
void yp_system_cleanup (void)
{
    log_debug("\nyp_system cleanup\n");
}


/* END example-system.c */
