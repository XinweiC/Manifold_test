#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <map>
#include <utility>
#include <stdlib.h>

#include "clock.h"

using namespace manifold::kernel;

//####################################################################
// helper classes
//####################################################################

int Freqs[] = {1, 2, 5, 10, 20};

class MyObj1 : public Component {
private:
    DVFSClock& m_clk;
    const int num_freqs;
    int m_cur_freq; //current freq
    int m_run_cycles; //run for this many cycles and then change frequency
    int m_cycle_count; //count how many cycles have passed at current frequency
                       //when this number reaches m_run_cycles frequency is changed.

    std::list<std::pair<Ticks_t, Time_t> > m_tick_time_pairs;
    std::list<std::pair<int, int> > m_cycle_freq_pairs; //record each frequency was run for how many cycles
                                                        // (c1, f1), (c2, f2) means from 0 to (c1-1) the
							//freq was f1, and from c1 to (c2-1) it was f2.

public:
    MyObj1(DVFSClock& clk) : m_clk(clk), num_freqs(sizeof(Freqs)/sizeof(Freqs[0]))
    {
        m_run_cycles = random() % 6 + 5; //5 to 10 cycles
//cout << "m_run_cycle= " << m_run_cycles << endl;
	m_cycle_count = 0;
	m_cur_freq = m_clk.freq;
	m_cycle_freq_pairs.push_back(pair<int,int>(m_run_cycles, m_cur_freq));
    }


    void risingTick()
    {
//cout << "tick " << m_clk.NowTicks() << endl;
        m_tick_time_pairs.push_back(pair<Ticks_t, Time_t>(m_clk.NowTicks(), m_clk.NextTickTime()));
	if(m_cycle_count == m_run_cycles) { //time to change frequency
	    m_run_cycles = random() % 6 + 5; //5 to 10 cycles
//cout << "m_run_cycle= " << m_run_cycles << endl;
	    m_cycle_count = 0;

	    int freq;
	    while((freq = Freqs[random() % num_freqs]) == m_cur_freq); //must be different from current freq
	    m_cur_freq = freq;
//cout << "at cycle " << m_clk.NowTicks() << " freq = " << m_cur_freq << endl;
	    m_cycle_freq_pairs.push_back(pair<int,int>(m_run_cycles+m_cycle_freq_pairs.back().first, m_cur_freq));
	    m_clk.set_frequency(m_cur_freq);
	}
    }

    void fallingTick()
    {
	m_cycle_count++;
    }

    std::list<std::pair<Ticks_t, Time_t> >& get_tick_time_pairs() { return m_tick_time_pairs; }
    std::list<std::pair<int, int> >& get_cycle_freq_pairs() { return m_cycle_freq_pairs; }

};




//####################################################################
//####################################################################
class DVFSTest : public CppUnit::TestFixture {
private:
    static const double DOUBLE_COMP_DELTA = 1.0E-5;

public:
    // Initialization function. Inherited from the CPPUnit framework.
    void setUp()
    {
    }


    //! @brief Verify the Clock::NextTickTime() function returns correct value when
    //! the frequency of the clock is changed from time to time.
    //!
    //! Create an object and register a tick() function (rising edge) to a clock.
    //! After a random number of ticks, change the frequency of the clock. Verify
    //! Clock::NextTickTime() behaves as expected.
    void test_set_frequency_0()
    {

        DVFSClock clk(1);


	MyObj1* comp1 = new MyObj1(clk);

	//register with clock
	Clock::Register<MyObj1>(clk, comp1, &MyObj1::risingTick, &MyObj1::fallingTick);

        Manifold::StopAtClock(200, clk);
	Manifold::Run();

	std::list<std::pair<Ticks_t, Time_t> >& tick_time_pairs = comp1->get_tick_time_pairs();
	std::list<std::pair<int, int> >& cycle_freq_pairs = comp1->get_cycle_freq_pairs();

#if 0
	for(std::list<std::pair<Ticks_t, Time_t> >::iterator it = tick_time_pairs.begin(); it != tick_time_pairs.end(); ++it) {
	    cout << "tick= " << (*it).first << "  time= " << (*it).second << endl;
	}

	for(std::list<std::pair<int, int> >::iterator it = cycle_freq_pairs.begin(); it != cycle_freq_pairs.end(); ++it) {
	    cout << "cycle= " << (*it).first << "  freq= " << (*it).second << endl;
	}
#endif

	//verification
	//note that the first of each pair in the cycle_freq pairs is the tick when freq change happened.
	std::list<std::pair<int, int> >::iterator cf_it = cycle_freq_pairs.begin();
	int cur_freq = (*cf_it).second;

	double current_time = 0;
	for(std::list<std::pair<Ticks_t, Time_t> >::iterator it = tick_time_pairs.begin(); it != tick_time_pairs.end(); ++it) {
	    Ticks_t tick = (*it).first;
	    Time_t time = (*it).second;

	    CPPUNIT_ASSERT_DOUBLES_EQUAL(current_time, time, 1.0E-6);


	    if(tick >= (*cf_it).first) {
	        ++cf_it;
		cur_freq = (*cf_it).second;
	    }
	    current_time += 1.0/cur_freq;
	}

    }


    /**
     * Build a test suite.
     */
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("ClockTest");

	mySuite->addTest(new CppUnit::TestCaller<DVFSTest>("test_set_frequency_0", &DVFSTest::test_set_frequency_0));

	return mySuite;
    }
};



int main()
{
    Manifold::Init();

    CppUnit::TextUi::TestRunner runner;
    runner.addTest( DVFSTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;
}



