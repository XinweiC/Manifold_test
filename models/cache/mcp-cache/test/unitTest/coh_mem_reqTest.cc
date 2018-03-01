#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <set>
#include <stdlib.h>
#include "../../coh_mem_req.h"

//using namespace manifold::kernel;
using namespace std;
using namespace manifold::mcp_cache_namespace;

//####################################################################
// helper classes
//####################################################################




//####################################################################
//####################################################################
class Coh_mem_msgTest : public CppUnit::TestFixture {
    private:
    public:
	//! Initialization function. Inherited from the CPPUnit framework.
        void setUp()
	{
	}
	//! Finialization function. Inherited from the CPPUnit framework.
        void tearDown()
	{
	}


        //======================================================================
        //======================================================================
	//! @brief Test constructor
	//!
	//! Test get_dst() gets the correct value.
	//!
	void testConstructor_0()
	{
	    const int N = 1000;

	    for(int i=0; i<N; i++) {
	        Mem_msg* req = new Mem_msg;
		int src = random() % 1024;
		req->src_id = src;

		CPPUNIT_ASSERT_EQUAL(src, req->get_src());
		delete req;
	    }

	}







	//! Build a test suite.
	static CppUnit::Test* suite()
	{
	    CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("Coh_mem_msgTest");

	    mySuite->addTest(new CppUnit::TestCaller<Coh_mem_msgTest>("testConstructor_0", &Coh_mem_msgTest::testConstructor_0));
	    return mySuite;
	}
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( Coh_mem_msgTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


