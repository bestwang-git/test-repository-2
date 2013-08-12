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
#ifndef _H_log_vendor_extern
#define _H_log_vendor_extern
/*  FILE: log_vendor_extern.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    Logging manager vendor API registration

*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
08-jan-06    abb      begun
02-jun-2012  mts      adapted from log.h
17-oct-2012  mts      cloned from log_vendor.h
*/

#include <stdio.h>
#include <xmlstring.h>

#include "procdefs.h"
#include "status.h"
#include "log.h"
#include "log_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*			 C O N S T A N T S			    *
*								    *
*********************************************************************/

/********************************************************************
*                                                                   *
*                            T Y P E S                              *
*                                                                   *
*********************************************************************/

/*
 * This is the vendor API type for receiving log messages from YumaPro.
 * Vendor will create a function of this type and register it via
 *
 *   log_vendor_register_send_fn (logfn_vendor_send_t vendor_logfn)
 *
 * This should be done very early in the YumaPro initialization process
 * in order not to miss log messages issued during the initialization,
 * i.e., before the call on  bootstrap_cli() by ncx_init().
 *
 * When the vendor callback is invoked, the vendor code should translate
 * the YumaPro app param into an "application" and/or "facility" equivalent
 * appropriate to the vendor logging schema. Likewise, it should translate
 * the YumaPro level param into a "message type/level" appropriate to its
 * own requirements.
 */
typedef void (*logfn_vendor_init1_t) ( boolean pre_cli );
typedef void (*logfn_vendor_send_t) ( log_debug_app_t app, log_debug_t level,
				     const char *fstr, va_list args );
typedef void (*logfn_vendor_cleanup_t) ( void );


/********************************************************************
*								    *
*			V A R I A B L E S			    *
*								    *
*********************************************************************/

/* Vendor external API callback vector: */
extern logfn_vendor_send_t    logfn_vendor_send;

/********************************************************************
*								    *
*			F U N C T I O N S			    *
*								    *
*********************************************************************/

/********************************************************************
* FUNCTION log_vendor_extern_register_send_fn
*
*   Register vendor function to accept YumaPro log output stream (mandatory)
*
*       ***** THIS IS THE VENDOR API FUNCTION *****
*
* This is the manditory vendor API registration function for receiving
* log messages from YumaPro. This should be called very early in the
* YumaPro initialization process in order not to miss log messages issued
* during initialization (e.g. before bootstrap_cli() in ncx_init()).
*
*********************************************************************/
extern void
  log_vendor_extern_register_send_fn (logfn_vendor_send_t vendor_logfn);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_log_vendor_extern */
