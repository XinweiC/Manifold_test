#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <list>
#include <stdlib.h>
#include "Rank.h"

using namespace std;

using namespace manifold::caffdram;

//####################################################################
// helper classes
//####################################################################


//####################################################################
//! Class DreqTest is the test class for class Dreq. 
//####################################################################
class DreqTest : public CppUnit::TestFixture {
private:


public:

    //! Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }




    //======================================================================
    //======================================================================
    //! @brief Test generateId
    //!
    //! numChannels of Dsetting is 1.
    void test_generateId_0()
    {
	Dsettings setting;
	setting.numChannels = 1;

	unsigned long addr = random();

	Dreq* r = new Dreq(addr, 0, &setting);

	r->generateId(addr, &setting);

	CPPUNIT_ASSERT_EQUAL( (addr >> (setting.rowShiftBits)) & (setting.rowMask), (unsigned long)r->rowId);
	CPPUNIT_ASSERT_EQUAL( (addr >> (setting.bankShiftBits)) & (setting.bankMask), (unsigned long)r->bankId);
	CPPUNIT_ASSERT_EQUAL( (addr >> (setting.rankShiftBits)) & (setting.rankMask), (unsigned long)r->rankId);
	CPPUNIT_ASSERT_EQUAL(0, (int)r->chId);

	delete r;
    }





    //======================================================================
    //======================================================================
    //! @brief Test generateId
    //!
    //! numChannels of Dsetting is not 1.
    void test_generateId_1()
    {
	Dsettings setting;
	setting.numChannels = random() % 100 + 1;

	CPPUNIT_ASSERT(setting.numChannels != 1);

	unsigned long addr = random();

	Dreq* r = new Dreq(addr, 0, &setting);

	r->generateId(addr, &setting);

	CPPUNIT_ASSERT_EQUAL( (addr >> (setting.rowShiftBits)) & (setting.rowMask), (unsigned long)r->rowId);
	CPPUNIT_ASSERT_EQUAL( (addr >> (setting.bankShiftBits)) & (setting.bankMask), (unsigned long)r->bankId);
	CPPUNIT_ASSERT_EQUAL( (addr >> (setting.rankShiftBits)) & (setting.rankMask), (unsigned long)r->rankId);
	CPPUNIT_ASSERT_EQUAL( (addr >> (setting.channelShiftBits)) % (setting.numChannels), (unsigned long)r->chId);

	delete r;
    }




    //======================================================================
    //======================================================================
    //! @brief Test constructor
    //!
    //! Create N Dreqs; verify initialization.
    void test_Constructor_0()
    {
	const int N = 1000;

        for(int i=0; i<N; i++) {
	    unsigned long addr = random();
	    unsigned long currentTime = random();

	    Dsettings setting;
	    setting.t_CMD = random() % 99 + 1;
	    if(random() / (RAND_MAX + 1.0) < 0.5)
		setting.numChannels = 1;
            else
		setting.numChannels = random() % 100 + 1;

	    Dreq* req = new Dreq(addr, currentTime, &setting);

	    CPPUNIT_ASSERT_EQUAL(currentTime + setting.t_CMD, req->originTime);

	    CPPUNIT_ASSERT_EQUAL( (addr >> (setting.rowShiftBits)) & (setting.rowMask), (unsigned long)req->rowId);
	    CPPUNIT_ASSERT_EQUAL( (addr >> (setting.bankShiftBits)) & (setting.bankMask), (unsigned long)req->bankId);
	    CPPUNIT_ASSERT_EQUAL( (addr >> (setting.rankShiftBits)) & (setting.rankMask), (unsigned long)req->rankId);
	    if(setting.numChannels == 1)
		CPPUNIT_ASSERT_EQUAL(0, (int)req->chId);
	    else
		CPPUNIT_ASSERT_EQUAL( (addr >> (setting.channelShiftBits)) % (setting.numChannels), (unsigned long)req->chId);

	    delete req;
	}

    }







    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("DreqTest");

	mySuite->addTest(new CppUnit::TestCaller<DreqTest>("test_Constructor_0", &DreqTest::test_Constructor_0));
	mySuite->addTest(new CppUnit::TestCaller<DreqTest>("test_generateId_0", &DreqTest::test_generateId_0));
	mySuite->addTest(new CppUnit::TestCaller<DreqTest>("test_generateId_1", &DreqTest::test_generateId_1));

	return mySuite;
    }
};



int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( DreqTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


