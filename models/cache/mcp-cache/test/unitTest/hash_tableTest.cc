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
//! Class hash_tableTest is the unit test class for class hash_table. 
//####################################################################
class hash_tableTest : public CppUnit::TestFixture {
    private:
	static const int HT_SIZE = 0x1 << 14; //2^14 = 16k;
	static const int HT_ASSOC = 4;
	static const int HT_BLOCK_SIZE = 32;
	hash_table* m_table;

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
	    delete m_table;
	}


        //======================================================================
        //======================================================================
	//! @brief Test constructor
	//!
	//! Verify configuration parameters.
	void test_constructor_0()
	{
	    CPPUNIT_ASSERT(m_table->size == HT_SIZE);
	    CPPUNIT_ASSERT(m_table->assoc == HT_ASSOC);
	    CPPUNIT_ASSERT(m_table->block_size == HT_BLOCK_SIZE);
	}




        //======================================================================
        //======================================================================
	//! @brief Test constructor
	//!
	//! Test various sizes and make sure the number of sets and masks are set correctly.
	void test_constructor_1()
	{
	    cache_settings settings;
	    settings.name = "testCache";
	    //settings.type = CACHE_DATA;
	    settings.hit_time = 1;
	    settings.lookup_time = 2;
	    settings.replacement_policy = RP_LRU;

	    for(unsigned size=(0x1 << 10); size <= (0x1 << 30); size <<= 1) {
	        for(unsigned assoc=1; assoc <= 16; assoc <<= 1) {
		    for(unsigned block=2; block <=128; block <<= 1) {
		        if(assoc * block > size) // number of sets < 1; doesn't make sense
			    break;
		        if(size/assoc/block > 4096) //sets > 4096; takes too long for constructor
			    break;
			settings.size = size;
			settings.assoc = assoc;
			settings.block_size = block;

			hash_table myTable(settings);
			//cout << "size= " << size << " ass= " << assoc << " block= " << block << endl;
			// size  assoc  block   sets      index_mask                    tag_mask
			// 2^10    1      2     512=2^9   9 bits + one 0(for offset)     54bits + 10 0's
			// 2^10    1      4     256=2^8   8 bits + 2 0's(for offset)     54bits + 10 0's
			// 2^10    2      2     256=2^8   8 bits + one 0(for offset)     55bits +  9 0's
			// 2^10    2      4     128=2^7   7 bits + 2 0's(for offset)     55bits +  9 0's
			CPPUNIT_ASSERT_EQUAL((paddr_t)((size/assoc-1) ^ (block-1)), myTable.index_mask);
			paddr_t tagmask = ~0x0;
			CPPUNIT_ASSERT_EQUAL((tagmask ^ (size/assoc-1)), myTable.tag_mask);
		    }
		}
	    }//for(size
	}


        //======================================================================
        //======================================================================
	//! @brief Test constructor
	// ????????????????????? need a test case where assoc is not power of 2


        //======================================================================
        //======================================================================
	//! @brief Test constructor
	//!
	//! Test various sizes and make sure the index of the entries are set to 0 to
	//! sets*assoc-1, consecutively. Here we test with assocs that are not power of 2.
	void test_constructor_2()
	{
	    cache_settings settings;
	    settings.name = "testCache";
	    //settings.type = CACHE_DATA;

	    for(unsigned size=(0x1 << 10); size <= (0x1 << 30); size <<= 1) {
	        for(unsigned assoc=1; assoc <= 16; assoc++) {
		    for(unsigned block=2; block <=128; block <<= 1) {
		        if(assoc * block > size) // number of sets < 1; doesn't make sense
			    break;
		        if(size/assoc/block > 4096) //sets > 4096; takes too long for constructor
			    break;
			settings.size = size;
			settings.assoc = assoc;
			settings.block_size = block;

			hash_table myTable(settings);

			//Verify the entries' index is set to 0 to sets*assoc-1, consecutively.
			for(int s =0; s<size/assoc/block; s++) {
			    int e=0;
			    for(list<hash_entry*>::iterator it = myTable.my_sets[s]->set_entries.begin(); it != myTable.my_sets[s]->set_entries.end();
				  ++it) {
				hash_entry* entry = *it;
				CPPUNIT_ASSERT_EQUAL(unsigned(s*assoc + assoc-1-e), entry->get_idx());
				e++;
			    }
			}
		    }
		}
	    }//for(size
	}





	//! Build a test suite.
	static CppUnit::Test* suite()
	{
	    CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("hash_tableTest");

	    mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("test_constructor_0", &hash_tableTest::test_constructor_0));
	    mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("test_constructor_1", &hash_tableTest::test_constructor_1));
	    mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("test_constructor_2", &hash_tableTest::test_constructor_2));

	    return mySuite;
	}
};



int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( hash_tableTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


