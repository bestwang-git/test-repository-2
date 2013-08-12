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
/*  FILE: log_vendor_extern.c

Vendor specifc registration API.

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
08jan06      abb      begun, borrowed from openssh code
02jun12      mts      adapted from log.c
17oct12      mts      cloned from log_vendor.c

         1         2         3         4         5         6         7         8
12345678901234567890123456789012345678901234567890123456789012345678901234567890

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <syslog.h>
#include <memory.h>

#include "procdefs.h"
#include "log.h"
#include "log_util.h"
#include "log_syslog.h"
#include "log_vendor.h"
#include "log_vendor_extern.h"
#include "status.h"
#include "tstamp.h"
#include "xml_util.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

/********************************************************************
*                                                                   *
*               T Y P E   D E C L A R A T I O N S                   *
*                                                                   *
*********************************************************************/

/********************************************************************
*                                                                   *
*                         V A R I A B L E S                         *
*                                                                   *
*********************************************************************/

/* Vendor external API callback vector: */
logfn_vendor_send_t    logfn_vendor_send    = NULL;


/********************************************************************
*                                                                   *
*                         F U N C T I O N S                         *
*                                                                   *
*********************************************************************/

/********************************************************************
* FUNCTION log_vendor_extern_register_send_fn
*
*   Register vendor function to accept YumaPro log output stream.
*
*       ***** THIS IS THE VENDOR API FUNCTION *****
*
* This is the vendor API registration function for receiving log messages
* from YumaPro. Ideally, this should be called very early in the YumaPro
* initialization process in order not to miss log messages issued during
* initialization (e.g. before bootstrap_cli() in ncx_init()). (Note that at
* the present time this is not possible as yp_system symbol resolution is
* not performed until agt_init1() ...
*
*********************************************************************/
void
    log_vendor_extern_register_send_fn (logfn_vendor_send_t new_vendor_logfn)
{
    boolean logfn_vendor_send_was_set;
    boolean logfn_vendor_send_was_syslog;
    boolean logfn_vendor_send_was_vendor;
    boolean new_vendor_logfn_is_syslog;
    boolean new_vendor_logfn_is_vendor;
    boolean new_vendor_logfn_is_vendor_test;
    log_debug_t log_level;

    if (!new_vendor_logfn) {
        return;
    }

    /* No need to replace vendor fn vector with same */
    if (logfn_vendor_send == new_vendor_logfn) {
        return;
    }

    /*
     * At this point we know that we have a NEW, non-NULL vector.
     *
     * Set up a couple of "simple, obvious" booleans to make the logic
     * that follows easier to understand. This function is invoked rarely
     * so performance is NBD.
     */
    new_vendor_logfn_is_syslog =
      (new_vendor_logfn == &log_vendor_send_to_syslog);

    new_vendor_logfn_is_vendor = !new_vendor_logfn_is_syslog;
    new_vendor_logfn_is_vendor_test =
      (new_vendor_logfn == &log_vendor_test_send_to_syslog);


    logfn_vendor_send_was_set = (logfn_vendor_send != NULL);

    logfn_vendor_send_was_syslog =
      (logfn_vendor_send_was_set &&
       (logfn_vendor_send == &log_vendor_send_to_syslog));

    logfn_vendor_send_was_vendor =
      (logfn_vendor_send_was_set && !logfn_vendor_send_was_syslog);

    /*
     * By default, log_vendor init code will initialize this vector
     * internally to direct it's output to syslog. This allows testing of
     * the log_vendor param in the absence of a registered vendor specific
     * callback handler.
     *
     * HOWEVER, if a vendor target function IS ALREADY PRESENT, do NOT
     * allow the internal default behavior to replace it.
     */
    if (logfn_vendor_send_was_set && new_vendor_logfn_is_syslog) {
        /*
	 * Fail silently ... this is "normal", in the presence of
	 * a vendor having registered their callback function early
	 * in the initialization sequence.
	 */
        return;
    }

    /* Set the vendor callback now, prior to the logging calls below */
    logfn_vendor_send = new_vendor_logfn;

    /*
     * DO ALLOW replacement of the internal syslog stream call by vendor
     * specified routine, even if it's coming in later than expected.
     */
    if (logfn_vendor_send_was_set && logfn_vendor_send_was_syslog) {
        log_debug("\nSyslog -> vendor log callback registration (%p)",
		  new_vendor_logfn);
    }

    /*
     * If vendor is replacing one vendor registered callback with
     * a different one, allow it, but issue a warning, just in
     * case they are confused.
     */
    if (logfn_vendor_send_was_vendor) {
        log_warn("\nReplacing vendor log callback registration (%p)",
		 logfn_vendor_send);
    }

#ifdef DEBUG
    log_level = new_vendor_logfn_is_syslog ? LOG_DEBUG_DEBUG : LOG_DEBUG_INFO;
#else
    log_level = LOG_DEBUG_DEBUG;
#endif

    log_write_level(log_level, "\nRegister '%s' logging callback API (%p)",
		    new_vendor_logfn_is_syslog ? "syslog" : "vendor",
		    new_vendor_logfn);

    /*
     * Make sure syslog stream is closed down if default syslog vector
     * is being replaced.
    */
    if (logfn_vendor_send_was_syslog &&
	new_vendor_logfn_is_vendor && !new_vendor_logfn_is_vendor_test) {
        log_debug("\nLogger closing syslog stream ...");
        log_syslog_cleanup();
    }

}

/* END file log_vendor_extern.c */
