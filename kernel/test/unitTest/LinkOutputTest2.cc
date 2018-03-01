/**
 * This program test misc functions related to LinkOutput
 */
#include <TestFixture.h>
#include <TestAssert.h>
#include <TestSuite.h>
#include <Test.h>
#include <TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include "link-decl.h"
#include "link.h"
#include "manifold.h"
#include "clock.h"

using namespace manifold::kernel;

//####################################################################
// helper classes
//####################################################################
class MyDataType1 {
    public:
        int x;
};

class MyDataType2 {
    public:
        enum { SZ_BY_REF = 123, SZ_BY_CONST_P = 456};
        //static const int SZ_BY_CONST_P = 456;
        int x;
};

namespace manifold {
namespace kernel {
template<>
size_t Get_serialize_size<MyDataType2>(const MyDataType2& d)
{
    return MyDataType2 :: SZ_BY_REF;
}

template<>
size_t Get_serialize_size<MyDataType2>(const MyDataType2* d)
{
    return MyDataType2 :: SZ_BY_CONST_P;
}

}
}

//####################################################################
// Class LinkOutputTest is the unit test class for LinkOutput. 
//####################################################################
class LinkOutputTest : public CppUnit::TestFixture {

    public:
        /**
	 * Initialization function. Inherited from the CPPUnit framework.
	 */
        void setUp()
	{
	}


        /**
	 * Test get_serialize_size_internal(const T&)
	 */
	void testGet_serialize_size_internal_0()
	{
	    MyDataType1 d1;
	    CPPUNIT_ASSERT_EQUAL(sizeof(MyDataType1), get_serialize_size_internal(d1));
	}

        /**
	 * Test get_serialize_size_internal(const T&)
	 */
	void testGet_serialize_size_internal_1()
	{
	    MyDataType1 d1;
	    const MyDataType1 d2 = d1;  //const data
	    CPPUNIT_ASSERT_EQUAL(sizeof(MyDataType1), get_serialize_size_internal(d2));
	}

        /**
	 * Test get_serialize_size_internal(T*)
	 */
	void testGet_serialize_size_internal_2()
	{
	    MyDataType1* d1 = new MyDataType1;
	    CPPUNIT_ASSERT_EQUAL(sizeof(MyDataType1), get_serialize_size_internal(d1));
	}

        /**
	 * Test get_serialize_size_internal(const T*)
	 */
	void testGet_serialize_size_internal_3()
	{
	    const MyDataType1* d1 = new MyDataType1;  //const pointer
	    CPPUNIT_ASSERT_EQUAL(sizeof(MyDataType1), get_serialize_size_internal(d1));
	}

        /**
	 * Test get_serialize_size_internal(const T&) with data type specializing Get_serialize_size()
	 */
	void testGet_serialize_size_internal_s0()
	{
	    MyDataType2 d1;
	    CPPUNIT_ASSERT_EQUAL((int)MyDataType2::SZ_BY_REF, (int)get_serialize_size_internal(d1));
	}

        /**
	 * Test get_serialize_size_internal(const T&) with data type specializing Get_serialize_size()
	 */
	void testGet_serialize_size_internal_s1()
	{
	    MyDataType2 d1;
	    const MyDataType2 d2 = d1;  //const data
	    CPPUNIT_ASSERT_EQUAL((int)MyDataType2::SZ_BY_REF, (int)get_serialize_size_internal(d2));
	}

        /**
	 * Test get_serialize_size_internal(T*) with data type specializing Get_serialize_size()
	 */
	void testGet_serialize_size_internal_s2()
	{
	    MyDataType2* d1 = new MyDataType2;
	    CPPUNIT_ASSERT_EQUAL((int)MyDataType2::SZ_BY_CONST_P, (int)get_serialize_size_internal(d1));
	}

        /**
	 * Test get_serialize_size_internal(T*) with data type specializing Get_serialize_size()
	 */
	void testGet_serialize_size_internal_s3()
	{
	    const MyDataType2* d1 = new MyDataType2;  //const pointer
	    CPPUNIT_ASSERT_EQUAL((int)MyDataType2::SZ_BY_CONST_P, (int)get_serialize_size_internal(d1));
	}



        /**
	 * Build a test suite.
	 */
	static CppUnit::Test* suite()
	{
	    CppUnit::TestSuite* mySuite = new CppUnit::TestSuite("LinkOutputTest");

	    mySuite->addTest(new CppUnit::TestCaller<LinkOutputTest>("testGet_serialize_size_internal_0", &LinkOutputTest::testGet_serialize_size_internal_0));
	    mySuite->addTest(new CppUnit::TestCaller<LinkOutputTest>("testGet_serialize_size_internal_1", &LinkOutputTest::testGet_serialize_size_internal_1));
	    mySuite->addTest(new CppUnit::TestCaller<LinkOutputTest>("testGet_serialize_size_internal_2", &LinkOutputTest::testGet_serialize_size_internal_2));
	    mySuite->addTest(new CppUnit::TestCaller<LinkOutputTest>("testGet_serialize_size_internal_3", &LinkOutputTest::testGet_serialize_size_internal_3));
	    mySuite->addTest(new CppUnit::TestCaller<LinkOutputTest>("testGet_serialize_size_internal_s0", &LinkOutputTest::testGet_serialize_size_internal_s0));
	    mySuite->addTest(new CppUnit::TestCaller<LinkOutputTest>("testGet_serialize_size_internal_s1", &LinkOutputTest::testGet_serialize_size_internal_s1));
	    mySuite->addTest(new CppUnit::TestCaller<LinkOutputTest>("testGet_serialize_size_internal_s2", &LinkOutputTest::testGet_serialize_size_internal_s2));
	    mySuite->addTest(new CppUnit::TestCaller<LinkOutputTest>("testGet_serialize_size_internal_s3", &LinkOutputTest::testGet_serialize_size_internal_s3));
	    return mySuite;
	}
};



int main()
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( LinkOutputTest::suite() );
    if(runner.run("", false))
	return 0; //all is well
    else
	return 1;
}



