#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include "coherence/sharers.h"

//using namespace manifold::kernel;
using namespace std;
using namespace manifold::mcp_cache_namespace;

//####################################################################
// helper classes
//####################################################################




//####################################################################
//! Class sharersTest is the unit test class for class sharers. 
//####################################################################
class sharersTest : public CppUnit::TestFixture {
    private:
    public:
	//! Initialization function. Inherited from the CPPUnit framework.
        void setUp()
	{
	}
	//! Finialization function. Inherited from the CPPUnit framework.
        void tearDown()
	{
	    //delete m_table;
	}


        //======================================================================
        //======================================================================
	//! @brief Test set()
	//! 
	void test_set_0()
	{
	    sharers theSharers;

	    const int NUM = random() % 80 + 20;  //20 to 100 times

	    for(int i=0; i<NUM; i++) {
	        int ind = random() % 1024;
		theSharers.set(ind);
		CPPUNIT_ASSERT(ind < theSharers.data.size());
		CPPUNIT_ASSERT_EQUAL(true, (bool)theSharers.data[ind]);
	    }

	}



        //======================================================================
        //======================================================================
	//! @brief Test reset()
	//! 
	void test_reset_0()
	{
	    sharers theSharers;

	    const int NUM = random() % 80 + 20;  //20 to 100 times

	    for(int i=0; i<NUM; i++) {
	        int ind = random() % 1024;
		theSharers.reset(ind);
		CPPUNIT_ASSERT(ind < theSharers.data.size());
		CPPUNIT_ASSERT_EQUAL(false, (bool)theSharers.data[ind]);
	    }

	}



        //======================================================================
        //======================================================================
	//! @brief Test get()
	//! 
	void test_get_0()
	{
	    sharers theSharers;

	    const int NUM = random() % 80 + 20;  //20 to 100 times

	    for(int i=0; i<NUM; i++) {
	        int ind = random() % 1024;
		theSharers.set(ind);
		CPPUNIT_ASSERT(ind < theSharers.data.size());
		CPPUNIT_ASSERT_EQUAL(true, theSharers.get(ind));
	    }

	    for(int i=0; i<NUM; i++) {
	        int ind = random() % 1024;
		theSharers.reset(ind);
		CPPUNIT_ASSERT(ind < theSharers.data.size());
		CPPUNIT_ASSERT_EQUAL(false, theSharers.get(ind));
	    }

	}




        //======================================================================
        //======================================================================
	//! @brief Test reset()
	//! 
	void test_ones_0()
	{
	    sharers theSharers;

	    const int NUM = random() % 80 + 20;  //20 to 100 times
	    int numbers[NUM];

	    for(int i=0; i<NUM; i++) {
	        numbers[i] = random() % 1024;
		theSharers.set(numbers[i]);
	    }

	    vector<int> ones;
	    theSharers.ones(ones);

	    //verify all the numbers that were set above are in the vector.
	    for(int i=0; i<NUM; i++) {
	        CPPUNIT_ASSERT(find(ones.begin(), ones.end(), numbers[i]) != ones.end());
	    }


	}




	//! Build a test suite.
	static CppUnit::Test* suite()
	{
	    CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("sharersTest");

	    mySuite->addTest(new CppUnit::TestCaller<sharersTest>("test_set_0", &sharersTest::test_set_0));
	    mySuite->addTest(new CppUnit::TestCaller<sharersTest>("test_reset_0", &sharersTest::test_reset_0));
	    mySuite->addTest(new CppUnit::TestCaller<sharersTest>("test_get_0", &sharersTest::test_get_0));
	    mySuite->addTest(new CppUnit::TestCaller<sharersTest>("test_ones_0", &sharersTest::test_ones_0));
	    return mySuite;
	}
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( sharersTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


