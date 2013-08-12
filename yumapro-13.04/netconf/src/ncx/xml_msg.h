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
#ifndef _H_xml_msg
#define _H_xml_msg
/*  FILE: xml_msg.h
*********************************************************************
*                                                                   *
*                         P U R P O S E                             *
*                                                                   *
*********************************************************************

   XML Message send and receive

   Deals with generic namespace and xmlns optimization
   and tries to keep changing the default namespace so
   most nested elements do not have prefixes

   Deals with the NETCONF requirement that the attributes
   in <rpc> are returned in <rpc-reply> unchanged.  Although
   XML allows the xnmlns prefixes to change, the same prefixes
   are used in the <rpc-reply> that the NMS provided in the <rpc>.

   NOTE: The xml_msg_hst_t strust is used for both XML and JSON encoding
   It is no longer XML-specific!!!

*********************************************************************
*                                                                   *
*                   C H A N G E         H I S T O R Y               *
*                                                                   *
*********************************************************************

date             init     comment
----------------------------------------------------------------------
14-jan-07    abb      Begun; split from agt_rpc.h
28-feb-13    abb      Add JSON, <get2>, and YANG-API support
*/

#ifndef _H_ncxtypes
#include "ncxtypes.h"
#endif

#ifndef _H_status
#include "status.h"
#endif

#ifndef _H_val
#include "val.h"
#endif

#ifndef _H_xmlns
#include "xmlns.h"
#endif

#ifndef _H_xml_util
#include "xml_util.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*                                                                   *
*                         C O N S T A N T S                         *
*                                                                   *
*********************************************************************/
/* xml_msg_hdr_t flags field bit definitions */


/* if PREFIX set then prefixes will be used for
 * element start and end tags. FALSE then default NS
 * will be used so no prefixes will be needed except
 * for XML content with QNames
 */
#define XML_MSG_FL_PREFIX     bit0

/* indicates that EntityTags (etag attribute) should be generated */
#define XML_MSG_FL_ETAGS      bit1

/* indicates the ETag test sense 1= if_match, 0= if_none_match */
#define XML_MSG_FL_ETAGS_TEST  bit2

/* indicates that Last-Modified timestamps (last-modified attribute)
 * should be generated 
 */
#define XML_MSG_FL_TSTAMPS    bit3

/* indicates the Tstamps test sense 1= modified_since, 0= unmodified_since */
#define XML_MSG_FL_TSTAMPS_TEST  bit4

/* indicates that keys-only is selected for the retrieval output */
#define XML_MSG_FL_KEYS       bit5



/* PREFIXES */
#define XML_MSG_USE_PREFIX(M)   ((M)->flags & XML_MSG_FL_PREFIX)

#define XML_MSG_NO_PREFIX(M)    ((M)->flags &= ~XML_MSG_FL_PREFIX)


/* ETAGS */
#define XML_MSG_WITH_ETAGS(M)   ((M)->flags & XML_MSG_FL_ETAGS)

#define XML_MSG_WITH_ETAGS_TEST(M)   ((M)->flags & XML_MSG_FL_ETAGS_TEST)

#define XML_MSG_SET_WITH_ETAGS(M) (M)->flags |= XML_MSG_FL_ETAGS

#define XML_MSG_SET_WITH_ETAGS_TEST(M) (M)->flags |= XML_MSG_FL_ETAGS_TEST

#define XML_MSG_CLR_WITH_ETAGS_TEST(M) (M)->flags &= ~XML_MSG_FL_ETAGS_TEST


/* TIMESTAMPS */
#define XML_MSG_WITH_TSTAMPS(M) ((M)->flags & XML_MSG_FL_TSTAMPS)

#define XML_MSG_WITH_TSTAMPS_TEST(M) ((M)->flags & XML_MSG_FL_TSTAMPS_TEST)

#define XML_MSG_SET_WITH_TSTAMPS(M) (M)->flags |= XML_MSG_FL_TSTAMPS

#define XML_MSG_SET_WITH_TSTAMPS_TEST(M) (M)->flags |= XML_MSG_FL_TSTAMPS_TEST

#define XML_MSG_CLR_WITH_TSTAMPS_TEST(M) (M)->flags &= ~XML_MSG_FL_TSTAMPS_TEST


/* KEYS-ONLY (TBD) */
#define XML_MSG_KEYS_ONLY(M)    ((M)->flags & XML_MSG_FL_KEYS)

#define XML_MSG_SET_KEYS_ONLY(M)  (M)->flags |= XML_MSG_FL_KEYS



/********************************************************************
*                                                                   *
*                         T Y P E S                                 *
*                                                                   *
*********************************************************************/

/* Common XML Message Header */
typedef struct xml_msg_hdr_t_ {

    uint16             flags;

    ncx_withdefaults_t withdef;           /* with-defaults value */

    /* prefixQ: (incoming)
     * All the namespace decls that were in the <rpc>
     * request are used in the <rpc-reply>, so the same prefixes
     * will be used, and the XML on the wire will be easier to debug
     * by examining packet traces
     */
    dlq_hdr_t          prefixQ;             /* Q of xmlns_pmap_t */

    dlq_hdr_t          errQ;               /* Q of rpc_err_rec_t */

    /* agent access control for database reads and writes;
     * !!! shadow pointer to per-session cache, not malloced
     */
    struct agt_acm_cache_t_ *acm_cache;

    /* incremented on every initialized message header
     * rolls over at MAX_UINT32 back to zero
     * used by agt_acm_ietf to cache message-ids to
     * speed up data rule processing for read requests
     */
    uint32                   msgid;

    /* the xml_msg_set_cfgid function is used by the retrieval
     * operation (e.g., <get>, <get-config> to set the cfgid field
     */
    ncx_cfg_t                cfgid;
    boolean                  cfgid_valid;

    /* agent access control read authorization 
     * callback function: xml_msg_authfn_t
     */
    void                    *acm_cbfn;

    uint32             max_depth;    /* max-depth value, 0=ignore */
    uint32             cur_depth;          /* current depth value */

    time_t              match_tstamp;
    ncx_etag_t          match_etag;
    ncx_display_mode_t  input_encoding;
    ncx_display_mode_t  output_encoding;
} xml_msg_hdr_t;


/* read authorization callback template */
typedef boolean (*xml_msg_authfn_t) (xml_msg_hdr_t *msg,
				     const xmlChar *username,
				     const val_value_t *val);


				      
/********************************************************************
*                                                                   *
*                        F U N C T I O N S                          *
*                                                                   *
*********************************************************************/


/********************************************************************
* FUNCTION xml_msg_init_hdr
*
* Initialize a new xml_msg_hdr_t struct
*
* INPUTS:
*   msg == xml_msg_hdr_t memory to initialize
* RETURNS:
*   none
*********************************************************************/
extern void
    xml_msg_init_hdr (xml_msg_hdr_t *msg);


/********************************************************************
* FUNCTION xml_msg_clean_hdr
*
* Clean all the memory used by the specified xml_msg_hdr_t
* but do not free the struct itself
*
* Do NOT reuse this header without calling xml_msg_init_hdr first!!!
* xml_msg_t headers were originally meant to be used
* only in other messages, but <hello> messages use the hdr
* without a wrapper message
*
* INPUTS:
*   msg == xml_msg_hdr_t to clean
* RETURNS:
*   none
*********************************************************************/
extern void
    xml_msg_clean_hdr (xml_msg_hdr_t *msg);


/********************************************************************
* FUNCTION xml_msg_get_prefix
*
* Find the namespace prefix for the specified namespace ID
* If it is not there then create one
*
* INPUTS:
*    msg  == message to search
*    parent_nsid == parent namespace ID
*    nsid == namespace ID to find
*    curelem == value node for current element if available
*    xneeded == pointer to xmlns needed flag output value
*
* OUTPUTS:
*   *xneeded == TRUE if the prefix is new and an xmlns
*               decl is needed in the element being generated
*
* RETURNS:
*   pointer to prefix if found, else NULL if not found
*********************************************************************/
extern const xmlChar *
    xml_msg_get_prefix (xml_msg_hdr_t *msg,
			xmlns_id_t parent_nsid,
			xmlns_id_t nsid,
			val_value_t *curelem,
			boolean  *xneeded);


/********************************************************************
* FUNCTION xml_msg_get_prefix_xpath
*
* Find the namespace prefix for the specified namespace ID
* If it is not there then create one in the msg prefix map
* Always returns a prefix, instead of using a default
*
* creates a new pfixmap if needed
*
* !!! MUST BE CALLED BEFORE THE <rpc-reply> XML OUTPUT
* !!! HAS BEGUN.  CANNOT BE CALLED BY OUTPUT FUNCTIONS
* !!! DURING THE <get> OR <get-config> OUTPUT GENERATION
*
* INPUTS:
*    msg  == message to search
*    nsid == namespace ID to find
*
* RETURNS:
*   pointer to prefix if found, else NULL if not found
*********************************************************************/
extern const xmlChar *
    xml_msg_get_prefix_xpath (xml_msg_hdr_t *msg,
			      xmlns_id_t nsid);


/********************************************************************
* FUNCTION xml_msg_get_prefix_start_tag
*
* Find the namespace prefix for the specified namespace ID
* DO NOT CREATE A NEW PREFIX MAP IF IT IS NOT THERE
* does not create any pfixmap, just returns NULL if not found
*
* INPUTS:
*    msg  == message to search
*    nsid == namespace ID to find
*
* RETURNS:
*   pointer to prefix if found, else NULL if not found
*********************************************************************/
extern const xmlChar *
    xml_msg_get_prefix_start_tag (xml_msg_hdr_t *msg,
				  xmlns_id_t nsid);


/********************************************************************
* FUNCTION xml_msg_gen_new_prefix
*
* Generate a new namespace prefix
* 
* INPUTS:
*    msg  == message to search and generate a prefix for
*    nsid == namespace ID to generate prefix for
*    retbuff == address of return buffer
*    buffsize == buffer size
* OUTPUTS:
*   if *retbuff is NULL it will be created
*   else *retbuff is filled in with the new prefix if NO_ERR
*
* RETURNS:
*    status
*********************************************************************/
extern status_t 
    xml_msg_gen_new_prefix (xml_msg_hdr_t *msg,
			    xmlns_id_t  nsid,
			    xmlChar **retbuff,
			    uint32 buffsize);


/********************************************************************
* FUNCTION xml_msg_build_prefix_map
*
* Build a queue of xmlns_pmap_t records for the current message
* 
* INPUTS:
*    msg == message in progrss
*    attrs == the top-level attrs list (e;g, rpc_in_attrs)
*    addncid == TRUE if a prefix entry for the NC namespace
*                should be added
*            == FALSE if the NC nsid should not be added
*    addncxid == TRUE if a prefix entry for the NCX namespace
*                should be added
*             == FALSE if the NCX nsid should not be added
* OUTPUTS:
*   msg->prefixQ will be populated as needed,
*   could be partially populated if some error returned
*
*   XMLNS Entries for NETCONF and NCX will be added if they 
*   are not present
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    xml_msg_build_prefix_map (xml_msg_hdr_t *msg,
			      xml_attrs_t *attrs,
			      boolean addncid,
			      boolean addncxid);


/********************************************************************
* FUNCTION xml_msg_finish_prefix_map
*
* Finish the queue of xmlns_pmap_t records for the current message
* 
* INPUTS:
*    msg == message in progrss
*    attrs == the top-level attrs list (e;g, rpc_in_attrs)
* OUTPUTS:
*   msg->prefixQ will be populated as needed,
*   could be partially populated if some error returned
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    xml_msg_finish_prefix_map (xml_msg_hdr_t *msg,
                               xml_attrs_t *attrs);


/********************************************************************
* FUNCTION xml_msg_check_xmlns_attr
*
* Check the default NS and the prefix map in the msg;
* 
* INPUTS:
*    msg == message in progress
*    nsid == namespace ID to check
*    badns == namespace URI of the bad namespace
*             used if the nsid is the INVALID marker
*    attrs == Q to hold the xml_attr_t, if generated
*
* OUTPUTS:
*   msg->prefixQ will be populated as needed,
*   could be partially populated if some error returned
*  
*   XMLNS attr entry may be added to the attrs Q
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    xml_msg_check_xmlns_attr (xml_msg_hdr_t *msg, 
			      xmlns_id_t nsid,
			      const xmlChar *badns,
			      xml_attrs_t  *attrs);


/********************************************************************
* FUNCTION xml_msg_gen_xmlns_attrs
*
* Generate any xmlns directives in the top-level
* attribute Q
*
* INPUTS:
*    msg == message in progress
*    attrs == xmlns_attrs_t Q to process
*    addncx == TRUE if an xmlns for the NCX prefix (for errors)
*              should be added to the <rpc-reply> element
*              FALSE if not
*
* OUTPUTS:
*   *attrs will be populated as needed,
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    xml_msg_gen_xmlns_attrs (xml_msg_hdr_t *msg, 
			     xml_attrs_t *attrs,
                             boolean addncx);


/********************************************************************
* FUNCTION xml_msg_clean_defns_attr
*
* Get rid of an xmlns=foo default attribute
*
* INPUTS:
*    attrs == xmlns_attrs_t Q to process
*
* OUTPUTS:
*   *attrs will be cleaned as needed,
*
* RETURNS:
*   status
*********************************************************************/
extern status_t
    xml_msg_clean_defns_attr (xml_attrs_t *attrs);


/********************************************************************
 * Add an ncid or ncxid to a prefix map.
 * 
 * \param msg the message in progrss
 * \param attrs the the top-level attrs list (e;g, rpc_in_attrs)
 * \param ncid the ncid to add.
 * \return status
 *********************************************************************/
extern status_t 
    xml_msg_add_ncid_to_prefix_map (xml_msg_hdr_t* msg, 
                                    xml_attrs_t* attrs,
                                    xmlns_id_t ncid);



/********************************************************************
 * xml_msg_init
 * Init this module
 *********************************************************************/
extern void xml_msg_init (void);


/********************************************************************
 * xml_msg_cleanup
 * Cleanup this module
 *********************************************************************/
extern void xml_msg_cleanup (void);


/********************************************************************
 * xml_msg_set_cfgid
 * Set the config ID in the message
 * INPUTS:
 *   msg == message header to change
 *   cfgid == configuration ID to use
 *********************************************************************/
extern void xml_msg_set_cfgid (xml_msg_hdr_t *msg,
                               ncx_cfg_t cfgid);


/********************************************************************
 * xml_msg_get_cfgid
 * Set the config ID in the message
 * INPUTS:
 *   msg == message header to change
 *   isvalid == address of return valus is valid flag
 * OUTPUTS:
 *   *isvalid == TRUE if it has been set; FALSE otherwise
 * RETURNS:
 *  config ID (ignore if *isvalid is FALSE)
 *********************************************************************/
extern ncx_cfg_t 
    xml_msg_get_cfgid (xml_msg_hdr_t *msg,
                       boolean *isvalid);


/********************************************************************
 * xml_msg_get_msgid
 * Set the message sequence ID in the message
 * INPUTS:
 *   msg == message header to use
 * RETURNS:
 *  message ID
 *********************************************************************/
extern uint32
    xml_msg_get_msgid (xml_msg_hdr_t *msg);


/********************************************************************
 * xml_msg_get_withdef
 * Get the message withdef enum
 * INPUTS:
 *   msg == message header to use
 * RETURNS:
 *   with defaults value in the message header
 *********************************************************************/
extern ncx_withdefaults_t
    xml_msg_get_withdef (xml_msg_hdr_t *msg);
       


/********************************************************************
 * xml_msg_set_withdef
 * Set the message withdef enum
 * INPUTS:
 *   msg == message header to set
 *   withdef == enum value to set
 *********************************************************************/
extern void
    xml_msg_set_withdef (xml_msg_hdr_t *msg,
                         ncx_withdefaults_t withdef);


/********************************************************************
 * xml_msg_get_max_depth
 * Get the message max_depth value
 * INPUTS:
 *   msg == message header to use
 * RETURNS:
 *   maxdepth value
 *********************************************************************/
extern uint32
    xml_msg_get_max_depth (xml_msg_hdr_t *msg);


/********************************************************************
 * xml_msg_set_max_depth
 * Set the message max_depth value
 * INPUTS:
 *   msg == message header to set
 *   max_depth == number value to set
 *********************************************************************/
extern void
    xml_msg_set_max_depth (xml_msg_hdr_t *msg,
                          uint32 max_depth);


/********************************************************************
 * xml_msg_get_cur_depth
 * Get the message cur_depth value
 * INPUTS:
 *   msg == message header to use
 * RETURNS:
 *   cur_depth value
 *********************************************************************/
extern uint32
    xml_msg_get_cur_depth (xml_msg_hdr_t *msg);


/********************************************************************
 * xml_msg_set_cur_depth
 * Set the message cur_depth value
 * INPUTS:
 *   msg == message header to set
 *   cur_depth == new depth value
 *********************************************************************/
extern void
    xml_msg_set_cur_depth (xml_msg_hdr_t *msg,
                          uint32 cur_depth);


/********************************************************************
 * xml_msg_get_encoding
 * Get the message encoding value
 *
 * INPUTS:
 *   msg == message header to check
 *   is_output == TRUE to get output encoding
 *             == FALSE to get input encoding
 * RETURNS:
 *   encoding enumeration
 *********************************************************************/
extern ncx_display_mode_t
    xml_msg_get_encoding (xml_msg_hdr_t *msg,
                          boolean is_output);


/********************************************************************
 * xml_msg_set_encoding
 * Set the message encoding value
 *
 * INPUTS:
 *   msg == message header to set
 *   is_output == TRUE to set output encoding
 *             == FALSE to set input encoding
 *   encoding == new encoding value
 *********************************************************************/
extern void
    xml_msg_set_encoding (xml_msg_hdr_t *msg,
                          boolean is_output,
                          ncx_display_mode_t encoding);


/********************************************************************
 * xml_msg_get_etag
 * Get the message etag match value and test sense
 * 
 * INPUTS:
 *   msg == message header to check
 *   test == address of return test sense flag
 * OUTPUTS:
 *  *test == TRUE for If-Match; FALSE for If-None-Match
 * RETURNS:
 *  etag value to match
 *********************************************************************/
extern ncx_etag_t
    xml_msg_get_etag (xml_msg_hdr_t *msg,
                      boolean *test);


/********************************************************************
 * xml_msg_set_etag
 * Set the etag match parameters
 *
 * INPUTS:
 *   msg == message header to set
 *   etag == etag to match
 *   test_sense == TRUE to match, FALSE for none-match
 *********************************************************************/
extern void
    xml_msg_set_etag (xml_msg_hdr_t *msg,
                      ncx_etag_t etag,
                      boolean test_sense);


/********************************************************************
 * xml_msg_get_tstamp
 * Get the message timestamp match value and test sense
 * 
 * INPUTS:
 *   msg == message header to check
 *   test == address of return test sense flag
 * OUTPUTS:
 *  *test == TRUE for If-Modified-Since; FALSE for If-Unmodified-Since
 * RETURNS:
 *  timestamp value to match
 *********************************************************************/
extern time_t
    xml_msg_get_tstamp (xml_msg_hdr_t *msg,
                        boolean *test);


/********************************************************************
 * xml_msg_set_tstamp
 * Set the timestamp match parameters
 *
 * INPUTS:
 *   msg == message header to set
 *   tstamp == timestamp to match
 *   test_sense == TRUE to modified-since, FALSE for unmodified-since
 *********************************************************************/
extern void
    xml_msg_set_tstamp (xml_msg_hdr_t *msg,
                        time_t tstamp,
                        boolean test_sense);

#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif            /* _H_xml_msg */
