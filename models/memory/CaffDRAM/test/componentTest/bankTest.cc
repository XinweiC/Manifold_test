#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <list>
#include <stdlib.h>
#include "Bank.h"
#include "Dsettings.h"
#include "Dreq.h"

using namespace std;

using namespace manifold::caffdram;

//####################################################################
// helper classes
//####################################################################


//####################################################################
//! Class BankTest is the test class for class Bank. 
//####################################################################
class BankTest : public CppUnit::TestFixture {
private:


public:

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }




    //======================================================================
    //======================================================================
    //! @brief Test processRequest()
    //!
    //! Create a few Banks. For each Bank, call processRequest() N times with
    //! different requests. Verify bankAvailableAtTime, lastAccessedRow and return value.
    void test_processRequest_0()
    {
        //Different settings:
	// Dramsetting: 
	//     - Page policy: open, or closed
	//     - t_CAS, t_RP, t_RCD, t_BURST, t_CMD
	// Bank:
	//     We should not manipulate the Bank's parameters becase Bank is the
	//     class under test.
	//
	// Dreq:
	//     - origin time : > , ==, or < current bankAvailableAtTime
	//     - rowId: ==, or != Bank's lastAccessedRow
	// 

	const int Nbanks = 10;  //number of Banks to test
	const int Nreqs = 10000;  // number of requests to process for each Bank

	for(int b=0; b<Nbanks; b++) {
	    unsigned long Last_originTime = 0;  //remember the originTime of the last request

	    //Create a Bank
	    Dsettings setting;

	    if(random() / (RAND_MAX + 1.0) < 0.5) //50% chance
		setting.memPagePolicy = OPEN_PAGE;
	    else
		setting.memPagePolicy = CLOSED_PAGE;

	    Bank* myBank = new Bank(&setting); //constructor requires a settings object


	    //try Nreqs requests
	    for(int r=0; r<Nreqs; r++) {
		//Init the setting object
		setting.t_CAS = random() % 90 + 10; //between 10 and 100
		setting.t_RP = random() % 90 + 10; //between 10 and 100
		setting.t_RCD = random() % 90 + 10; //between 10 and 100
		setting.t_BURST = random() % 90 + 10; //between 10 and 100
		setting.t_CMD =  5; //fixed
		setting.update();

		//Create a request
		unsigned long addr = random();
		unsigned long currentSimTime = 0; //this is used to set originTime; since we are
		                                  //going to set originTime below, this value is not important.

		Dreq dreq(addr, currentSimTime, &setting);

		double rn = random() / (RAND_MAX + 1.0);
		if(rn < 0.4 && r > 0)  { //40% chance originTime < bankAvailableTime
					 //of course this doesn't apply to the 1st request
		    CPPUNIT_ASSERT(myBank->bankAvailableAtTime > Last_originTime);
		    dreq.originTime = Last_originTime + random() % (myBank->bankAvailableAtTime - Last_originTime);
		}
		else if(rn < 0.8) { //40% chance originTime > bankAvailableTime
		    dreq.originTime = myBank->bankAvailableAtTime + random() % 19 + 1; //current availableTime plus a
										       //a number between 1 and 20
		}
		else {
		    dreq.originTime = myBank->bankAvailableAtTime;
		    if(dreq.originTime == 0) //originTime can never be 0
		        dreq.originTime = 1;
		}

		if(setting.memPagePolicy == OPEN_PAGE) {
		    if((random() / (RAND_MAX + 1.0) < 0.6)) {
			dreq.rowId = myBank->lastAccessedRow;
		    }
		    else {
		        //set rowId to a different value
			while((dreq.rowId = random()) == myBank->lastAccessedRow);
		    }
		}


		//save the current bankAvailableAtTime
		unsigned long old_bankAvailableAtTime = myBank->bankAvailableAtTime;
		unsigned old_lastAccessedRow = myBank->lastAccessedRow;


		//verify firstAccess
		if(setting.memPagePolicy == OPEN_PAGE) {
		    if(r != 0)
			CPPUNIT_ASSERT_EQUAL(false, myBank->firstAccess);
		    else
			CPPUNIT_ASSERT_EQUAL(true, myBank->firstAccess);
		}


		//==============================================
		unsigned long rv = myBank->processRequest(&dreq);
		//==============================================



		//expected_availableTime is what we expect the Bank's bankAvailableAtTime
		//would be after the call of processRequest().
		unsigned long expected_availableTime = old_bankAvailableAtTime;
		if(dreq.originTime > old_bankAvailableAtTime)
		    expected_availableTime = dreq.originTime;

		if(setting.memPagePolicy == CLOSED_PAGE) {
			expected_availableTime +=  setting.t_RCD + setting.t_CAS + setting.t_RP;
			CPPUNIT_ASSERT_EQUAL(expected_availableTime, myBank->bankAvailableAtTime);
		}
		else { //open page
		    if(r == 0) { //first access
			expected_availableTime +=  setting.t_RCD + setting.t_CAS;
			CPPUNIT_ASSERT_EQUAL(expected_availableTime, myBank->bankAvailableAtTime);
		    }
		    else {//not 1st access
		        if(dreq.rowId != old_lastAccessedRow) {
			    expected_availableTime += setting.t_RP + setting.t_RCD + setting.t_CAS;
			    CPPUNIT_ASSERT_EQUAL(expected_availableTime, myBank->bankAvailableAtTime);
			}
			else {
			    expected_availableTime += setting.t_CAS;
			    CPPUNIT_ASSERT_EQUAL(expected_availableTime, myBank->bankAvailableAtTime);
			}
		    }
		}

		//verify return value of processRequest()
		CPPUNIT_ASSERT_EQUAL(expected_availableTime + setting.t_BURST, rv);

		//verify lastAccessedRow
		if(setting.memPagePolicy == OPEN_PAGE)
		    CPPUNIT_ASSERT_EQUAL(dreq.rowId, myBank->lastAccessedRow);

		Last_originTime = dreq.originTime;

	    }

	    delete myBank;
	}

    }





    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("BankTest");

	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processRequest_0", &BankTest::test_processRequest_0));

	return mySuite;
    }
};



int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( BankTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


