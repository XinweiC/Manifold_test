#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <list>
#include <stdlib.h>
#include "Rank.h"

using namespace std;

using namespace manifold::caffdram;

//####################################################################
// helper classes
//####################################################################


//####################################################################
//! Class RankTest is the test class for class Rank. 
//####################################################################
class RankTest : public CppUnit::TestFixture {
private:


public:

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }




    //======================================================================
    //======================================================================
    //! @brief Test constructor
    //!
    void test_Constructor_0()
    {
	Dsettings setting;
	setting.numBanks = random() % 999 + 1;  //1 to 1000 Banks

	Rank* myRank = new Rank(&setting);

	CPPUNIT_ASSERT(&setting == myRank->dramSetting);

	for(int i=0; i<setting.numBanks; i++) {
	    Bank* b = myRank->myBank[i];
	    CPPUNIT_ASSERT(&setting == b->dramSetting);
	}

	delete myRank;

    }




    //======================================================================
    //======================================================================
    //! @brief Test processRequest()
    //!
    //! Create a Rank with a random number of Banks. Call processRequest()
    //! N times; verify the return value is the same as calling the Bank's
    //! processRequest().  Pages are OPEN_PAGE.
    void test_processRequest_0()
    {
	Dsettings setting;
	setting.memPagePolicy = OPEN_PAGE;
	setting.numBanks = random() % 99 + 1;  //1 to 100 Banks

	Rank* myRank = new Rank(&setting);

	const int Nreqs = 10000;

	for(int r=0; r<Nreqs; r++) {
	    unsigned long addr = random();
	    unsigned long currentSimTime = random(); //current simulation time

	    Dreq dreq(addr, currentSimTime, &setting);
	    dreq.bankId = random() % setting.numBanks;

	    Bank bank_copy = *(myRank->myBank[dreq.bankId]);
	    Dreq dreq_copy = dreq;

	    unsigned long rv = myRank->processRequest(&dreq);

	    //verify return value
	    CPPUNIT_ASSERT_EQUAL(bank_copy.processRequest(&dreq_copy), rv);
	}

	delete myRank;

    }




    //======================================================================
    //======================================================================
    //! @brief Test processRequest()
    //!
    //! Create a Rank with a random number of Banks. Call processRequest()
    //! N times; verify the return value is the same as calling the Bank's
    //! processRequest().  Pages are CLOSED_PAGE.
    void test_processRequest_1()
    {
	Dsettings setting;
	setting.memPagePolicy = CLOSED_PAGE;
	setting.numBanks = random() % 99 + 1;  //1 to 100 Banks

	Rank* myRank = new Rank(&setting);

	const int Nreqs = 10000;

	for(int r=0; r<Nreqs; r++) {
	    unsigned long addr = random();
	    unsigned long currentSimTime = random(); //current simulation time

	    Dreq dreq(addr, currentSimTime, &setting);
	    dreq.bankId = random() % setting.numBanks;

	    Bank bank_copy = *(myRank->myBank[dreq.bankId]);
	    Dreq dreq_copy = dreq;

	    unsigned long rv = myRank->processRequest(&dreq);

	    //verify return value
	    CPPUNIT_ASSERT_EQUAL(bank_copy.processRequest(&dreq_copy), rv);
	}

	delete myRank;

    }





    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("RankTest");

	mySuite->addTest(new CppUnit::TestCaller<RankTest>("test_Constructor_0", &RankTest::test_Constructor_0));
	mySuite->addTest(new CppUnit::TestCaller<RankTest>("test_processRequest_0", &RankTest::test_processRequest_0));
	mySuite->addTest(new CppUnit::TestCaller<RankTest>("test_processRequest_1", &RankTest::test_processRequest_1));

	return mySuite;
    }
};



int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( RankTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


