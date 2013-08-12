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
/*  FILE: agt_connect.c

    Handle the <ncx-connect> (top-level) element.
    This message is used for thin clients to connect
    to the ncxserver. 

   Client --> SSH2 --> OpenSSH.subsystem(netconf) -->
 
      ncxserver_connect --> AF_LOCAL/ncxserver.sock -->

      ncxserver.listen --> top_dispatch -> ncx_connect_handler

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
15jan07      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "procdefs.h"
#include "agt.h"

#ifdef WITH_CLI
//#include "agt_cli_proto.h"
#endif

#include "agt_connect.h"
#include "agt_hello.h"

#ifdef WITH_YANGAPI
#include "agt_yangapi.h"
#endif

#include "agt_rpcerr.h"
#include "agt_ses.h"
#include "agt_state.h"
#include "agt_sys.h"
#include "agt_util.h"
#include "cap.h"
#include "cfg.h"
#include "log.h"
#include "ncx.h"
#include "ncx_num.h"

#ifdef WITH_YANGAPI
#include "yangapi.h"
#endif

#include "ses.h"
#include "status.h"
#include "top.h"
#include "val.h"


/********************************************************************
*                                                                   *
*                       C O N S T A N T S                           *
*                                                                   *
*********************************************************************/


/* ncxserver magic cookie hack
 * this must match the definition in subsys/subsystem.c
 * It is purposely different than the string in Yuma
 */
#define NCX_SERVER_MAGIC \
  "x56o8937ab1erfgertgertb99r9tgb9rtyb99rtbwdesd9902k40vfrevef0Opal1t2p"



/********************************************************************
*                                                                   *
*                       V A R I A B L E S                           *
*                                                                   *
*********************************************************************/
static boolean agt_connect_init_done = FALSE;

/********************************************************************
* FUNCTION agt_connect_init
*
* Initialize the agt_connect module
* Adds the agt_connect_dispatch function as the handler
* for the NCX <ncx-connect> top-level element.
*
* INPUTS:
*   none
* RETURNS:
*   NO_ERR if all okay, the minimum spare requests will be malloced
*********************************************************************/
status_t 
    agt_connect_init (void)
{
    status_t  res;

    if (!agt_connect_init_done) {
        res = top_register_node(NCX_MODULE, 
                                NCX_EL_NCXCONNECT, 
                                agt_connect_dispatch);
        if (res != NO_ERR) {
            return res;
        }
        agt_connect_init_done = TRUE;
    }
    return NO_ERR;

} /* agt_connect_init */


/********************************************************************
* FUNCTION agt_connect_cleanup
*
* Cleanup the agt_connect module.
* Unregister the top-level NCX <ncx-connect> element
*
*********************************************************************/
void 
    agt_connect_cleanup (void)
{
    if (agt_connect_init_done) {
        top_unregister_node(NCX_MODULE, NCX_EL_NCXCONNECT);
        agt_connect_init_done = FALSE;
    }

} /* agt_connect_cleanup */


/********************************************************************
* FUNCTION agt_connect_dispatch
*
* Handle an incoming <ncx-connect> request
*
* INPUTS:
*   scb == session control block
*   top == top element descriptor
*********************************************************************/
void 
    agt_connect_dispatch (ses_cb_t *scb,
                          xml_node_t *top)
{
    assert( scb && "scb is NULL!" );
    assert( top && "top is NULL!" );

    log_debug("\nagt_connect: got node");

    status_t res = NO_ERR;
    xml_attr_t      *attr;
    ncx_num_t        num;

    /* make sure 'top' is the right kind of node */
    if (top->nodetyp != XML_NT_EMPTY) {
        log_error("\nError: expected empty element");
        res = ERR_NCX_WRONG_NODETYP;
        /* TBD: stats update */
    }

    /* only process this message in session init state */
    if (res==NO_ERR && scb->state != SES_ST_INIT) {
        log_error("\nError: session not in init state");
        /* TBD: stats update */
        res = ERR_NCX_NO_ACCESS_STATE;
    } else {
        scb->state = SES_ST_IN_MSG;
    }

    /* check the ncxserver version */
    if (res == NO_ERR) {
        attr = xml_find_attr(top, 0, NCX_EL_VERSION);
        if (attr && attr->attr_val) {
            res = ncx_convert_num(attr->attr_val, NCX_NF_DEC,
                                  NCX_BT_UINT32, &num);
            if (res == NO_ERR) {
                if (num.u != NCX_SERVER_VERSION) {
                    log_info("\nagt_connect error: wrong ncx-connect "
                             "msg version");
                    res = ERR_NCX_WRONG_VERSION;
                }
            } else {
                log_info("\nagt_connect error: invalid number '%s' (%s)",
                          attr->attr_val, get_error_string(res));
            }
            if (res == NO_ERR && LOGDEBUG3) {
                log_debug3("\nagt_connect: got valid version attr");
            }
        } else {
            log_info("\nagt_connect error: missing version attr in "
                     "ncx-connect msg");
            res = ERR_NCX_MISSING_ATTR;
        }
    }

    /* check the magic password string */
    if (res == NO_ERR) {
        attr = xml_find_attr(top, 0, NCX_EL_MAGIC);
        if (attr && attr->attr_val) {
            if (xml_strcmp(attr->attr_val, 
                           (const xmlChar *)NCX_SERVER_MAGIC)) {
                log_info("\nagt_connect error: wrong ncx-connect msg magic");
                res = ERR_NCX_ACCESS_DENIED;
            } else if (LOGDEBUG3) {
                log_debug3("\nagt_connect: got valid magic attr");
            }
        } else {
            log_info("\nagt_connect error: missing magic attr in "
                     "ncx-connect msg");
            res = ERR_NCX_MISSING_ATTR;
        }
    }

    agt_profile_t *profile = agt_get_profile();
    boolean proto_enabled = TRUE;

    /* check the transport */
    if (res == NO_ERR) {
        attr = xml_find_attr(top, 0, NCX_EL_TRANSPORT);
        if (attr && attr->attr_val) {
            log_debug("\nagt_connect: transport='%s'", attr->attr_val);

            /* check the default -- SSH */
            if ( 0 == xml_strcmp(attr->attr_val, 
                           (const xmlChar *)NCX_SERVER_TRANSPORT)) {

                proto_enabled = profile->agt_use_netconf;

                /* transport indicates an external connection over
                 * ssh, check the ncxserver port number */
                attr = xml_find_attr(top, 0, NCX_EL_PORT);
                if (attr && attr->attr_val) {
                    res = ncx_convert_num(attr->attr_val, NCX_NF_DEC,
                                          NCX_BT_UINT16, &num);
                    if (res == NO_ERR) {
                        if (!agt_ses_ssh_port_allowed((uint16)num.u)) {
                            log_info("\nagt_connect error: port %u not enabled",
                                      num.u);
                            res = ERR_NCX_ACCESS_DENIED;
                        }
                    } else {
                        log_info("\nagt_connect error: invalid port "
                                 "number '%s' (%s)",
                                  attr->attr_val, get_error_string(res));
                    }
                    if (res == NO_ERR && LOGDEBUG3) {
                        log_debug3("\nagt_connect: got valid port attr");
                    }
                } else {
                    log_info("\nagt_connect error: missing port attr "
                             "in ncx-connect msg");
                    res = ERR_NCX_MISSING_ATTR;
                }
            }
            else if ( 0 == xml_strcmp(attr->attr_val, 
                           (const xmlChar *)NCX_SERVER_TRANSPORT_LOCAL)) {
                /* transport is local -- treated as if it is SSH
                 * and ignore the port number  */
                proto_enabled = profile->agt_use_local_transport;
            } else if ( 0 == xml_strcmp(attr->attr_val, 
                           (const xmlChar *)NCX_SERVER_TRANSPORT_HTTP)) {
                /* transport is HTTP/REST */
                //scb->transport = SES_TRANSPORT_HTTP;
                /* assuming protocol instead of checking attribute! */

#ifdef WITH_YANGAPI
                proto_enabled = profile->agt_use_yangapi;
                scb->protocol = NCX_PROTO_YUMA_YANGAPI;
                if (proto_enabled) {
                    scb->rcb = yangapi_new_rcb();
                    if (scb->rcb == NULL) {
                        log_error("\nError: cannot malloc YANG-API msg");
                        res = ERR_INTERNAL_MEM;
                    }
                } else {
                    log_info("\nagt_connect error: YANG-API disabled\n");
                    res = ERR_NCX_ACCESS_DENIED;
                }
#else
                proto_enabled = FALSE;
#endif  // WITH_YANGAPI

            } else if ( 0 == xml_strcmp(attr->attr_val, 
                           (const xmlChar *)NCX_SERVER_TRANSPORT_CLI)) {

#ifdef WITH_CLI
                /* transport is internal CLI */
                scb->transport = SES_TRANSPORT_CLI;
                /* assuming protocol instead of checking attribute! */
                scb->protocol = NCX_PROTO_YUMA_CLI;
                proto_enabled = profile->agt_use_cli;
#else
                proto_enabled = FALSE;
#endif  // WITH_CLI

            } else {
                /* transport is unsupported */
                log_info("\nagt_connect error: transport '%s' not supported",
                          attr->attr_val);
                res = ERR_NCX_ACCESS_DENIED;
            }
        } else {
            log_info("\nagt_connect error: missing transport attr "
                     "in ncx-connect msg");
            res = ERR_NCX_MISSING_ATTR;
        }
    }

    if (res == NO_ERR && !proto_enabled) {
        log_info("\nagt_connect error: protocol not enabled");
        res = ERR_NCX_ACCESS_DENIED;
    }

    /* get the username */
    if (res == NO_ERR) {
        attr = xml_find_attr(top, 0, NCX_EL_USER);
        if (attr && attr->attr_val) {
            scb->username = xml_strdup(attr->attr_val);
            if (!scb->username) {
                res = ERR_INTERNAL_MEM;
            }
        } else {
            log_info("\nagt_connect error: missing user attr in "
                     "ncx-connect msg");
            res = ERR_NCX_MISSING_ATTR;
        }
    }

    /* get the client address */
    if (res == NO_ERR) {
        attr = xml_find_attr(top, 0, NCX_EL_ADDRESS);
        if (attr && attr->attr_val) {
            scb->peeraddr = xml_strdup(attr->attr_val);
            if (!scb->peeraddr) {
                res = ERR_INTERNAL_MEM;
            }
        } else {
            log_info("\nagt_connect error: missing address attr in "
                     "ncx-connect msg");
            res = ERR_NCX_MISSING_ATTR;
        }
    }

#ifdef WITH_YANGAPI
    if (res == NO_ERR && scb->protocol == NCX_PROTO_YUMA_YANGAPI) {
        /* get the extra attributes in the connect message
         * this list matches the attributes defined in the
         * <ncx-connect> message in:
         *   subsys/subsystem.c:send_yangapi_connect function
         */
        attr = xml_find_attr(top, 0, NCX_EL_METHOD);
        if (attr && attr->attr_val) {
            scb->rcb->request_method = xml_strdup(attr->attr_val);
            if (!scb->rcb->request_method) {
                res = ERR_INTERNAL_MEM;
            }
        } else {
            res = ERR_NCX_MISSING_ATTR;
        }
        if (res == NO_ERR) {
            attr = xml_find_attr(top, 0, NCX_EL_URI);
            if (attr && attr->attr_val) {
                scb->rcb->request_uri = xml_strdup(attr->attr_val);
                if (!scb->rcb->request_uri) {
                    res = ERR_INTERNAL_MEM;
                }
            } else {
                res = ERR_NCX_MISSING_ATTR;
            }
        }
        if (res == NO_ERR) {
            attr = xml_find_attr(top, 0, NCX_EL_TYPE);
            if (attr && attr->attr_val && 
                xml_strcmp(attr->attr_val, NCX_EL_NONE)) {
                scb->rcb->content_type = xml_strdup(attr->attr_val);
                if (!scb->rcb->content_type) {
                    res = ERR_INTERNAL_MEM;
                }
            }
        }
        if (res == NO_ERR) {
            attr = xml_find_attr(top, 0, NCX_EL_LENGTH);
            if (attr && attr->attr_val && 
                xml_strcmp(attr->attr_val, (const xmlChar *)"0")) {
                scb->rcb->content_length = xml_strdup(attr->attr_val);
                if (!scb->rcb->content_length) {
                    res = ERR_INTERNAL_MEM;
                }
            }
        }
        if (res == NO_ERR) {
            attr = xml_find_attr(top, 0, NCX_EL_SINCE);
            if (attr && attr->attr_val && 
                xml_strcmp(attr->attr_val, NCX_EL_NONE)) {
                scb->rcb->if_modified_since = xml_strdup(attr->attr_val);
                if (!scb->rcb->if_modified_since) {
                    res = ERR_INTERNAL_MEM;
                }
            }
        }
        if (res == NO_ERR) {
            attr = xml_find_attr(top, 0, NCX_EL_ACCEPT);
            if (attr && attr->attr_val && 
                xml_strcmp(attr->attr_val, NCX_EL_NONE)) {
                scb->rcb->accept = xml_strdup(attr->attr_val);
                if (!scb->rcb->accept) {
                    res = ERR_INTERNAL_MEM;
                }
            }
        }
        if (res == NO_ERR) {
            attr = xml_find_attr(top, 0, NCX_EL_MATCH);
            if (attr && attr->attr_val && 
                xml_strcmp(attr->attr_val, NCX_EL_NONE)) {
                scb->rcb->if_match = xml_strdup(attr->attr_val);
                if (!scb->rcb->if_match) {
                    res = ERR_INTERNAL_MEM;
                }
            }
        }
        if (res == NO_ERR) {
            attr = xml_find_attr(top, 0, NCX_EL_NOMATCH);
            if (attr && attr->attr_val && 
                xml_strcmp(attr->attr_val, NCX_EL_NONE)) {
                scb->rcb->if_none_match = xml_strdup(attr->attr_val);
                if (!scb->rcb->if_none_match) {
                    res = ERR_INTERNAL_MEM;
                }
            }
        }
        if (res == NO_ERR) {
            attr = xml_find_attr(top, 0, NCX_EL_NOSINCE);
            if (attr && attr->attr_val && 
                xml_strcmp(attr->attr_val, NCX_EL_NONE)) {
                scb->rcb->if_unmodified_since = xml_strdup(attr->attr_val);
                if (!scb->rcb->if_unmodified_since) {
                    res = ERR_INTERNAL_MEM;
                }
            }
        }
    }
#endif  // WITH_YANGAPI

    if (res == NO_ERR) {
        res = agt_state_add_session(scb);
    }

    if (res == NO_ERR) {
        /* bump the session state and send the agent hello message */

        if (scb->protocol == NCX_PROTO_YUMA_YANGAPI) {
            /* YANGAPI does not use <hello> message; process request
             * and exit */
#ifdef WITH_YANGAPI
            if (scb->rcb->content_length == NULL ||
                !xml_strcmp(scb->rcb->content_length,
                            (const xmlChar *)"0")) {
                /* process this message right now */
                scb->state = SES_ST_IN_MSG;
            } else {
                /* get the content into a ses_msg buffer then process */
                scb->state = SES_ST_IDLE;
            }
#endif  // WITH_YANGAPI
        } else {
            /* add the session to the netconf-state DM
               assume NETCONF, expects hello */
            res = agt_hello_send(scb);
            if (res == NO_ERR) {
                scb->state = SES_ST_HELLO_WAIT;
            } else {
                agt_state_remove_session(scb->sid);
            }
        }
    }

    /* report first error and close session */
    if (res != NO_ERR) {
        agt_ses_request_close(scb, scb->sid, SES_TR_BAD_START);
        if (LOGINFO) {
            log_info("\nagt_connect error (%s)\n"
                     "  dropping session %d",
                     get_error_string(res), scb->sid);
        }
    } else {
        log_debug("\nagt_connect: msg ok");
        agt_sys_send_sysSessionStart(scb);

#ifdef WITH_YANGAPI
        if (scb->protocol == NCX_PROTO_YUMA_YANGAPI &&
            scb->state == SES_ST_IN_MSG) {
            agt_yangapi_dispatch(scb);
        }
#endif  // WITH_YANGAPI

    }
    
} /* agt_connect_dispatch */


/* END file agt_connect.c */
