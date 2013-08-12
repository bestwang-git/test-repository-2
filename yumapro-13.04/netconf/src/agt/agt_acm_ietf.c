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
/*  FILE: agt_acm_ietf.c

object identifiers:


container /nacm
leaf /nacm/enable-nacm
leaf /nacm/read-default
leaf /nacm/write-default
leaf /nacm/exec-default
leaf /nacm/enable-external-groups
leaf /nacm/denied-operations
leaf /nacm/denied-data-writes
leaf /nacm/denied-notifications
container /nacm/groups
list /nacm/groups/group
leaf /nacm/groups/group/name
leaf-list /nacm/groups/group/user-name
list /nacm/rule-list
leaf /nacm/rule-list/name
leaf-list /nacm/rule-list/group
list /nacm/rule-list/rule
leaf /nacm/rule-list/rule/name
leaf /nacm/rule-list/rule/module-name
choice /nacm/rule-list/rule/rule-type
case /nacm/rule-list/rule/rule-type/protocol-operation
leaf /nacm/rule-list/rule/rule-type/protocol-operation/rpc-name
case /nacm/rule-list/rule/rule-type/notification
leaf /nacm/rule-list/rule/rule-type/notification/notification-name
case /nacm/rule-list/rule/rule-type/data-node
leaf /nacm/rule-list/rule/rule-type/data-node/path
leaf /nacm/rule-list/rule/access-operations
leaf /nacm/rule-list/rule/action
leaf /nacm/rule-list/rule/comment


Design Description:

This module caches the contexts of the NACM data model.
The SIL callbacks are used to update the cache.

        nacm --> trigger reinit context cache
        nacm/enable-nacm --> context->acmode
        nacm/read-default -> context->dflags
        nacm/write-default -> context->dflags
        nacm/exec-default -> context->dflags
        nacm/groups/group --> context->groupQ
        nacm/rule-list --> context->rulelistQ

Each session caches the user 2 groups entry for the user
associated with the session.  This is just a backptr into
the queue maintained in the module cache.  The same queue
is used to store these entries for notifications.

        new_cache --> context->usergroupsQ

Each data node contains an array of VAL_MAX_DATARULES back-ptrs
to the rule entry in the cache.  If a data node cannot be
cached in all its instaces, it is backed out, and manual
node validation needs to be done instead for that rule.
The rule->flags FL_ACM_CACHE_FAILED bit is set in this case.

Note that data rules apply only to the instances that exist
when the rule is created.  These instances are saved in an
XPath result nodeset. The nodeset is validated each request
only if the rule datarule cache is invalid.


*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
18jun12      abb      begun; split from agt_acm.c


*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include  <assert.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>

#include "procdefs.h"
#include "agt.h"
#include "agt_acm.h"
#include "agt_acm_ietf.h"
#include "agt_cb.h"
#include "agt_not.h"
#include "agt_ses.h"
#include "agt_util.h"
#include "agt_val.h"
#include "def_reg.h"
#include "dlq.h"
#include "ncx.h"
#include "ncx_num.h"
#include "ncx_list.h"
#include "ncxconst.h"
#include "ncxmod.h"
#include "obj.h"
#include "status.h"
#include "val.h"
#include "val_util.h"
#include "xmlns.h"
#include "xpath.h"
#include "xpath1.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/

#define AGT_ACM_IETF_MODULE      (const xmlChar *)"ietf-netconf-acm"


/* dflags fields for the agt_acm_context_t */
#define FL_ACM_DEFREAD_PERMIT   bit0
#define FL_ACM_DEFWRITE_PERMIT  bit1
#define FL_ACM_DEFEXEC_PERMIT   bit2

#define FL_ACM_CACHE_VALID      bit3

/* flags fields for the agt_acm_rule_t */
#define FL_ACM_OP_CREATE        bit0
#define FL_ACM_OP_READ          bit1
#define FL_ACM_OP_UPDATE        bit2
#define FL_ACM_OP_DELETE        bit3
#define FL_ACM_OP_EXEC          bit4
#define FL_ACM_PERMIT           bit5
#define FL_ACM_ALLMODULES       bit6
#define FL_ACM_ALLTARGET        bit7
#define FL_ACM_CACHE_FAILED     bit8

/* wildcard ops for rule->flags, matches definitions above */
#define FL_ACM_ALLOPS           0x1f

#define nacm_OID_nacm (const xmlChar *)"/nacm"
#define nacm_OID_nacm_enable_nacm (const xmlChar *)"/nacm/enable-nacm"

#define NACM_WILDCARD (const xmlChar *)"*"

#define EXEC_DEFAULT (const xmlChar *)"exec-default"
#define WRITE_DEFAULT (const xmlChar *)"write-default"
#define READ_DEFAULT (const xmlChar *)"read-default"

/* how many times to try to init context after it failed
 * context reinit is done on subsequent requests, not all in a row 
 */
#define MAX_REINIT_FAILS 2


/********************************************************************
*                                                                    *
*                             T Y P E S                              *
*                                                                    *
*********************************************************************/

/* choice rule-liet/rule/rule-type */
typedef enum rule_type_t_ {
    AGT_ACM_RULE_NONE,
    AGT_ACM_RULE_OP,
    AGT_ACM_RULE_NOTIF,
    AGT_ACM_RULE_DATA,
    AGT_ACM_RULE_ALL
} rule_type_t;

/* typedef access-operations-type */
typedef enum access_t_ {
    ACCESS_NONE,
    ACCESS_CREATE,
    ACCESS_READ,
    ACCESS_UPDATE,
    ACCESS_DELETE,
    ACCESS_EXEC
} access_t;


/* 1 group that the user is a member
 * also used for a user name pointer in agt_acm_group_t 
 */
typedef struct agt_acm_name_t_ {
    dlq_hdr_t    qhdr;
    xmlChar     *name;
} agt_acm_name_t;


/* cache list of users that belong to a group */
typedef struct agt_acm_group_t_ {
    dlq_hdr_t    qhdr;
    xmlChar     *groupname;
    dlq_hdr_t    userQ;   /* Q of agt_acm_name_t */
} agt_acm_group_t;


/* cache list of groups that the user is a member */
typedef struct agt_acm_usergroups_t_ {
    dlq_hdr_t         qhdr;
    xmlChar          *username;
    dlq_hdr_t         groupQ;   /* Q of agt_acm_name_t */
    uint32            groupcount;
} agt_acm_usergroups_t;


/* cache for 1 NACM rule entry */
typedef struct agt_acm_rule_t_ {
    dlq_hdr_t        qhdr;
    xmlChar         *name;
    xmlChar         *modulename;  // only if not '*' matchall string
    xmlChar         *targetname;  // rpc-name, notification-name
    xpath_pcb_t     *pcb;         // path for data rule

    // result for each target datastore that may be used (3)
    xpath_result_t  *result[CFG_NUM_STATIC];

    // msgid when the result was saved; 0 == first cache during load
    uint32           result_msgid[CFG_NUM_STATIC];

    rule_type_t      ruletype;
    uint8            flags;       // bits for ops, permit, allmodules,. etc.
} agt_acm_rule_t;


/* cache for 1 NACM rule-list entry */
typedef struct agt_acm_rulelist_t_ {
    dlq_hdr_t           qhdr;
    xmlChar            *name;
    dlq_hdr_t           groupQ;  // Q of agt_acm_name_t 
    dlq_hdr_t           ruleQ;   // Q of agt_acm_rule_t 
    boolean             allgroups;
} agt_acm_rulelist_t;


/* NACM per-session cache control block */
typedef struct agt_acm_cache_t_ {
    agt_acm_usergroups_t *usergroups;    // back-ptr
    boolean isvalid;
} agt_acm_cache_t;


/* 1 access control context */
typedef struct agt_acm_context_t_ {
    ncx_module_t  *nacmmod;
    val_value_t *nacmrootval;  /* /nacm node in the running config */
    dlq_hdr_t rulelistQ;      /* Q of agt_acm_rulelist_t */
    dlq_hdr_t usergroupQ;     /* Q of agt_acm_usergroups_t */
    dlq_hdr_t groupQ;         /* Q of agt_acm_group_t */
    uint32   dflags;          /* default flags for *-default leafs */
    agt_acmode_t  acmode;
    boolean cache_valid;
    boolean log_reads;
    boolean log_writes;
    uint32  reinit_fail_count;
} agt_acm_context_t;


/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/

/* just supporting 1 context at this time */
static agt_acm_context_t acm_context;


#define get_context() (&acm_context)


/********************************************************************
* FUNCTION get_cfgid
*
* Get the correct config ID to use
*
* INPUTS:
*    msg == XML header from incoming message in progress
*
* RETURNS:
*     cfg_ncx_t ID to use
*********************************************************************/
static ncx_cfg_t
    get_cfgid (xml_msg_hdr_t *msg)
{
    ncx_cfg_t cfgid = NCX_CFGID_RUNNING;
    if (msg) {
        boolean isvalid = FALSE;
        cfgid = xml_msg_get_cfgid(msg, &isvalid);
        if (!isvalid) {
            cfgid = NCX_CFGID_RUNNING;
        }
    }
    return cfgid;

}  /* get_cfgid */


/********************************************************************
* FUNCTION check_mode
*
* Check the access-control mode being used
* 
* INPUTS:
*   access == requested access mode
*   obj == object template to check
*
* RETURNS:
*   TRUE if access granted
*   FALSE to check the rules and find out
*********************************************************************/
static boolean 
    check_mode (access_t access,
                const obj_template_t *obj)
{
    switch (agt_acm_get_acmode()) {
    case AGT_ACMOD_ENFORCING:
        /* must check the rules and defaults */
        break;
    case AGT_ACMOD_PERMISSIVE:
        /* must check the rules and defaults for writes;
         * else never allow a nacm:very-secure node to be accessed
         * by default   */
        switch (access) {
        case ACCESS_READ:
        case ACCESS_EXEC:
            return !obj_is_very_secure(obj);
        default:
            break;
        }
        break;
    case AGT_ACMOD_DISABLED:
        /* do not need to check the rules and defaults for writes;
         * else never allow a nacm:very-secure node to be accessed
         * by default   */

        switch (access) {
        case ACCESS_READ:
        case ACCESS_EXEC:
            return !obj_is_very_secure(obj);
        default:
            return !(obj_is_secure(obj) || obj_is_very_secure(obj));
        }
        break;
    case AGT_ACMOD_OFF:
        /* do not check any rules/defaults or any security tagging */
        return TRUE;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
    }
    return FALSE;

}  /* check_mode */


/********************************************************************
* FUNCTION new_name_ptr
*
* create a group or user pointer
*
* INPUTS:
*   name == group or user name string to use
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static agt_acm_name_t *
    new_name_ptr (const xmlChar *name)
{
    agt_acm_name_t *grptr;

    grptr = m__getObj(agt_acm_name_t);
    if (!grptr) {
        return NULL;
    }
    memset(grptr, 0x0, sizeof(agt_acm_name_t));
    grptr->name = xml_strdup(name);
    if (!grptr->name) {
        m__free(grptr);
        return NULL;
    }
    return grptr;

}  /* new_name_ptr */


/********************************************************************
* FUNCTION free_name_ptr
*
* free a group pointer
*
* INPUTS:
*   grptr == group to free
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static void
    free_name_ptr (agt_acm_name_t *grptr)
{
    if (!grptr) {
        return;
    }
    m__free(grptr->name);
    m__free(grptr);

}  /* free_name_ptr */


/********************************************************************
* FUNCTION find_name_ptr
*
* find a group or user pointer
*
* INPUTS:
*   groupQ == Q of agt_acm_name_t structs
*   name == group name to find
*
* RETURNS:
*   pointer to found record or NULL if not found
*********************************************************************/
static agt_acm_name_t *
    find_name_ptr (dlq_hdr_t *groupQ,
                    const xmlChar *name)
{
    agt_acm_name_t   *grptr;

    for (grptr = (agt_acm_name_t *)dlq_firstEntry(groupQ);
         grptr != NULL;
         grptr = (agt_acm_name_t *)dlq_nextEntry(grptr)) {

        if (!xml_strcmp(grptr->name, name)) {
            return grptr;
        }
    }
    return NULL;

}  /* find_name_ptr */


/********************************************************************
* FUNCTION new_usergroups
*
* create a user-to-groups struct
*
* INPUTS:
*   username == name of user to use
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static agt_acm_usergroups_t * new_usergroups (const xmlChar *username)
{
    agt_acm_usergroups_t *usergroups;

    usergroups = m__getObj(agt_acm_usergroups_t);
    if (!usergroups) {
        return NULL;
    }

    memset(usergroups, 0x0, sizeof(agt_acm_usergroups_t));
    dlq_createSQue(&usergroups->groupQ);
    usergroups->username = xml_strdup(username);
    if (!usergroups->username) {
        m__free(usergroups);
        return NULL;
    }

    return usergroups;

}  /* new_usergroups */


/********************************************************************
* FUNCTION free_usergroups
*
* free a user-to-groups struct
*
* INPUTS:
*   usergroups == struct to free
*
*********************************************************************/
static void
    free_usergroups (agt_acm_usergroups_t *usergroups)
{
    agt_acm_name_t *grptr;

    if (!usergroups) {
        return;
    }

    while (!dlq_empty(&usergroups->groupQ)) {
        grptr = (agt_acm_name_t *)
            dlq_deque(&usergroups->groupQ);
        free_name_ptr(grptr);
    }
    m__free(usergroups->username);
    m__free(usergroups);

}  /* free_usergroups */


/********************************************************************
* FUNCTION new_usergroups_entry
*
* create a user-to-groups cache entry
* for the specified username, based on the /nacm/groups
* cache contents at this time
*
* INPUTS:
*   context == context to use
*   username == user name to create mapping for
*
* RETURNS:
*   pointer to malloced usergroups entry for the specified user
*********************************************************************/
static agt_acm_usergroups_t *
    new_usergroups_entry (agt_acm_context_t *context,
                          const xmlChar *username)
{
    agt_acm_usergroups_t *ug = new_usergroups(username);
    if (!ug) {
        return NULL;
    }

    /* check each cached /nacm/groups/group node */
    status_t res = NO_ERR;
    agt_acm_group_t *gu = (agt_acm_group_t *)
        dlq_firstEntry(&context->groupQ);
    for (; gu != NULL && res == NO_ERR;
         gu =  (agt_acm_group_t *)dlq_nextEntry(gu)) {

        if (find_name_ptr(&gu->userQ, username)) {
            /* user is a member of this group */
            agt_acm_name_t *nameptr = new_name_ptr(gu->groupname);
            if (nameptr) {
                dlq_enque(nameptr, &ug->groupQ);
                ug->groupcount++;
            } else {
                res = ERR_INTERNAL_MEM;
            }
        }
    }

    if (res != NO_ERR) {
        log_error("\nError: agt_acm add user2group entry failed");
        free_usergroups(ug);
        return NULL;
    }

    return ug;

}  /* new_usergroups_entry */


/********************************************************************
* FUNCTION find_usergroups_entry
*
* check the context usergroupsQ for the specified user
*
* INPUTS:
*   context == ACM context to use
*   username == user name to create mapping for
*
* RETURNS:
*    const pointer to the usergroups ptr !! do not free !!
*********************************************************************/
static agt_acm_usergroups_t *
    find_usergroups_entry (agt_acm_context_t *context,
                           const xmlChar *username)
{
    /* check each cached usergroups entry */
    agt_acm_usergroups_t *ug;

    for (ug = (agt_acm_usergroups_t *)dlq_firstEntry(&context->usergroupQ);
         ug != NULL;
         ug = (agt_acm_usergroups_t *)dlq_nextEntry(ug)) {
        if (!xml_strcmp(ug->username, username)) {
            return ug;
        }
    }
    return NULL;

}  /* find_usergroups_entry */


/********************************************************************
* FUNCTION get_usergroups_entry
*
* get the user-to-groups cache entry or create one if needed
*
* INPUTS:
* context == context to use
*   username == user name to create mapping for
*
* RETURNS:
*   pointer to usergroups entry for the specified user
*   !! do not free !! held in context->usergroupsQ
*********************************************************************/
static agt_acm_usergroups_t *
    get_usergroups_entry (agt_acm_context_t *context,
                          const xmlChar *username)
{
    agt_acm_usergroups_t *ug = 
        find_usergroups_entry(context, username);
    if (ug) {
        return ug;
    }

    ug = new_usergroups_entry(context, username);
    if (!ug) {
        return NULL;
    }
    dlq_enque(ug, &context->usergroupQ);
    return ug;

} /* get_usergroups_entry */


/********************************************************************
* FUNCTION new_group
*
* create a group-to-users struct
*
* INPUTS:
*   groupname == name of user to use
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static agt_acm_group_t * new_group (const xmlChar *groupname)
{
    agt_acm_group_t *group;

    group = m__getObj(agt_acm_group_t);
    if (!group) {
        return NULL;
    }

    memset(group, 0x0, sizeof(agt_acm_group_t));
    dlq_createSQue(&group->userQ);
    group->groupname =xml_strdup(groupname);
    if (!group->groupname) {
        m__free(group);
        return NULL;
    }
    return group;

}  /* new_group */


/********************************************************************
* FUNCTION free_group
*
* free a group-to-users struct
*
* INPUTS:
*   group == struct to free
*
*********************************************************************/
static void
    free_group (agt_acm_group_t *group)
{
    agt_acm_name_t *grptr;

    if (!group) {
        return;
    }

    while (!dlq_empty(&group->userQ)) {
        grptr = (agt_acm_name_t *)
            dlq_deque(&group->userQ);
        free_name_ptr(grptr);
    }

    m__free(group->groupname);
    m__free(group);

}  /* free_group */


/********************************************************************
* FUNCTION find_group
*
* find a group-to-users struct
*
* INPUTS:
*   groupname == name of group to find
*   groupQ == Q of agt_acm_name_t to check
* RETURNS:
*   pointer to found group or NULL if not found
*********************************************************************/
static agt_acm_group_t * find_group (const xmlChar *groupname,
                                     dlq_hdr_t *groupQ)
{
    agt_acm_group_t *group = (agt_acm_group_t *)dlq_firstEntry(groupQ);

    for (; group != NULL; group = (agt_acm_group_t *)dlq_nextEntry(group)) {
        if (!xml_strcmp(group->groupname, groupname)) {
            return group;
        }
    }
    return NULL;

}  /* find_group */


/********************************************************************
* FUNCTION get_group
*
* Create group-to-users cache for 1 group
*  Read the config and clone some fields; 
*  does not keep any back-ptrs into the config tree
*
* INPUTS:
*   groupval == root of the group node, already fetched
*   res == address of return status
*
* OUTPUTS:
*   return status
*
* RETURNS:
*   pointer to malloced group or NULL if *res is not NO_ERR
*********************************************************************/
static agt_acm_group_t *
    get_group (val_value_t *groupval,
               status_t *res)

{
    /* check the group/name key leaf */
    val_value_t *nameval = 
        val_find_child(groupval, AGT_ACM_IETF_MODULE,
                       y_ietf_netconf_acm_N_name);
    if (!nameval) {
        *res = SET_ERROR(ERR_INTERNAL_VAL);
        return NULL;
    }

    /* find first group/user-name node */
    val_value_t *usernameval = 
        val_find_child(groupval, AGT_ACM_IETF_MODULE,
                       y_ietf_netconf_acm_N_user_name);
    if (!usernameval) {
        /* there are no users in this list */
        if (LOGDEBUG) {
            log_debug("\nagt_acm: Skipping group '%s' with empty "
                      "user list", VAL_STR(nameval));
        }
        *res = ERR_NCX_SKIPPED;
        return NULL;
    }

    /* malloc a group entry */
    agt_acm_group_t *gu = new_group(VAL_STR(nameval));
    if (!gu) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    /* save all the user names */
    for (; usernameval != NULL && *res == NO_ERR;
         usernameval = val_find_next_child(groupval, AGT_ACM_IETF_MODULE,
                                           y_ietf_netconf_acm_N_user_name,
                                           usernameval)) {

        agt_acm_name_t *nameptr = new_name_ptr(VAL_STR(usernameval));
        if (!nameptr) {
            free_group(gu);
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }
        dlq_enque(nameptr, &gu->userQ);
    }
    *res = NO_ERR;
    return gu;

}  /* get_group */


/********************************************************************
* FUNCTION get_all_groups
*
* Create group-to-users cache
*  Read the config and clone some fields; 
*  does not keep any back-ptrs into the config tree
*
* INPUTS:
*   nacmroot == root of the nacm tree, already fetched
*   entryQ == queue to hold agt_acm_group_t structs
*
* OUTPUTS:
*   entryQ will have an entry added for each non-empty groupuser
*
* RETURNS:
*   status
*********************************************************************/
static status_t
    get_all_groups (val_value_t *nacmroot,
                    dlq_hdr_t *entryQ)

{
    /* get /nacm/groups node */
    val_value_t *groupsval = 
        val_find_child(nacmroot, AGT_ACM_IETF_MODULE,
                       y_ietf_netconf_acm_N_groups);
    if (!groupsval) {
        return NO_ERR;
    }

    /* cache each /nacm/groups/group node */
    status_t res = NO_ERR;
    val_value_t *groupval = val_get_first_child(groupsval);
    for (; groupval != NULL && res == NO_ERR;
         groupval = val_get_next_child(groupval)) {

        agt_acm_group_t *gu = get_group(groupval, &res);
        if (gu != NULL) {
            dlq_enque(gu, entryQ);
        } else if (res == ERR_NCX_SKIPPED) {
            res = NO_ERR;
        }
    }

    return res;
    
}  /* get_all_groups */


/********************************************************************
* FUNCTION update_data_rule
*
* Check if the specified data rule has a valid result,
* and if not, re-evaluate the XPath expression for the data rule
*
* INPUTS:
*    msg == XML header from incoming message in progress
*    rule == rule entry to update for
*
* RETURNS:
*    NO_ERR on success or an error if the operation failed.
*
*********************************************************************/
static status_t
    update_data_rule (xml_msg_hdr_t *msg,
                      agt_acm_rule_t *rule)
{
    ncx_cfg_t cfgid = get_cfgid(msg);
    uint32 msgid = xml_msg_get_msgid(msg);  // msgid zero not allowed

    if (msgid == rule->result_msgid[cfgid]) {
        if (LOGDEBUG4) {
            log_debug4("\nagt_acm: Skipping data-rule eval for '%s', "
                       "same msgid (%u)", rule->pcb->exprstr, msgid);
        }
        return NO_ERR;
    }

    /* check if the XPath expression result is still valid */
    if (ncx_use_xpath_backptrs()) {
        if (xpath_check_backptrs_dirty(rule->pcb, cfgid)) {
            if (LOGDEBUG3) {
                log_debug3("\nagt_acm: Not skipping data-rule eval for '%s',"
                           " dirty", rule->pcb->exprstr);
            }
        } else {
            if (LOGDEBUG3) {
                log_debug3("\nagt_acm: Skipping data-rule eval for '%s',"
                           " not dirty", rule->pcb->exprstr);
            }
            rule->result_msgid[cfgid] = msgid;
            return NO_ERR;
        }
    }

    val_value_t *valroot = cfg_get_root(cfgid);
    status_t res = NO_ERR;

    /* get the result for the specified datastore */
    xpath_free_result(rule->result[cfgid]);
    rule->result[cfgid] = 
        xpath1_eval_expr(rule->pcb, valroot, valroot, FALSE, TRUE, &res);
    if (rule->result[cfgid] == NULL) {
        return res;
    }

    /* save the message ID so this code will only be executed once
     * during the message that causes the update       */
    rule->result_msgid[cfgid] = msgid;

    /* set the value cache for each node */
    res = NO_ERR;
    xpath_resnode_t *resnode = xpath_get_first_resnode(rule->result[cfgid]);
    for (; resnode != NULL && res == NO_ERR; 
         resnode = xpath_get_next_resnode(resnode)) {
        val_value_t *valnode = xpath_get_resnode_valptr(resnode);
        if (valnode) {
            if (!val_match_datarule(valnode, rule)) {
                res = val_cache_datarule(valnode, rule);
            }
        }
    }

    if (res != NO_ERR) {
        /* undo all the back-ptrs just set */
        xpath_resnode_t *fixnode = xpath_get_first_resnode(rule->result[cfgid]);
        for (; fixnode != NULL && fixnode != resnode; 
             fixnode = xpath_get_next_resnode(fixnode)) {
            val_value_t *valnode = xpath_get_resnode_valptr(fixnode);
            if (valnode) {
                val_clear_datarule(valnode, rule);
            }
        }
        rule->flags |= FL_ACM_CACHE_FAILED;
    } else if (ncx_use_xpath_backptrs()) {
        xpath_set_backptrs_dirty(rule->pcb, cfgid, FALSE);
    }

    return res;

}  /* update_data_rule */


/********************************************************************
* FUNCTION cache_data_rule
*
* Cache the specified data rule.
*
* INPUTS:
*    ruleval == value node to use
*    rule == rule entry to fill in
*
* OUTPUTS:
*    rule fields (pcb & result) are set
*
* RETURNS:
*    NO_ERR on success or an error if the operation failed.
*
*********************************************************************/
static status_t
    cache_data_rule (val_value_t *pathval,
                     agt_acm_rule_t *rule)
{
    rule->pcb = xpath_clone_pcb(pathval->xpathpcb);
    if (!rule->pcb) {
        return ERR_INTERNAL_MEM;
    }
    xpath_set_manual_clear(rule->pcb);

    obj_template_t *obj = pathval->obj;
    ncx_module_t *mod = obj_get_mod(obj);

    /* set the xpath_backptrs for the target object in
     * the data rule        */
    status_t res = xpath1_validate_expr_ex(mod, obj, rule->pcb, FALSE, TRUE);

    /* pcb->source is currently set to XP_SRC_NONE!
     * Make sure the source is not XML so the defunct reader
     * does not get accessed; the clone should save the
     * NSID bindings in all the tokens
     */
    rule->pcb->source = XP_SRC_YANG;

    // Hardwire the initial config to the running config
    ncx_cfg_t cfgid = NCX_CFGID_RUNNING;

    val_value_t *valroot = cfg_get_root(cfgid);

    res = NO_ERR;
    rule->result[cfgid] =
        xpath1_eval_expr(rule->pcb, valroot, valroot, FALSE, TRUE, &res);
    if (!rule->result) {
        return res;
    }
    // rule->result_msgid[] already set to zero

    /* set the value cache for each node */
    res = NO_ERR;
    xpath_resnode_t *resnode = xpath_get_first_resnode(rule->result[cfgid]);
    for (; resnode != NULL && res == NO_ERR; 
         resnode = xpath_get_next_resnode(resnode)) {
        val_value_t *valnode = xpath_get_resnode_valptr(resnode);
        if (valnode) {
            res = val_cache_datarule(valnode, rule);
        }
    }

    if (res != NO_ERR) {
        /* undo all the back-ptrs just set */
        xpath_resnode_t *fixnode = xpath_get_first_resnode(rule->result[cfgid]);
        for (; fixnode != NULL && fixnode != resnode; 
             fixnode = xpath_get_next_resnode(fixnode)) {
            val_value_t *valnode = xpath_get_resnode_valptr(fixnode);
            if (valnode) {
                val_clear_datarule(valnode, rule);
            }
        }
        rule->flags |= FL_ACM_CACHE_FAILED;
    }
    
    return res;

}  /* cache_data_rule */


/********************************************************************
* FUNCTION free_rule
*
* free a rule cache entry
*
* INPUTS:
*   rule == entry to free
*
*********************************************************************/
static void
    free_rule (agt_acm_rule_t *rule)
{
    if (!rule) {
        return;
    }

    // leaving out FAILED optimization since it is
    // not reliable now that there is a result for each datastore
    //if (rule->result && !(rule->flags & FL_ACM_CACHE_FAILED)) {

    ncx_cfg_t cfgid = 0;
    if (ncx_use_xpath_backptrs()) {
        for (cfgid = 0; cfgid < CFG_NUM_STATIC; cfgid++) {
            if (rule->result[cfgid]) {
                xpath_resnode_t *fixnode =
                    xpath_get_first_resnode(rule->result[cfgid]);
                for (; fixnode != NULL;
                     fixnode = xpath_get_next_resnode(fixnode)) {
                    val_value_t *valnode = xpath_get_resnode_valptr(fixnode);
                    if (valnode) {
                        val_clear_datarule(valnode, rule);
                    }
                }
            }
        }
    }

    m__free(rule->name);
    m__free(rule->modulename);
    m__free(rule->targetname);

    for (cfgid = 0; cfgid < CFG_NUM_STATIC; cfgid++) {
        xpath_free_result(rule->result[cfgid]);
    }
    xpath_free_pcb(rule->pcb);
    m__free(rule);

}  /* free_rule */


/********************************************************************
* FUNCTION new_rule
*
* create a rule cache entry
*
* INPUTS:
*   context == context to use
*   val == /nacm/rulelist/rule list entry
*   res == address of return status
* OUTPUTS:
*   *res == return status, ERR_NCX_SKIPPED, ERR_INTERNAL_MEM, NO_ERR
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static agt_acm_rule_t *
    new_rule (agt_acm_context_t *context,
              val_value_t *val,
              status_t *res)
{
    // was used to get config root, now changed to cfg_get_root
    (void)context;

    /* get the mandatory rule/name field */
    val_value_t *childval =  
        val_find_child(val, AGT_ACM_IETF_MODULE,
                       y_ietf_netconf_acm_N_name);
    if (!childval) {
        *res = SET_ERROR(ERR_INTERNAL_VAL);
        return NULL;
    }

    agt_acm_rule_t *rule = m__getObj(agt_acm_rule_t);
    if (!rule) {
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }
    memset(rule, 0x0, sizeof(agt_acm_rule_t));
    rule->name = xml_strdup(VAL_STR(childval));
    if (!rule->name) {
        m__free(rule);
        *res = ERR_INTERNAL_MEM;
        return NULL;
    }

    /* get the optional rule/module-name */
    childval = val_find_child(val, AGT_ACM_IETF_MODULE,
                              y_ietf_netconf_acm_N_module_name);
    if (childval) {
        const xmlChar *modname = VAL_STR(childval);
        if (!xml_strcmp(modname, NACM_WILDCARD)) {
            rule->flags |= FL_ACM_ALLMODULES;
        } else if (!ncx_valid_name2(modname)) {
            if (LOGDEBUG) {
                log_debug("\nagt_acm: Skipping rule '%s' with invalid "
                          "module name '%s'", rule->name, modname);
            }
            free_rule(rule);
            *res = ERR_NCX_SKIPPED;
            return NULL;
        } else {
            rule->modulename = xml_strdup(modname);
            if (!rule->modulename) {
                free_rule(rule);
                *res = ERR_INTERNAL_MEM;
                return NULL;
            }
        }
    } else {
        /* default is all modules */
        rule->flags |= FL_ACM_ALLMODULES;
    }

    /* figure out which rule type is present
     * 1) check protocol-operation/rpc-name leaf */
    childval = val_find_child(val, AGT_ACM_IETF_MODULE,
                              y_ietf_netconf_acm_N_rpc_name);
    if (childval) {
        const xmlChar *rpcname = VAL_STR(childval);
        if (!xml_strcmp(rpcname, NACM_WILDCARD)) {
            rule->flags |= FL_ACM_ALLTARGET;
        } else if (!ncx_valid_name2(rpcname)) {
            if (LOGDEBUG) {
                log_debug("\nagt_acm: Skipping rule '%s' with invalid "
                          "rpc name '%s'", rule->name, rpcname);
            }
            free_rule(rule);
            *res = ERR_NCX_SKIPPED;
            return NULL;
        } else {
            rule->targetname = xml_strdup(rpcname);
            if (!rule->targetname) {
                free_rule(rule);
                *res = ERR_INTERNAL_MEM;
                return NULL;
            }
        }
        rule->ruletype = AGT_ACM_RULE_OP;
    } else {
        /* 2) check notification/notification-name leaf */
        childval = val_find_child(val, AGT_ACM_IETF_MODULE,
                                  y_ietf_netconf_acm_N_notification_name);
        if (childval) {
            const xmlChar *notname = VAL_STR(childval);
            if (!xml_strcmp(notname, NACM_WILDCARD)) {
                rule->flags |= FL_ACM_ALLTARGET;
            } else if (!ncx_valid_name2(notname)) {
                if (LOGDEBUG) {
                    log_debug("\nagt_acm: Skipping rule '%s' with invalid "
                              "notification name '%s'", rule->name, notname);
                }
                free_rule(rule);
                *res = ERR_NCX_SKIPPED;
                return NULL;
            } else {
                rule->targetname = xml_strdup(notname);
                if (!rule->targetname) {
                    free_rule(rule);
                    *res = ERR_INTERNAL_MEM;
                    return NULL;
                }
            }
            rule->ruletype = AGT_ACM_RULE_NOTIF;
        } else {
            /* 3) check data-node/path leaf */
            childval = val_find_child(val, AGT_ACM_IETF_MODULE,
                                      y_ietf_netconf_acm_N_path);
            if (childval) {
                *res = cache_data_rule(childval, rule);
                if (*res != NO_ERR) {
                    log_warn("\nWarning: Cache of data rule '%s' "
                              "failed '%s', using manual mode", rule->name, 
                              get_error_string(*res));
                    *res = NO_ERR;
                }
                rule->ruletype = AGT_ACM_RULE_DATA;
            } else {
                /* none of the rule type targets found -- matches all */
                rule->ruletype = AGT_ACM_RULE_ALL;
            }
        }
    }

    /* get the optional rule/access-operations leaf */
    childval = val_find_child(val, AGT_ACM_IETF_MODULE,
                              y_ietf_netconf_acm_N_access_operations);
    if (childval) {
        if (childval->btyp == NCX_BT_BITS) {
            ncx_list_t *bits = &VAL_BITS(childval);
            
            if (ncx_string_in_list(NCX_EL_CREATE, bits)) {
                rule->flags |= FL_ACM_OP_CREATE;
            }
            if (ncx_string_in_list(NCX_EL_READ, bits)) {
                rule->flags |= FL_ACM_OP_READ;
            }
            if (ncx_string_in_list(NCX_EL_UPDATE, bits)) {
                rule->flags |= FL_ACM_OP_UPDATE;
            }
            if (ncx_string_in_list(NCX_EL_DELETE, bits)) {
                rule->flags |= FL_ACM_OP_DELETE;
            }
            if (ncx_string_in_list(NCX_EL_EXEC, bits)) {
                rule->flags |= FL_ACM_OP_EXEC;
            }

            /* check no bits set */
            if (!(rule->flags & FL_ACM_ALLOPS)) {
                if (LOGDEBUG) {
                    log_debug("\nagt_acm: Skipping rule '%s' with empty "
                              "access-operations bits", rule->name);
                }
                free_rule(rule);
                *res = ERR_NCX_SKIPPED;
                return NULL;
            }
        } else {
            const xmlChar *opstr = VAL_STR(childval);
            if (!xml_strcmp(opstr, NACM_WILDCARD)) {
                rule->flags |= FL_ACM_ALLOPS;
            } else {
                if (LOGDEBUG) {
                    log_debug("\nagt_acm: Skipping rule '%s' with invalid "
                              "access-operations string '%s'", 
                              rule->name, opstr);
                }
                free_rule(rule);
                *res = ERR_NCX_SKIPPED;
                return NULL;
            }
        }
    } else {
        /* default is all operations */
        rule->flags |= FL_ACM_ALLOPS;
    }

    /* get the mandatory rule/action leaf */
    childval = val_find_child(val, AGT_ACM_IETF_MODULE,
                              y_ietf_netconf_acm_N_action);
    if (childval && !xml_strcmp(VAL_ENUM_NAME(childval), NCX_EL_PERMIT)) {
        rule->flags |= FL_ACM_PERMIT;
    }

    /* ignoring rule/comment for now */

    return rule;

}  /* new_rule */


/********************************************************************
* FUNCTION free_rulelist
*
* free a rule-list cache entry
*
* INPUTS:
*   rulelist == entry to free
*
*********************************************************************/
static void
    free_rulelist (agt_acm_rulelist_t *rulelist)
{

    if (!rulelist) {
        return;
    }

    while (!dlq_empty(&rulelist->groupQ)) {
        agt_acm_name_t *grp = (agt_acm_name_t *)
            dlq_deque(&rulelist->groupQ);
        free_name_ptr(grp);
    }

    while (!dlq_empty(&rulelist->ruleQ)) {
        agt_acm_rule_t *rule = (agt_acm_rule_t *)
            dlq_deque(&rulelist->ruleQ);
        free_rule(rule);
    }

    m__free(rulelist->name);
    m__free(rulelist);

}  /* free_rulelist */


/********************************************************************
* FUNCTION new_rulelist
*
* create a rule-list cache entry
*
* INPUTS:
*   context == context to use
*   val == value node for the rule-list
*
* RETURNS:
*   filled in, malloced struct or NULL if malloc error
*********************************************************************/
static agt_acm_rulelist_t *
    new_rulelist (agt_acm_context_t *context,
                  val_value_t *val)
{
    val_value_t *childval = val_find_child(val, AGT_ACM_IETF_MODULE,
                                           y_ietf_netconf_acm_N_name);
    if (!childval) {
        SET_ERROR(ERR_INTERNAL_VAL);
        return NULL;
    }
    const xmlChar *name = VAL_STR(childval);

    agt_acm_rulelist_t *rulelist = m__getObj(agt_acm_rulelist_t);
    if (!rulelist) {
        return NULL;
    }
    memset(rulelist, 0x0, sizeof(agt_acm_rulelist_t));
    dlq_createSQue(&rulelist->groupQ);
    dlq_createSQue(&rulelist->ruleQ);
    rulelist->name = xml_strdup(name);
    if (!rulelist->name) {
        m__free(rulelist);
        return NULL;
    }

    /* add each /nacm/rule-list/group leaf-list node */
    boolean done = FALSE;
    for (childval = val_find_child(val, AGT_ACM_IETF_MODULE,
                                   y_ietf_netconf_acm_N_group);
         childval != NULL && !done;
         childval = val_find_next_child(val, AGT_ACM_IETF_MODULE,
                                        y_ietf_netconf_acm_N_group,
                                        childval)) {
        const xmlChar *groupname = VAL_STR(childval);

        if (!xml_strcmp(groupname, NACM_WILDCARD)) {
            /* skip rest of group names since the matchall is present */
            rulelist->allgroups = TRUE;
            done = TRUE;
        } else {
            agt_acm_name_t *grptr = 
                find_name_ptr(&rulelist->groupQ, groupname);
            if (grptr) {
                continue;;
            }

            grptr = new_name_ptr(groupname);
            if (grptr) {
                dlq_enque(grptr, &rulelist->groupQ);
            } else {
                free_rulelist(rulelist);
                return NULL;
            }
        }
    }

    for (childval = val_find_child(val, AGT_ACM_IETF_MODULE,
                                   y_ietf_netconf_acm_N_rule);
         childval != NULL;
         childval = val_find_next_child(val, AGT_ACM_IETF_MODULE,
                                        y_ietf_netconf_acm_N_rule,
                                        childval)) {

        status_t res = NO_ERR;
        agt_acm_rule_t *rule = new_rule(context, childval, &res);
        if (rule) {
            dlq_enque(rule, &rulelist->ruleQ);
        } else if (res == ERR_NCX_SKIPPED) {
            res = NO_ERR;
        } else {
            log_error("\nError: create rule failed (%s)",
                      get_error_string(res));
            free_rulelist(rulelist);
            return NULL;
        }
    }

    return rulelist;

}  /* new_rulelist */


/********************************************************************
* FUNCTION find_rulelist
*
* find a rule-list struct
*
* INPUTS:
*   name == name of rulelist to find
*   rulelistQ == Q of agt_acm_rulelist_t to search
* RETURNS:
*   pointer to found rulelist or NULL if not found
*********************************************************************/
static agt_acm_rulelist_t * 
    find_rulelist (const xmlChar *name,
                   dlq_hdr_t *rulelistQ)
{
    agt_acm_rulelist_t *rulelist = 
        (agt_acm_rulelist_t *)dlq_firstEntry(rulelistQ);

    for (; rulelist != NULL; 
         rulelist = (agt_acm_rulelist_t *)dlq_nextEntry(rulelist)) {
        if (!xml_strcmp(rulelist->name, name)) {
            return rulelist;
        }
    }
    return NULL;

}  /* find_rulelist */


/********************************************************************
* FUNCTION new_acm_cache
*
* Malloc and initialize an agt_acm_cache_t stuct
*
* INPUTS:
*   context == context to use
*   username == username requesting a new cache
*
* RETURNS:
*   malloced session/msg cache or NULL if error
*********************************************************************/
static agt_acm_cache_t  *
    new_acm_cache (agt_acm_context_t *context,
                   const xmlChar *username)
{
    agt_acm_cache_t  *acm_cache = m__getObj(agt_acm_cache_t);
    if (!acm_cache) {
        return NULL;
    }
    memset(acm_cache, 0x0, sizeof(agt_acm_cache_t));
    acm_cache->isvalid = TRUE;

    agt_acm_usergroups_t *usergroups = 
        get_usergroups_entry(context, username);
    if (!usergroups) {
        /* out of memory! deny all access! */
        if (LOGDEBUG2) {
            log_debug2("\nagt_acm: DENY (out of memory)");
        }
        m__free(acm_cache);
        acm_cache = NULL;
    } else {
        acm_cache->usergroups = usergroups;
    }

    return acm_cache;

} /* new_acm_cache */


/********************************************************************
* FUNCTION free_acm_cache
*
* Clean and free a agt_acm_cache_t struct
*
* INPUTS:
*   acm_cache == cache struct to free
*********************************************************************/
static void
    free_acm_cache (agt_acm_cache_t  *acm_cache)
{
    if (!acm_cache) {
        return;
    }
    m__free(acm_cache);

} /* free_acm_cache */


/********************************************************************
* FUNCTION init_context_cache
* 
*   Initialize the global cache entries
* 
* INPUTS:
*   context == context to use
*   added == TRUE if empty nacm node just added
*            FALSE for normal init call
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
static status_t 
    init_context_cache (agt_acm_context_t *context,
                        boolean added)
{
    status_t res = NO_ERR;

    if (context->reinit_fail_count >= MAX_REINIT_FAILS) {
        return ERR_NCX_OPERATION_FAILED;
    }

    context->acmode = agt_acm_get_acmode();
    context->log_reads = agt_acm_get_log_reads();
    context->log_writes = agt_acm_get_log_writes();

    if (!added) {
        /* make the initial rulelist cache unless <nacm> node is new */
        val_value_t *childval = 
            val_find_child(context->nacmrootval, AGT_ACM_IETF_MODULE,
                           y_ietf_netconf_acm_N_rule_list);
        for (; childval != NULL && res == NO_ERR; 
             childval = val_find_next_child(context->nacmrootval, 
                                            AGT_ACM_IETF_MODULE,
                                            y_ietf_netconf_acm_N_rule_list,
                                            childval)) {
        
            agt_acm_rulelist_t *rulelist = new_rulelist(context, childval);
            if (rulelist) {
                dlq_enque(rulelist, &context->rulelistQ);
            } else {
                res = ERR_INTERNAL_MEM;
            }
        }
    }

    /* make the initial group cache unless <nacm> node is new */
    if (res == NO_ERR && !added) {
        res = get_all_groups(context->nacmrootval, &context->groupQ);
    }

    /* set the default flags */
    if (res == NO_ERR) {
        context->dflags = 0;
        val_value_t *dval = val_find_child(context->nacmrootval,
                                           AGT_ACM_IETF_MODULE,
                                           y_ietf_netconf_acm_N_read_default);
        if (dval) {
            if (!xml_strcmp(VAL_ENUM_NAME(dval), NCX_EL_PERMIT)) {
                context->dflags |= FL_ACM_DEFREAD_PERMIT;
            }
        } else {
            context->dflags |= FL_ACM_DEFREAD_PERMIT;
        }

        dval = val_find_child(context->nacmrootval,
                              AGT_ACM_IETF_MODULE,
                              y_ietf_netconf_acm_N_write_default);
        if (dval && !xml_strcmp(VAL_ENUM_NAME(dval), NCX_EL_PERMIT)) {
            context->dflags |= FL_ACM_DEFWRITE_PERMIT;
        }

        dval = val_find_child(context->nacmrootval,
                              AGT_ACM_IETF_MODULE,
                              y_ietf_netconf_acm_N_exec_default);
        if (dval) {
            if (!xml_strcmp(VAL_ENUM_NAME(dval), NCX_EL_PERMIT)) {
                context->dflags |= FL_ACM_DEFEXEC_PERMIT;
            }
        } else {
            context->dflags |= FL_ACM_DEFEXEC_PERMIT;
        }
    }

    if (res == NO_ERR) {
        context->cache_valid = TRUE;
        context->reinit_fail_count = 0;
    } else {
        context->reinit_fail_count++;
    }

    return res;

}  /* init_context_cache */


/********************************************************************
* FUNCTION validate_context
* 
*  Validate the context cache
*  Initialize the global cache entries if needed
* 
* INPUTS:
*   context == context to validate
*
* RETURNS:
*   status of the validation/re-initialization procedure
*********************************************************************/
static status_t 
    validate_context (agt_acm_context_t *context)
{
    if (context == NULL) {
        return ERR_INTERNAL_VAL;
    }
    if (context->cache_valid) {
        return NO_ERR;
    }
    return init_context_cache(context, FALSE);

}  /* validate_context */


/********************************************************************
* FUNCTION clean_usergroups_cache
* 
* Cleanup the global user 2 groups cache
* 
* INPUTS:
*   context == context to use
* RETURNS:
*   none
*********************************************************************/
static void
    clean_usergroups_cache (agt_acm_context_t *context)
{
    while (!dlq_empty(&context->usergroupQ)) {
        agt_acm_usergroups_t *ug = (agt_acm_usergroups_t *)
            dlq_deque(&context->usergroupQ);
        free_usergroups(ug);
    }

}  /* clean_usergroups_cache */


/********************************************************************
* FUNCTION clean_context_cache
* 
* Cleanup the global context cache
* 
* INPUTS:
*   context == context to use
* RETURNS:
*   none
*********************************************************************/
static void
    clean_context_cache (agt_acm_context_t *context)
{
    if (LOGDEBUG2) {
        log_debug2("\nagt_acm: Clearing context cache");
    }

    while (!dlq_empty(&context->rulelistQ)) {
        agt_acm_rulelist_t *rulelist = (agt_acm_rulelist_t *)
            dlq_deque(&context->rulelistQ);
        free_rulelist(rulelist);
    }

    clean_usergroups_cache(context);

    while (!dlq_empty(&context->groupQ)) {
        agt_acm_group_t *gu = (agt_acm_group_t *)
            dlq_deque(&context->groupQ);
        free_group(gu);
    }
    context->cache_valid = FALSE;
    context->dflags = 0;

}  /* clean_context_cache */


/********************************************************************
* FUNCTION get_default_rpc_response
*
* get the default response for the specified RPC object
* there are no rules that match any groups with this user
*
*  INPUTS:
*    context == context to use
*    rpcobj == RPC template for this request
*    
* RETURNS:
*   TRUE if access granted
*   FALSE if access denied
*********************************************************************/
static boolean
    get_default_rpc_response (agt_acm_context_t *context,
                              const obj_template_t *rpcobj)
{
    /* check if the RPC method is tagged as 
     * nacm:very-secure/nacm:default-deny-all;
     * deny access if so
     * !!! this is different than yuma-nacm where RPC is denied
     * by nacm:secure or nacm:very-secure !!!
     */
    if (obj_is_very_secure(rpcobj)) {
        return FALSE;
    }

    /* check the exec-default setting */
    return (context->dflags & FL_ACM_DEFEXEC_PERMIT) ? TRUE : FALSE;

}  /* get_default_rpc_response */


/********************************************************************
* FUNCTION get_default_notif_response
*
* get the default response for the specified notification object
* there are no rules that match any groups with this user
*
*  INPUTS:
*    context == context to use
*    notifobj == notification template for this request
*    
* RETURNS:
*   TRUE if access granted
*   FALSE if access denied
*********************************************************************/
static boolean
    get_default_notif_response (agt_acm_context_t *context,
                                const obj_template_t *notifobj)
{
    /* check if the notification event is tagged as 
     * nacm:secure or nacm:very-secure and
     * deny access if so
     */
    if (obj_is_secure(notifobj) || obj_is_very_secure(notifobj)) {
        return FALSE;
    }

    /* check the read-default setting */
    return (context->dflags & FL_ACM_DEFREAD_PERMIT) ? TRUE : FALSE;

}  /* get_default_notif_response */


/********************************************************************
* FUNCTION get_default_data_response
*
* get the default response for the specified data object
* there are no rules that match any groups with this user
*
*  INPUTS:
*    context == context to use
*    val == data node for this request
*    iswrite == TRUE for write access
*               FALSE for read access
*
* RETURNS:
*   TRUE if access granted
*   FALSE if access denied
*********************************************************************/
static boolean
    get_default_data_response (agt_acm_context_t *context,
                               const val_value_t *val,
                               boolean iswrite)
{
    /* special case -- there are no ACM rules for the
     * config root, so allow all reads and writes on this
     * container and start checking at the top-level
     * YANG nodes instead
     */
    if (obj_is_root(val->obj)) {
        return TRUE;
    }

    /* check if the default is already 'deny' */
    if (iswrite) {
        if (!(context->dflags & FL_ACM_DEFWRITE_PERMIT)) {
            return FALSE;
        }
    } else {
        if (!(context->dflags & FL_ACM_DEFREAD_PERMIT)) {
            return FALSE;
        }
    }

    /* check if the data node is tagged as 
     * ncx:secure or ncx:very-secure and deny access if so;
     * make sure this is not an nested object within a
     * object tagged as ncx:secure or ncx:very-secure
     */
    const obj_template_t *testobj = val->obj;
    while (testobj) {
        if (iswrite) {
            /* reject any ncx:secure or ncx:very-secure object */
            if (obj_is_secure(testobj) || obj_is_very_secure(testobj)) {
                return FALSE;
            }
        } else {
            /* allow ncx:secure to be read; reject ncx:very-secure */
            if (obj_is_very_secure(testobj)) {
                return FALSE;
            }
        }

        /* stop at root */
        if (obj_is_root(testobj)) {
            testobj = NULL;
        } else {
            testobj = testobj->parent;
        }

        /* no need to check further if the parent was the root
         * need to make sure not to go past the
         * config parameter into the rpc input
         * then the secret <load-config> rpc
         */
        if (testobj && obj_is_root(testobj)) {
            testobj = NULL;
        }
    }

    /* check the noDefaultRule setting on this agent */
    boolean retval = FALSE;
    if (iswrite) {
        return (context->dflags & FL_ACM_DEFWRITE_PERMIT) ? TRUE : FALSE;
    } else {
        return (context->dflags & FL_ACM_DEFREAD_PERMIT) ? TRUE : FALSE;
    }

    return retval;

}  /* get_default_data_response */


/********************************************************************
* FUNCTION rulelist_applies_to_user
*
* Check the cached /nacm/rule-list/group leaf-list to see if the
* specified rulelist applies to the user
*
* INPUTS:
*    usergroups == usergroups entry for the user making the request
*    rulelist == cached rulelist entry
* RETURNS:
*   TRUE if rulelist applies
*   FALSE if rulelist should be skipped
*********************************************************************/
static boolean
    rulelist_applies_to_user (agt_acm_usergroups_t *usergroups,
                              agt_acm_rulelist_t *rulelist)
{
    if (usergroups->groupcount == 0) {
        return FALSE;
    }

    if (rulelist->allgroups) {
        return TRUE;
    }

    agt_acm_name_t *nameptr = (agt_acm_name_t *)
        dlq_firstEntry(&usergroups->groupQ);
    for (; nameptr != NULL;
         nameptr = (agt_acm_name_t *)dlq_nextEntry(nameptr)) {
        /* check if this group is in the groups list 
         * for the ruleliest */

        if (find_name_ptr(&rulelist->groupQ, nameptr->name)) {
            return TRUE;
        }
    }

    return FALSE;

}  /* rulelist_applies_to_user */


/********************************************************************
* FUNCTION rule_applies_to_access
*
* Check the cached /nacm/rule-list/rule/access-operations leaf
* to see if the specified rule applies to the access requested
*
* INPUTS:
*    access == access requested
*    rule == cached rule entry to check
* RETURNS:
*   TRUE if rule applies
*   FALSE if rule should be skipped
*********************************************************************/
static boolean
    rule_applies_to_access (access_t access,
                            agt_acm_rule_t *rule)
{
    switch (access) {
    case ACCESS_NONE:
        return FALSE;
    case ACCESS_CREATE:
        return (rule->flags & FL_ACM_OP_CREATE) ? TRUE : FALSE;
    case ACCESS_READ:
        return (rule->flags & FL_ACM_OP_READ) ? TRUE : FALSE;
    case ACCESS_UPDATE:
        return (rule->flags & FL_ACM_OP_UPDATE) ? TRUE : FALSE;
    case ACCESS_DELETE:
        return (rule->flags & FL_ACM_OP_DELETE) ? TRUE : FALSE;
    case ACCESS_EXEC:
        return (rule->flags & FL_ACM_OP_EXEC) ? TRUE : FALSE;
    default:
        SET_ERROR(ERR_INTERNAL_VAL);
        return FALSE;
    }

}  /* rule_applies_to_access */


/********************************************************************
* FUNCTION rule_applies_to_module
*
* Check the cached /nacm/rule-list/rule/module-name leaf
* to see if the specified rule applies to the module requested
*
* INPUTS:
*    access == access requested
*    rule == cached rule entry to check
* RETURNS:
*   TRUE if rule applies
*   FALSE if rule should be skipped
*********************************************************************/
static boolean
    rule_applies_to_module (const xmlChar *modname,
                            agt_acm_rule_t *rule)
{
    if (rule->flags & FL_ACM_ALLMODULES) {
        return TRUE;
    }

    if (!xml_strcmp(rule->modulename, modname)) {
        return TRUE;
    }

    /* deal with the ietf-netconf == yuma-netconf hack */
    if (!xml_strcmp(modname, NCXMOD_YUMA_NETCONF) &&
        !xml_strcmp(rule->modulename, NCXMOD_IETF_NETCONF)) {
        return TRUE;
    }

    return FALSE;

}  /* rule_applies_to_module */


/********************************************************************
* FUNCTION data_rule_applies
*
* Check the configured /nacm/rulelist/rule entry to see if the
* data node is selected by this rule
*
* !!! This code is only used if ncx_use_xpath_backptrs is FALSE
* !!! because the feature is disabled.  It is also used if a
* !!! malloc-failed occurred and the CACHE_FAILED flag gets set
*
* INPUTS:
*    msg == XML header from incoming message in progress
*    val == value node requested
*    rule == cached rule to check
*
* RETURNS:
*     TRUE if rule applies
*     FALSE if rule does not apply
*********************************************************************/
static boolean
    data_rule_applies (xml_msg_hdr_t *msg,
                       const val_value_t *val,
                       agt_acm_rule_t *rule)
{

    ncx_cfg_t cfgid = get_cfgid(msg);

    if (rule->pcb == NULL || rule->result[cfgid] == NULL) {
        return FALSE;
    }

    dlq_hdr_t *resnodeQ = xpath_get_resnodeQ(rule->result[cfgid]);
    if (!resnodeQ) {
        return FALSE;
    }

    if (xpath1_check_node_exists_slow(rule->pcb, resnodeQ, val)) {
        /* this dataRule is for the specified node
         * check if any of the groups in the usergroups
         * list for this user match any of the groups in
         * the allowedGroup leaf-list
         */
        return TRUE;
    }

    return FALSE;

} /* data_rule_applies */


/********************************************************************
* FUNCTION check_rulelist
*
* Check the configured /nacm/rule-list list to see if the
* access is allowed
*
* INPUTS:
*    msg == XML header from incoming message in progress
*    context == context to use
*    usergroups == usergroups entry for the user making the request
*    obj == object template requested
*    val == value node requested (NULL except for data requests
*    access == requested access type
*    rule_type == type of rule (OP, NOTIF, DATA) that applies here
*    rulefound == address of return rule found flag
* OUTPUTS:
*   *rulefound == TRUE if a rule was found; use return value
*                 FALSE if the default response should be used
* RETURNS:
*  Use only if *rulefound == TRUE; Otherwise use default_reponse
*   TRUE if authorization to perform the request is permitted
*   FALSE if authorization to perform the request is denied
*********************************************************************/
static boolean
    check_rulelist (xml_msg_hdr_t *msg,
                    agt_acm_context_t *context,
                    agt_acm_usergroups_t *usergroups,
                    const obj_template_t *obj,
                    val_value_t *val,
                    access_t access,
                    rule_type_t rule_type,
                    boolean *rulefound)
{
    /* check all the rule-list entries, looking for the ones
     * that can possible be related to this request
     */
    *rulefound = FALSE;
    agt_acm_rulelist_t *rulelist = (agt_acm_rulelist_t *)
        dlq_firstEntry(&context->rulelistQ);
    for (; rulelist != NULL;
         rulelist = (agt_acm_rulelist_t *)dlq_nextEntry(rulelist)) {

        if (!rulelist_applies_to_user(usergroups, rulelist)) {
            if (LOGDEBUG4) {
                log_debug4("\nagt_acm: skipping rulelist '%s': group does not "
                           "apply for user '%s'", rulelist->name, 
                           usergroups->username);
            }
            continue;
        }

        /* this rule-list applies to this list.
         * go through all the rules in this rulelist to
         * find ones that apply to ACCESS_EXEC */
        agt_acm_rule_t *rule = (agt_acm_rule_t *)
            dlq_firstEntry(&rulelist->ruleQ);
        for (; rule != NULL;
             rule = (agt_acm_rule_t *)dlq_nextEntry(rule)) {

            /* check if the rule type applies */
            if (!(rule->ruletype == rule_type ||
                  rule->ruletype == AGT_ACM_RULE_ALL ||
                  (rule->flags & FL_ACM_ALLTARGET))) {
                if (LOGDEBUG4) {
                    log_debug4("\nagt_acm: skipping rule '%s': "
                               "rule type does not apply", rule->name);
                }
                continue;
            }
            
            /* check if the access operation applies */
            if (!rule_applies_to_access(access, rule)) {
                if (LOGDEBUG4) {
                    log_debug4("\nagt_acm: skipping rule '%s': access does not "
                               "apply", rule->name);
                }
                continue;
            }

            /* this rule applies to this operation and this group
             * check if the module name applies   */
            if (!rule_applies_to_module(obj_get_mod_name(obj), rule)) {
                if (LOGDEBUG4) {
                    log_debug4("\nagt_acm: skipping rule '%s': module '%s'"
                               "does not apply", 
                               rule->name, obj_get_mod_name(obj));
                }
                continue;
            }

            /* check if there is a target name specified */
            if (rule->targetname) {
                if (xml_strcmp(rule->targetname, obj_get_name(obj))) {
                    if (LOGDEBUG4) {
                        log_debug4("\nagt_acm: skipping rule '%s': "
                                   "target '%s' does not apply", 
                                   rule->name, rule->targetname);
                    }
                    continue;
                }
            }

            if (val && rule->pcb && rule_type == AGT_ACM_RULE_DATA) {
                boolean rule_applies = FALSE;
                /* check if the pcb and result match this node */
                if (rule->flags & FL_ACM_CACHE_FAILED) {
                    rule_applies = data_rule_applies(msg, val, rule);
                } else {
                    status_t res = update_data_rule(msg, rule);
                    if (res == NO_ERR) {
                        if (rule->pcb && val_match_datarule(val, rule)) {
                            rule_applies = TRUE;
                        }
                    } else {
                        rule->flags |= FL_ACM_CACHE_FAILED;
                        rule_applies = data_rule_applies(msg, val, rule);
                    }
                }

                if (!rule_applies) {
                    if (LOGDEBUG4) {
                        log_debug4("\nagt_acm: skipping rule '%s': "
                                   "data target '%s' does not apply", 
                                   rule->name, rule->pcb->exprstr);
                    }
                    continue;
                }
            }

            /* this rule applies to everything, return the permit/deny */
            if (LOGDEBUG2) {
                if (LOGDEBUG2) {
                    log_debug2("\nagt_acm: applying rule '%s/%s' for "
                               "request on %s:%s'", 
                               rulelist->name, rule->name,
                               obj_get_mod_name(obj), obj_get_name(obj));
                }
            }
            *rulefound = TRUE;
            return (rule->flags & FL_ACM_PERMIT) ? TRUE : FALSE;
        }
    }
    return FALSE;

} /* check_rulelist */


/********************************************************************
* FUNCTION valnode_access_allowed
*
* Check if the specified user is allowed to access a value node
* The val->obj template will be checked against the val->editop
* requested access and the user's configured max-access
* 
* INPUTS:
*   msg == XML header from incoming message in progress
*   context == context to use
*   cache == cache for this session/message
*   user == user name string
*   val  == val_value_t in progress to check
*   newval  == newval val_value_t in progress to check (write only)
*   curval  == curval val_value_t in progress to check (write only)
*   iswrite == TRUE if a write access; FALSE if a read access
*   editop == edit operation if this is a write; ignored otherwise
*
* RETURNS:
*   TRUE if user allowed this level of access to the value node
*********************************************************************/
static boolean 
    valnode_access_allowed (xml_msg_hdr_t *msg,
                            agt_acm_context_t *context,
                            agt_acm_cache_t *cache,
                            const xmlChar *user,
                            val_value_t *val,
                            const val_value_t *newval,
                            const val_value_t *curval,
                            boolean iswrite,
                            op_editop_t editop)
{
    logfn_t logfn;

    /* check if this is a read or a write */
    if (iswrite) {
        logfn = (context->log_writes) ? log_debug2 : log_noop;
    } else {
        logfn = (context->log_reads) ? log_debug4 : log_noop;
    }

    status_t res = validate_context(context);
    if (res != NO_ERR) {
        (*logfn)("\nagt_acm: DENY (invalid context)");
        return FALSE;
    }

    /* make sure object is not the special node <config> */
    if (obj_is_root(val->obj)) {
        (*logfn)("\nagt_acm: PERMIT (root-node)");
        return TRUE;
    }

    /* ncx:user-write blocking has highest priority */
    if (iswrite) {
        switch (editop) {
        case OP_EDITOP_CREATE:
            if (obj_is_block_user_create(val->obj)) {
                (*logfn)("\nagt_acm: DENY (block-user-create)");
                return FALSE;
            }
            break;
        case OP_EDITOP_DELETE:
        case OP_EDITOP_REMOVE:
            if (obj_is_block_user_delete(val->obj)) {
                (*logfn)("\nagt_acm: DENY (block-user-delete)");
                return FALSE;
            }
            break;
        default:
            /* This is a merge or replace.  If blocked,
             * first make sure this requested operation is
             * also the effective operation; otherwise
             * the user will never be able to update a sub-node
             * of a list or container with update access blocked  */
            if (obj_is_block_user_update(val->obj)) {
                if (agt_apply_this_node(editop, newval, curval)) {
                    (*logfn)("\nagt_acm: DENY (block-user-update)");
                    return FALSE;
                }
            }
        }
    }

    /* super user is allowed to access anything except user-write blocked */
    if (agt_acm_is_superuser(user)) {
        (*logfn)("\nagt_acm: PERMIT (superuser)");
        return TRUE;
    }

    if (context->acmode == AGT_ACMOD_DISABLED) {
        (*logfn)("\nagt_acm: PERMIT (NACM disabled)");
        return TRUE;
    }

    /* figure out the access requested */
    access_t access;
    if (iswrite) {
        switch (editop) {
        case OP_EDITOP_NONE:
            return TRUE;   /* should not happen */
        case OP_EDITOP_MERGE:
        case OP_EDITOP_REPLACE:
            access = (curval) ? ACCESS_UPDATE : ACCESS_CREATE;
            break;
        case OP_EDITOP_CREATE:
            access = ACCESS_CREATE;
            break;
        case OP_EDITOP_DELETE:
            access = ACCESS_DELETE;
            break;
        case OP_EDITOP_LOAD:         /* internal enumeration */
            access = ACCESS_CREATE;
            break;
        case OP_EDITOP_COMMIT:
            access = ACCESS_CREATE;  /* should not happen */
            break;
        case OP_EDITOP_REMOVE:               /* base:1.1 only */
            access = ACCESS_DELETE;
            break;
        default:
            SET_ERROR(ERR_INTERNAL_VAL);
            return FALSE;
        }
    } else {
        access = ACCESS_READ;
    }

    /* check if access granted without any rules */
    if (check_mode(access, val->obj)) {
        (*logfn)("\nagt_acm: PERMIT (permissive mode)");
        return TRUE;
    }

    agt_acm_usergroups_t *usergroups = cache->usergroups;
    boolean retval = FALSE;
    boolean done = FALSE;
    const xmlChar *substr = NULL;

    if (usergroups->groupcount) {
        substr = y_ietf_netconf_acm_N_rule_list;
        retval = check_rulelist(msg, context, usergroups, val->obj, val,
                                access, AGT_ACM_RULE_DATA, &done);
    }
    if (!done) {
        substr = iswrite ? WRITE_DEFAULT : READ_DEFAULT;
        retval = get_default_data_response(context, val, iswrite);
    }

    if (iswrite) {
        (*logfn)("\nagt_acm: %s write (%s)", retval ? "PERMIT" : "DENY",
                 substr ? substr : NCX_EL_NONE);
    } else {
        (*logfn)("\nagt_acm: %s read (%s)", retval ? "PERMIT" : "DENY",
                 substr ? substr : NCX_EL_NONE);
    }

    return retval;

}   /* valnode_access_allowed */


/********************************************************************
* FUNCTION handle_group_edit
*
* Adjust the cache based on the current edit
* Just alter the 1 group that is changing
* Clear the user2group entries (context cache and session caches)
* 
* INPUTS:
*   editval == value node being edited 
*   editop == edit operation
*   name == name of group that is being edited
* RETURNS:
*   TRUE if user allowed this level of access to the value node
*********************************************************************/
static status_t 
    handle_group_edit (val_value_t *editval,
                       op_editop_t editop,
                       const xmlChar *name)
{
    agt_acm_context_t *context = get_context();
    val_value_t *listval = NULL;
    boolean islist = FALSE;
    agt_acm_group_t *group = NULL;

    status_t res = validate_context(context);
    if (res != NO_ERR) {
        return res;
    }

    /* get the group node */
    if (xml_strcmp(editval->name, y_ietf_netconf_acm_N_group)) {
        /* !!! get parent directly !!! */
        listval = editval->parent;
    } else {
        listval = editval;
        islist = TRUE;
    }
    if (listval == NULL) {
        return ERR_NCX_OPERATION_FAILED;
    }

    if (LOGDEBUG2) {
        log_debug2("\nClearing user-2-group entries in ACM cache");
    }

    /* clear all the session user-2-group entries */
    agt_ses_invalidate_session_acm_caches();
    clean_usergroups_cache(context);

    if (!islist || editop != OP_EDITOP_CREATE) {
        group = find_group(name, &context->groupQ);
    }

    if (editop == OP_EDITOP_DELETE && islist) {
        /* just delete the old list */
        if (group) {
            dlq_remove(group);
            free_group(group);
        }
    } else {
        /* add, delete, or replace the entry */
        agt_acm_group_t *newgroup = NULL;
        if (editop != OP_EDITOP_DELETE) {
            newgroup = get_group(listval, &res);
        }
        if (newgroup == NULL) {
            if (res == ERR_NCX_SKIPPED) {
                res = NO_ERR;
            }
            if (group != NULL) {
                dlq_remove(group);
                free_group(group);
            }
        } else {
            if (group != NULL) {
                dlq_swap(newgroup, group);
                free_group(group);
            } else {
                dlq_enque(newgroup, &context->groupQ);
            }
        }
    }
    return res;

}  /* handle_group_edit */


/********************************************************************
* FUNCTION handle_rulelist_edit
*
* Adjust the cache based on the current edit
* Just alter the 1 rulelist that is changing
* 
* INPUTS:
*   editval == value node being edited 
*   editop == edit operation
*   name == name of rulelist that is being edited
* RETURNS:
*   TRUE if user allowed this level of access to the value node
*********************************************************************/
static status_t 
    handle_rulelist_edit (val_value_t *editval,
                          op_editop_t editop,
                          const xmlChar *name)
{
    agt_acm_context_t *context = get_context();
    val_value_t *listval = editval;
    agt_acm_rulelist_t *rulelist = NULL;

    status_t res = validate_context(context);
    if (res != NO_ERR) {
        return res;
    }

    /* get the rulelist node */
    boolean done = FALSE;
    while (!done) {
        if (xml_strcmp(listval->name, y_ietf_netconf_acm_N_rule_list)) {
            /* !!! get parent directly !!! */
            listval = listval->parent;
            if (!listval || obj_is_root(listval->obj)) {
                return ERR_NCX_OPERATION_FAILED;
            }
        } else {
            done = TRUE;
        }
    }
    if (listval == NULL) {
        return ERR_NCX_OPERATION_FAILED;
    }

    /* flag if the rulelist node is changing or 1 of the descendant nodes */
    boolean islist = (listval == editval);

    if (LOGDEBUG2) {
        log_debug2("\nClearing rulelist entry '%s' in ACM cache", name);
    }

    if (!islist || editop != OP_EDITOP_CREATE) {
        rulelist = find_rulelist(name, &context->rulelistQ);
    }

    if (editop == OP_EDITOP_DELETE && islist) {
        /* just delete the old list */
        if (rulelist) {
            dlq_remove(rulelist);
            free_rulelist(rulelist);
        }
    } else {
        /* add, delete, or replace the entry */
        agt_acm_rulelist_t *newrulelist = NULL;
        if (editop != OP_EDITOP_DELETE) {
            newrulelist = new_rulelist(context, listval);
            if (newrulelist == NULL) {
                return ERR_INTERNAL_MEM;
            }
        }
        if (newrulelist == NULL) {
            if (rulelist != NULL) {
                dlq_remove(rulelist);
                free_rulelist(rulelist);
            }
        } else {
            if (rulelist != NULL) {
                dlq_swap(newrulelist, rulelist);
                free_rulelist(rulelist);
            } else {
                /*** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                 *** !!! need real insertion order !!!!
                 *** !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                 ***/
                dlq_enque(newrulelist, &context->rulelistQ);
            }
        }
    }
    return NO_ERR;

}  /* handle_rulelist_edit */


/***********************  SIL Callbacks  ***************************/


/********************************************************************
* FUNCTION get_deniedRpcs
*
* <get> operation handler for the nacm/denied-operations counter
*
* INPUTS:
*    see ncx/getcb.h getcb_fn_t for details
*
* RETURNS:
*    status
*********************************************************************/
static status_t 
    get_deniedRpcs (ses_cb_t *scb,
                    getcb_mode_t cbmode,
                    const val_value_t *virval,
                    val_value_t  *dstval)
{
    (void)scb;
    (void)virval;

    if (cbmode != GETCB_GET_VALUE) {
        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    VAL_UINT(dstval) = agt_acm_get_deniedRpcs();
    return NO_ERR;

} /* get_deniedRpcs */


/********************************************************************
* FUNCTION get_deniedDataWrites
*
* <get> operation handler for the nacm/denied-data-writes counter
*
* INPUTS:
*    see ncx/getcb.h getcb_fn_t for details
*
* RETURNS:
*    status
*********************************************************************/
static status_t 
    get_deniedDataWrites (ses_cb_t *scb,
                          getcb_mode_t cbmode,
                          const val_value_t *virval,
                          val_value_t  *dstval)
{
    (void)scb;
    (void)virval;

    if (cbmode != GETCB_GET_VALUE) {
        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    VAL_UINT(dstval) = agt_acm_get_deniedDataWrites();
    return NO_ERR;

} /* get_deniedDataWrites */


/********************************************************************
* FUNCTION get_deniedNotifications
*
* <get> operation handler for the nacm/denied-notifications counter
*
* INPUTS:
*    see ncx/getcb.h getcb_fn_t for details
*
* RETURNS:
*    status
*********************************************************************/
static status_t 
    get_deniedNotifications (ses_cb_t *scb,
                             getcb_mode_t cbmode,
                             const val_value_t *virval,
                             val_value_t  *dstval)
{
    (void)scb;
    (void)virval;

    if (cbmode != GETCB_GET_VALUE) {
        return ERR_NCX_OPERATION_NOT_SUPPORTED;
    }

    VAL_UINT(dstval) = agt_acm_get_deniedNotifications();
    return NO_ERR;

} /* get_deniedNotifications */


/********************************************************************
* FUNCTION nacm_callback
*
* top-level nacm callback function
* Path: /nacm
*
* INPUTS:
*    see agt/agt_cb.h  (agt_cb_pscb_t)
*
* RETURNS:
*    status
*********************************************************************/
static status_t 
    nacm_callback (ses_cb_t  *scb,
                    rpc_msg_t  *msg,
                    agt_cbtyp_t cbtyp,
                    op_editop_t  editop,
                    val_value_t  *newval,
                    val_value_t  *curval)
{
    if (editop == OP_EDITOP_LOAD) {
        return NO_ERR;
    }

    val_value_t *useval = NULL;

    (void)scb;
    (void)msg;

    agt_acm_context_t *context = get_context();

    if (newval != NULL) {
        useval = newval;
    } else if (curval != NULL) {
        useval = curval;
    }

    if (LOGDEBUG2) {
        log_debug2("\nServer %s callback: t: %s:%s, op:%s\n", 
                   agt_cbtype_name(cbtyp),
                   (useval) ? val_get_mod_name(useval) : NCX_EL_NONE,
                   (useval) ? useval->name : NCX_EL_NONE,
                   op_editop_name(editop));
    }

    boolean clear_cache = FALSE;
    boolean init_cache = FALSE;
    boolean move_counters = FALSE;
    status_t res = NO_ERR;

    /* don't care about the external-groups leaf yet */
    if (!xml_strcmp(useval->name, 
                    y_ietf_netconf_acm_N_enable_external_groups)) {
        return NO_ERR;
    }

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        break;
    case AGT_CB_APPLY:
        break;
    case AGT_CB_COMMIT:
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
            /* if /nacm or /nacm/groups was the target of a merge
             * then nothing changed if curval exists   */
            if (curval == NULL) {
                clear_cache = TRUE;
                init_cache = TRUE;
            }
            break;
        case OP_EDITOP_REPLACE:
            move_counters = TRUE;
            // fall-through
        case OP_EDITOP_DELETE:
        case OP_EDITOP_REMOVE:
        case OP_EDITOP_CREATE:
            clear_cache = TRUE;
            init_cache = TRUE;
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        clear_cache = TRUE;
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (clear_cache) {
        clean_context_cache(context);
        agt_ses_invalidate_session_acm_caches();
    }

    if (init_cache) {
        res = init_context_cache(context, FALSE);
    }

    if (res == NO_ERR && move_counters) {
        res = val_move_config_false(newval, curval);
    }

    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            (useval) ? NCX_NT_VAL : NCX_NT_NONE,
            useval,
            (useval) ? NCX_NT_VAL : NCX_NT_NONE,
            useval);
    }

    return res;

} /* nacm_callback */


/********************************************************************
* FUNCTION nacm_enable_nacm_callback
*
* /nacm/enable-nacm callback function
*
* INPUTS:
*    see agt/agt_cb.h  (agt_cb_pscb_t)
*
* RETURNS:
*    status
*********************************************************************/
static status_t 
    nacm_enable_nacm_callback (ses_cb_t  *scb,
                                rpc_msg_t  *msg,
                                agt_cbtyp_t cbtyp,
                                op_editop_t  editop,
                                val_value_t  *newval,
                                val_value_t  *curval)
{
    if (editop == OP_EDITOP_LOAD) {
        return NO_ERR;
    }

    (void)scb;
    (void)msg;

    if (LOGDEBUG2) {
        val_value_t *useval = NULL;
        if (newval != NULL) {
            useval = newval;
        } else if (curval != NULL) {
            useval = curval;
        }

        log_debug2("\nServer %s callback: t: %s:%s, op:%s\n", 
                   agt_cbtype_name(cbtyp),
                   (useval != NULL) ? 
                   val_get_mod_name(useval) : NCX_EL_NONE,
                   (useval != NULL) ? useval->name : NCX_EL_NONE,
                   op_editop_name(editop));
    }

    agt_acm_context_t *context = get_context();
    status_t res = validate_context(context);
    if (res != NO_ERR) {
        return res;
    }

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        break;
    case AGT_CB_APPLY:
        break;
    case AGT_CB_COMMIT:
        switch (editop) {
        case OP_EDITOP_LOAD:
        case OP_EDITOP_MERGE:
        case OP_EDITOP_REPLACE:
        case OP_EDITOP_CREATE:
            if (newval != NULL && VAL_BOOL(newval)) {
                if (context->acmode != AGT_ACMOD_ENFORCING) {
                    log_info("\nEnabling NACM Enforcing mode");
                    agt_acm_set_acmode(AGT_ACMOD_ENFORCING);
                    context->acmode = AGT_ACMOD_ENFORCING;
                }
            } else {
                log_warn("\nWarning: Disabling NACM");
                agt_acm_set_acmode(AGT_ACMOD_DISABLED);
                context->acmode = AGT_ACMOD_DISABLED;
            }
            break;
        case OP_EDITOP_DELETE:
        case OP_EDITOP_REMOVE:
            /* return NACM back to default == enabled */
            if (context->acmode != AGT_ACMOD_ENFORCING) {
                log_info("\nEnabling NACM Enforcing mode");
                agt_acm_set_acmode(AGT_ACMOD_ENFORCING);
                context->acmode = AGT_ACMOD_ENFORCING;
            }
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }
    
    return res;

} /* nacm_enable_nacm_callback */


/********************************************************************
* FUNCTION ietf_netconf_acm_nacm_read_default_edit
* 
* Edit database object callback
* Path: /nacm/read-default
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t ietf_netconf_acm_nacm_read_default_edit (
    ses_cb_t *scb,
    rpc_msg_t *msg,
    agt_cbtyp_t cbtyp,
    op_editop_t editop,
    val_value_t *newval,
    val_value_t *curval)
{
    if (editop == OP_EDITOP_LOAD) {
        return NO_ERR;
    }

    val_value_t *errorval = (curval) ? curval : newval;
    const xmlChar *newval_val = (newval) ? VAL_ENUM_NAME(newval) : 0;
    //const xmlChar *curval_val = (curval) ? VAL_ENUM_NAME(curval) : 0;

    if (LOGDEBUG2) {
        log_debug2("\nEnter ietf_netconf_acm_nacm_read_default_edit "
                   "callback for %s phase", agt_cbtype_name(cbtyp));
    }

    agt_acm_context_t *context = get_context();
    status_t res = validate_context(context);
    if (res != NO_ERR) {
        return res;
    }

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
        case OP_EDITOP_REPLACE:
        case OP_EDITOP_CREATE:
            if (newval_val && !xml_strcmp(newval_val, NCX_EL_PERMIT)) {
                context->dflags |= FL_ACM_DEFREAD_PERMIT;
            } else if (newval_val && !xml_strcmp(newval_val, NCX_EL_DENY)) {
                context->dflags &= ~FL_ACM_DEFREAD_PERMIT;
            } else if (newval_val) {
                log_info("\nagt_acm: Ignoring unknown value '%s'",
                         newval_val);
            } else {
                log_info("\nagt_acm: Missing value");
            }
            break;
        case OP_EDITOP_DELETE:
            /* set back to default -- should not happen */
            context->dflags |= FL_ACM_DEFREAD_PERMIT;
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            (errorval) ? NCX_NT_VAL : NCX_NT_NONE,
            errorval,
            (errorval) ? NCX_NT_VAL : NCX_NT_NONE,
            errorval);
    }
    return res;

} /* ietf_netconf_acm_nacm_read_default_edit */


/********************************************************************
* FUNCTION ietf_netconf_acm_nacm_write_default_edit
* 
* Edit database object callback
* Path: /nacm/write-default
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t ietf_netconf_acm_nacm_write_default_edit (
    ses_cb_t *scb,
    rpc_msg_t *msg,
    agt_cbtyp_t cbtyp,
    op_editop_t editop,
    val_value_t *newval,
    val_value_t *curval)
{
    if (editop == OP_EDITOP_LOAD) {
        return NO_ERR;
    }

    val_value_t *errorval = (curval) ? curval : newval;
    const xmlChar *newval_val = (newval) ? VAL_ENUM_NAME(newval) : 0;
    //const xmlChar *curval_val = (curval) ? VAL_ENUM_NAME(curval) : 0;

    if (LOGDEBUG2) {
        log_debug2("\nEnter ietf_netconf_acm_nacm_write_default_edit "
                   "callback for %s phase", agt_cbtype_name(cbtyp));
    }

    agt_acm_context_t *context = get_context();
    status_t res = validate_context(context);
    if (res != NO_ERR) {
        return res;
    }

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
        case OP_EDITOP_REPLACE:
        case OP_EDITOP_CREATE:
            if (newval_val && !xml_strcmp(newval_val, NCX_EL_PERMIT)) {
                context->dflags |= FL_ACM_DEFWRITE_PERMIT;
            } else if (newval_val && !xml_strcmp(newval_val, NCX_EL_DENY)) {
                context->dflags &= ~FL_ACM_DEFWRITE_PERMIT;
            } else if (newval_val) {
                log_info("\nagt_acm: Ignoring unknown value '%s'",
                         newval_val);
            } else {
                log_info("\nagt_acm: Missing value");
            }
            break;
        case OP_EDITOP_DELETE:
            /* set back to default -- should not happen */
            context->dflags &= ~FL_ACM_DEFWRITE_PERMIT;
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            (errorval) ? NCX_NT_VAL : NCX_NT_NONE,
            errorval,
            (errorval) ? NCX_NT_VAL : NCX_NT_NONE,
            errorval);
    }
    return res;

} /* ietf_netconf_acm_nacm_write_default_edit */


/********************************************************************
* FUNCTION ietf_netconf_acm_nacm_exec_default_edit
* 
* Edit database object callback
* Path: /nacm/exec-default
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t ietf_netconf_acm_nacm_exec_default_edit (
    ses_cb_t *scb,
    rpc_msg_t *msg,
    agt_cbtyp_t cbtyp,
    op_editop_t editop,
    val_value_t *newval,
    val_value_t *curval)
{
    if (editop == OP_EDITOP_LOAD) {
        return NO_ERR;
    }

    val_value_t *errorval = (curval) ? curval : newval;
    const xmlChar *newval_val = (newval) ? VAL_ENUM_NAME(newval) : 0;
    //const xmlChar *curval_val = (curval) ? VAL_ENUM_NAME(curval) : 0;

    if (LOGDEBUG2) {
        log_debug2("\nEnter ietf_netconf_acm_nacm_exec_default_edit "
                   "callback for %s phase", agt_cbtype_name(cbtyp));
    }

    agt_acm_context_t *context = get_context();
    status_t res = validate_context(context);
    if (res != NO_ERR) {
        return res;
    }

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
        case OP_EDITOP_REPLACE:
        case OP_EDITOP_CREATE:
            if (newval_val && !xml_strcmp(newval_val, NCX_EL_PERMIT)) {
                context->dflags |= FL_ACM_DEFEXEC_PERMIT;
            } else if (newval_val && !xml_strcmp(newval_val, NCX_EL_DENY)) {
                context->dflags &= ~FL_ACM_DEFEXEC_PERMIT;
            } else if (newval_val) {
                log_info("\nagt_acm: Ignoring unknown value '%s'",
                         newval_val);
            } else {
                log_info("\nagt_acm: Missing value");
            }
            break;
        case OP_EDITOP_DELETE:
            /* set back to default -- should not happen */
            context->dflags |= FL_ACM_DEFEXEC_PERMIT;
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            (errorval) ? NCX_NT_VAL : NCX_NT_NONE,
            errorval,
            (errorval) ? NCX_NT_VAL : NCX_NT_NONE,
            errorval);
    }
    return res;

} /* ietf_netconf_acm_nacm_exec_default_edit */


/********************************************************************
* FUNCTION ietf_netconf_acm_nacm_groups_group_edit
* 
* Edit database object callback
* Path: /nacm/groups/group
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t ietf_netconf_acm_nacm_groups_group_edit (
    ses_cb_t *scb,
    rpc_msg_t *msg,
    agt_cbtyp_t cbtyp,
    op_editop_t editop,
    val_value_t *newval,
    val_value_t *curval)
{
    if (editop == OP_EDITOP_LOAD) {
        return NO_ERR;
    }

    status_t res = NO_ERR;
    val_value_t *errorval = (curval) ? curval : newval;
    val_value_t *lastkey = NULL;
    const xmlChar *name = 
        VAL_STRING(agt_get_key_value(errorval, &lastkey));

    if (LOGDEBUG2) {
        log_debug2("\nEnter ietf_netconf_acm_nacm_groups_group_edit "
                   "callback for %s phase", agt_cbtype_name(cbtyp));
    }

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */

        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
        case OP_EDITOP_REPLACE:
        case OP_EDITOP_CREATE:
        case OP_EDITOP_DELETE:
            res = handle_group_edit(errorval, editop, name);
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            (errorval) ? NCX_NT_VAL : NCX_NT_NONE,
            errorval,
            (errorval) ? NCX_NT_VAL : NCX_NT_NONE,
            errorval);
    }
    return res;

} /* ietf_netconf_acm_nacm_groups_group_edit */


/********************************************************************
* FUNCTION ietf_netconf_acm_nacm_rule_list_edit
* 
* Edit database object callback
* Path: /nacm/rule-list
* Add object instrumentation in COMMIT phase.
* 
* INPUTS:
*     see agt/agt_cb.h for details
* 
* RETURNS:
*     error status
********************************************************************/
static status_t ietf_netconf_acm_nacm_rule_list_edit (
    ses_cb_t *scb,
    rpc_msg_t *msg,
    agt_cbtyp_t cbtyp,
    op_editop_t editop,
    val_value_t *newval,
    val_value_t *curval)
{
    if (editop == OP_EDITOP_LOAD) {
        return NO_ERR;
    }

    status_t res = NO_ERR;
    val_value_t *errorval = (curval) ? curval : newval;
    val_value_t *lastkey = NULL;
    const xmlChar *name = 
        VAL_STRING(agt_get_key_value(errorval, &lastkey));

    if (LOGDEBUG2) {
        log_debug2("\nEnter ietf_netconf_acm_nacm_rule_list_edit "
                   "callback for %s phase", agt_cbtype_name(cbtyp));
    }

    switch (cbtyp) {
    case AGT_CB_VALIDATE:
        /* description-stmt validation here */
        break;
    case AGT_CB_APPLY:
        /* database manipulation done here */
        break;
    case AGT_CB_COMMIT:
        /* device instrumentation done here */
        switch (editop) {
        case OP_EDITOP_LOAD:
            break;
        case OP_EDITOP_MERGE:
        case OP_EDITOP_REPLACE:
        case OP_EDITOP_CREATE:
        case OP_EDITOP_DELETE:
            res = handle_rulelist_edit(errorval, editop, name);
            break;
        default:
            res = SET_ERROR(ERR_INTERNAL_VAL);
        }
        break;
    case AGT_CB_ROLLBACK:
        /* undo device instrumentation here */
        break;
    default:
        res = SET_ERROR(ERR_INTERNAL_VAL);
    }

    if (res != NO_ERR) {
        agt_record_error(
            scb,
            &msg->mhdr,
            NCX_LAYER_CONTENT,
            res,
            NULL,
            (errorval) ? NCX_NT_VAL : NCX_NT_NONE,
            errorval,
            (errorval) ? NCX_NT_VAL : NCX_NT_NONE,
            errorval);
    }
    return res;

} /* ietf_netconf_acm_nacm_rule_list_edit */

/**************    E X T E R N A L   F U N C T I O N S **********/


/********************************************************************
* FUNCTION agt_acm_ietf_init1
* 
* Phase 1:
*   Load the yuma-nacm.yang module
* 
* INPUTS:
*   none
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
status_t 
    agt_acm_ietf_init1 (void)
{
    if (LOGDEBUG2) {
        log_debug2("\nmodel: ietf-netconf-acm.yang");
    }

    agt_acm_context_t *context = get_context();
    memset(context, 0x0, sizeof(agt_acm_context_t));

    /* init context variables */
    context->nacmmod = NULL;
    context->nacmrootval = NULL;
    dlq_createSQue(&context->rulelistQ);
    dlq_createSQue(&context->usergroupQ);
    dlq_createSQue(&context->groupQ);
    context->dflags = 0;
    context->acmode = 0;
    context->cache_valid = FALSE;
    context->log_reads = FALSE;
    context->log_writes = FALSE;
    context->reinit_fail_count = 0;

    agt_profile_t *profile = agt_get_profile();
    status_t res = NO_ERR;

    /* load in the access control module */
    res = ncxmod_load_module(AGT_ACM_IETF_MODULE,
                             y_ietf_netconf_acm_R_ietf_netconf_acm,
                             &profile->agt_savedevQ, 
                             &context->nacmmod);
    if (res != NO_ERR) {
        return res;
    }

    res = agt_cb_register_callback(AGT_ACM_IETF_MODULE, nacm_OID_nacm,
                                   NULL, nacm_callback);
    if (res != NO_ERR) {
        return res;
    }

    res = agt_cb_register_callback(AGT_ACM_IETF_MODULE, 
                                   nacm_OID_nacm_enable_nacm,
                                   NULL, nacm_enable_nacm_callback);
    if (res != NO_ERR) {
        return res;
    }

    res = agt_cb_register_callback(
        y_ietf_netconf_acm_M_ietf_netconf_acm,
        (const xmlChar *)"/nacm/read-default",
        y_ietf_netconf_acm_R_ietf_netconf_acm,
        ietf_netconf_acm_nacm_read_default_edit);
    if (res != NO_ERR) {
        return res;
    }

    res = agt_cb_register_callback(
        y_ietf_netconf_acm_M_ietf_netconf_acm,
        (const xmlChar *)"/nacm/write-default",
        y_ietf_netconf_acm_R_ietf_netconf_acm,
        ietf_netconf_acm_nacm_write_default_edit);
    if (res != NO_ERR) {
        return res;
    }

    res = agt_cb_register_callback(
        y_ietf_netconf_acm_M_ietf_netconf_acm,
        (const xmlChar *)"/nacm/exec-default",
        y_ietf_netconf_acm_R_ietf_netconf_acm,
        ietf_netconf_acm_nacm_exec_default_edit);
    if (res != NO_ERR) {
        return res;
    }

    res = agt_cb_register_callback(
        y_ietf_netconf_acm_M_ietf_netconf_acm,
        (const xmlChar *)"/nacm/groups/group",
        y_ietf_netconf_acm_R_ietf_netconf_acm,
        ietf_netconf_acm_nacm_groups_group_edit);
    if (res != NO_ERR) {
        return res;
    }

    res = agt_cb_register_callback(
        y_ietf_netconf_acm_M_ietf_netconf_acm,
        (const xmlChar *)"/nacm/rule-list",
        y_ietf_netconf_acm_R_ietf_netconf_acm,
        ietf_netconf_acm_nacm_rule_list_edit);
    if (res != NO_ERR) {
        return res;
    }


    return res;

}  /* agt_acm_ietf_init1 */


/********************************************************************
* FUNCTION agt_acm_ietf_init2
* 
* Phase 2:
*   Initialize the yuma-nacm.yang configuration data structures
* 
* INPUTS:
*   none
* RETURNS:
*   status of the initialization procedure
*********************************************************************/
status_t 
    agt_acm_ietf_init2 (void)
{
    status_t res = NO_ERR;
    boolean added = FALSE;
    agt_acm_context_t *context = get_context();

    context->nacmrootval = 
        agt_add_top_node_if_missing(context->nacmmod, 
                                    y_ietf_netconf_acm_N_nacm,
                                    &added, &res);
    if (res != NO_ERR || context->nacmrootval == NULL) {
        return res;
    }
    if (added) {
        /* add following leafs:

           leaf /nacm/enable-nacm
           leaf /nacm/read-default
           leaf /nacm/write-default
           leaf /nacm/exec-default
           leaf /nacm/enable-external-groups

           These values are saved in NV-store, even if the CLI config
           has disabled NACM.  TBD: what to do about operator thinking
           NACM is enabled but it is really turned off!!    */

        res = agt_set_val_defaults(context->nacmrootval, 
                                   context->nacmrootval->parent, NULL);
        if (res != NO_ERR) {
            return res;
        }
    }

    /* add read-only virtual leafs to the nacm value node
     * create /nacm/denied-operations
     */
    val_value_t *childval = 
        agt_make_virtual_leaf(context->nacmrootval->obj,
                              y_ietf_netconf_acm_N_denied_operations,
                              get_deniedRpcs, &res);
    if (childval != NULL) {
        val_add_child_sorted(childval, context->nacmrootval);
    }

    /* create /nacm/denied-data-writes */
    if (res == NO_ERR) {
        childval = 
            agt_make_virtual_leaf(context->nacmrootval->obj, 
                                  y_ietf_netconf_acm_N_denied_data_writes,
                                  get_deniedDataWrites, &res);
        if (childval != NULL) {
            val_add_child_sorted(childval, context->nacmrootval);
        }
    }

    /* create /nacm/denied-notifications */
    if (res == NO_ERR) {
        childval = 
            agt_make_virtual_leaf(context->nacmrootval->obj, 
                                  y_ietf_netconf_acm_N_denied_notifications,
                                  get_deniedNotifications, &res);
        if (childval != NULL) {
            val_add_child_sorted(childval, context->nacmrootval);
        }
    }

    /* make the initial rulelist cache unless <nacm> node is new */
    if (res == NO_ERR) {
        res = init_context_cache(context, added);
    }

    return res;

}  /* agt_acm_ietf_init2 */


/********************************************************************
* FUNCTION agt_acm_ietf_cleanup
* 
* Cleanup the yuma-nacm.yang access control module
* 
* INPUTS:
*   none
* RETURNS:
*   none
*********************************************************************/
void
    agt_acm_ietf_cleanup (void)
{

    agt_acm_context_t *context = get_context();
    clean_context_cache(context);

    memset(context, 0x0, sizeof(agt_acm_context_t));

    agt_cb_unregister_callbacks(AGT_ACM_IETF_MODULE, 
                                nacm_OID_nacm);

    agt_cb_unregister_callbacks(AGT_ACM_IETF_MODULE, 
                                nacm_OID_nacm_enable_nacm);

    agt_cb_unregister_callbacks(
        y_ietf_netconf_acm_M_ietf_netconf_acm,
        (const xmlChar *)"/nacm/read-default");

    agt_cb_unregister_callbacks(
        y_ietf_netconf_acm_M_ietf_netconf_acm,
        (const xmlChar *)"/nacm/write-default");

    agt_cb_unregister_callbacks(
        y_ietf_netconf_acm_M_ietf_netconf_acm,
        (const xmlChar *)"/nacm/exec-default");

    agt_cb_unregister_callbacks(
        y_ietf_netconf_acm_M_ietf_netconf_acm,
        (const xmlChar *)"/nacm/groups/group");

    agt_cb_unregister_callbacks(
        y_ietf_netconf_acm_M_ietf_netconf_acm,
        (const xmlChar *)"/nacm/rule-list");
    
}  /* agt_acm_ietf_cleanup */


/********************************************************************
* FUNCTION agt_acm_ietf_rpc_allowed
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
boolean 
    agt_acm_ietf_rpc_allowed (xml_msg_hdr_t *msg,
                              const xmlChar *user,
                              const obj_template_t *rpcobj)
{
    (void)user;
    agt_acm_context_t *context = get_context();
    status_t res = validate_context(context);
    if (res != NO_ERR) {
        if (LOGDEBUG2) {
            log_debug2("\nagt_acm: DENY (invalid context)");
        }
        return FALSE;
    }

    /* check if access granted without any rules */
    if (check_mode(ACCESS_EXEC, rpcobj)) {
        if (LOGDEBUG2) {
            log_debug2("\nagt_acm: PERMIT (permissive mode)");
        }
        return TRUE;
    }

    agt_acm_cache_t *cache = msg->acm_cache;
    if (!cache) {
        if (LOGDEBUG2) {
            log_debug2("\nagt_acm: DENY (NULL cache)");
        }
        return FALSE;
    }

    boolean retval = FALSE;
    boolean rulefound = FALSE;
    const xmlChar *substr = NULL;
    if (cache->usergroups->groupcount) {
        substr = y_ietf_netconf_acm_N_rule_list;
        retval = check_rulelist(NULL, context, cache->usergroups, 
                                rpcobj, NULL, ACCESS_EXEC, AGT_ACM_RULE_OP, 
                                &rulefound);
    }
    if (!rulefound) {
        substr = EXEC_DEFAULT;
        retval = get_default_rpc_response(context, rpcobj);
    }

    if (LOGDEBUG2) {
        log_debug2("\nagt_acm: %s (%s)", retval ? "PERMIT" : "DENY",
                   substr ? substr : NCX_EL_NONE);
    }

    return retval;

}   /* agt_acm_ietf_rpc_allowed */


/********************************************************************
* FUNCTION agt_acm_ietf_notif_allowed
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
boolean 
    agt_acm_ietf_notif_allowed (const xmlChar *user,
                                const obj_template_t *notifobj)
{
    agt_acm_context_t *context = get_context();

    logfn_t logfn = (context->log_reads) ? log_debug2 : log_noop;

    status_t res = validate_context(context);
    if (res != NO_ERR) {
        (*logfn)("\nagt_acm: DENY (invalid context)");
        return FALSE;
    }

    /* check if access granted without any rules */
    if (check_mode(ACCESS_READ, notifobj)) {
        (*logfn)("\nagt_acm: PERMIT (permissive mode)");
        return TRUE;
    }

    agt_acm_usergroups_t *usergroups = 
        get_usergroups_entry(context, user);
    if (!usergroups) {
        /* out of memory! deny all access! */
        (*logfn)("\nagt_acm: DENY (out of memory)");
        return FALSE;
    }

    boolean retval = FALSE;
    boolean rulefound = FALSE;
    const xmlChar *substr = NULL;
    if (usergroups->groupcount) {
        substr = y_ietf_netconf_acm_N_rule_list;
        retval = check_rulelist(NULL, context, usergroups, notifobj, NULL,
                                ACCESS_READ, AGT_ACM_RULE_NOTIF, 
                                &rulefound);
    }
    if (!rulefound) {
        substr = READ_DEFAULT;
        retval = get_default_notif_response(context, notifobj);
    }

    (*logfn)("\nagt_acm: %s (%s)", retval ? "PERMIT" : "DENY",
             substr ? substr : NCX_EL_NONE);

    return retval;

}   /* agt_acm_ietf_notif_allowed */


/********************************************************************
* FUNCTION agt_acm_ietf_val_write_allowed
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
boolean 
    agt_acm_ietf_val_write_allowed (xml_msg_hdr_t *msg,
                                    const xmlChar *user,
                                    val_value_t *newval,
                                    val_value_t *curval,
                                    op_editop_t editop)
{
    agt_acm_context_t *context = get_context();
    val_value_t *val = (newval) ? newval : curval;
    return valnode_access_allowed(msg, context, msg->acm_cache, user, val, 
                                  newval, curval, TRUE, editop);

} /* agt_acm_ietf_write_allowed */


/********************************************************************
* FUNCTION agt_acm_ietf_val_read_allowed
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
boolean 
    agt_acm_ietf_val_read_allowed (xml_msg_hdr_t *msg,
                                   const xmlChar *user,
                                   val_value_t *val)
{
    agt_acm_context_t *context = get_context();
    return valnode_access_allowed(msg, context, msg->acm_cache, user, val, 
                                  NULL, NULL, FALSE, OP_EDITOP_NONE);

}   /* agt_acm_ietf_val_read_allowed */


/********************************************************************
* FUNCTION agt_acm_ietf_init_msg_cache
*
* Malloc and initialize an agt_acm_cache_t struct
* and attach it to the incoming message
*
* INPUTS:
*   scb == session control block to use
*   msg == message to use
*
* OUTPUTS:
*   scb->acm_cache pointer may be set, if it was NULL
*   msg->acm_cache pointer set
*
* RETURNS:
*   status
*********************************************************************/
status_t
    agt_acm_ietf_init_msg_cache (ses_cb_t *scb,
                                 xml_msg_hdr_t *msg)
{

    msg->acm_cbfn = agt_acm_val_read_allowed;

    if (agt_acm_session_cache_valid(scb)) {
        msg->acm_cache = scb->acm_cache;
    } else {
        free_acm_cache(scb->acm_cache);
        agt_acm_context_t *context = get_context();
        scb->acm_cache = new_acm_cache(context, SES_MY_USERNAME(scb));
        msg->acm_cache = scb->acm_cache;
    }

    if (msg->acm_cache == NULL) {
        return ERR_INTERNAL_MEM;
    } else {
        return NO_ERR;
    }

} /* agt_acm_ietf_init_msg_cache */


/********************************************************************
* FUNCTION agt_acm_ietf_clear_session_cache
*
* Clear an agt_acm_cache_t struct in a session control block
*
* INPUTS:
*   scb == sesion control block to use
*
* OUTPUTS:
*   scb->acm_cache pointer is freed and set to NULL
*
*********************************************************************/
void agt_acm_ietf_clear_session_cache (ses_cb_t *scb)
{
    free_acm_cache(scb->acm_cache);
    scb->acm_cache = NULL;
} /* agt_acm_ietf_clear_session_cache */


/********************************************************************
* FUNCTION agt_acm_ietf_invalidate_session_cache
*
* Invalidate an agt_acm_cache_t struct in a session control block
*
* INPUTS:
*   scb == sesion control block to use
*
* OUTPUTS:
*   scb->acm_cache pointer is freed and set to NULL
*
*********************************************************************/
void agt_acm_ietf_invalidate_session_cache (ses_cb_t *scb)
{
    agt_acm_cache_t *cache = scb->acm_cache;
    if (cache) {
        cache->isvalid = FALSE;
    }

} /* agt_acm_ietf_invalidate_session_cache */


/********************************************************************
* FUNCTION agt_acm_ietf_session_cache_valid
*
* Check if a session ACM cache is valid
*
* INPUTS:
*   scb == sesion control block to check
*
* RETURNS:
*   TRUE if cache calid
*   FALSE if cache invalid or NULL
*********************************************************************/
boolean agt_acm_ietf_session_cache_valid (const ses_cb_t *scb)
{
    agt_acm_cache_t *cache = scb->acm_cache;
    if (cache) {
        return cache->isvalid;
    }
    return FALSE;
} /* agt_acm_ietf_session_cache_valid */


/********************************************************************
* FUNCTION agt_acm_ietf_clean_xpath_cache
*
* Clean any cached XPath results because the data rule results
* may not be valid anymore.
*
*********************************************************************/
void
    agt_acm_ietf_clean_xpath_cache (void)
{
    agt_acm_context_t *context = get_context();

    if (LOGDEBUG3) {
        log_debug3("\nxpath_backptr: clean XPath cache started");
    }

    /* go through all the rulelists, rules looking for data rules */
    agt_acm_rulelist_t *rulelist = (agt_acm_rulelist_t *)
        dlq_firstEntry(&context->rulelistQ);
    for (; rulelist != NULL;
         rulelist = (agt_acm_rulelist_t *)dlq_nextEntry(rulelist)) {

        agt_acm_rule_t *rule = (agt_acm_rule_t *)
            dlq_firstEntry(&rulelist->ruleQ);
        for (; rule != NULL; rule = (agt_acm_rule_t *)dlq_nextEntry(rule)) {
            if (rule->pcb) {
                /* this is a data rule; free any cached XPath results */
                ncx_cfg_t cfgid = 0;
                if (LOGDEBUG2) {
                    log_debug2("\nxpath_backptr: clean XPath results for "
                               "%s in rule %s",
                               rule->pcb->exprstr, rule->name);
                }

                for (; cfgid < CFG_NUM_STATIC; cfgid++) {
                    xpath_free_result(rule->result[cfgid]);
                    rule->result[cfgid] = NULL;
                    rule->result_msgid[cfgid] = 0;
                }
            }
        }
    }
}  /* agt_acm_ietf_clean_xpath_cache */


/* END file agt_acm_ietf.c */
