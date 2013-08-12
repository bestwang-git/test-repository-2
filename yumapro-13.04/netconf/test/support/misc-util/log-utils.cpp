// ---------------------------------------------------------------------------|
// Yuma Test Harness includes
// ---------------------------------------------------------------------------|
#include "test/support/misc-util/log-utils.h"

// ---------------------------------------------------------------------------|
// Linux includes
// ---------------------------------------------------------------------------|
#include <sys/ioctl.h>

// ---------------------------------------------------------------------------|
// Standard includes
// ---------------------------------------------------------------------------|

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

// ---------------------------------------------------------------------------|
// Boost Test Framework
// ---------------------------------------------------------------------------|
#include <boost/test/unit_test.hpp>
#include <boost/test/results_collector.hpp> 

#include "src/ncx/log.h"

// ---------------------------------------------------------------------------|
// File wide namespace use
// ---------------------------------------------------------------------------|
using namespace std;

// ---------------------------------------------------------------------------|
namespace 
{

/** text width for result display */
const uint8_t resultWidth(6);

/** Mapping of test name to brief.  */
unordered_map<string, string> briefMap;

static string systest_desc_log =   "./yuma-op/systest_desc.log";
static string systest_result_log = "./yuma-op/systest_result.log";
static FILE*  desc_fp = NULL;
static FILE*  systest_result_fp = NULL;

// ---------------------------------------------------------------------------|
/**
 * Get the result of the current test.
 *
 * \return true if the current test passed.
 */
inline bool CurrentTestPassed()
{
    using boost::unit_test::results_collector;
    using boost::unit_test::framework::current_test_case;
    using boost::unit_test::test_case;

    return results_collector.results( current_test_case().p_id ).passed();
}

// ---------------------------------------------------------------------------|
/**
 * Get the name of the current test.
 *
 * \return the name of the current test.
 */
inline string CurrentTestName()
{
    using boost::unit_test::results_collector;
    using boost::unit_test::framework::current_test_case;
    return current_test_case().p_name;
}

// ---------------------------------------------------------------------------|
inline string CurrentTestSuiteName()
{
    using boost::unit_test::results_collector;
    using boost::unit_test::framework::current_test_case;
    using boost::unit_test::framework::get;
    using boost::unit_test::test_suite;

    return get<test_suite>( current_test_case().p_parent_id ).p_name;
}

// ---------------------------------------------------------------------------|
/**
 * Add supplied brief to the mapping between test name and test brief.
 *
 * \param brief a brief description of the currently running test.
 */
void AddTestBrief( const string& brief )
{
    using boost::unit_test::framework::current_test_case;
    using boost::unit_test::test_case;

    briefMap[ CurrentTestName() ] = brief;
}

// ---------------------------------------------------------------------------|
/**
 * Get the width of the terminal
 *
 * \return the width of the terminal
 */
inline uint16_t GetTerminalWidth()
{
    struct winsize w;
    ioctl( 0, TIOCGWINSZ, &w );

    return w.ws_col;
}

/**
 * Get the width of the terminal to use for displaying text. 
 *
 * \return the width of the terminal
 */
inline uint16_t GetParagraphWidth()
{
    return GetTerminalWidth() - resultWidth;
}

// ---------------------------------------------------------------------------|
/**
 * Split the supplied string across multiple lines. This function splits the
 * supplied string into words then it formats the text so that it wraps neatly 
 * across multiple lines. The width of each line is determined by the 
 * width of the terminal.
 *
 * \param text The text to prepare
 * \return a vector of strings, each one is one line of the text.
 */
vector<string> SplitWorker( const string& text )
{
    vector<string> lines;

    // allow space for result to be printed
    uint16_t wid = GetParagraphWidth();
    if ( text.size() > wid )
    {
        vector<string> strs;
        boost::split(strs, text, boost::is_any_of( " " ));
        ostringstream oss;
        BOOST_FOREACH( string& str, strs )
        {
            if ( oss.str().size() + str.size() > wid ) 
            {
                lines.push_back( oss.str() );
                oss.str("");
                oss.clear();
            }
            oss << str << " ";
        }
        lines.push_back( oss.str() );
    }
    else
    {
        lines.push_back( text );
    }

    return lines;
}

// ---------------------------------------------------------------------------|
/**
 * Format text for display.
 * This function first splits the string into pre-formatted lines,
 * delimited by '\n'. It the calls SplitWorker to format each of those
 * individual lines.
 *
 * \param text The text to prepare
 * \return a vector of strings, each one is one line of the text.
 */
vector<string> SplitText( const string& text )
{
    vector<string> rawlines;
    boost::split( rawlines, text, boost::is_any_of( "\n" ) );

    vector<string> formattedlines;
    BOOST_FOREACH( string& rawline, rawlines )
    {
        vector<string> lines = SplitWorker( rawline );
        formattedlines.insert( formattedlines.end(), 
                               lines.begin(), lines.end() );
    }
    
    return formattedlines;
}

// ---------------------------------------------------------------------------|
/**
 * Prepare the supplied string for display. This function first splits
 * the supplied string into N lines then creates a single string
 * containing the format text.
 *
 * \param text The text to prepare
 * \return a multi-line text string for display.
 */
string FormatText( const string& text )
{
    vector<string> lines = SplitText( text );

    ostringstream oss;
    copy( lines.begin(), lines.end(), ostream_iterator<string>( oss, "\n" ) );
    return oss.str();
}

// ---------------------------------------------------------------------------|
/** 
 * Display the suplied string as formatted text.
 *
 * \param text The text to display.
 */
void DisplayFormattedText( const string& text )
{
    if ( !text.empty() )
    {
        BOOST_TEST_MESSAGE( FormatText( text ) );
    }
}
} // anonymous namespace

// ---------------------------------------------------------------------------|
namespace YumaTest
{

// ---------------------------------------------------------------------------|
void DisplayTestBreak( char chr, bool force )
{
    const uint16_t terminalWidth = GetTerminalWidth();
    string txt( terminalWidth, chr );
    if ( force )
    {
        cout << GREY_BOLD << txt << DEFAULT_TEXT;
    }
    else
    {
        BOOST_TEST_MESSAGE( GREY_BOLD << txt << DEFAULT_TEXT );
    }
}

// ---------------------------------------------------------------------------|
void DisplayTestTitle( const string& title )
{
    DisplayTestBreak();
    
    BOOST_TEST_MESSAGE( HIGHLIGHT_ON << FormatText( title ) 
                        << DEFAULT_TEXT );
}

// ---------------------------------------------------------------------------|
void DisplayTestDescrption( const string& brief, 
                            const string& desc,
                            const string& notes )
{
    AddTestBrief( brief );
    DisplayTestTitle( brief );
    DisplayFormattedText( desc );
    DisplayFormattedText( notes );
    DisplayCurrentTestSummary();

    // only integ-test has netconfd logging included
    // sys-test has no netconfd logging included
    FILE  *outputfile = NULL;

    if ((outputfile = log_get_logfile()) == NULL) {
       desc_fp = fopen ( "./yuma-op/systest_desc.log", "w");
       outputfile = desc_fp;
    }

    char chr='=';
    string txt( 70, chr );

    // Start logging test case description.
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( txt.c_str(), sizeof(char), txt.size(), outputfile );
   
    string test = "TEST CASE ";
    string testName = CurrentTestName();

    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( test.c_str(), sizeof(char), test.size(), outputfile );
    fwrite( testName.c_str(), sizeof(char), testName.size(), outputfile );

    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( txt.c_str(), sizeof(char), txt.size(), outputfile );
  
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( brief.c_str(), sizeof(char), brief.size(), outputfile );

    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( desc.c_str(), sizeof(char), desc.size(), outputfile );

    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( notes.c_str(), sizeof(char), notes.size(), outputfile );

    // Close systest log if running sys-test cases
    if (desc_fp) {
      fclose(desc_fp);
      desc_fp = NULL;
    }
}


// ---------------------------------------------------------------------------|
void DisplayCurrentTestSummary( const char* highlight )
{
    uint16_t wid = GetParagraphWidth();
    vector<string> lines = SplitText( briefMap[ CurrentTestName() ] );

    auto beg = lines.begin();
    auto end = lines.end();

    cout << highlight;
    // Display all lines except the last one
    copy( beg, --end, ostream_iterator<string>( cout, "\n" ) );
    
    cout.setf( ios::left );
    cout << setw( wid ) << setfill( '.' ) << ( *end + " " );
    cout << DEFAULT_TEXT;
    cout.flush();
}

// ---------------------------------------------------------------------------|
void DisplayCurrentTestId( const char* highlight )
{
    cout << highlight << CurrentTestSuiteName() << "/" 
         << CurrentTestName() << " : " << DEFAULT_TEXT;
}

// ---------------------------------------------------------------------------|
void DisplayCurrentTestResult()
{

    cout << HIGHLIGHT_ON;
    if ( CurrentTestPassed() )
    {
        cout << " PASS\n";
    }
    else
    {
        DisplayCurrentTestSummary();
        cout << RED_BOLD << " FAIL\n";
        DisplayCurrentTestId( RED_BOLD );
        cout << RED_BOLD << "FAILED!\n";
    }
    cout << DEFAULT_TEXT;

    // only integ-test has netconfd logging included
    // sys-test has no netconfd logging included

    FILE   *outputfile = NULL;
    if ((outputfile = log_get_logfile()) == NULL) {
       systest_result_fp = fopen ( systest_result_log.c_str(), "a");
       outputfile = systest_result_fp;
    }

    string testName = CurrentTestName();
    char chr='=';
    string txt( 70, chr );

    string test = "\nTest Result of TEST CASE ";
    fwrite( test.c_str(), sizeof(char), test.size(), outputfile );
    fwrite( testName.c_str(), sizeof(char), testName.size(), outputfile );

    if ( CurrentTestPassed() )
    {
        string passed = " PASSED " ;
        fwrite( passed.c_str(), sizeof(char), passed.size(), outputfile );
    }
    else
    {
        string failed = " FAILED " ;
        fwrite( failed.c_str(), sizeof(char), failed.size(), outputfile );
    }
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( txt.c_str(), sizeof(char), txt.size(), outputfile );

    // close sys-test systest_result.log if for the sys-test.
    if (systest_result_fp) {
       fclose(systest_result_fp);
    }
}

/********************************************************************
* FUNCTION log_systest_get
*
* Get the open logfile for direct output
* Needed by libtecla to write command line history
*
* RETURNS:
*   pointer to open FILE if any
*   NULL if no open logfile
*********************************************************************/
FILE * log_systest_get (void)
{
    return desc_fp;

}  /* log_systest_get */

/********************************************************************
* FUNCTION log_systest_get
*
* Get the logfile name for direct output
* RETURNS: logfile name
*********************************************************************/
const std::string log_filename_systest_get (void)
{
    return systest_desc_log;

}  /* log_filename_systest_get */

} // namespace YumaTest
