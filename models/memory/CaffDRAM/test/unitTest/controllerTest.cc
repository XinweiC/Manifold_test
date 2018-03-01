#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <list>
#include <stdlib.h>
#include "Controller.h"

using namespace std;

using namespace manifold::caffdram;

//####################################################################
// helper classes
//####################################################################

#define MEM_MSG 123
#define CREDIT_MSG 456

//####################################################################
//! Class ControllerTest is the test class for class Controller. 
//####################################################################
class ControllerTest : public CppUnit::TestFixture {
private:


public:

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
        Controller :: Msg_type_set = false;
	Controller :: Set_msg_types(MEM_MSG, CREDIT_MSG);
    }




    //======================================================================
    //======================================================================
    //! @brief Test constructor
    //!
    //! Ensure the specified number of Channels, Ranks, Banks are created,
    //! and they share the same settings object.
    void test_Constructor_0()
    {
	Dsettings setting;
	setting.numChannels = 0x1 << (random() % 8);  //1, 2, ... 128 Channels
	setting.numRanks = 0x1 << (random() % 8 + 1);  //2, 4, ..., 256 Ranks
	setting.numBanks = 0x1 << (random() % 8 + 1);  //2, 4, .., 256 Banks

	int nid = random();
	bool st_resp = false;
	if(random() / (RAND_MAX + 1.0) < 0.5)
	    st_resp = true;

	Controller* myController = new Controller(nid, setting, st_resp);


	CPPUNIT_ASSERT_EQUAL(nid, myController->get_nid());
	CPPUNIT_ASSERT_EQUAL(st_resp, myController->m_send_st_response);


	//verify all banks of all ranks of all channels use same setting object.
	for(int c=0; c<setting.numChannels; c++) {
	    Channel* chan = myController->myChannel[c];
	    for(int r=0; r<setting.numRanks; r++) {
		Rank* rnk = chan->myRank[r];

		for(int b=0; b<setting.numBanks; b++) {
		    Bank* bnk = rnk->myBank[b];
		    CPPUNIT_ASSERT(myController->dramSetting == bnk->dramSetting);
		}
	    }
	}

	delete myController;

    }




    //======================================================================
    //======================================================================
    //! @brief Test processRequest()
    //!
    //! Create a Controller with a random number of Channels. Call processRequest()
    //! N times; verify the return value is the same as calling the Channel's
    //! processRequest().  Pages are OPEN_PAGE.
    void test_processRequest_0()
    {
	Dsettings setting;
	setting.memPagePolicy = OPEN_PAGE;
	setting.numChannels = 0x1 << (random() % 3);  //1, 2, or 4 Channels
	setting.numRanks = 0x1 << (random() % 3 + 1);  //2, 4, or 8 Ranks
	setting.numBanks = 0x1 << (random() % 5 + 1);  //2, 4, .., 32 Banks

	int nid = random();
	bool st_resp = false;
	if(random() / (RAND_MAX + 1.0) < 0.5)
	    st_resp = true;

	Controller* myController = new Controller(nid, setting, st_resp);
	setting = *(myController->dramSetting);

	const int Nreqs = 10000;

	for(int r=0; r<Nreqs; r++) {
	    unsigned long addr = random();
	    unsigned long currentSimTime = random(); //current simulation time

	    Dreq dreq(addr, currentSimTime, &setting);

	    //make a deep copy of the channel, all the way to the bank.
	    Channel channel_copy = *(myController->myChannel[dreq.chId]);
	    channel_copy.myRank = new Rank*[setting.numRanks];
	    for(int r=0; r<setting.numRanks; r++)
	        channel_copy.myRank[r] = 0;
            //make a copy of the rank of interest.
	    channel_copy.myRank[dreq.rankId] = new Rank(*(myController->myChannel[dreq.chId]->myRank[dreq.rankId]));
	    Rank* rank_copy = channel_copy.myRank[dreq.rankId];

	    rank_copy->myBank = new Bank*[setting.numBanks];
	    for(int b=0; b<setting.numBanks; b++)
	        rank_copy->myBank[b] = 0;
            //make a copy of the bank
	    rank_copy->myBank[dreq.bankId] = new Bank(*(myController->myChannel[dreq.chId]->myRank[dreq.rankId]->myBank[dreq.bankId]));

	    unsigned long rv = myController->processRequest(addr, currentSimTime);

	    //verify return value
	    CPPUNIT_ASSERT_EQUAL(channel_copy.processRequest(&dreq), rv);
	}

	delete myController;

    }







    //======================================================================
    //======================================================================
    //! @brief Test processRequest()
    //!
    //! Create a Controller with a random number of Channels. Call processRequest()
    //! N times; verify the return value is the same as calling the Channel's
    //! processRequest().  Pages are CLOSED_PAGE.
    void test_processRequest_1()
    {
	Dsettings setting;
	setting.memPagePolicy = CLOSED_PAGE;
	setting.numChannels = 0x1 << (random() % 3);  //1, 2, or 4 Channels
	setting.numRanks = 0x1 << (random() % 3 + 1);  //2, 4, or 8 Ranks
	setting.numBanks = 0x1 << (random() % 5 + 1);  //2, 4, .., 32 Banks

	int nid = random();
	bool st_resp = false;
	if(random() / (RAND_MAX + 1.0) < 0.5)
	    st_resp = true;

	Controller* myController = new Controller(nid, setting, st_resp);
	setting = *(myController->dramSetting);

	const int Nreqs = 10000;

	for(int r=0; r<Nreqs; r++) {
	    unsigned long addr = random();
	    unsigned long currentSimTime = random(); //current simulation time

	    Dreq dreq(addr, currentSimTime, &setting);

	    //make a deep copy of the channel, all the way to the bank.
	    Channel channel_copy = *(myController->myChannel[dreq.chId]);
	    channel_copy.myRank = new Rank*[setting.numRanks];
	    for(int r=0; r<setting.numRanks; r++)
	        channel_copy.myRank[r] = 0;
            //make a copy of the rank of interest.
	    channel_copy.myRank[dreq.rankId] = new Rank(*(myController->myChannel[dreq.chId]->myRank[dreq.rankId]));
	    Rank* rank_copy = channel_copy.myRank[dreq.rankId];

	    rank_copy->myBank = new Bank*[setting.numBanks];
	    for(int b=0; b<setting.numBanks; b++)
	        rank_copy->myBank[b] = 0;
            //make a copy of the bank
	    rank_copy->myBank[dreq.bankId] = new Bank(*(myController->myChannel[dreq.chId]->myRank[dreq.rankId]->myBank[dreq.bankId]));

	    unsigned long rv = myController->processRequest(addr, currentSimTime);

	    //verify return value
	    CPPUNIT_ASSERT_EQUAL(channel_copy.processRequest(&dreq), rv);
	}

	delete myController;

    }







    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("ControllerTest");

	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_Constructor_0", &ControllerTest::test_Constructor_0));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_processRequest_0", &ControllerTest::test_processRequest_0));
	mySuite->addTest(new CppUnit::TestCaller<ControllerTest>("test_processRequest_1", &ControllerTest::test_processRequest_1));

	return mySuite;
    }
};



int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( ControllerTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


