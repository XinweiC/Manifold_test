#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <list>
#include <stdlib.h>
#include "Bank.h"
#include "Dsettings.h"
#include "Dreq.h"

using namespace std;

using namespace manifold::caffdram;

//####################################################################
// helper classes
//####################################################################


//####################################################################
//! Class BankTest is the test class for class Bank. 
//####################################################################
class BankTest : public CppUnit::TestFixture {
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
    void test_Constructor_0()
    {
	Dsettings setting;

	Bank* myBank = new Bank(&setting);

	CPPUNIT_ASSERT(&setting == myBank->dramSetting);
	CPPUNIT_ASSERT_EQUAL(true, myBank->firstAccess);
	CPPUNIT_ASSERT_EQUAL((unsigned)0, myBank->lastAccessedRow);
	CPPUNIT_ASSERT_EQUAL((unsigned long)0, myBank->bankAvailableAtTime);

	delete myBank;

    }



    //======================================================================
    //======================================================================
    //! @brief Test processClosedPageRequest()
    //!
    //! Create a Bank and a Dreq; call processClosedPageRequest(); verify the
    //! returned latency, the updated bankAvailableAtTime and endTime of the Dreq.
    void test_processClosedPageRequest_0()
    {
	Dsettings setting;

	Bank* myBank = new Bank(&setting);

	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	myBank->bankAvailableAtTime = random(); //randomly set bankAvailableAtTime before the call

	unsigned long old_bankAvailableAtTime = myBank->bankAvailableAtTime;

	Dreq dreq(addr, currentSimTime, &setting);

	unsigned long rv = myBank->processClosedPageRequest(&dreq);

	unsigned long new_bankAvailableAtTime = old_bankAvailableAtTime + setting.t_RCD + setting.t_CAS + setting.t_RP;

	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime, myBank->bankAvailableAtTime);
	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime + setting.t_BURST, rv);
	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime + setting.t_BURST, dreq.endTime);


	delete myBank;

    }



    //======================================================================
    //======================================================================
    //! @brief Test processOpenPageRequest(): firstAccess==false, rowId==lastAccessedRow.
    //!
    //! Depending on firstAccess == true) and rowId == lastAccessedRow), there
    //! are 4 cases. This is case 0, (firstAccess == false), (rowId == lastAccessedRow).
    //! Create a Bank and a Dreq; call processOpenPageRequest(); verify the
    //! returned latency, the new bankAvailableAtTime, and endTime of the Dreq.
    //!
    void test_processOpenPageRequest_0()
    {
	Dsettings setting;

	Bank* myBank = new Bank(&setting);
	myBank->firstAccess = false;
	myBank->bankAvailableAtTime = random(); //randomly set bankAvailableAtTime before the call
	unsigned long old_bankAvailableAtTime = myBank->bankAvailableAtTime;

	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	Dreq dreq(addr, currentSimTime, &setting);

	//set lastAccessedRow to rowId of dreq
	myBank->lastAccessedRow = dreq.rowId;


	unsigned long rv = myBank->processOpenPageRequest(&dreq);


	//We are accessing the same row as last time, so new availableTime is incremented by t_CAS.
	unsigned long new_bankAvailableAtTime = old_bankAvailableAtTime + setting.t_CAS;

	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime, myBank->bankAvailableAtTime);
	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime + setting.t_BURST, rv);
	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime + setting.t_BURST, dreq.endTime);


	delete myBank;

    }





    //======================================================================
    //======================================================================
    //! @brief Test processOpenPageRequest(): firstAccess==false, rowId!=lastAccessedRow.
    //!
    //! Depending on firstAccess == true) and rowId == lastAccessedRow), there
    //! are 4 cases. This is case 1, (firstAccess == false), (rowId != lastAccessedRow).
    //! Create a Bank and a Dreq; call processOpenPageRequest(); verify the
    //! returned latency, the new bankAvailableAtTime, and endTime of the Dreq.
    //!
    void test_processOpenPageRequest_1()
    {
	Dsettings setting;

	Bank* myBank = new Bank(&setting);
	myBank->firstAccess = false;
	myBank->bankAvailableAtTime = random(); //randomly set bankAvailableAtTime before the call
	unsigned long old_bankAvailableAtTime = myBank->bankAvailableAtTime;

	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	Dreq dreq(addr, currentSimTime, &setting);

	//set lastAccessedRow to a value different from rowId of dreq
	unsigned r;
	while((r = random()) == dreq.rowId);
	myBank->lastAccessedRow = r;


	unsigned long rv = myBank->processOpenPageRequest(&dreq);


	//We are accessing different row as last time, so new availableTime is incremented by (t_RP+t_RCD+t_CAS).
	unsigned long new_bankAvailableAtTime = old_bankAvailableAtTime + setting.t_RP + setting.t_RCD + setting.t_CAS;

	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime, myBank->bankAvailableAtTime);
	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime + setting.t_BURST, rv);
	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime + setting.t_BURST, dreq.endTime);


	delete myBank;

    }





    //======================================================================
    //======================================================================
    //! @brief Test processOpenPageRequest(): firstAccess==true, rowId==lastAccessedRow.
    //!
    //! Depending on firstAccess == true) and rowId == lastAccessedRow), there
    //! are 4 cases. This is case 2, (firstAccess == true), (rowId == lastAccessedRow).
    //! Create a Bank and a Dreq; call processOpenPageRequest(); verify the
    //! returned latency, the new bankAvailableAtTime, and endTime of the Dreq.
    //!
    void test_processOpenPageRequest_2()
    {
	Dsettings setting;

	Bank* myBank = new Bank(&setting);
	CPPUNIT_ASSERT_EQUAL(true, myBank->firstAccess);
	myBank->bankAvailableAtTime = random(); //randomly set bankAvailableAtTime before the call
	unsigned long old_bankAvailableAtTime = myBank->bankAvailableAtTime;

	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	Dreq dreq(addr, currentSimTime, &setting);

	//set lastAccessedRow to rowId of dreq
	myBank->lastAccessedRow = dreq.rowId;


	unsigned long rv = myBank->processOpenPageRequest(&dreq);


	//First access, so new availableTime is incremented by (t_RCD + t_CAS).
	unsigned long new_bankAvailableAtTime = old_bankAvailableAtTime + setting.t_RCD + setting.t_CAS;

	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime, myBank->bankAvailableAtTime);
	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime + setting.t_BURST, rv);
	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime + setting.t_BURST, dreq.endTime);


	delete myBank;

    }






    //======================================================================
    //======================================================================
    //! @brief Test processOpenPageRequest(): firstAccess==true, rowId!=lastAccessedRow.
    //!
    //! Depending on firstAccess == true) and rowId == lastAccessedRow), there
    //! are 4 cases. This is case 3, (firstAccess == true), (rowId != lastAccessedRow).
    //! Create a Bank and a Dreq; call processOpenPageRequest(); verify the
    //! returned latency, the new bankAvailableAtTime, and endTime of the Dreq.
    //!
    void test_processOpenPageRequest_3()
    {
	Dsettings setting;

	Bank* myBank = new Bank(&setting);
	CPPUNIT_ASSERT_EQUAL(true, myBank->firstAccess);
	myBank->bankAvailableAtTime = random(); //randomly set bankAvailableAtTime before the call
	unsigned long old_bankAvailableAtTime = myBank->bankAvailableAtTime;

	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	Dreq dreq(addr, currentSimTime, &setting);

	//set lastAccessedRow to a value different from rowId of dreq
	unsigned r;
	while((r = random()) == dreq.rowId);
	myBank->lastAccessedRow = r;


	unsigned long rv = myBank->processOpenPageRequest(&dreq);


	//First access, so new availableTime is incremented by (t_RCD + t_CAS).
	unsigned long new_bankAvailableAtTime = old_bankAvailableAtTime + setting.t_RCD + setting.t_CAS;

	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime, myBank->bankAvailableAtTime);
	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime + setting.t_BURST, rv);
	CPPUNIT_ASSERT_EQUAL(new_bankAvailableAtTime + setting.t_BURST, dreq.endTime);


	delete myBank;

    }





    //======================================================================
    //======================================================================
    //! @brief Test processRequest(): originTime of request < bankAvailableAtTime
    //! AND memPagePolicy == OPEN_PAGE.
    //!
    //! Based on originTime and bandkAvailableAtTime, and memPagePolicy, there
    //! are 6 cases. In this case, originTime < bankAvailableAtTime, and
    //! memPagePolicy == OPEN_PAGE.
    void test_processRequest_0()
    {
	Dsettings setting;
	setting.memPagePolicy = OPEN_PAGE;

	Bank* myBank = new Bank(&setting);
	//set bankAvailableAtTime to a random value.
	myBank->bankAvailableAtTime = random();


	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	Dreq dreq(addr, currentSimTime, &setting);

	//ensure (dreq->originTime < myBank->get_bankAvailableAtTime())
	if(dreq.originTime >= myBank->bankAvailableAtTime) {
	    unsigned long t;
	    while((t = random()) >= myBank->bankAvailableAtTime);
	    dreq.originTime = t;
	}

        //make a copy of Bank and Dreq
	Bank myBank_copy = *myBank;
        Dreq dreq_copy = dreq;



	unsigned long rv = myBank->processRequest(&dreq);



	//under the condition (originTime < bankAvailableTime) and OPEN_PAGE,
	//processRequest() eventually calls processOpenPageRequest().
	CPPUNIT_ASSERT_EQUAL(myBank_copy.processOpenPageRequest(&dreq_copy), rv);
	CPPUNIT_ASSERT_EQUAL(rv, dreq.endTime);
	CPPUNIT_ASSERT_EQUAL(rv, myBank->bankAvailableAtTime + setting.t_BURST);
	


	delete myBank;

    }


    //======================================================================
    //======================================================================
    //! @brief Test processRequest(): originTime of request < bankAvailableAtTime
    //! AND memPagePolicy == CLOSED_PAGE.
    //!
    //! Based on originTime and bandkAvailableAtTime, and memPagePolicy, there
    //! are 6 cases. In this case, originTime < bankAvailableAtTime, and
    //! memPagePolicy == CLOSED_PAGE.
    void test_processRequest_1()
    {
	Dsettings setting;
	setting.memPagePolicy = CLOSED_PAGE;

	Bank* myBank = new Bank(&setting);
	//set bankAvailableAtTime to a random value.
	myBank->bankAvailableAtTime = random();


	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	Dreq dreq(addr, currentSimTime, &setting);

	//ensure (dreq->originTime < myBank->get_bankAvailableAtTime())
	if(dreq.originTime >= myBank->bankAvailableAtTime) {
	    unsigned long t;
	    while((t = random()) >= myBank->bankAvailableAtTime);
	    dreq.originTime = t;
	}

        //make a copy of Bank and Dreq
	Bank myBank_copy = *myBank;
        Dreq dreq_copy = dreq;



	unsigned long rv = myBank->processRequest(&dreq);



	//under the condition (originTime < bankAvailableTime) and CLOSED_PAGE,
	//processRequest() eventually calls processClosedPageRequest().
	CPPUNIT_ASSERT_EQUAL(myBank_copy.processClosedPageRequest(&dreq_copy), rv);
	CPPUNIT_ASSERT_EQUAL(rv, dreq.endTime);
	CPPUNIT_ASSERT_EQUAL(rv, myBank->bankAvailableAtTime + setting.t_BURST);
	


	delete myBank;

    }



    //======================================================================
    //======================================================================
    //! @brief Test processRequest(): originTime of request == bankAvailableAtTime
    //! AND memPagePolicy == OPEN_PAGE.
    //!
    //! Based on originTime and bandkAvailableAtTime, and memPagePolicy, there
    //! are 6 cases. In this case, originTime == bankAvailableAtTime, and
    //! memPagePolicy == OPEN_PAGE.
    void test_processRequest_2()
    {
	Dsettings setting;
	setting.memPagePolicy = OPEN_PAGE;

	Bank* myBank = new Bank(&setting);
	//set bankAvailableAtTime to a random value.
	myBank->bankAvailableAtTime = random();


	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	Dreq dreq(addr, currentSimTime, &setting);

	//ensure (dreq->originTime == myBank->get_bankAvailableAtTime())
	dreq.originTime = myBank->bankAvailableAtTime;

        //make a copy of Bank and Dreq
	Bank myBank_copy = *myBank;
        Dreq dreq_copy = dreq;



	unsigned long rv = myBank->processRequest(&dreq);



	//under the condition (originTime == bankAvailableTime) and OPEN_PAGE,
	//processRequest() eventually calls processOpenPageRequest().
	CPPUNIT_ASSERT_EQUAL(myBank_copy.processOpenPageRequest(&dreq_copy), rv);
	CPPUNIT_ASSERT_EQUAL(rv, dreq.endTime);
	CPPUNIT_ASSERT_EQUAL(rv, myBank->bankAvailableAtTime + setting.t_BURST);
	


	delete myBank;

    }


    //======================================================================
    //======================================================================
    //! @brief Test processRequest(): originTime of request == bankAvailableAtTime
    //! AND memPagePolicy == CLOSED_PAGE.
    //!
    //! Based on originTime and bandkAvailableAtTime, and memPagePolicy, there
    //! are 6 cases. In this case, originTime == bankAvailableAtTime, and
    //! memPagePolicy == CLOSED_PAGE.
    void test_processRequest_3()
    {
	Dsettings setting;
	setting.memPagePolicy = CLOSED_PAGE;

	Bank* myBank = new Bank(&setting);
	//set bankAvailableAtTime to a random value.
	myBank->bankAvailableAtTime = random();


	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	Dreq dreq(addr, currentSimTime, &setting);

	//ensure (dreq->originTime == myBank->get_bankAvailableAtTime())
	dreq.originTime = myBank->bankAvailableAtTime;

        //make a copy of Bank and Dreq
	Bank myBank_copy = *myBank;
        Dreq dreq_copy = dreq;



	unsigned long rv = myBank->processRequest(&dreq);



	//under the condition (originTime == bankAvailableTime) and CLOSED_PAGE,
	//processRequest() eventually calls processClosedPageRequest().
	CPPUNIT_ASSERT_EQUAL(myBank_copy.processClosedPageRequest(&dreq_copy), rv);
	CPPUNIT_ASSERT_EQUAL(rv, dreq.endTime);
	CPPUNIT_ASSERT_EQUAL(rv, myBank->bankAvailableAtTime + setting.t_BURST);
	


	delete myBank;

    }



    //======================================================================
    //======================================================================
    //! @brief Test processRequest(): originTime of request > bankAvailableAtTime
    //! AND memPagePolicy == OPEN_PAGE.
    //!
    //! Based on originTime and bandkAvailableAtTime, and memPagePolicy, there
    //! are 6 cases. In this case, originTime > bankAvailableAtTime, and
    //! memPagePolicy == OPEN_PAGE.
    void test_processRequest_4()
    {
	Dsettings setting;
	setting.memPagePolicy = OPEN_PAGE;

	Bank* myBank = new Bank(&setting);
	//set bankAvailableAtTime to a random value.
	myBank->bankAvailableAtTime = random();


	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	Dreq dreq(addr, currentSimTime, &setting);

	//ensure (dreq->originTime > myBank->get_bankAvailableAtTime())
	if(dreq.originTime <= myBank->bankAvailableAtTime) {
	    unsigned long t;
	    while((t = random()) <= myBank->bankAvailableAtTime);
	    dreq.originTime = t;
	}

        //make a copy of Bank and Dreq
	Bank myBank_copy = *myBank;
	//In this case, the bankAvailableAtTime is first set to the originTime. So
	//we need to do the same for myBank_copy.
	myBank_copy.bankAvailableAtTime = dreq.originTime;
        Dreq dreq_copy = dreq;



	unsigned long rv = myBank->processRequest(&dreq);



	//under the condition (originTime < bankAvailableTime) and OPEN_PAGE,
	//processRequest() eventually calls processOpenPageRequest().
	CPPUNIT_ASSERT_EQUAL(myBank_copy.processOpenPageRequest(&dreq_copy), rv);
	CPPUNIT_ASSERT_EQUAL(rv, dreq.endTime);
	CPPUNIT_ASSERT_EQUAL(rv, myBank->bankAvailableAtTime + setting.t_BURST);
	


	delete myBank;

    }


    //======================================================================
    //======================================================================
    //! @brief Test processRequest(): originTime of request > bankAvailableAtTime
    //! AND memPagePolicy == CLOSED_PAGE.
    //!
    //! Based on originTime and bandkAvailableAtTime, and memPagePolicy, there
    //! are 6 cases. In this case, originTime > bankAvailableAtTime, and
    //! memPagePolicy == CLOSED_PAGE.
    void test_processRequest_5()
    {
	Dsettings setting;
	setting.memPagePolicy = CLOSED_PAGE;

	Bank* myBank = new Bank(&setting);
	//set bankAvailableAtTime to a random value.
	myBank->bankAvailableAtTime = random();


	unsigned long addr = random();
	unsigned long currentSimTime = random(); //current simulation time

	Dreq dreq(addr, currentSimTime, &setting);

	//ensure (dreq->originTime > myBank->get_bankAvailableAtTime())
	if(dreq.originTime <= myBank->bankAvailableAtTime) {
	    unsigned long t;
	    while((t = random()) <= myBank->bankAvailableAtTime);
	    dreq.originTime = t;
	}

        //make a copy of Bank and Dreq
	Bank myBank_copy = *myBank;
	//In this case, the bankAvailableAtTime is first set to the originTime. So
	//we need to do the same for myBank_copy.
	myBank_copy.bankAvailableAtTime = dreq.originTime;
        Dreq dreq_copy = dreq;



	unsigned long rv = myBank->processRequest(&dreq);



	//under the condition (originTime < bankAvailableTime) and CLOSED_PAGE,
	//processRequest() eventually calls processClosedPageRequest().
	CPPUNIT_ASSERT_EQUAL(myBank_copy.processClosedPageRequest(&dreq_copy), rv);
	CPPUNIT_ASSERT_EQUAL(rv, dreq.endTime);
	CPPUNIT_ASSERT_EQUAL(rv, myBank->bankAvailableAtTime + setting.t_BURST);
	


	delete myBank;

    }




    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("BankTest");

	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_Constructor_0", &BankTest::test_Constructor_0));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processClosedPageRequest_0", &BankTest::test_processClosedPageRequest_0));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processOpenPageRequest_0", &BankTest::test_processOpenPageRequest_0));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processOpenPageRequest_1", &BankTest::test_processOpenPageRequest_1));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processOpenPageRequest_2", &BankTest::test_processOpenPageRequest_2));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processOpenPageRequest_3", &BankTest::test_processOpenPageRequest_3));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processRequest_0", &BankTest::test_processRequest_0));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processRequest_1", &BankTest::test_processRequest_1));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processRequest_2", &BankTest::test_processRequest_2));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processRequest_3", &BankTest::test_processRequest_3));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processRequest_4", &BankTest::test_processRequest_4));
	mySuite->addTest(new CppUnit::TestCaller<BankTest>("test_processRequest_5", &BankTest::test_processRequest_5));
/*
	*/

	return mySuite;
    }
};



int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( BankTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


