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
using namespace manifold::simple_cache;

//####################################################################
// helper classes
//####################################################################




//####################################################################
//! Class hash_setTest is the unit test class for class hash_set. 
//####################################################################
class hash_setTest : public CppUnit::TestFixture {
private:
    hash_table* m_table;
public:
    void mySetUp()
    {
	int size = 0x1 << (random() % 10 + 13); //8K to 4M
	int assoc = 0x1 << (random() % 5); //1, 2,4,8,16
	int block_size = 0x1 << (random() % 2 + 5); //32, 64
	int hit_time = 1;
	int lookup_time = 2;
	m_table = new hash_table(size, assoc, block_size, hit_time, lookup_time);
    }

    void mySetUp2(int assoc)
    {
	int size = 0x1 << (random() % 10 + 13); //8K to 4M
	int block_size = 0x1 << (random() % 2 + 5); //32, 64
	int hit_time = 1;
	int lookup_time = 2;

	m_table = new hash_table(size, assoc, block_size, hit_time, lookup_time);
    }


    //======================================================================
    //======================================================================
    //! Test constructor
    void testConstructor_0()
    {
	int assocs[] = {1, 2, 4, 8, 16};
	for(int i=0; i<sizeof(assocs)/sizeof(assocs[0]); i++) {
	    mySetUp2(assocs[i]);

	    hash_set* myset = new hash_set(m_table);
	    CPPUNIT_ASSERT(m_table == myset->my_table);
	    CPPUNIT_ASSERT_EQUAL(assocs[i], (int)myset->set_entries.size());

	    //check all hash entries
	    for(list<hash_entry*>::iterator it = myset->set_entries.begin(); it != myset->set_entries.end();
		      ++it) {
		hash_entry* entry = *it;
		CPPUNIT_ASSERT_EQUAL(false, entry->valid);
		CPPUNIT_ASSERT_EQUAL(false, entry->dirty);
	    }
	    delete myset;
	    delete m_table;
	}
	
    }



    //======================================================================
    //======================================================================
    //! @brief test get_entry(): non-exiting tag
    //!
    //! Create a new set; generate a random tag; get_entry() returns NULL.
    void test_get_entry_0()
    {
	mySetUp();
	hash_set* myset = new hash_set(m_table);

	paddr_t tag;
	while((tag = random()) == 0); //randomly generate a non-0 tag

	CPPUNIT_ASSERT(0 == myset->get_entry(tag));
	delete myset;
	delete m_table;
    }


    //======================================================================
    //======================================================================
    //! @brief Test get_entry(): put a tag in the set; get_entry() returns an entry.
    //!
    //! @brief Create a new set; generate a random tag; get_entry() returns NULL;
    //! Use replace_entry() to put the tag in the set; get_entry() returns
    //! an entry.
    void test_get_entry_1()
    {
	mySetUp();
	hash_set* myset = new hash_set(m_table);

	paddr_t addr;
	while((addr = random()) == 0); //randomly generate a non-0 tag

	CPPUNIT_ASSERT(0 == myset->get_entry(addr));

	myset->replace_entry(m_table->get_tag(addr));

	hash_entry* e = myset->get_entry(m_table->get_tag(addr));

	CPPUNIT_ASSERT(e != 0);
	CPPUNIT_ASSERT_EQUAL(m_table->get_tag(addr), e->tag);

	delete myset;
	delete m_table;
    }




    //======================================================================
    //======================================================================
    //! @brief Test replace_entry()
    //!
    //! @brief Create a new set; randomly generate ASSOC different tags, T[0..ASSOC-1].
    //! Use replace_entry() to put T[0], T[1], ..., T[ASSOC-1] in the set.
    //! After this the next one to be replaced would be T[0] should a miss happens.
    void test_replace_entry_0()
    {
	int assoc = 4;

	mySetUp2(assoc);

	hash_set* myset = new hash_set(m_table);

	paddr_t tags[assoc];

	//generate assoc different tags
	for(int i=0; i<assoc; i++) {
	    paddr_t tag;
	    bool ok=false;
	    while(!ok) { //make sure tag is new
		ok = true;
		tag = m_table->get_tag(random());
		for(int j=0; j<i; j++) {
		    if(tags[j] == tag) {
			ok = false;
			break;
		    }
		}
	    }
	    tags[i] = tag;
	}


	for(int i=0; i<assoc; i++) {
	    myset->replace_entry(tags[i]);
	}

	CPPUNIT_ASSERT_EQUAL(tags[0], myset->set_entries.back()->tag);

	delete myset;
	delete m_table;
    }




    //======================================================================
    //======================================================================
    //! @brief test replace_entry()
    //!
    //! Create a new set; randomly generate N different tags, T[0..N-1].
    //! Use replace_entry() to put T[0], T[1], ..., T[N-1] in the set.
    //! At the end the last ASSOC tags should be in the set.
    void test_replace_entry_1()
    {
	int assoc = 4;

	mySetUp2(assoc);

	hash_set* myset = new hash_set(m_table);

	int Ntags = assoc * 100;
	paddr_t tags[Ntags];

	//generate assoc different tags
	set<paddr_t> tag_set;
	for(int i=0; i<Ntags; i++) {
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


	for(int i=0; i<Ntags; i++) {
	    myset->replace_entry(tags[i]);
	}

	int i=0;
	for(std::list<hash_entry*>::iterator it=myset->set_entries.begin();
	                              it != myset->set_entries.end(); ++it, ++i) {
	    CPPUNIT_ASSERT_EQUAL(tags[Ntags-1-i], (*it)->tag);
	}


	delete myset;
	delete m_table;
    }





    //======================================================================
    //======================================================================
    //! @brief test update_lru()
    //!
    //! Create a new set; randomly generate N different tags, T[0..N-1].
    //! Use replace_entry() to put T[0], T[1], ..., T[N-1] in the set.
    //! At the end the last ASSOC tags should be in the set. Randomly pick one
    //! to hit; verify it is moved to the front.
    void test_update_lru_0()
    {
	int assoc = 4;

	mySetUp2(assoc);

	hash_set* myset = new hash_set(m_table);

	int Ntags = assoc * 100;
	paddr_t tags[Ntags];

	//generate assoc different tags
	set<paddr_t> tag_set;
	for(int i=0; i<Ntags; i++) {
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


	for(int i=0; i<Ntags; i++) {
	    myset->replace_entry(tags[i]);
	}

	//now the last assoc tags from the tags[] array are in the set.
	int idx = random() % assoc; //randomly pick one

        hash_entry* e = myset->get_entry(tags[Ntags-assoc+idx]);
	CPPUNIT_ASSERT(e != 0);

	myset->update_lru(e);

	//verify the one we just picked is in the front
	CPPUNIT_ASSERT_EQUAL(tags[Ntags-assoc+idx], myset->set_entries.front()->tag);

	delete myset;
	delete m_table;
    }





    //======================================================================
    //======================================================================
    //! @brief test process_request
    //!
    //! Create a new set; randomly generate N > HT_ASSOC different tags, T[0..N-1].
    //! Use replace_entry() to put T[0], T[1], ..., T[N-2] in the set.
    //! At the end the last ASSOC tags should be in the set. T[N-1] would miss.
    void test_process_request_0()
    {
	const int assoc = 4;

	mySetUp2(assoc);

	hash_set* myset = new hash_set(m_table);

	int Ntags = assoc + 1;
	paddr_t tags[Ntags];

	//generate Ntags different tags
	set<paddr_t> tag_set;
	for(int i=0; i<Ntags; i++) {
	    paddr_t tag;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						   //value is already in the set
	    do { //make sure the tag in not in the set
		tag = m_table->get_tag(random()); //for process_request, the tag must be created with the table
		ret = tag_set.insert(tag);
	    } while(ret.second == false);

	    tags[i] = tag;
	}
	tag_set.clear();


	//put all but the last one in the set
	for(int i=0; i<Ntags-1; i++) {
	    myset->replace_entry(tags[i]);
	}

	//verify process_request() returns false
	CPPUNIT_ASSERT(false == myset->process_request(tags[Ntags-1]));

	delete myset;
	delete m_table;
    }





    //======================================================================
    //======================================================================
    //! @brief test process_request
    //!
    //! Create a new set; randomly generate N > ASSOC different tags, T[0..N-1].
    //! Use replace_entry() to put T[0], T[1], ..., T[N-1] in the set.
    //! At the end the last ASSOC tags should be in the set. Randomly pick one
    //! to hit; verify it is moved to the front.
    void test_process_request_1()
    {
	const int assoc = 4;

	mySetUp2(assoc);

	hash_set* myset = new hash_set(m_table);

	int Ntags = assoc + (random() % 32 + 1);
	CPPUNIT_ASSERT(Ntags > assoc);
	paddr_t tags[Ntags];

	//generate assoc different tags
	set<paddr_t> tag_set;
	for(int i=0; i<Ntags; i++) {
	    paddr_t tag;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						   //value is already in the set
	    do { //make sure the tag in not in the set
		tag = m_table->get_tag(random()); //for process_request, the tag must be created with the table
		ret = tag_set.insert(tag);
	    } while(ret.second == false);

	    tags[i] = tag;
	}
	tag_set.clear();


	//put all in the set; the last assoc stay in the set
	for(int i=0; i<Ntags; i++) {
	    myset->replace_entry(tags[i]);
	}

	//now the last assoc tags from the tags[] array are in the set.
	int idx = random() % assoc; //randomly pick one

	//verify process_request() returns true
	CPPUNIT_ASSERT(true == myset->process_request(tags[Ntags-assoc+idx]));

	//in addition, the one we just picked is in the front
	CPPUNIT_ASSERT_EQUAL(tags[Ntags-assoc+idx], myset->set_entries.front()->tag);

	delete myset;
	delete m_table;
    }




    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("hash_setTest");

	mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("testConstructor_0", &hash_setTest::testConstructor_0));
	mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_get_entry_0", &hash_setTest::test_get_entry_0));
	mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_get_entry_1", &hash_setTest::test_get_entry_1));
	mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_replace_entry_0", &hash_setTest::test_replace_entry_0));
	mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_replace_entry_1", &hash_setTest::test_replace_entry_1));
	mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_update_lru_0", &hash_setTest::test_update_lru_0));
	mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_process_request_0", &hash_setTest::test_process_request_0));
	mySuite->addTest(new CppUnit::TestCaller<hash_setTest>("test_process_request_1", &hash_setTest::test_process_request_1));
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


