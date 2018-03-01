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
	int size = 0x1 << (random() % 10 + 13); //8K to 4M
	int assoc = 0x1 << (random() % 5); //1, 2,4,8,16
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
	if(m_table)
	    delete m_table;

	int size = 0x1 << (random() % 10 + 13); //8K to 4M
	int assoc = 0x1 << (random() % 5); //1, 2,4,8,16
	int block_size = 0x1 << (random() % 2 + 5); //32, 64
	int hit_time = 1;
	int lookup_time = 2;

	m_table = new hash_table(size, assoc, block_size, hit_time, lookup_time);

	CPPUNIT_ASSERT_EQUAL(size, m_table->size);
	CPPUNIT_ASSERT_EQUAL(assoc, m_table->assoc);
	CPPUNIT_ASSERT_EQUAL(block_size, m_table->block_size);

	delete m_table;
    }




    //======================================================================
    //======================================================================
    //! Test constructor
    //! Test various sizes and make sure the number of sets and masks are set correctly.
    void testConstructor_1()
    {
	if(m_table)
	    delete m_table;

	int hit_time = 1;
	int lookup_time = 2;

	for(unsigned sets=(0x1 << 7); sets <= (0x1 << 11); sets <<= 1) { //sets must be power of 2
	    for(unsigned assoc=1; assoc <= 16; assoc++) { //assoc: 1,2,3,4,5, ..., 16
		for(unsigned block=4; block <=128; block <<= 1) {
		    int size = sets * assoc * block;

		    hash_table myTable(size, assoc, block, hit_time, lookup_time);

		    // size/assoc = sets * block
		    CPPUNIT_ASSERT_EQUAL((paddr_t)((size/assoc-1) ^ (block-1)), myTable.index_mask);
		    paddr_t tagmask = ~0x0;
		    CPPUNIT_ASSERT_EQUAL((tagmask ^ (size/assoc-1)), myTable.tag_mask);
		}
	    }
	}//for(sets
    }



    //======================================================================
    //======================================================================
    //! @brief Empty cache; load with a random address; miss
    void test_process_request_load_0()
    {
	paddr_t addr = random();

	CPPUNIT_ASSERT_EQUAL(false, m_table->process_request(addr));

	delete m_table;
    }




    //======================================================================
    //======================================================================
    //! @brief Empty cache; load with a random address; miss; get response; load again; hit
    void test_process_request_load_1()
    {
	paddr_t addr = random();

	CPPUNIT_ASSERT_EQUAL(false, m_table->process_request(addr));

	//call process_response
	m_table->process_response(addr);

	//now, it should hit.
	CPPUNIT_ASSERT_EQUAL(true, m_table->process_request(addr));

	delete m_table;
    }




    //======================================================================
    //======================================================================
    //! @brief Test N load request with different addresses. No response. All should miss.
    void test_process_request_load_2()
    {
	set<paddr_t> addrs; //holds the addresses tested

	//SIZE/BLOCK_SIZE = numbr of lines
	for(int i=0; i < m_table->size/m_table->block_size *2; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret;
	    //make sure ad is not tried yet.
	    do {
		ad = random();
		ret = addrs.insert(ad);
	    } while(ret.second == false);

	    CPPUNIT_ASSERT_EQUAL(false, m_table->process_request(ad));
	}

	delete m_table;
    }




    //======================================================================
    //======================================================================
    //! @brief Test N load request with different addresses, that is they must
    //! differ in at least the index or the tag portion of the address.
    //! With response. All should miss.
    void test_process_request_load_3()
    {
	set<paddr_t> addrs; //holds the addresses tested, with offset part set to 0

	paddr_t offset_mask = m_table->block_size - 1;
	offset_mask = ~offset_mask; //offset_mask is used to clear the offset portion of
				    //an address

	//SIZE/BLOCK_SIZE = numbr of lines
	for(int i=0; i < m_table->size/m_table->block_size *2; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret;
	    //make sure ad is not tried yet.
	    do {
		ad = random() & offset_mask;
		ret = addrs.insert(ad);
	    } while(ret.second == false);


	    CPPUNIT_ASSERT_EQUAL(false, m_table->process_request(ad));

	    //call process_response
	    m_table->process_response(ad);

	    //now, it should hit.
	    CPPUNIT_ASSERT_EQUAL(true, m_table->process_request(ad));
	}

	delete m_table;
    }




    //======================================================================
    //======================================================================
    //! @brief Empty cache; load addr x; miss; get response; x hit; then all y that
    //! differs from x only in offset should hit.
    void test_process_request_load_4()
    {
	paddr_t org_addr = random(); //original addr is randomly created.

	CPPUNIT_ASSERT_EQUAL(false, m_table->process_request(org_addr));

	//call process_response
	m_table->process_response(org_addr);

	//now it hits
	CPPUNIT_ASSERT_EQUAL(true, m_table->process_request(org_addr));

	paddr_t offset_mask = m_table->block_size - 1;
	offset_mask = ~offset_mask; //offset_mask is used to clear the offset portion of
				    //an address

	for(int i=0; i<m_table->block_size; i++) { //try all offsets from 0 to BLOCK_SIZE-1
	    paddr_t ad = (org_addr & offset_mask) | i;
	    CPPUNIT_ASSERT_EQUAL(true, m_table->process_request(ad));
	}

	delete m_table;
    }



    //======================================================================
    //======================================================================
    //! @brief Empty cache; load addr x; miss; get response; x hit; then all y that
    //! differs from x ONLY in index should miss.
    void test_process_request_load_5()
    {
	paddr_t org_addr = random(); //original addr is randomly created.

	CPPUNIT_ASSERT_EQUAL(false, m_table->process_request(org_addr));

	//call process_response
	m_table->process_response(org_addr);

	//now it hits
	CPPUNIT_ASSERT_EQUAL(true, m_table->process_request(org_addr));

	int offset_num_bits=0;
	int temp = m_table->block_size-1;
	while(temp > 0) {
	    temp >>= 1;
	    offset_num_bits++;
	}
	paddr_t index_mask = m_table->size/m_table->block_size/m_table->assoc - 1;
	index_mask <<= offset_num_bits; //index_mask: the index portion is all 0's;
	index_mask = ~index_mask;       //the tag and offset portions are all 1's;

	int count=0;
	for(int i=0; i<m_table->size/m_table->block_size/m_table->assoc; i++) { //try all index
	    paddr_t ad = (org_addr & index_mask) | (i << offset_num_bits);
	    if(ad != org_addr) {
		count++;
		CPPUNIT_ASSERT_EQUAL(false, m_table->process_request(ad));
	    }
	}
	CPPUNIT_ASSERT_EQUAL(m_table->size/m_table->block_size/m_table->assoc-1, count);

	delete m_table;
    }




    //======================================================================
    //======================================================================
    //! @brief Empty cache; load addr x; miss; get response; x hit; then all y that
    //! differs from x ONLY in tag should miss.
    void test_process_request_load_6()
    {
	paddr_t org_addr = random(); //original addr is randomly created.

	CPPUNIT_ASSERT_EQUAL(false, m_table->process_request(org_addr));

	//call process_response
	m_table->process_response(org_addr);

	//now it hits
	CPPUNIT_ASSERT_EQUAL(true, m_table->process_request(org_addr));

	// | tag || index || offset |
	int index_offset_num_bits=0; //number of index and offset bits
	int temp = m_table->size/m_table->assoc-1;
	while(temp > 0) {
	    temp >>= 1;
	    index_offset_num_bits++;
	}

	//number of possible tags is usually big
	paddr_t largest_tag = (~0x0);
	largest_tag >>= index_offset_num_bits;

	//since addresses with the same index and offset are put in the same set,
	//up to 200 different tags should be enough for the test purposes
	int num_tags = (200 < largest_tag) ? 200 : largest_tag; //take the minimum


	paddr_t tag_mask = largest_tag << index_offset_num_bits; //tag portion is all 1's; rest 0's
	set<paddr_t> addrset; //use a set to generate unique values
	for(int i=0; i<num_tags; i++) {
	    paddr_t ad;
	    pair<set<paddr_t>::iterator,bool> ret; //return value; 2nd of the pair is false if the
						   //value is already in the set
	    //make sure ad is new and different from org_addr
	    do {
		ad = org_addr & (~tag_mask); //clear the tag portion
		ad |= ((random() & largest_tag) << index_offset_num_bits); // OR-in a random tag
		if(ad != org_addr)
		    ret = addrset.insert(ad);
	    } while(ad == org_addr || ret.second == false);

	    CPPUNIT_ASSERT_EQUAL(false, m_table->process_request(ad));
	}

	delete m_table;
    }





    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("hash_tableTest");

	mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("testConstructor_0", &hash_tableTest::testConstructor_0));
	mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("testConstructor_1", &hash_tableTest::testConstructor_1));
	mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("test_process_request_load_0", &hash_tableTest::test_process_request_load_0));
	mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("test_process_request_load_1", &hash_tableTest::test_process_request_load_1));
	mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("test_process_request_load_2", &hash_tableTest::test_process_request_load_2));
	mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("test_process_request_load_3", &hash_tableTest::test_process_request_load_3));
	mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("test_process_request_load_4", &hash_tableTest::test_process_request_load_4));
	mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("test_process_request_load_5", &hash_tableTest::test_process_request_load_5));
	mySuite->addTest(new CppUnit::TestCaller<hash_tableTest>("test_process_request_load_6", &hash_tableTest::test_process_request_load_6));

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


