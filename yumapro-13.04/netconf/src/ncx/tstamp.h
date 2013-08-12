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
#ifndef _H_tstamp
#define _H_tstamp
/*  FILE: tstamp.h
*********************************************************************
*								    *
*			 P U R P O S E				    *
*								    *
*********************************************************************

    Timestamp utilities

*********************************************************************
*								    *
*		   C H A N G E	 H I S T O R Y			    *
*								    *
*********************************************************************

date	     init     comment
----------------------------------------------------------------------
17-apr-06    abb      begun
*/

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
*								    *
*			 C O N S T A N T S			    *
*								    *
*********************************************************************/

#define TSTAMP_MIN_SIZE   22
#define TSTAMP_DATE_SIZE  12
#define TSTAMP_SQL_SIZE   20
#define TSTAMP_HTML_SIZE  50


/********************************************************************
* FUNCTION tstamp_datetime
*
* Set the current date and time in an XML dateTime string format
*
* INPUTS:
*   buff == pointer to buffer to hold output
*           MUST BE AT LEAST 21 CHARS
* OUTPUTS:
*   buff is filled in
*********************************************************************/
extern void 
    tstamp_datetime (xmlChar *buff);


/********************************************************************
* FUNCTION tstamp_local_datetime
*
* Set the current date and time in an XML dateTime string format
*
* INPUTS:
*   buff == pointer to buffer to hold output
*           MUST BE AT LEAST 21 CHARS
* OUTPUTS:
*   buff is filled in
*********************************************************************/
extern void 
    tstamp_local_datetime (xmlChar *buff);


/********************************************************************
* FUNCTION tstamp_date
*
* Set the current date in an XML dateTime string format
*
* INPUTS:
*   buff == pointer to buffer to hold output
*           MUST BE AT LEAST 11 CHARS
* OUTPUTS:
*   buff is filled in
*********************************************************************/
extern void 
    tstamp_date (xmlChar *buff);


/********************************************************************
* FUNCTION tstamp_datetime_sql
*
* Set the current date and time in an XML dateTime string format
*
* INPUTS:
*   buff == pointer to buffer to hold output
*           MUST BE AT LEAST 20 CHARS
* OUTPUTS:
*   buff is filled in
*********************************************************************/
extern void 
    tstamp_datetime_sql (xmlChar *buff);


/********************************************************************
* FUNCTION tstamp_convert_to_utctime
*
* Check if the specified string is a valid dateTime or 
* date-and-time string is valid and if so, convert it
* to UTC time
*
* INPUTS:
*   buff == pointer to buffer to check
*   isNegative == address of return negative date flag
*   res == address of return status
*
* OUTPUTS:
*   *isNegative == TRUE if a negative dateTime string is given
*                  FALSE if no starting '-' sign found
*   *res == return status
*
* RETURNS:
*   malloced pointer to converted date time string
*   or NULL if some error
*********************************************************************/
extern xmlChar *
    tstamp_convert_to_utctime (const xmlChar *timestr,
			       boolean *isNegative,
			       status_t *res);


/********************************************************************
* FUNCTION tstamp_datetime_dirname
*
* Set the current date and time in an XML dateTime string format
*
* INPUTS:
*   buff == pointer to buffer to hold output
*           MUST BE AT LEAST 21 CHARS
* OUTPUTS:
*   buff is filled in
*********************************************************************/
extern void 
    tstamp_datetime_dirname (xmlChar *buff);


/********************************************************************
* FUNCTION tstamp_time2datetime
*
* Convert the specified time_t to a YANG data-and-time format
*
* INPUTS:
*   timerec == pointer to time_t to convert
*   buff == pointer to buffer to hold output
*           MUST BE AT LEAST 21 CHARS
* OUTPUTS:
*   buff is filled in
*********************************************************************/
extern void 
    tstamp_time2datetime (time_t *timerec,
                          xmlChar *buff);

/********************************************************************
* FUNCTION tstamp_time2htmltime
*
* Convert the specified time_t to a HTML timestamp string
*
* INPUTS:
*   timerec == pointer to time_t to convert
*   buff == pointer to buffer to hold output
*           MUST BE AT LEAST 40 CHARS
* OUTPUTS:
*   buff is filled in
* RETURNS:
*  status
*********************************************************************/
status_t
    tstamp_time2htmltime (time_t *timerec,
                          xmlChar *buff,
                          size_t buffsize);


/********************************************************************
* FUNCTION tstamp_htmltime2time
*
* Convert the specified HTML timestamp string to time_t format
*
* INPUTS:
*   timestr == timestamp string to convert
*   timerec == address of return time_t struct
* OUTPUTS:
*   *timerec struct is filled in if NO_ERR
* RETURNS:
*   status
*********************************************************************/
extern status_t
    tstamp_htmltime2time (const xmlChar *timestr,
                          time_t *timerec);


/********************************************************************
* FUNCTION tstamp_difftime
*
* Compare 2 time_t structs
*
* INPUTS:
*   time1 == time struct 1
*   time2 == time struct 2
*
* RETURNS:
*   -1 if time1 < time2
*    0 if time1 == time2
*    1 if time1 > time2
*********************************************************************/
extern int32
    tstamp_difftime (time_t *time1,
                     time_t *time2);


/********************************************************************
* FUNCTION tstamp_now
*
* Set the time_t to the current time
*
* INPUTS:
*   tim == address of time_t to set
* OUTPUTS:
*   *tim = set
*********************************************************************/
extern void 
    tstamp_now (time_t *tim);


#ifdef __cplusplus
}  /* end extern 'C' */
#endif

#endif	    /* _H_tstamp */
