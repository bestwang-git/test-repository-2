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
/*  FILE: tstamp.c

                
*********************************************************************
*                                                                   *
*                  C H A N G E   H I S T O R Y                      *
*                                                                   *
*********************************************************************

date         init     comment
----------------------------------------------------------------------
17apr06      abb      begun

*********************************************************************
*                                                                   *
*                     I N C L U D E    F I L E S                    *
*                                                                   *
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#define __USE_XOPEN 1
#include <time.h>
#include <xmlstring.h>

#include "procdefs.h"
#include "log.h"
#include "status.h"
#include "tstamp.h"
#include "xml_util.h"


/********************************************************************
* FUNCTION time_to_string
*
* Convert the tm to a string in YANG canonical format
*
* INPUTS:
*   curtime == time struct to use
*   buff == pointer to buffer to hold output
*           MUST BE AT LEAST 21 CHARS
* OUTPUTS:
*   buff is filled in
*********************************************************************/
static void 
    time_to_string (const struct tm *curtime,
                       xmlChar *buff)
{
    (void)sprintf((char *)buff, 
                  "%04u-%02u-%02uT%02u:%02u:%02uZ",
                  (uint32)(curtime->tm_year+1900),
                  (uint32)(curtime->tm_mon+1),
                  (uint32)curtime->tm_mday,
                  (uint32)curtime->tm_hour,
                  (uint32)curtime->tm_min,
                  (uint32)curtime->tm_sec);

} /* time_to_string */


/********************************************************************
* FUNCTION time_to_local_string
*
* Convert the tm to a string in friendly, readable format
*
* INPUTS:
*   curtime == time struct to use
*   buff == pointer to buffer to hold output
*           MUST BE AT LEAST 21 CHARS
* OUTPUTS:
*   buff is filled in
*********************************************************************/
static void 
    time_to_local_string (const struct tm *curtime,
			  xmlChar *buff)
{
    (void)sprintf((char *)buff, 
                  "%04u-%02u-%02u %02u:%02u:%02u",
                  (uint32)(curtime->tm_year+1900),
                  (uint32)(curtime->tm_mon+1),
                  (uint32)curtime->tm_mday,
                  (uint32)curtime->tm_hour,
                  (uint32)curtime->tm_min,
                  (uint32)curtime->tm_sec);

} /* time_to_local_string */


/********************************************************************
* FUNCTION time_to_dirname
*
* Convert the tm to a directory name for yangcli
*
* INPUTS:
*   curtime == time struct to use
*   buff == pointer to buffer to hold output
*           MUST BE AT LEAST 21 CHARS
* OUTPUTS:
*   buff is filled in
*********************************************************************/
static void 
    time_to_dirname (const struct tm *curtime,
                     xmlChar *buff)
{
    (void)sprintf((char *)buff, 
                  "%04u%02u%02u%02u%02u%02u",
                  (uint32)(curtime->tm_year+1900),
                  (uint32)(curtime->tm_mon+1),
                  (uint32)curtime->tm_mday,
                  (uint32)curtime->tm_hour,
                  (uint32)curtime->tm_min,
                  (uint32)curtime->tm_sec);

} /* time_to_dirname */


/**************    E X T E R N A L   F U N C T I O N S **********/


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
void 
    tstamp_datetime (xmlChar *buff)
{
    assert(buff && "buff is NULL!");

    time_t  utime;
    (void)time(&utime);

    struct tm *curtime = gmtime(&utime);
    time_to_string(curtime, buff);

} /* tstamp_datetime */


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
void 
    tstamp_local_datetime (xmlChar *buff)
{
    assert(buff && "buff is NULL!");

    time_t utime;
    (void)time(&utime);

    struct tm *curtime = localtime(&utime);
    time_to_local_string(curtime, buff);

} /* tstamp_local_datetime */


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
void 
    tstamp_date (xmlChar *buff)
{
    assert(buff && "buff is NULL!");

    time_t  utime;
    (void)time(&utime);

    struct tm *curtime = localtime(&utime);
    (void)sprintf((char *)buff, 
                  "%04u-%02u-%02u",
                  (uint32)(curtime->tm_year+1900),
                  (uint32)(curtime->tm_mon+1),
                  (uint32)curtime->tm_mday);

} /* tstamp_date */


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
void 
    tstamp_datetime_sql (xmlChar *buff)
{
    assert(buff && "buff is NULL!");

    time_t  utime;
    (void)time(&utime);

    struct tm *curtime = localtime(&utime);
    /***  milliseconds not returned, hardwired to '00' ***/
    (void)sprintf((char *)buff, 
                  "%04u-%02u-%02u %02u:%02u:%02u",
                  (uint32)(curtime->tm_year+1900),
                  (uint32)(curtime->tm_mon+1),
                  (uint32)curtime->tm_mday,
                  (uint32)curtime->tm_hour,
                  (uint32)curtime->tm_min,
                  (uint32)curtime->tm_sec);
    
} /* tstamp_datetime_sql */


/********************************************************************
* FUNCTION tstamp_convert_to_utctime
*
* Check if the specified string is a valid dateTime or 
* date-and-time string is valid and if so, convert it
* to 
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
xmlChar *
    tstamp_convert_to_utctime (const xmlChar *timestr,
                               boolean *isNegative,
                               status_t *res)
{
    assert(timestr && "timestr is NULL!");
    assert(isNegative && "isNegative is NULL!");
    assert(res && "res is NULL!");

    *res = NO_ERR;

    struct tm convertedtime;
    memset(&convertedtime, 0x0, sizeof(struct tm));

    if (*timestr == '-') {
        *isNegative = TRUE;
        timestr++;
    } else {
        *isNegative = FALSE;
    }

    xmlChar *buffer = NULL;
    const char *retptr = NULL;

    uint32 len = xml_strlen(timestr);
    if (len == 20) {
        /* could be in canonical form */
        retptr = strptime((const char *)timestr, "%FT%TZ", &convertedtime);
        if (retptr && *retptr == '\0') {
            buffer = xml_strdup(timestr);
            if (!buffer) {
                *res = ERR_INTERNAL_MEM;
                return NULL;
            } else {
                return buffer;
            }
        } else {
            *res = ERR_NCX_INVALID_VALUE;
            return NULL;
        }
    } else if (len > 20) {
        retptr = strptime((const char *)timestr, "%FT%T", &convertedtime);
        if (retptr == NULL || *retptr == '\0') {
            *res = ERR_NCX_INVALID_VALUE;
            return NULL;
        }

        /* check is frac-seconds entered, and skip it */
        if (*retptr == '.') {
            retptr++;
            if (!isdigit((int)*retptr)) {
                *res = ERR_NCX_INVALID_VALUE;
                return NULL;
            }

            retptr++;  /* got a start digit */
            while (isdigit((int)*retptr)) {
                retptr++;
            }
        }

        /* check if a timezone offset is present */
        retptr = strptime(retptr, "%z", &convertedtime);
        if (retptr == NULL) {
            *res = ERR_NCX_INVALID_VALUE;
            return NULL;
        }

        /* check where retptr ended up */
        if (*retptr == '\0') {
            /* OK read all the bytes */
            ;
        } else if (*retptr == ':') {
            if (strcmp(retptr, ":00")) {
                /* the linux strptime function does
                 * not process the 'time-minute' field in the
                 * time string; since this is so rare
                 * just treat as a special error
                 */
                *res = ERR_NCX_OPERATION_NOT_SUPPORTED;
                return NULL;
            } /* else time-minute field == '00' and no error */
        } else {
            *res = ERR_NCX_INVALID_VALUE;
            return NULL;
        }

        buffer = m__getMem(TSTAMP_MIN_SIZE);
        if (!buffer) {
            *res = ERR_INTERNAL_MEM;
            return NULL;
        }

        time_t utime = mktime(&convertedtime);
        if (utime == (utime)-1) {
            *res = ERR_NCX_INVALID_VALUE;
            m__free(buffer);
            return NULL;
        }

        struct tm *curtime = gmtime(&utime);
        time_to_string(curtime, buffer);
        return buffer;
    } else {
        /* improper length */
        *res = ERR_NCX_INVALID_VALUE;
        return NULL;
    }
    
} /* tstamp_convert_to_utctime */


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
void 
    tstamp_datetime_dirname (xmlChar *buff)
{
    assert(buff && "buff is NULL!");

    time_t  utime;
    (void)time(&utime);

    struct tm *curtime = gmtime(&utime);
    time_to_dirname(curtime, buff);

} /* tstamp_datetime_dirname */


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
void 
    tstamp_time2datetime (time_t *timerec,
                          xmlChar *buff)
{
    assert(timerec && "timerec is NULL!");
    assert(buff && "buff is NULL!");

    struct tm *curtime = gmtime(timerec);
    time_to_string(curtime, buff);

} /* tstamp_time2datetime */


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
                          size_t buffsize)
{
    assert(timerec && "timerec is NULL!");
    assert(buff && "buff is NULL!");

    status_t res = NO_ERR;
    struct tm *curtime = gmtime(timerec);
    size_t ret = strftime((char *)buff, buffsize, "%a, %d %b %Y %T %Z",
                          curtime);
    if (ret == 0) {
        res = ERR_NCX_INVALID_VALUE;
    }
    return res;
    
} /* tstamp_time2htmltime */


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
status_t
    tstamp_htmltime2time (const xmlChar *timestr,
                          time_t *timerec)
{
    assert(timestr && "timestr is NULL!");
    assert(timerec && "timerec is NULL!");

    status_t res = NO_ERR;
    struct tm restime;

    const char *format = "%a, %d %b %Y %T %Z";

    char *ret = strptime((const char *)timestr, format, &restime);
    if (ret == NULL) {
        res = ERR_NCX_INVALID_VALUE;
    } else {
        *timerec = timegm(&restime);
    }
    return res;

} /* tstamp_time2htmltime */


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
int32
    tstamp_difftime (time_t *time1,
                     time_t *time2)
{
    double result = difftime(*time1, *time2);
    if (result < 0) {
        return -1;
    } else if (result == 0) {
        return 0;
    } else {
        return 1;
    }
    /*NOTREACHED*/

}  /* tstamp_difftime */


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
void 
    tstamp_now (time_t *tim)
{
    assert(tim && "tim is NULL!");

    (void)time(tim);

} /* tstamp_now */


/* END file tstamp.c */
