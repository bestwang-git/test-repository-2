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
/*  FILE: netconf-subsystem.c

   netconf-subsystem     : default usage for SSH
   netconf-subsystem -f file | -filename file  : set the trace file
   netconf-subsystem -t level | -trace level  : set the trace level
   netconf-subsystem -p proto | -protocol proto  : set the protocol

*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
14-jan-07    abb      begun;
03-mar-11    abb      get rid of usleeps and replace with
                      design that checks for EAGAIN

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#define _C_main 1

#include "procdefs.h"
#include "subsystem.h"


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
int main (int argc, char **argv)
{
    int ret = run_subsystem(PROTO_ID_NETCONF, 0, argc, argv);

    return ret;
} /* main */

