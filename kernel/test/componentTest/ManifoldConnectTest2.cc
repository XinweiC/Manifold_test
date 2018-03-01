/**
This program tests the Connect() functions of the Manifold class.
*/
#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <stdlib.h>
#include <sys/time.h>

#include "manifold.h"
#include "component.h"
#include "link.h"

using namespace std;
using namespace manifold::kernel;


//####################################################################
// helper classes, functions, and data
//####################################################################
class MyDataType {
};


class MyC0 : public Component 
{
public:
    void handler(int, MyDataType)
    {}
};


//####################################################################
// ManifoldTest is the unit test class for the class Manifold.
//####################################################################
class ManifoldTest : public CppUnit::TestFixture {
private:
    static Clock MasterClock;  //clock has to be global or static.

    enum { MASTER_CLOCK_HZ = 10 };

public:
    void setUp()
    {}


	 //! @brief Test Connect
	 //!
	 //! Verify predcessors and successors are correctly set up.
	 void testConnect_0()
	 {
	    Manifold::Reset(Manifold::TICKED, SyncAlg::SA_CMB);

	    int Mytid;
	    MPI_Comm_rank(MPI_COMM_WORLD, &Mytid);
	    int Size;
	    MPI_Comm_size(MPI_COMM_WORLD, &Size);

	    //all processes must use same seed
	    srandom(Size*2);


	    std::vector<CompId_t> cids(Size);

            for(int i=0; i<Size; i++) {
		cids[i] = Component::Create<MyC0>(i);
	    }

	    std::vector<std::list<int> > successors(Size);
	    std::vector<std::list<int> > predcessors(Size);

            for(int i=0; i<Size; i++) {
	        //connect each component randomly to other components
	        double threshold = random() / (RAND_MAX + 1.0); // if this is x, then component i connects to 100x% of the
		                                                // other components
		//for each component randomly pick components to connect to
		for(int j=0; j<Size; j++) {
		    if(j != i && random() / (RAND_MAX + 1.0) < threshold) {
			Manifold::Connect(cids[i], j, cids[j], i, &MyC0::handler, 1);
			successors[i].push_back(j);
			predcessors[j].push_back(i);
		    }
		}
	    }

	    //verify successors and predecessors
	    std::vector<LpId_t>& preds = Manifold::get_scheduler()->get_predecessors();
	    std::vector<LpId_t>& succs = Manifold::get_scheduler()->get_successors();


	    CPPUNIT_ASSERT_EQUAL(successors[Mytid].size(), succs.size());

	    for(int i=0; i<succs.size(); i++) {
	        bool found  = false;
		for(std::list<int>::iterator it = successors[Mytid].begin(); it != successors[Mytid].end(); ++it) {
		    if(*it == succs[i]) {
		        found = true;
			break;
		    }
		}
		CPPUNIT_ASSERT_EQUAL(true, found);
	    }

	    CPPUNIT_ASSERT_EQUAL(predcessors[Mytid].size(), preds.size());

	    for(int i=0; i<preds.size(); i++) {
	        bool found  = false;
		for(std::list<int>::iterator it = predcessors[Mytid].begin(); it != predcessors[Mytid].end(); ++it) {
		    if(*it == preds[i]) {
		        found = true;
			break;
		    }
		}
		CPPUNIT_ASSERT_EQUAL(true, found);
	    }


	 }


	 void testConnectHalf_0()
	 {
	 }


	 void testConnectClock_0()
	 {
	 }

	 void testConnectClockHalf_0()
	 {
	 }

	 void testConnectTime_0()
	 {
	 }




        /**
	 * Build a test suite.
	 */
	static CppUnit::Test* suite()
	{
	    CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("ManifoldTest");



	    mySuite->addTest(new CppUnit::TestCaller<ManifoldTest>("testConnect_0", &ManifoldTest::testConnect_0));
	    /*
	    mySuite->addTest(new CppUnit::TestCaller<ManifoldTest>("testConnectHalf_0", &ManifoldTest::testConnectHalf_0));
	    mySuite->addTest(new CppUnit::TestCaller<ManifoldTest>("testConnectClock_0", &ManifoldTest::testConnectClock_0));
	    mySuite->addTest(new CppUnit::TestCaller<ManifoldTest>("testConnectClockHalf_0", &ManifoldTest::testConnectClockHalf_0));
	    mySuite->addTest(new CppUnit::TestCaller<ManifoldTest>("testConnectTime_0", &ManifoldTest::testConnectTime_0));
	    */

	    return mySuite;
	}
};

Clock ManifoldTest::MasterClock(MASTER_CLOCK_HZ);



int main(int argc, char** argv)
{
    Manifold :: Init(argc, argv);
    if(2 > TheMessenger.get_node_size()) {
        cerr << "ERROR: Must specify \"-np N (N>1)\" for mpirun!" << endl;
	return 1;
    }

    CppUnit::TextUi::TestRunner runner;
    runner.addTest( ManifoldTest::suite() );
    bool rc = runner.run("", false);

    Manifold :: Finalize();

    if(rc)
	return 0; //all is well
    else
	return 1;
}

