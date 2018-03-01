#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <list>
#include <stdlib.h>
#include "McMap.h"

using namespace std;

using namespace manifold::caffdram;
//using namespace manifold::simple_cache;

//####################################################################
// helper classes
//####################################################################


//####################################################################
//! Class CaffDramMcMapTest is the test class for class CaffDramMcMap. 
//####################################################################
class CaffDramMcMapTest : public CppUnit::TestFixture {
private:


public:

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }




    //======================================================================
    //======================================================================
    //! @brief Test constructor
    //!
    //! Verify m_mc_selector_mask is set correctly.
    void test_Constructor_0()
    {
	Dsettings setting;

	for(unsigned i=1; i<12; i++) {
	    vector<int> nodes(0x1 << i);
	    CaffDramMcMap* myMap = new CaffDramMcMap(nodes, setting);

	    //create a mask with lowest i bits set to 1, and the rest 0
	    unsigned long mask = ~0x0;
	    mask >>= i;
	    mask <<= i;
	    mask = ~mask;
	    //verify this is the mask used to select MCs.
	    CPPUNIT_ASSERT_EQUAL(mask, myMap->m_mc_selector_mask);
	    delete myMap;
	}

    }



    //======================================================================
    //======================================================================
    //! @brief Test constructor
    //!
    //! Verify m_mc_shift_bits, which is the settings's channelShiftBits plus
    //! the number of bits required to select channels, is set correctly. 
    void test_Constructor_1()
    {
	for(int i=0; i<1000; i++) { //try 1000 random Dsettings
	    int ch_bits = random() % 5; //0 to 4 bits
	    int rk_bits = random() % 5; //0 to 4 bits
	    int bk_bits = random() % 5; //0 to 4 bits
	    int ro_bits = random() % 15; //0 to 14 bits
	    int co_bits = random() % 15; //0 to 14 bits
	    //max 40 bits; leaving 24 for MCs

	    Dsettings setting(0x1<<ch_bits, 0x1<<rk_bits, 0x1<<bk_bits, 0x1<<ro_bits, 0x1<<co_bits);

	    for(unsigned b=1; b<12; b++) {
		vector<int> nodes(0x1 << b);
		CaffDramMcMap* myMap = new CaffDramMcMap(nodes, setting);

		//verify this is the mask used to select MCs.
		CPPUNIT_ASSERT_EQUAL(ch_bits+rk_bits+bk_bits+ro_bits+co_bits, myMap->m_mc_shift_bits);
		delete myMap;
	    }
	}

    }



    //======================================================================
    //======================================================================
    //! @brief Test constructor
    //!
    //! Test when system has only 1 MC.
    void test_lookup_0()
    {
	for(int i=0; i<1000; i++) { //try 1000 random Dsettings
	    int ch_bits = random() % 5; //0 to 4 bits
	    int rk_bits = random() % 5; //0 to 4 bits
	    int bk_bits = random() % 5; //0 to 4 bits
	    int ro_bits = random() % 15; //0 to 14 bits
	    int co_bits = random() % 15; //0 to 14 bits
	    //max 40 bits; leaving 24 for MCs

	    Dsettings setting(0x1<<ch_bits, 0x1<<rk_bits, 0x1<<bk_bits, 0x1<<ro_bits, 0x1<<co_bits);

	    vector<int> nodes(1); //Only 1 MC

	    nodes[0] = random();  //populate the vector with random values

	    CaffDramMcMap* myMap = new CaffDramMcMap(nodes, setting);

	    unsigned long addr = random();
	    int rv = myMap->lookup(addr);

	    CPPUNIT_ASSERT_EQUAL(nodes[0], rv);

	    delete myMap;
	}

    }






    //======================================================================
    //======================================================================
    //! @brief Test constructor
    //!
    void test_lookup_1()
    {
	for(int i=0; i<1000; i++) { //try 1000 random Dsettings
	    int ch_bits = random() % 5; //0 to 4 bits
	    int rk_bits = random() % 5; //0 to 4 bits
	    int bk_bits = random() % 5; //0 to 4 bits
	    int ro_bits = random() % 15; //0 to 14 bits
	    int co_bits = random() % 15; //0 to 14 bits
	    //max 40 bits; leaving 24 for MCs

	    Dsettings setting(0x1<<ch_bits, 0x1<<rk_bits, 0x1<<bk_bits, 0x1<<ro_bits, 0x1<<co_bits);

	    int mc_bits = random() % 12 + 1;
	    vector<int> nodes(0x1 << mc_bits);

	    for(unsigned j=0; j<nodes.size(); j++) {
	        nodes[j] = random();  //populate the vector with random values
	    }

	    CaffDramMcMap* myMap = new CaffDramMcMap(nodes, setting);

	    for(unsigned a=0; a<nodes.size(); a++) {
	        unsigned long addr = random();
		//set the MC part of the addr to a
		unsigned long mask = myMap->m_mc_selector_mask << myMap->m_mc_shift_bits;
		mask = ~mask;
		addr &= mask; //the MC part of the addr is cleared
		addr |= ((unsigned long)a << myMap->m_mc_shift_bits);
		int rv = myMap->lookup(addr);

		CPPUNIT_ASSERT_EQUAL(nodes[a], rv);
	    }

	    delete myMap;
	}

    }







    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("CaffDramMcMapTest");

	mySuite->addTest(new CppUnit::TestCaller<CaffDramMcMapTest>("test_Constructor_0", &CaffDramMcMapTest::test_Constructor_0));
	mySuite->addTest(new CppUnit::TestCaller<CaffDramMcMapTest>("test_Constructor_1", &CaffDramMcMapTest::test_Constructor_1));
	mySuite->addTest(new CppUnit::TestCaller<CaffDramMcMapTest>("test_lookup_0", &CaffDramMcMapTest::test_lookup_0));
	mySuite->addTest(new CppUnit::TestCaller<CaffDramMcMapTest>("test_lookup_1", &CaffDramMcMapTest::test_lookup_1));

	return mySuite;
    }
};



int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( CaffDramMcMapTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


