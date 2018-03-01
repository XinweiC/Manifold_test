#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <set>
#include <stdlib.h>
#include "hash_table.h"

//using namespace manifold::kernel;
using namespace std;
using namespace manifold::mcp_cache_namespace;

//####################################################################
// helper classes
//####################################################################




//####################################################################
//! Class hash_entryTest is the unit test class for class hash_entry. 
//####################################################################
class hash_entryTest : public CppUnit::TestFixture {
    private:
	static const int HT_SIZE = 0x1 << 14; //2^14 = 16k;
	static const int HT_ASSOC = 4;
	static const int HT_BLOCK_SIZE = 32;
        hash_table* m_table;
    /*
*/
    public:
	//! Initialization function. Inherited from the CPPUnit framework.
        void setUp()
	{
	    cache_settings settings;
	    settings.name = "testCache";
	    //settings.type = CACHE_DATA;
	    settings.size = HT_SIZE;
	    settings.assoc = HT_ASSOC;
	    settings.block_size = HT_BLOCK_SIZE;
	    settings.hit_time = 1;
	    settings.lookup_time = 2;
	    settings.replacement_policy = RP_LRU;

	    m_table = new hash_table(settings);
	}
	//! Finialization function. Inherited from the CPPUnit framework.
        void tearDown()
	{
	    //delete m_table;
	}


        //======================================================================
        //======================================================================
	//! @brief Test constructor
	//!
	//! 
	void testConstructor_0()
	{
	    int assoc = random() % 32 + 1;
	    hash_set* myset = new hash_set(m_table, assoc, 0);

	    //create the hash entry
	    const unsigned IDX = random() % 4096;
	    hash_entry* myentry = new hash_entry(myset, IDX);

	    //CPPUNIT_ASSERT(m_table == myentry->my_table);
	    CPPUNIT_ASSERT(myset == myentry->my_set);
	    CPPUNIT_ASSERT_EQUAL(IDX,  myentry->get_idx());
	    CPPUNIT_ASSERT_EQUAL(true,  myentry->free);
	    CPPUNIT_ASSERT_EQUAL(false,  myentry->dirty);
	    CPPUNIT_ASSERT_EQUAL(false,  myentry->have_data);
	}







	//! Build a test suite.
	static CppUnit::Test* suite()
	{
	    CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("hash_entryTest");

	    mySuite->addTest(new CppUnit::TestCaller<hash_entryTest>("testConstructor_0", &hash_entryTest::testConstructor_0));
	    return mySuite;
	}
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( hash_entryTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


