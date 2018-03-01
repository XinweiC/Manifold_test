#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>
#include <vector>
#include <stdlib.h>
#include "../../data_types/flit.h"
#include "../../components/genericBuffer.h"

#include "kernel/manifold.h"
#include "kernel/component.h"

using namespace manifold::kernel;
using namespace std;
using namespace manifold::iris;

//####################################################################
// helper classes
//####################################################################



//####################################################################
//! Class GenericBufferTest is the test class for class GenericBuffer. 
//####################################################################
class GenericBufferTest : public CppUnit::TestFixture {
private:

public:
    
    //======================================================================
    //======================================================================
    //! @brief Test push()
    //!
    //! Create a GenericBuffer with a certain size; randomly push flits into
    //! the buffer; verify the size of the buffers change correctly.
    void test_push_0()
    {
	const unsigned vcs = random() % 5 + 5; //5 to 10 virtual channels
	const unsigned MAX_SIZE = random() % 90 + 10; //size of each buffer is 10 to 100

        GenericBuffer* gbuffer = new GenericBuffer(vcs, MAX_SIZE);

	unsigned expected_size[vcs];
	for(unsigned i=0; i<vcs; i++)
	    expected_size[i] = 0;

	std::vector<std::deque<Flit*> >& buffers = gbuffer->get_buffers();

	for(unsigned i=0; i<vcs * MAX_SIZE; i++) {
	    unsigned vc = random() % vcs; //randomly select a vc
	    if(expected_size[vc] < MAX_SIZE) {
		Flit* f;
		double rnd = random()/(RAND_MAX+1.0);
		if(rnd < 0.2)
		    f = new HeadFlit();
                else if(rnd < 0.8)
		    f = new BodyFlit();
		else
		    f = new TailFlit();

	        gbuffer->push(vc, f);
		expected_size[vc]++;
		CPPUNIT_ASSERT_EQUAL(expected_size[vc], (unsigned)buffers[vc].size());
	    }
	}
	delete gbuffer;
        
    } 

    


    //======================================================================
    //======================================================================
    //! @brief Test pull()
    //!
    //! Create a GenericBuffer with a certain size; randomly push flits into
    //! the buffer; then randomly pull flits out; verify the size of the buffers
    //! change correctly.
    void test_pull_0()
    {
	const unsigned vcs = random() % 5 + 5; //5 to 10 virtual channels
	const unsigned MAX_SIZE = random() % 90 + 10; //size of each buffer is 10 to 100

        GenericBuffer* gbuffer = new GenericBuffer(vcs, MAX_SIZE);

	unsigned expected_size[vcs];
	for(unsigned i=0; i<vcs; i++)
	    expected_size[i] = 0;

	std::vector<std::deque<Flit*> >& buffers = gbuffer->get_buffers();

	for(unsigned i=0; i<vcs * MAX_SIZE; i++) {
	    unsigned vc = random() % vcs; //randomly select a vc
	    if(expected_size[vc] < MAX_SIZE) {
		Flit* f;
		double rnd = random()/(RAND_MAX+1.0);
		if(rnd < 0.2)
		    f = new HeadFlit();
                else if(rnd < 0.8)
		    f = new BodyFlit();
		else
		    f = new TailFlit();

	        gbuffer->push(vc, f);
		expected_size[vc]++;
		CPPUNIT_ASSERT_EQUAL(expected_size[vc], (unsigned)buffers[vc].size());
	    }
	}

	//now randomly pull flits out
	for(unsigned i=0; i<vcs * MAX_SIZE; i++) {
	    unsigned vc = random() % vcs; //randomly select a vc
	    if(expected_size[vc] > 0) {
		Flit* f = gbuffer->pull(vc);
		expected_size[vc]--;
		CPPUNIT_ASSERT_EQUAL(expected_size[vc], (unsigned)buffers[vc].size());
	    }
	}

	delete gbuffer;
        
    } 

    

    //======================================================================
    //======================================================================
    //! @brief Test pull(): verify the flits pulled out are the ones pushed in.
    //!
    //! Create a GenericBuffer with a certain size; randomly push flits into
    //! the buffer; then randomly pull flits out; verify flits are indeed the
    //! ones pushed in.
    void test_pull_1()
    {
	const unsigned vcs = random() % 5 + 5; //5 to 10 virtual channels
	const unsigned MAX_SIZE = random() % 90 + 10; //size of each buffer is 10 to 100

        GenericBuffer* gbuffer = new GenericBuffer(vcs, MAX_SIZE);

	vector<Flit*> myFlits[vcs];

	for(unsigned i=0; i<vcs * MAX_SIZE; i++) {
	    unsigned vc = random() % vcs; //randomly select a vc
	    if(myFlits[vc].size() < MAX_SIZE) {
		Flit* f;
		double rnd = random()/(RAND_MAX+1.0);
		if(rnd < 0.2)
		    f = new HeadFlit();
                else if(rnd < 0.8)
		    f = new BodyFlit();
		else
		    f = new TailFlit();

	        gbuffer->push(vc, f);
		myFlits[vc].push_back(f);
	    }
	}

	//now randomly pull flits out
	unsigned n_pulled[vcs]; //remember how many have been pulled
	for(unsigned i=0; i<vcs; i++)
	    n_pulled[i] = 0;

	for(unsigned i=0; i<vcs * MAX_SIZE; i++) {
	    unsigned vc = random() % vcs; //randomly select a vc
	    if(n_pulled[vc] < myFlits[vc].size()) {
		Flit* f = gbuffer->pull(vc);
		//verify the flit's addresses match
		CPPUNIT_ASSERT(myFlits[vc][n_pulled[vc]] == f);
		n_pulled[vc]++;
	    }
	}

	for(unsigned i=0; i<vcs; i++)
	    for(unsigned j=0; j<n_pulled[i]; j++)
	        delete myFlits[i][j];

	delete gbuffer;
        
    } 

    
    //======================================================================
    //======================================================================
    //! @brief Test peek()
    //!
    //! Create a GenericBuffer with a certain size; randomly push flits into
    //! the buffer; call peek() as flits are pushed in; then randomly pull flits
    //! out; call peek() as flits are pulled out.
    void test_peek_0()
    {
	const unsigned vcs = random() % 5 + 5; //5 to 10 virtual channels
	const unsigned MAX_SIZE = random() % 90 + 10; //size of each buffer is 10 to 100

        GenericBuffer* gbuffer = new GenericBuffer(vcs, MAX_SIZE);

	vector<Flit*> myFlits[vcs];

	for(unsigned i=0; i<vcs * MAX_SIZE; i++) {
	    unsigned vc = random() % vcs; //randomly select a vc
	    if(myFlits[vc].size() < MAX_SIZE) {
		Flit* f;
		double rnd = random()/(RAND_MAX+1.0);
		if(rnd < 0.2)
		    f = new HeadFlit();
                else if(rnd < 0.8)
		    f = new BodyFlit();
		else
		    f = new TailFlit();

	        gbuffer->push(vc, f);
		myFlits[vc].push_back(f);
		CPPUNIT_ASSERT(gbuffer->peek(vc) == myFlits[vc][0]);
	    }
	}

	//now randomly pull flits out
	unsigned n_pulled[vcs]; //remember how many have been pulled
	for(unsigned i=0; i<vcs; i++)
	    n_pulled[i] = 0;

	for(unsigned i=0; i<vcs * MAX_SIZE; i++) {
	    unsigned vc = random() % vcs; //randomly select a vc
	    if(n_pulled[vc] < myFlits[vc].size()) {
		Flit* f = gbuffer->pull(vc);
		n_pulled[vc]++;
		//After a flit is pulled, peek() would return the next one.
		if(n_pulled[vc] < myFlits[vc].size()) //after the last one of the vc was pulled, nothing to peek at.
		    CPPUNIT_ASSERT(gbuffer->peek(vc) == myFlits[vc][n_pulled[vc]]);
	    }
	}

	for(unsigned i=0; i<vcs; i++)
	    for(unsigned j=0; j<n_pulled[i]; j++)
	        delete myFlits[i][j];

	delete gbuffer;
        
    } 

    

    //! Build a test suite.
    static CppUnit::Test* suite()
    {
	CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("GenericBufferTest");

	mySuite->addTest(new CppUnit::TestCaller<GenericBufferTest>("test_push_0", &GenericBufferTest::test_push_0));
	mySuite->addTest(new CppUnit::TestCaller<GenericBufferTest>("test_pull_0", &GenericBufferTest::test_pull_0));
	mySuite->addTest(new CppUnit::TestCaller<GenericBufferTest>("test_pull_1", &GenericBufferTest::test_pull_1));
	mySuite->addTest(new CppUnit::TestCaller<GenericBufferTest>("test_peek_0", &GenericBufferTest::test_peek_0));
	return mySuite;
    }
};


int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( GenericBufferTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;

}


