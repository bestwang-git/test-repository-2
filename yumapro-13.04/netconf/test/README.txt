The YumaPro Test Harness (v13.04)
===============================================================================

1: Pre-requisites:
    Operating System	Ubuntu 10.04 or later	
    Compiler	        GCC 4.4 or later	
    Boost	        Boost 1.47 or later
    Lib Xml 	        libxml2 / libxml2-dev
    Python	        2.6 Python interpreter
    Libbz2-dev	        1.05-4ubuntu0.1	
    Doxygen		Source code documentation tool
    Graphviz		Graphical Visualisation Tool used by Doxygen
    Texlive-font-utils  Text formatting used by doxygen
    Libncursesw5	5.7 Shared libraries for terminal handling
    libssh2-1-dev	1.2.2-1 SSH2 ssh2 client-side libary

[ABB: 
  *** mandatory to install boost:
  ***  rest optional or already installed to develop yumapro
   sudo apt-get install libboost-all-dev
]

To build all test harnesses (takes about 4 minutes)

   ypwork> make clean
   ypwork> make PRODUCTION=1 USE_WERROR=1 [add more flags as needed]
   ypwork> sudo make install
   ypwork> make TEST=1 clean
   ypwork> make TEST=1

[ABB: Note that the 'poorly formed XML message' tests are supposed
 to cause the server to drop the session, which causes the test to wait
 30 seconds for the reply that would be an error.  The test is expecting
 the timeout to be the correct response]

2: The Integration Test Harness
netconf/test/integ-tests

The Integration test harness is built as a stand alone executable that 
includes most of the Yuma agt and ncx sources (which make up the system 
under test). This test harness runs on the local host machine and can be
 used to quickly verify the behaviour of netconfd.

To Run Tests:
  ypwork>  cd netconf/test/integ-tests
  ypwork>  ./alltests.py

Note: tests can be run individually, by specifying the name of the
test suite and test on the command line as follows:
    ./test-simple-edit-running \
        --run_test=simple_get_test_suite_running/edit_slt_entries_test1

For more information on test harness command line options see:
    http://www.boost.org/doc/libs/1_47_0/libs/test/doc/html/utf/user-guide/runtime-config.html

3: The System Test Harness
netconf/test/sys-test

The System test harness is a stand alone program that is capable of 
running full NETCONF sessions against a full Yuma/Netconf Server
(the system under test). The System test harness provides a fast 
way of verifying the behaviour of a full Yuma/Netconf system.
It behaves in the same way as a real Netconf client. To use 
this test harness the Netconf Server must have the appropriate
YANG modules installed. The SIL modules for the YANG libraries
can be loaded, but this is not required.

Make sure to build and install the server you are testing!
Make sure to run the configuration scripts before the tests!

To make sure the proper modules get loaded, either configure
the netconfd --modpath parameter, set the YUMA_MODPATH environment
variable, or put the YANG files a directory called 'modules' in
your HOME directory.

***************************************************************
   ABB: Make sure to remove the following files if they exist:

   /etc/yuma/netconfd.conf
   /etc/yumapro/netconfd-pro.conf

  Not sure what CLI parms passed to server but if the
  default config file is read it will often cause tests to fail
  because the assumed CLI parameter settings are not in ue
*****************************************************************

To build and run this test harness:
    1: cp netconf/test/modules/yang/*.yang $HOME/modules/
    2: start the server and make sure at least these flags are set:
       --access-control=off OR --superuser=<user>
       --target=candidate OR --target=running
       --no-startup
     ** optional: set log-level=warn
    3: run ./configure-env.py to configure the test client
       - make sure to give IP addresses as numbers, such as
         127.0.0.1 instead of localhost. port is always 830.
       - enter the same user name as the <user> in step 2.
    4: source ./config.sh
    5: run either all-candidate.py or all-running.py, depending on
       which target was selected in step 2.


