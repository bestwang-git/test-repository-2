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
/*  FILE: yangcli-pro.c

   NETCONF YANG-based CLI Tool

   See ./README for details

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
01-jun-08    abb      begun; started from ncxcli.c
15-oct-12    abb      split out to library to support yp-shell

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* #define MEMORY_DEBUG 1 */

#ifdef MEMORY_DEBUG
#include <mcheck.h>
#endif

#define _C_main 1

#include "procdefs.h"
#include "log.h"
#include "mgr.h"
#include "mgr_io.h"
#include "ncx.h"
#include "ncxconst.h"
#include "obj.h"
#include "status.h"
#include "yangcli.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/
#ifdef DEBUG
#define YANGCLI_PRO_DEBUG   1
#endif


/********************************************************************
*                                                                   *
*                          T Y P E S                                *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*             F O R W A R D   D E C L A R A T I O N S               *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/


/********************************************************************
*                                                                   *
*                       FUNCTION main                               *
*                                                                   *
*********************************************************************/
int main (int argc, char *argv[])
{

#ifdef MEMORY_DEBUG
    mtrace();
#endif

    boolean quickexit = FALSE;
    status_t res = yangcli_init(argc, argv, PROG_MODE_CLIENT, &quickexit);
    if (res != NO_ERR) {
        log_error("\nyangcli-pro: init returned error (%s)\n", 
                  get_error_string(res));
    } else if (!quickexit) {
        /* normal run mode */
        res = mgr_io_run();
        if (res != NO_ERR) {
            log_error("\nmgr_io failed (%d)\n", res);
        } else {
            log_write("\n");
        }
    }

    print_errors();

    print_error_count();

    yangcli_cleanup();

    print_error_count();

#ifdef MEMORY_DEBUG
    muntrace();
#endif

    return 0;

} /* main */

/* END yangcli-pro.c */
