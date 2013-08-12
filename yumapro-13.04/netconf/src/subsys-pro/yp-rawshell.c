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
/*  FILE: yp-rawshell.c

   yp-rawshell  : default usage for SSH

   *** NOT USED IN SERVER AT THIS TIME ***

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
07-oct-12   abb      begun; split from netconf_subsystem.c


*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/

#define _C_main 1

#include "subsystem.h"

#ifdef DEBUG
#define YP_RAWSHELL_DEBUG 1
#endif

/********************************************************************
* FUNCTION main
*
* STDIN is input from the SSH client (sent to ncxserver)
* STDOUT is output to the SSH client (rcvd from ncxserver)
* 
* RETURNS:
*   0 if NO_ERR
*   1 if error connecting or logging into ncxserver
*********************************************************************/
int main (int argc, char **argv, char **envp)
{
    int trace_level = 0;
#ifdef YP_RAWSHELL_DEBUG
    trace_level = 3;
#endif
    int ret = run_subsystem(PROTO_ID_CLI, trace_level, argc, argv, envp);
    return ret;
} /* main */
