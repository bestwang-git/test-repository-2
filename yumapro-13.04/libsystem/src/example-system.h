#ifndef _H_example_system
#define _H_example_system
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

#ifndef _H_agt
#include "agt.h"
#endif

#ifndef _H_status
#include "status.h"
#endif


/* system init server profile callback
 *
 * Initialize the server profile if needed
 *
 * INPUTS:
 *  profile == server profile to change if needed
 */
extern void yp_system_init_profile (agt_profile_t *profile);


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
extern status_t yp_system_init1 (boolean pre_cli);


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
extern status_t yp_system_init2 (boolean pre_load);


/* system cleanup callback
 * this callback is invoked once during agt_cleanup
 */
extern void yp_system_cleanup (void);


#endif
