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
/*  FILE: send_buff.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
20jan07      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include  "procdefs.h"
#include  "send_buff.h"
#include  "status.h"


/********************************************************************
* FUNCTION errno_to_status_copy
*
* Get the errno variable and convert it to a status_t
* COPIED HERE FROM status.c TO MINIMIZE SUBSYSTEM SIZE
*
* INPUTS:
*   none; must be called just after error occurred to prevent
*        the errno variable from being overwritten by a new operation
*
* RETURNS:
*     status_t for the errno enum
*********************************************************************/
static status_t
    errno_to_status_copy (void)
{
    switch (errno) {
    case EACCES:
        return ERR_NCX_ACCESS_DENIED;
    case EAFNOSUPPORT:
    case EPROTONOSUPPORT:
        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    case EINVAL:
        return ERR_NCX_INVALID_VALUE;
    case EMFILE:
        return ERR_NCX_OPERATION_FAILED;
    case ENFILE:
        return ERR_NCX_RESOURCE_DENIED;
    case ENOBUFS:
    case ENOMEM:
        return ERR_INTERNAL_MEM;
    default:
        return ERR_NCX_OPERATION_FAILED;
    }

} /* errno_to_status_copy */


/********************************************************************
* FUNCTION send_buff
*
* Send the buffer to the ncxserver
*
* This function is used by applications which do not
* select for write_fds, and may not block (if fnctl used)
* 
* INPUTS:
*   fd == the socket to write to
*   buffer == the buffer to write
*   cnt == the number of bytes to write
*
* RETURNS:
*   status
*********************************************************************/
status_t
    send_buff (int fd,
               const char *buffer, 
               size_t cnt)
{
    size_t sent, left;
    ssize_t  retsiz;
    uint32   retry_cnt;

    retry_cnt = 1000;
    sent = 0;
    left = cnt;
    
    while (sent < cnt) {
        retsiz = write(fd, buffer, left);
        if (retsiz < 0) {
            switch (errno) {
            case EAGAIN:
            case EBUSY:
                if (--retry_cnt) {
                    break;
                } /* else fall through */
            default:
                return errno_to_status_copy();
            }
        } else {
            sent += (size_t)retsiz;
            buffer += retsiz;
            left -= (size_t)retsiz;
        }
    }

    return NO_ERR;

} /* send_buff */

/* END file send_buff.c */
