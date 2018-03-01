#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <list>
#include <stdlib.h>
#include "Channel.h"

using namespace std;

using namespace manifold::caffdram;

//####################################################################
// helper classes
//####################################################################


//####################################################################
//! Class ChannelTest is the test class for class Channel. 
//####################################################################
class ChannelTest : public CppUnit::TestFixture {
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
	setting.numRanks = random() % 99 + 1;  //1 to 100 Ranks
	setting.numBanks = random() % 999 + 1;  //1 to 1000 Banks

	Channel* myChannel = new Channel(&setting);

	CPPUNIT_ASSERT(&setting == myChannel->dramSetting);

	//verify all banks of all ranks use same setting object.
	for(int i=0; i<setting.numRanks; i++) {
	    Rank* r = myChannel->myRank[i];

	    for(int j=0; j<setting.numBanks; j++) {
		Bank* b = r->myBank[j];
		CPPUNIT_ASSERT(&setting == b->dramSetting);
	    }
	}

	delete myChannel;

    }




    //======================================================================
    //======================================================================
    //! @brief Test processRequest()
    //!
    //! Create a Channel with a random number of Ranks. Call processRequest()
    //! N times; verify the return value is the same as calling the Rank's
    //! processRequest().  Pages are OPEN_PAGE.
    void test_processRequest_0()
    {
	Dsettings setting;
	setting.memPagePolicy = OPEN_PAGE;
	setting.numRanks = random() % 19 + 1;  //1 to 20 Ranks
	setting.numBanks = random() % 19 + 1;  //1 to 20 Banks

	Channel* myChannel = new Channel(&setting);

	const int Nreqs = 10000;

	for(int r=0; r<Nreqs; r++) {
	    unsigned long addr = random();
	    unsigned long currentSimTime = random(); //current simulation time

	    Dreq dreq(addr, currentSimTime, &setting);

	    dreq.rankId = random() % setting.numRanks;
	    dreq.bankId = random() % setting.numBanks;

	    //make a deep copy of the rank
	    Rank rank_copy = *(myChannel->myRank[dreq.rankId]);
	    rank_copy.myBank = new Bank*[setting.numBanks];
	    for(int b=0; b<setting.numBanks; b++)
	        rank_copy.myBank[b] = 0;
            //make a copy of the bank
	    rank_copy.myBank[dreq.bankId] = new Bank(*(myChannel->myRank[dreq.rankId]->myBank[dreq.bankId]));
	    Dreq dreq_copy = dreq;

	    unsigned long rv = myChannel->processRequest(&dreq);

	    //verify return value
	    CPPUNIT_ASSERT_EQUAL(rank_copy.processRequest(&dreq_copy), rv);
	}

	delete myChannel;

    }




    //======================================================================
    //======================================================================
    //! @brief Test processRequest()
    //!
    //! Create a Channel with a random number of Ranks. Call processRequest()
    //! N times; verify the return value is the same as calling the Rank's
    //! processRequest().  Pages are CLOSED_PAGE.
    void test_processRequest_1()
    {
	Dsettings setting;
	setting.memPagePolicy = CLOSED_PAGE;
	setting.numRanks = random() % 19 + 1;  //1 to 20 Ranks
	setting.numBanks = random() % 19 + 1;  //1 to 20 Banks

	Channel* myChannel = new Channel(&setting);

	const int Nreqs = 10000;

	for(int r=0; r<Nreqs; r++) {
	    unsigned long addr = random();
	    unsigned long currentSimTime = random(); //current simulation time

	    Dreq dreq(addr, currentSimTime, &setting);

	    dreq.rankId = random() % setting.numRanks;
	    dreq.bankId = random() % setting.numBanks;

	    //make a deep copy of the rank
	    Rank rank_copy = *(myChannel->myRank[dreq.rankId]);
	    rank_copy.myBank = new Bank*[setting.numBanks];
	    for(int b=0; b<setting.numBanks; b++)
	        rank_copy.myBank[b] = 0;
            //make a copy of the bank
	    rank_copy.myBank[dreq.bankId] = new Bank(*(myChannel->myRank[dreq.rankId]->myBank[dreq.bankId]));
	    Dreq dreq_copy = dreq;

	    unsigned long rv = myChannel->processRequest(&dreq);

	    //verify return value
	    CPPUNIT_ASSERT_EQUAL(rank_copy.processRequest(&dreq_copy), rv);
	}

	delete myChannel;

    }







    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("ChannelTest");

	mySuite->addTest(new CppUnit::TestCaller<ChannelTest>("test_Constructor_0", &ChannelTest::test_Constructor_0));
	mySuite->addTest(new CppUnit::TestCaller<ChannelTest>("test_processRequest_0", &ChannelTest::test_processRequest_0));
	mySuite->addTest(new CppUnit::TestCaller<ChannelTest>("test_processRequest_1", &ChannelTest::test_processRequest_1));

	return mySuite;
    }
};



int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( ChannelTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


