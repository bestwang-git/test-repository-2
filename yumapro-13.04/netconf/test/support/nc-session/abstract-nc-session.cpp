// ---------------------------------------------------------------------------|
// Yuma Test Harness includes
// ---------------------------------------------------------------------------|
#include "test/support/nc-session/abstract-nc-session.h"
#include "test/support/nc-query-util/nc-query-utils.h"
#include "test/support/nc-query-util/yuma-op-policies.h"

// ---------------------------------------------------------------------------|
// STD Includes 
// ---------------------------------------------------------------------------|
#include <cstdio>
#include <iostream>
#include <stdio.h>

#include <string>
#include <limits.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <boost/range/algorithm.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/bind.hpp>
#include <boost/phoenix/operator.hpp> 
#include <boost/phoenix/fusion/at.hpp> 
#include <boost/fusion/include/std_pair.hpp>

// ---------------------------------------------------------------------------|
// Boost Test Framework
// ---------------------------------------------------------------------------|
#include <boost/test/unit_test.hpp>
#include <boost/test/results_collector.hpp>

#include "src/ncx/log.h"
#include "test/support/misc-util/log-utils.h"


// ---------------------------------------------------------------------------|
// Boost Test Framework
// ---------------------------------------------------------------------------|
#include <boost/test/unit_test.hpp>

// ---------------------------------------------------------------------------|
// Global namespace usage
// ---------------------------------------------------------------------------|
using namespace std;

// ---------------------------------------------------------------------------|
// Anonymous namespace 
// ---------------------------------------------------------------------------|
namespace
{
    string lineBreak( 75, '-' );
    string testBreak( 75, '=' );
}

// ---------------------------------------------------------------------------!
namespace YumaTest
{

// ---------------------------------------------------------------------------!
AbstractNCSession::AbstractNCSession( 
        shared_ptr< AbstractYumaOpLogPolicy > policy, uint16_t sessionId )
    : sessionId_( sessionId ) 
    , messageCount_( 0 ) 
    , opLogFilenamePolicy_( policy )
    , messageIdToLogMap_()
{
}

// ---------------------------------------------------------------------------!
AbstractNCSession::~AbstractNCSession()
{
    concatenateLogFiles();
}

// ---------------------------------------------------------------------------!
void AbstractNCSession::concatenateLogFiles()
{
    namespace ph = boost::phoenix;
    using ph::arg_names::arg1;

    string logName = opLogFilenamePolicy_->generateSessionLog( sessionId_ );
    ofstream op( logName );
    BOOST_REQUIRE_MESSAGE( op, "Error opening output logfile: " << logName );

    op << testBreak << "\n\n";

    //=== Add Test description 
    ifstream in( "./yuma-op/systest_desc.log" );
    op << in.rdbuf() << "\n";

    std::vector< uint16_t > keys;
    boost::transform( messageIdToLogMap_, back_inserter( keys ), 
                      ph::at_c<0>( arg1 ) );

    auto sorted = boost::sort( keys );

    boost::for_each( keys, 
            ph::bind( &AbstractNCSession::appendLog, this,  ph::ref( op ), arg1 ) );

    //-----------------------------------------------
    // Remove the session log if the test suite log 
    // defined in integ-test's SpoofedArgs for client
    // xml message and server's debug log. 
    //-----------------------------------------------
    FILE   *outputfile = NULL;

    // remove this session log file.
    if ((outputfile = log_get_logfile()) != NULL) {
       removeLog(logName.c_str());
    }

    // remove systest temp log if there is one.
    FILE* templog = log_systest_get ();
    if ( templog != NULL) {
       string filename = log_filename_systest_get();
//       removeLog(filename.c_str());
    }
}

// ---------------------------------------------------------------------------!
void AbstractNCSession::appendLog( ofstream& op, uint16_t key )
{
    string logNameIn = messageIdToLogMap_[key];

    ifstream in( messageIdToLogMap_[key] );
    BOOST_REQUIRE_MESSAGE( in, "Error opening input logfile: " 
                           << messageIdToLogMap_[key] );
    
    op << in.rdbuf() << "\n";
    op << lineBreak << "\n\n";

    //-----------------------------------------------
    // remove this log to avoid duplicates clogging
    // the directory.
    //-----------------------------------------------
    removeLog(logNameIn.c_str());
}

// ---------------------------------------------------------------------------!
// After a small log file is appended to a final test result log,
// remove this log to avoid duplicates clogging the directory.
// This function is called in the desctructor at the exit of
// the test case.

void AbstractNCSession::removeLog( const char* log )
{
    char* the_path = new char[PATH_MAX];

     // -------------------------------------
     // Get extended full path for the file
     // -------------------------------------

     getcwd(the_path, PATH_MAX);
     strcat(the_path, "/");
     strcat(the_path, log);

     //-------- Delete this file ----------

     // printf("%s\n", the_path);
     std::remove(the_path);

     // Deallocate the memory
     delete the_path;
}

// ---------------------------------------------------------------------------!
string AbstractNCSession::newQueryLogFilename( const string& queryStr )
{
    uint16_t messageId = extractMessageId( queryStr );

    BOOST_REQUIRE_MESSAGE( 
            ( messageIdToLogMap_.find( messageId ) == messageIdToLogMap_.end() ), 
            "An entry already exists for message id: " << messageId );
    
    string logFilename ( opLogFilenamePolicy_->generate( queryStr, sessionId_ ) );
    messageIdToLogMap_[ messageId ] = logFilename;

    return logFilename;
}

// ---------------------------------------------------------------------------!
string AbstractNCSession::retrieveLogFilename( const uint16_t messageId ) const
{
    auto iter = messageIdToLogMap_.find( messageId );
    BOOST_REQUIRE_MESSAGE( iter != messageIdToLogMap_.end(),
           "Entry Not found for messageId: " << messageId );

    string logFilename = iter->second;
    if ( logFilename.empty() )
    {
        BOOST_FAIL( "Empty log filename for Message Id: " << messageId );
    }

    return logFilename;
}

// ---------------------------------------------------------------------------!
string AbstractNCSession::getSessionResult( const uint16_t messageId ) const
{
    // get the filename
    string logFilename( retrieveLogFilename( messageId ) );

    // open the file
    ifstream inFile( logFilename.c_str() );
    if ( !inFile )
    {
        BOOST_FAIL( "Error opening file for Yuma output" );
    }

    // read the result
    ostringstream oss;
    oss << inFile.rdbuf();

    return oss.str();
}

// ---------------------------------------------------------------------------!
FILE*  AbstractNCSession::logQueryStr( const string& queryStr )
{
    // Write out the initial query. to the logfile
    FILE   *outputfile = NULL;
    outputfile = log_get_logfile();

    // sys-test does not have netconfd bundled together for its logging.
    // Start logging for integ-test test case's xml query message.
    if (outputfile == NULL) {
       return outputfile;
    }

    char chr='-';
    string txt( 70, chr );
    string msg = "Client xml Query of Test Case ";

    using boost::unit_test::results_collector;
    using boost::unit_test::framework::current_test_case;
    string testname = current_test_case().p_name;

    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( txt.c_str(), sizeof(char), txt.size(), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( msg.c_str(), sizeof(char), msg.size(), outputfile );
    fwrite( testname.c_str(), sizeof(char), testname.size(), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );

    fwrite( txt.c_str(), sizeof(char), txt.size(), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );

    fwrite( queryStr.c_str(), sizeof(char), queryStr.size(), outputfile );
    fwrite( txt.c_str(), sizeof(char), txt.size(), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    
    return outputfile;
}

// ---------------------------------------------------------------------------!
FILE*  AbstractNCSession::logQueryResult ( const string& resultStr )
{
    FILE   *outputfile = NULL;
    outputfile = log_get_logfile();

    // sys-test does not have netconfd bundled together for its logging. 
    // Start logging for integ-test test case's xml query response message     
    if (outputfile == NULL) {
       return outputfile;
    }

    char chr='-';
    string txt( 70, chr );
    string msg = "Client xml Query and Result of Test Case ";
    using boost::unit_test::results_collector;
    using boost::unit_test::framework::current_test_case;
    string testname = current_test_case().p_name;

    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( txt.c_str(), sizeof(char), txt.size(), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( msg.c_str(), sizeof(char), msg.size(), outputfile );
    fwrite( testname.c_str(), sizeof(char), testname.size(), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );

    fwrite( txt.c_str(), sizeof(char), txt.size(), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    fwrite( resultStr.c_str(), sizeof(char), resultStr.size(), outputfile );
    fwrite( txt.c_str(), sizeof(char), txt.size(), outputfile );
    fwrite( "\n", sizeof(char), sizeof(char), outputfile );
    
    return outputfile;
}

} // namespace YumaTest


// ---------------------------------------------------------------------------!
