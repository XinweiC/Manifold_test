#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>
#include "../../coherence/MESI_client.h"
//#include "MESI_req.h"
#include <iostream>
#include <set>
#include <stdlib.h>

using namespace std;
using namespace manifold::mcp_cache_namespace;

//####################################################################
// helper classes
//####################################################################


struct Message {
    bool isreq;
    MESI_messages_t msg;
    int destID;
};

class My_MESI_client : public MESI_client {
public:
    My_MESI_client(int id) : MESI_client(id) {}

    virtual void sendmsg(bool req, MESI_messages_t msg, int destID=-1)
    {
        Message m;
	m.isreq = req;
	m.msg = msg;
	m.destID = destID;
	m_messages.push_back(m);
    }
    virtual void invalidate()
    {
    }

    vector<Message>& get_messages() { return m_messages; }
    void clear_messages() { m_messages.clear(); }

private:
    vector<Message> m_messages;
};

//####################################################################
//! Class MESI_clientTest is the unit test class for class MESI_client.
//####################################################################
class MESI_clientTest : public CppUnit::TestFixture
{
private:
    MESI_client* client;

public:
    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
        client = new My_MESI_client(0);
    }
    //! Finialization function. Inherited from the CPPUnit framework.
    void tearDown()
    {
        delete client;
    }


    //======================================================================
    //======================================================================
    //! Test constructor
    void testConstructor()
    {
        CPPUNIT_ASSERT(MESI_C_I == client->state);
    }


    void testIState()
    {
        client->transition_to_i();
        // Simulate READ Request
        client->process(GET_S, random() % 1024); //foward id is not used, so can be anything.
        CPPUNIT_ASSERT(MESI_C_IE == client->state);
        CPPUNIT_ASSERT(MESI_CM_I_to_S == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(true, ((My_MESI_client*)client)->get_messages().back().isreq);

        client->transition_to_i();
        // Simulate WRITE Request
        client->process(GET_E, random()%1024);
        CPPUNIT_ASSERT(MESI_C_IE == client->state);
        CPPUNIT_ASSERT(MESI_CM_I_to_E == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(true, ((My_MESI_client*)client)->get_messages().back().isreq);

    }

    void testEState()
    {
        client->transition_to_e();
        // Simulate READ Request
        client->process(GET_S, random() % 1024);
        CPPUNIT_ASSERT(MESI_C_E == client->state);
        CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_client*)client)->get_messages().size()); //no msg sent


        client->transition_to_e();
        // Simulate WRITE Request
        client->process(GET_E, random()%1024);
        CPPUNIT_ASSERT(MESI_C_M == client->state);
        CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_client*)client)->get_messages().size()); //no msg sent


        client->transition_to_e();
        // Simulate EVICT Request
        client->process(GET_EVICT, random()%1024);
        CPPUNIT_ASSERT(MESI_C_EI == client->state);
        CPPUNIT_ASSERT(MESI_CM_E_to_I == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(true, ((My_MESI_client*)client)->get_messages().back().isreq);


        client->transition_to_e();
        // Simulate DEMAND_I Request
        client->process(MESI_MC_DEMAND_I, -1);
        CPPUNIT_ASSERT(MESI_C_I == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_I == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);


        client->transition_to_e();
        // Simulate FWD Request
	int FWDID = random() % 1024;
        client->process(MESI_MC_FWD_E, FWDID);
        CPPUNIT_ASSERT(MESI_C_I == client->state);
        CPPUNIT_ASSERT(MESI_CC_E_DATA == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT(FWDID == ((My_MESI_client*)client)->get_messages().back().destID);
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);


        ((My_MESI_client*)client)->clear_messages();

        client->transition_to_e();
        // Simulate FWD Request
	FWDID = random() % 1024;
        client->process(MESI_MC_FWD_S, FWDID);
        CPPUNIT_ASSERT(MESI_C_S == client->state);
        CPPUNIT_ASSERT(MESI_CC_S_DATA == ((My_MESI_client*)client)->get_messages()[0].msg);
        CPPUNIT_ASSERT(FWDID == ((My_MESI_client*)client)->get_messages()[0].destID);
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages()[0].isreq);
        CPPUNIT_ASSERT(MESI_CM_CLEAN == ((My_MESI_client*)client)->get_messages()[1].msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages()[1].destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages()[1].isreq);
    }

    void testSState()
    {
        // No request is sent silent
        client->transition_to_s();
        // Simulate READ Request
        client->process(GET_S, random()%1024);
        CPPUNIT_ASSERT(MESI_C_S == client->state);
        CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_client*)client)->get_messages().size()); //no msg sent


        client->transition_to_s();
        // Simulate WRITE Request
        client->process(GET_E, random()%1024);
        CPPUNIT_ASSERT(MESI_C_SE == client->state);
        CPPUNIT_ASSERT(MESI_CM_I_to_E == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(true, ((My_MESI_client*)client)->get_messages().back().isreq);


        ((My_MESI_client*)client)->clear_messages();

        // Silent
        client->transition_to_s();
        // Simulate EVICTION
        client->process(GET_EVICT, random()%1024);
        CPPUNIT_ASSERT(MESI_C_I == client->state);
        CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_client*)client)->get_messages().size()); //no msg sent


        client->transition_to_s();
        // Simulate DEMAND_I Request
        client->process(MESI_MC_DEMAND_I, random()%1024);
        CPPUNIT_ASSERT(MESI_C_I == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_I == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);
    }

#if 0
    void testSIEState()
    {
        // No request is sent silent
        client->transition_to_sie();
        // Simulate READ Request
        client->process(MESI_MC_DEMAND_I, random()%1024);
        CPPUNIT_ASSERT(MESI_C_IE == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_I == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);
    }
#endif


    void testMState()
    {
        client->transition_to_m();
        // Simulate READ Request
        client->process(GET_S, random()%1024);
        CPPUNIT_ASSERT(MESI_C_M == client->state);
        CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_client*)client)->get_messages().size()); //no msg sent


        ((My_MESI_client*)client)->clear_messages();

        client->transition_to_m();
        // Simulate WRITE Request
        client->process(GET_E, -1);
        CPPUNIT_ASSERT(MESI_C_M == client->state);
        CPPUNIT_ASSERT_EQUAL(0, (int)((My_MESI_client*)client)->get_messages().size()); //no msg sent


        client->transition_to_m();
        // Simulate EVICT Request
        client->process(GET_EVICT, random());
        CPPUNIT_ASSERT(MESI_C_MI == client->state);
        CPPUNIT_ASSERT(MESI_CM_M_to_I == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(true, ((My_MESI_client*)client)->get_messages().back().isreq);


        client->transition_to_m();
        // Simulate DEMAND_I Request
        client->process(MESI_MC_DEMAND_I, random());
        CPPUNIT_ASSERT(MESI_C_I == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_I_DIRTY == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);


        client->transition_to_m();
        // Simulate FWD Request
	int FWDID = random() % 1024;
        client->process(MESI_MC_FWD_E, FWDID);
        CPPUNIT_ASSERT(MESI_C_I == client->state);
        CPPUNIT_ASSERT(MESI_CC_M_DATA == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT(FWDID == ((My_MESI_client*)client)->get_messages().back().destID);
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);


        ((My_MESI_client*)client)->clear_messages();

        client->transition_to_m();
        // Simulate FWD Request
	FWDID = random() % 1024;
        client->process(MESI_MC_FWD_S, FWDID);
        CPPUNIT_ASSERT(MESI_C_S == client->state);
        CPPUNIT_ASSERT(MESI_CC_S_DATA == ((My_MESI_client*)client)->get_messages()[0].msg);
        CPPUNIT_ASSERT(FWDID == ((My_MESI_client*)client)->get_messages()[0].destID);
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages()[0].isreq);
        CPPUNIT_ASSERT(MESI_CM_WRITEBACK == ((My_MESI_client*)client)->get_messages()[1].msg);
        CPPUNIT_ASSERT(-1 == ((My_MESI_client*)client)->get_messages()[1].destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages()[1].isreq);

    }

    void testIEState()
    {
        client->transition_to_ie();
        // Simulate CC_E_DATA Request
        client->process(MESI_CC_E_DATA, random()%1024);
        CPPUNIT_ASSERT(MESI_C_E == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_E == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);

        client->transition_to_ie();
        // Simulate CC_S_DATA Request
        client->process(MESI_CC_S_DATA, random()%1024);
        CPPUNIT_ASSERT(MESI_C_S == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_S == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);


        client->transition_to_ie();
        // Simulate CC_M_DATA Request
        client->process(MESI_CC_M_DATA, random()%1024);
        CPPUNIT_ASSERT(MESI_C_M == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_E == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);


        client->transition_to_ie();
        // Simulate GRANT E DATA Request
        client->process(MESI_MC_GRANT_E_DATA, random());
        CPPUNIT_ASSERT(MESI_C_E == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_E == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);

        client->transition_to_ie();
        // Simulate GRANT S DATA Request
        client->process(MESI_MC_GRANT_S_DATA, random());
        CPPUNIT_ASSERT(MESI_C_S == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_S == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);

    }

    void testEIState()
    {
        client->transition_to_ei();
        // Simulate GRANT I Request
        client->process(MESI_MC_GRANT_I, random());
        CPPUNIT_ASSERT(MESI_C_I == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_I == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);
    }

    void testMIState()
    {
        client->transition_to_mi();
        // Simulate GRANT I Request
        client->process(MESI_MC_GRANT_I, random());
        CPPUNIT_ASSERT(MESI_C_I == client->state);
        CPPUNIT_ASSERT_EQUAL((int)MESI_CM_UNBLOCK_I, (int) ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);
    }


    void testSEState()
    {
        client->transition_to_se();
        // Simulate GRANT_E_DATA Request
        client->process(MESI_MC_GRANT_E_DATA, random());
        CPPUNIT_ASSERT(MESI_C_E == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_E == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);

        client->transition_to_se();
        // Simulate DEMAND_I Request
        client->process(MESI_MC_DEMAND_I, random());
        CPPUNIT_ASSERT(MESI_C_IE == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_I == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);


        client->transition_to_se();
        // Simulate CC_M_DATA Request
        client->process(MESI_CC_M_DATA, random()%1024);
        CPPUNIT_ASSERT(MESI_C_M == client->state);
        CPPUNIT_ASSERT(MESI_CM_UNBLOCK_E == ((My_MESI_client*)client)->get_messages().back().msg);
        CPPUNIT_ASSERT_EQUAL(-1, ((My_MESI_client*)client)->get_messages().back().destID); //-1 represents manager
        CPPUNIT_ASSERT_EQUAL(false, ((My_MESI_client*)client)->get_messages().back().isreq);

    }

    //! Build a test suite.
    static CppUnit::Test* suite()
    {
        CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("MESI Client");

        mySuite->addTest(new CppUnit::TestCaller<MESI_clientTest>("testConstructor", &MESI_clientTest::testConstructor));
        mySuite->addTest(new CppUnit::TestCaller<MESI_clientTest>("test I State", &MESI_clientTest::testIState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_clientTest>("test E State", &MESI_clientTest::testEState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_clientTest>("test S State", &MESI_clientTest::testSState));
        //mySuite->addTest(new CppUnit::TestCaller<MESI_clientTest>("test SIE State", &MESI_clientTest::testSIEState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_clientTest>("test M State", &MESI_clientTest::testMState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_clientTest>("test IE State", &MESI_clientTest::testIEState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_clientTest>("test EI State", &MESI_clientTest::testEIState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_clientTest>("test MI State", &MESI_clientTest::testMIState));
        mySuite->addTest(new CppUnit::TestCaller<MESI_clientTest>("test SE State", &MESI_clientTest::testSEState));
        return mySuite;
    }
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( MESI_clientTest::suite() );
    if(runner.run("", false))
        return 0; //all is well
    else
        return 1;

}


