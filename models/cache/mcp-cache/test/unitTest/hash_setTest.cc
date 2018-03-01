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
//! Class hash_setTest is the unit test class for class hash_set. 
//####################################################################
class hash_setTest : public CppUnit::TestFixture {
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
	//! Test constructor
	void test_constructor_0()
	{
            int assocs[] = {4, 8, 16};
	    for(int i=0; i<sizeof(assocs)/sizeof(assocs[0]); i++) {
		const unsigned IDX = random() % 1024;
		hash_set* myset = new hash_set(m_table, assocs[i], IDX);
		CPPUNIT_ASSERT(m_table == myset->my_table);
		CPPUNIT_ASSERT_EQUAL(assocs[i], myset->assoc);
		CPPUNIT_ASSERT_EQUAL(assocs[i], (int)myset->set_entries.size());

                //check all hash entries
		int a=0;
		for(list<hash_entry*>::iterator it = myset->set_entries.begin(); it != myset->set_entries.end();
		      ++it) {
		    hash_entry* entry = *it;
		    CPPUNIT_ASSERT(myset == entry->my_set);
		    //hash_set constructor uses push_front() to insert the entries, so the 1st entry has the highest idx.
		    CPPUNIT_ASSERT_EQUAL(IDX*assocs[i] + assocs[i]-1-a, entry->get_idx());
		    a++;
		    CPPUNIT_ASSERT_EQUAL(true, entry->free);
		    CPPUNIT_ASSERT_EQUAL(false, entry->dirty);
		    CPPUNIT_ASSERT_EQUAL(false, entry->have_data);
		}
                delete myset;
	    }
            
	}



        //======================================================================
        //======================================================================
        //! @brief Create a new set; generate a random tag; get_entry() returns NULL.
	void test_get_entry_0()
	{
	    hash_set* myset = new hash_set(m_table, 4, random()%1024);

	    paddr_t tag;
	    while((tag = random()) == 0); //randomly generate a non-0 tag

	    CPPUNIT_ASSERT(0 == myset->get_entry(tag));
	    delete myset;
	}


        //======================================================================
        //======================================================================
        //! @brief Test get_entry(): success.
	//!
	//! Create a new set; generate a random tag; get_entry() returns NULL;
	//! Randomly pick an entry and set its tag to the random tag; do get_entry()
	//! again and it should return the manipulated entry.
	void test_get_entry_1()
	{
	    const int ASSOC = random() % 100 + 1;

	    hash_set* myset = new hash_set(m_table, ASSOC, random()%1024);

	    paddr_t tag;
	    while((tag = random()) == 0); //randomly generate a non-0 tag

	    CPPUNIT_ASSERT(0 == myset->get_entry(tag));

	    //randomly set one entry's tag to tag
	    int idx = random() % ASSOC;

	    list<hash_entry*>::iterator it = myset->set_entries.begin();
	    for(int i=0; i<idx; i++) {
	        ++it;
	    }
	    (*it)->tag = tag;
	    (*it)->free = false;

	    CPPUNIT_ASSERT((*it) == myset->get_entry(tag));
	    delete myset;

	}




        //======================================================================
        //======================================================================
        //! @brief test replace_entry()
	//!
	//! Create a new set; randomly set one entry to the tag to be replaced.
	//! Call replace_entry() and verify the entry is replaced, and the new
	//! entry is at the front of the entry list.
	void test_replace_entry_0()
	{
	    const int ASSOC = random() % 100 + 1;
	    hash_set* myset = new hash_set(m_table, ASSOC, random()%1024);


	    //randomly set one entry's tag to old_tag
	    paddr_t old_tag = random();
	    int idx = random() % ASSOC;

	    list<hash_entry*>::iterator it = myset->set_entries.begin();
	    for(int i=0; i<idx; i++) {
	        ++it;
	    }
	    hash_entry* outgoing = (*it);
	    outgoing->tag = old_tag;
	    outgoing->free = false;


	    paddr_t new_tag = random();
            hash_entry* new_entry = new hash_entry(myset, 0);
	    new_entry->tag = new_tag;

	    //call replace_entry()
	    myset->replace_entry(new_entry, outgoing);

	    //verify outgoing is gone
	    for(it = myset->set_entries.begin(); it != myset->set_entries.end(); ++it) {
	        CPPUNIT_ASSERT(outgoing != (*it));
	    }

	    //verify new entry is placed at the front of the list
	    CPPUNIT_ASSERT(outgoing != myset->set_entries.front());

	    delete myset;
	}





        //======================================================================
        //======================================================================
	//! @brief Test update_lru()
	//!
        //! Create a new set; randomly generate ASSOC different tags and set the
	//! entries to the tags. Randomly pick one entry and call update_lru();
	//! verify the entry is moved to the fron of the list.
	void testUpdate_lru_0()
	{
	    const int ASSOC = random() % 100 + 1;
	    hash_set* myset = new hash_set(m_table, ASSOC, random()%1024);

            //generate assoc different tags
	    paddr_t tags[ASSOC];
	    set<paddr_t> tag_set;
	    for(int i=0; i<ASSOC; i++) {
		paddr_t tag;
		pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						       //value is already in the set
		do { //make sure the tag in not in the set
		    tag = random();
		    ret = tag_set.insert(tag);
		} while(ret.second == false);

		tags[i] = tag;
	    }
	    tag_set.clear();


            hash_entry* entries[ASSOC];

	    //set the tags
	    list<hash_entry*>::iterator it = myset->set_entries.begin();
	    for(int i=0; i<ASSOC; i++) {
	        entries[i] = (*it);
	        (*it)->tag = tags[i];
		++it;
	    }

	    hash_entry* mru_entry = entries[random() % ASSOC];

	    //call update_lru()
	    myset->update_lru(mru_entry);

	    //verify mru entry is moved to the front
	    CPPUNIT_ASSERT(mru_entry == myset->set_entries.front());

	    //verify mru entry is only in the front
	    it = myset->set_entries.begin();
	    ++it;
	    for(; it != myset->set_entries.end(); ++it) {
	        CPPUNIT_ASSERT(mru_entry != (*it));
	    }



	    delete myset;
	}





	//! Build a test suite.
	static CppUnit::Test* suite()
	{
	    CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("hash_setTest");

	    mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_constructor_0", &hash_setTest::test_constructor_0));
	    mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_get_entry_0", &hash_setTest::test_get_entry_0));
	    mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_get_entry_1", &hash_setTest::test_get_entry_1));
	    mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_replace_entry_0", &hash_setTest::test_replace_entry_0));
	    mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("testUpdate_lru_0", &hash_setTest::testUpdate_lru_0));
	    /*
	    */
	    return mySuite;
	}
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( hash_setTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


