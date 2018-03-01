//!
//!
#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include "mpi.h"
#include "messenger.h"
#include "manifold.h"

using namespace std;
using namespace manifold::kernel;




//####################################################################
// helper classes
//####################################################################

// The following 2 classes are used to test unregistering an object from a
// clock and register it with another clock.

class MyObj1 : public Component {
private:
    vector<Ticks_t> m_ticks0;
    vector<Ticks_t> m_ticks1;

public:

    void rising0()
    {
        m_ticks0.push_back(0);
    }

    void rising1()
    {
        m_ticks1.push_back(0);
    }

    void falling() {}

    vector<Ticks_t>& get_ticks0() { return m_ticks0; }
    vector<Ticks_t>& get_ticks1() { return m_ticks1; }
};


Clock* Gclock;


class MyObj2 : public Component {
private:

public:

    //when this handler is called, it unregister and reregister the specified object.
    //Do NOT use reference parameters! Must use pointers.
    void handler(MyObj1* obj, Clock* c0, Clock* c1)
    {
	Clock::Unregister<MyObj1>(*c0, obj);

	Clock::Register<MyObj1>(*c1, obj, &MyObj1::rising1, &MyObj1::falling);
    }
};





//####################################################################
//####################################################################
class ClockTest : public CppUnit::TestFixture {
    private:
	static Clock MasterClock;  //clock has to be global or static.
	static Clock Clock1;
	enum { MASTER_CLOCK_HZ = 10 };
	enum { CLOCK1_HZ = 6 };

        static const double DOUBLE_COMP_DELTA = 1.0E-5;

    public:
        //!
	//! Initialization function. Inherited from the CPPUnit framework.
        //!
        void setUp()
	{
	}

	//=============================================================================
	//=============================================================================
	//! @brief Verify unregister and reregister of objects.
	//!
	//! Create an object A that is registered with a clock C0. Schedule an event
	//! T0 ticks from now. When the event is processed, it registers A from C0 and
	//! registers it with another clock C1. Continure running for T1 ticks. Verify
	//! the object A was indeed switched to clock C1.
        void test_unregister_0()
	{
	    Clock* clock0 = &MasterClock;
	    Clock* clock1 = &Clock1;

	    int clock0_HZ = MASTER_CLOCK_HZ;
	    int clock1_HZ = CLOCK1_HZ;



	    MyObj1* obj1 = new MyObj1;
	    MyObj2* obj2 = new MyObj2;

	    Manifold :: Reset(Manifold::TICKED);

	    //register obj1 with clock0
	    Clock::Register<MyObj1>(*clock0, obj1, &MyObj1::rising0, &MyObj1::falling);

            const int Duration0 = random() % 90 + 10; //run for this many ticks and then switch clock.
            const int Duration1 = random() % 90 + 10; //continue run this many ticks of clock0, then stop

	    //schedule MyObj2::handler at Duration0, which unregister/reregister obj1.
	    Manifold :: ScheduleClock(Duration0, *clock0, &MyObj2::handler, obj2, obj1, clock0, clock1);

	    Manifold :: StopAtClock(Duration0 + Duration1, *clock0);
	    Manifold :: Run();

	    //Now for verification
	    // Verify obj1 runs Duration0 ticks on clock0.
	    CPPUNIT_ASSERT_EQUAL(Duration0, (int)obj1->get_ticks0().size());

	    //Duration1 ticks of clock0 is  Duration1 * (clock1_HZ/clock0_HZ) ticks on clock1
	    if(Duration1 * clock1_HZ/clock0_HZ !=  (int)obj1->get_ticks1().size()) {
	        //the right side is an integer; the left could be a rounded number. For example, the
		//left could be 26 * 6/10=15.6, which rounds to 15, and the righst side is 16.
		//So if they are not equal, it must be due to the rounding.
		CPPUNIT_ASSERT_EQUAL(Duration1 * clock1_HZ/clock0_HZ + 1,  (int)obj1->get_ticks1().size());
	    }

	}




	//=============================================================================
	//=============================================================================
	//! @brief Verify unregister and reregister of objects.
	//!
	//! Same as above, except clock0 and clock1 are exchanged.
        void test_unregister_1()
	{
	    Clock* clock0 = &Clock1;
	    Clock* clock1 = &MasterClock;

	    int clock0_HZ = CLOCK1_HZ;
	    int clock1_HZ = MASTER_CLOCK_HZ;



	    MyObj1* obj1 = new MyObj1;
	    MyObj2* obj2 = new MyObj2;

	    Manifold :: Reset(Manifold::TICKED);

	    //register obj1 with clock0
	    Clock::Register<MyObj1>(*clock0, obj1, &MyObj1::rising0, &MyObj1::falling);

            const int Duration0 = random() % 90 + 10; //run for this many ticks and then switch clock.
            const int Duration1 = random() % 90 + 10; //continue run this many ticks of clock0, then stop

	    //schedule MyObj2::handler at Duration0, which unregister/reregister obj1.
	    Manifold :: ScheduleClock(Duration0, *clock0, &MyObj2::handler, obj2, obj1, clock0, clock1);

	    Manifold :: StopAtClock(Duration0 + Duration1, *clock0);
	    Manifold :: Run();

	    //Now for verification
	    // Verify obj1 runs Duration0 ticks on clock0.
	    CPPUNIT_ASSERT_EQUAL(Duration0, (int)obj1->get_ticks0().size());

	    //Duration1 ticks of clock0 is  Duration1 * (clock1_HZ/clock0_HZ) ticks on clock1
	    if(Duration1 * clock1_HZ/clock0_HZ !=  (int)obj1->get_ticks1().size()) {
	        //the right side is an integer; the left could be a rounded number. For example, the
		//left could be 26 * 6/10=15.6, which rounds to 15, and the righst side is 16.
		//So if they are not equal, it must be due to the rounding.
		CPPUNIT_ASSERT_EQUAL(Duration1 * clock1_HZ/clock0_HZ + 1,  (int)obj1->get_ticks1().size());
	    }

	}



        /**
	 * Build a test suite.
	 */
	static CppUnit::Test* suite()
	{
	    CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("ClockTest");

	    mySuite->addTest(new CppUnit::TestCaller<ClockTest>("test_unregister_0", &ClockTest::test_unregister_0));
	    mySuite->addTest(new CppUnit::TestCaller<ClockTest>("test_unregister_1", &ClockTest::test_unregister_1));

	    return mySuite;
	}
};


Clock ClockTest::MasterClock(MASTER_CLOCK_HZ);
Clock ClockTest::Clock1(CLOCK1_HZ);


int main(int argc, char** argv)
{
    Manifold :: Init();

    CppUnit::TextUi::TestRunner runner;
    runner.addTest( ClockTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;
}

