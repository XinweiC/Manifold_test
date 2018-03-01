#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>
#include "../../coherence/MESI_manager.h"
#include <iostream>
#include <stdlib.h>

using namespace std;
using namespace manifold::mcp_cache_namespace;


//####################################################################
// helper function Send
//####################################################################


struct Message {
    MESI_messages_t msg;
    int destID;
    int fwdID;
};

class My_MESI_manager : public MESI_manager {
public:
    My_MESI_manager(int id) : MESI_manager(id)
    {
        m_client_writeback_called = false;
        m_invalidate_called = false;
    }

    bool process_lower_client_request(void*, bool) {}
    void process_lower_client_reply(void*) {}
    bool is_invalidation_request(void*) {}
    void ignore() {}

    virtual void sendmsg(bool req, MESI_messages_t msg, int destID, int fwdID=0)
    {
        Message m;
	m.msg = msg;
	m.destID = destID;
	m.fwdID = fwdID;
	m_messages.push_back(m);
    }
    void client_writeback() { m_client_writeback_called = true; }
    void invalidate() { m_invalidate_called = true; }

    vector<Message>& get_messages() { return m_messages; }
    bool get_client_writeback_called() { return m_client_writeback_called; }
    bool get_invalidate_called() { return m_invalidate_called; }

    void clear()
    {
	m_messages.clear();
	m_client_writeback_called = false;
	m_invalidate_called = false;
    }

private:
    vector<Message> m_messages;
    bool m_client_writeback_called;
    bool m_invalidate_called;
};


//####################################################################
//! Class MESI_managerTest is the unit test class for class MESI_manager.
//####################################################################
class MESI_managerTest : public CppUnit::TestFixture
{
private:
    MESI_manager* manager;
    /*
    */
public:
    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
        manager = new My_MESI_manager(random() % 1024);
    }
    //! Finialization function. Inherited from the CPPUnit framework.
    void tearDown()
    {
        delete manager;
    }


    //======================================================================
    //======================================================================
    //! Test constructor
    void testConstructor()
    {
        CPPUNIT_ASSERT(MESI_MNG_I == manager->state);
    }


    void testIState()
    {
        int SOURCE_ID = random() % 1024;

        manager->transition_to_i();
        // Simulate Exclusive Request
        manager->process(MESI_CM_I_to_E, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_IE == manager->state);
        CPPUNIT_ASSERT(SOURCE_ID == manager->owner);
	CPPUNIT_ASSERT(MESI_MC_GRANT_E_DATA == ((My_MESI_manager*)manager)->get_messages().back().msg);
	CPPUNIT_ASSERT(SOURCE_ID == ((My_MESI_manager*)manager)->get_messages().back().destID);


        manager->transition_to_i();
        // Simulate Share (Read) Request
        manager->process(MESI_CM_I_to_S, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_IE == manager->state);
        CPPUNIT_ASSERT(SOURCE_ID == manager->owner);
	CPPUNIT_ASSERT(MESI_MC_GRANT_E_DATA == ((My_MESI_manager*)manager)->get_messages().back().msg);
	CPPUNIT_ASSERT(SOURCE_ID == ((My_MESI_manager*)manager)->get_messages().back().destID);
    }

    void testEState()
    {
        int SOURCE_ID = random() % 1024;
	int OWNER_ID;
	while((OWNER_ID = random() % 1024) == SOURCE_ID);

        // No request is sent silent
        manager->transition_to_e();
        manager->owner = OWNER_ID; // set owner.
        // Simulate READ Request
        manager->process(MESI_CM_I_to_E, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_EE == manager->state);
	CPPUNIT_ASSERT(MESI_MC_FWD_E == ((My_MESI_manager*)manager)->get_messages().back().msg);
	CPPUNIT_ASSERT(OWNER_ID == ((My_MESI_manager*)manager)->get_messages().back().destID);
	CPPUNIT_ASSERT(SOURCE_ID == ((My_MESI_manager*)manager)->get_messages().back().fwdID);


        manager->transition_to_e();
        manager->owner = OWNER_ID;
        // Simulate Another read request.
        manager->process(MESI_CM_I_to_S, SOURCE_ID);
        // Should transition to ES no more owner sent fwds to ID1 now have 2 sharers.
        CPPUNIT_ASSERT(MESI_MNG_ES == manager->state);
        CPPUNIT_ASSERT(-1 == manager->owner);
        CPPUNIT_ASSERT(manager->sharersList.get(OWNER_ID));
        CPPUNIT_ASSERT(manager->sharersList.get(SOURCE_ID));
	//msg to owner
	CPPUNIT_ASSERT(MESI_MC_FWD_S == ((My_MESI_manager*)manager)->get_messages().back().msg);
	CPPUNIT_ASSERT_EQUAL(OWNER_ID, ((My_MESI_manager*)manager)->get_messages().back().destID);


        manager->transition_to_e();
        manager->owner = OWNER_ID;
        // Simulate EVICTION
        manager->process(GET_EVICT, 0);
        CPPUNIT_ASSERT(MESI_MNG_EI_EVICT == manager->state);
        CPPUNIT_ASSERT(-1 == manager->owner);
	//msg to owner
	CPPUNIT_ASSERT(MESI_MC_DEMAND_I == ((My_MESI_manager*)manager)->get_messages().back().msg);
	CPPUNIT_ASSERT(OWNER_ID == ((My_MESI_manager*)manager)->get_messages().back().destID);


        // No request is sent silent
        manager->transition_to_e();
        manager->owner = OWNER_ID;
        // Simulate Evict Request
        manager->process(MESI_CM_E_to_I, OWNER_ID); //for CM_E_to_I, sending client must be the owner
        CPPUNIT_ASSERT(MESI_MNG_EI_PUT == manager->state);
        CPPUNIT_ASSERT(-1 == manager->owner);
	CPPUNIT_ASSERT(MESI_MC_GRANT_I == ((My_MESI_manager*)manager)->get_messages().back().msg);
	CPPUNIT_ASSERT(OWNER_ID == ((My_MESI_manager*)manager)->get_messages().back().destID);


	//((My_MESI_manager*)manager)->clear_messages();
	CPPUNIT_ASSERT(false == ((My_MESI_manager*)manager)->get_client_writeback_called());

        manager->transition_to_e();
        manager->owner = OWNER_ID;
        // Simulate Evict Request
        manager->process(MESI_CM_M_to_I, OWNER_ID); //for CM_M_to_I, sending client must be the owner
        CPPUNIT_ASSERT(MESI_MNG_EI_PUT == manager->state);
        CPPUNIT_ASSERT(-1 == manager->owner);
	CPPUNIT_ASSERT(true == ((My_MESI_manager*)manager)->get_client_writeback_called());
	//msg to owner
	CPPUNIT_ASSERT(MESI_MC_GRANT_I == ((My_MESI_manager*)manager)->get_messages().back().msg);
	CPPUNIT_ASSERT(OWNER_ID == ((My_MESI_manager*)manager)->get_messages().back().destID);

    }

    void testIEState()
    {
        int SOURCE_ID = random() % 1024;
        int OWNER_ID = random() % 1024;

        manager->transition_to_ie();
        manager->owner = OWNER_ID;
        // Simulate UNBLOCK E
        manager->process(MESI_CM_UNBLOCK_E, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_E == manager->state);
	//no msg sent
	CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_manager*)manager)->get_messages().size());
    }

    void testEEState()
    {
        int SOURCE_ID = random() % 1024;

        manager->transition_to_ee();
        manager->owner = 0;
        // Simulate UNBLOCK E
        manager->process(MESI_CM_UNBLOCK_E, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_E == manager->state);
	//no msg sent
	CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_manager*)manager)->get_messages().size());
    }

    void testEIPUTState()
    {
        int SOURCE_ID = random() % 1024;

	CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_manager*)manager)->get_invalidate_called());

        manager->transition_to_ei_put();
        // Simulate UNBLOCK I
        manager->process(MESI_CM_UNBLOCK_I, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_I == manager->state);
	CPPUNIT_ASSERT_EQUAL(true, ((My_MESI_manager*)manager)->get_invalidate_called());
	//no msg sent
	CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_manager*)manager)->get_messages().size());
    }

    void testEIEVICTState()
    {
        int SOURCE_ID = random() % 1024;

	CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_manager*)manager)->get_invalidate_called());

        manager->transition_to_ei_evict();
        // Simulate UNBLOCK I
        manager->process(MESI_CM_UNBLOCK_I, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_I == manager->state);
	CPPUNIT_ASSERT_EQUAL(true, ((My_MESI_manager*)manager)->get_invalidate_called());


	((My_MESI_manager*)manager)->clear();
	CPPUNIT_ASSERT(false == ((My_MESI_manager*)manager)->get_client_writeback_called());
	CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_manager*)manager)->get_invalidate_called());

        manager->transition_to_ei_evict();
        // Simulate UNBLOCK I_DIRTY
        manager->process(MESI_CM_UNBLOCK_I_DIRTY, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_I == manager->state);
	CPPUNIT_ASSERT_EQUAL(true, ((My_MESI_manager*)manager)->get_client_writeback_called());
	CPPUNIT_ASSERT_EQUAL(true, ((My_MESI_manager*)manager)->get_invalidate_called());
    }

    void testESState()
    {
        int SOURCE_ID = random() % 1024;

        manager->transition_to_es();
        // Simulate Requests needed
        manager->process(MESI_CM_UNBLOCK_S, SOURCE_ID);
        manager->process(MESI_CM_WRITEBACK, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_S == manager->state);
	//no msg sent
	CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_manager*)manager)->get_messages().size());


        manager->transition_to_es();
        // Simulate Requests needed
        manager->process(MESI_CM_WRITEBACK, SOURCE_ID);
        manager->process(MESI_CM_UNBLOCK_S, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_S == manager->state);
	//no msg sent
	CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_manager*)manager)->get_messages().size());
    }

    void testSState()
    {
	//generic unique IDs to represent sharers
        int SHARERS[5];

	for(int i=0; i<5; i++) {
	    bool ok = true;
	    int id;
	    do {
		id = random() % 1024;
		for(int j=0; j<i; j++) {
		    if(id == SHARERS[i]) {
		        ok = false;
			break;
		    }
		}
	    } while(ok == false);
	    SHARERS[i] = id;
	}

//for(int i=0; i<5; i++)
//cout << "id= " << SHARERS[i] << endl;

        int SOURCE_ID = SHARERS[0];

        manager->transition_to_s();
        manager->sharersList.set(SHARERS[1]);
        manager->sharersList.set(SHARERS[2]);
        // Simulate Another Sharer joining in
        manager->process(MESI_CM_I_to_S, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_SS == manager->state);
        CPPUNIT_ASSERT(manager->sharersList.get(SOURCE_ID));
	//msg to sender
	CPPUNIT_ASSERT(MESI_MC_GRANT_S_DATA == ((My_MESI_manager*)manager)->get_messages().back().msg);
	CPPUNIT_ASSERT(SOURCE_ID == ((My_MESI_manager*)manager)->get_messages().back().destID);


        // Clear state.
        manager->sharersList.clear();
        
	((My_MESI_manager*)manager)->clear();
        
        manager->transition_to_s();
        manager->sharersList.set(SHARERS[1]);
        manager->sharersList.set(SHARERS[2]);
        manager->sharersList.set(SHARERS[3]);
        std::vector<int> sharersList;
        // Simulate Requests needed
        manager->process(MESI_CM_I_to_E, SOURCE_ID);
        manager->sharersList.ones(sharersList); // Get list of sharers
        CPPUNIT_ASSERT(MESI_MNG_SIE == manager->state);
        CPPUNIT_ASSERT(SOURCE_ID == manager->owner);
        CPPUNIT_ASSERT(0 == sharersList.size());
        CPPUNIT_ASSERT(0 == manager->num_invalidations);
        CPPUNIT_ASSERT(3 == manager->num_invalidations_req);
	//msg to sharers
	CPPUNIT_ASSERT(MESI_MC_DEMAND_I == ((My_MESI_manager*)manager)->get_messages()[0].msg);
	CPPUNIT_ASSERT(MESI_MC_DEMAND_I == ((My_MESI_manager*)manager)->get_messages()[1].msg);
	CPPUNIT_ASSERT(MESI_MC_DEMAND_I == ((My_MESI_manager*)manager)->get_messages()[2].msg);
	for(int i=1; i<=3; i++) {
	    bool found = false;
	    for(int j=0; j<3; j++) {
		if(SHARERS[i] == ((My_MESI_manager*)manager)->get_messages()[j].destID) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}


        // Clear state.
        manager->sharersList.clear();
        
	((My_MESI_manager*)manager)->clear();

        manager->transition_to_s();
        manager->sharersList.set(SHARERS[1]);
        manager->sharersList.set(SHARERS[2]);
        manager->sharersList.set(SHARERS[3]);
        sharersList.clear();
        // Simulate Requests needed
        manager->process(GET_EVICT, SOURCE_ID);
        manager->sharersList.ones(sharersList); // Get list of sharers
        CPPUNIT_ASSERT(MESI_MNG_SI_EVICT == manager->state);
        CPPUNIT_ASSERT(SOURCE_ID == manager->owner);
        CPPUNIT_ASSERT(0 == sharersList.size());
        CPPUNIT_ASSERT(0 == manager->num_invalidations);
        CPPUNIT_ASSERT(3 == manager->num_invalidations_req);
	//msg to sharers
	CPPUNIT_ASSERT(MESI_MC_DEMAND_I == ((My_MESI_manager*)manager)->get_messages()[0].msg);
	CPPUNIT_ASSERT(MESI_MC_DEMAND_I == ((My_MESI_manager*)manager)->get_messages()[1].msg);
	CPPUNIT_ASSERT(MESI_MC_DEMAND_I == ((My_MESI_manager*)manager)->get_messages()[2].msg);
	for(int i=1; i<=3; i++) {
	    bool found = false;
	    for(int j=0; j<3; j++) {
		if(SHARERS[i] == ((My_MESI_manager*)manager)->get_messages()[j].destID) {
		    found = true;
		    break;
		}
	    }
	    CPPUNIT_ASSERT_EQUAL(true, found);
	}
    }

    void testSSState()
    {
        int SOURCE_ID = random() % 1024;

        manager->transition_to_ss();
        // Simulate Another Sharer joining in
        manager->process(MESI_CM_UNBLOCK_S, SOURCE_ID);
        CPPUNIT_ASSERT(MESI_MNG_S == manager->state);
	//no msg sent
	CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_manager*)manager)->get_messages().size());
    }


    void testSIEVICTState()
    {
        int SOURCE_ID0 = random() % 1024;
        int SOURCE_ID1 = random() % 1024;
        int SOURCE_ID2 = random() % 1024;

	CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_manager*)manager)->get_invalidate_called());

        manager->transition_to_si_evict();
        manager->num_invalidations = 0;
        manager->num_invalidations_req = 3;
        // Simulate Another Sharer joining in
        manager->process(MESI_CM_UNBLOCK_I, SOURCE_ID0);
        CPPUNIT_ASSERT(MESI_MNG_SI_EVICT == manager->state);
        CPPUNIT_ASSERT(1 == manager->num_invalidations);
        manager->process(MESI_CM_UNBLOCK_I, SOURCE_ID1);
        CPPUNIT_ASSERT(MESI_MNG_SI_EVICT == manager->state);
        CPPUNIT_ASSERT(2 == manager->num_invalidations);
        manager->process(MESI_CM_UNBLOCK_I, SOURCE_ID2);
        CPPUNIT_ASSERT(MESI_MNG_I == manager->state);
        CPPUNIT_ASSERT(3 == manager->num_invalidations);
	CPPUNIT_ASSERT_EQUAL(true, ((My_MESI_manager*)manager)->get_invalidate_called());
	//no msg sent
	CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_manager*)manager)->get_messages().size());
    }

    void testSIEState()
    {
        int SOURCE_ID0 = random() % 1024;
        int SOURCE_ID1 = random() % 1024;
        int SOURCE_ID2 = random() % 1024;
        int OWNER_ID = random() % 1024;

        manager->transition_to_sie();
        manager->num_invalidations = 0;
        manager->num_invalidations_req = 3;
        manager->owner = OWNER_ID;
        // Simulate Another Sharer joining in
        manager->process(MESI_CM_UNBLOCK_I, SOURCE_ID0);
        CPPUNIT_ASSERT(MESI_MNG_SIE == manager->state);
        CPPUNIT_ASSERT(1 == manager->num_invalidations);
        manager->process(MESI_CM_UNBLOCK_I, SOURCE_ID1);
        CPPUNIT_ASSERT(MESI_MNG_SIE == manager->state);
        CPPUNIT_ASSERT(2 == manager->num_invalidations);
        manager->process(MESI_CM_UNBLOCK_I, SOURCE_ID2);
	//transit to IE when all MESI_CM_UNBLOCK_I are received
        CPPUNIT_ASSERT(3 == manager->num_invalidations);
        CPPUNIT_ASSERT(MESI_MNG_IE == manager->state);
	//msg to owner
	CPPUNIT_ASSERT(MESI_MC_GRANT_E_DATA == ((My_MESI_manager*)manager)->get_messages().back().msg);
	CPPUNIT_ASSERT(OWNER_ID == ((My_MESI_manager*)manager)->get_messages().back().destID);
    }


    //! Build a test suite.
    static CppUnit::Test* suite()
    {
        CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MESI manager");

        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("testConstructor", &MESI_managerTest::testConstructor));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test I State", &MESI_managerTest::testIState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test E State", &MESI_managerTest::testEState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test IE State", &MESI_managerTest::testIEState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test EE State", &MESI_managerTest::testEEState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test EI PUT State", &MESI_managerTest::testEIPUTState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test EI EVICT State", &MESI_managerTest::testEIEVICTState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test ES State", &MESI_managerTest::testESState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test S State", &MESI_managerTest::testSState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test SS State", &MESI_managerTest::testSSState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test SI EVICT State", &MESI_managerTest::testSIEVICTState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_managerTest>("test SIE State", &MESI_managerTest::testSIEState));
	/*
	*/
        return mySuite;
    }
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( MESI_managerTest::suite() );
    if(runner.run("", false))
        return 0; //all is well
    else
        return 1;

}
