/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Author:  Michael Davidsaver */

#include <sstream>

#include <epicsUnitTest.h>
#include <testMain.h>

#include "pv/vectorAlloc.h"

using namespace epics::pvData;

static void testDynamic(void)
{
    testDiag("Test dynamic \"pool\"");
    vector_allocator<int> A(pool_builder().build<int>());

    shared_vector<int> X(A.malloc(16));
    testOk1(X.unique());
    testOk1(X.data()!=NULL);
    testOk1(X.size()==16);

    shared_vector<int> Y(A.calloc(1024));
    testOk1(Y.unique());
    testOk1(Y.data()!=NULL);
    testOk1(Y.size()==1024);
    bool fail=false;
    for(shared_vector<int>::const_iterator it=Y.begin(), end=Y.end();
        it!=end; ++it)
    {
        if(*it) {
            fail = true;
            testDiag("Non-zero element");
        }
    }
    testOk1(!fail);
}

static void testCached(void)
{
    testDiag("Test caching pool");
    vector_allocator<int> A(pool_builder()
                            .name("testCached")
                            .fixed(16)
                            .cached(2)
                            .build<int>());

    testOk1(A.info().fixedsize);
    testOk1(A.info().num_free==1); // initial defaults to 1
    testOk1(A.info().allocsize==sizeof(int)*16);

    try{
        shared_vector<int> X(A.malloc(17));
        testFail("Missed expected exception");
    }catch(std::bad_alloc&){
        testPass("Got expected exception");
    }

    {
        shared_vector<int> X(A.malloc(16));
        testOk1(X.unique());
        testOk1(X.data()!=NULL);
        testOk1(X.size()==16);

        shared_vector<int> Y(A.malloc(8));
        testOk1(Y.unique());
        testOk1(Y.data()!=NULL);
        testOk1(Y.size()==8);

        shared_vector<int> Z(A.malloc(8));
        testOk1(Z.unique());
        testOk1(Z.data()!=NULL);
        testOk1(Z.size()==8);

        testOk1(X.data()!=Y.data());
        testOk1(X.data()!=Z.data());

        allocator_info I(A.info());
        testOk1(I.has_stats);
        testOk1(I.num_allocs==3);
        testOk1(I.num_free==0);
    }

    // of the 3 allocations made above,
    // 2 go into the free list (cache)
    // 1 is free()'d immediately
    allocator_info I(A.info());
    testOk1(I.has_stats);
    testOk1(I.num_allocs==0);
    testOk1(I.num_free==2);
}

static void testCapped(void)
{
    testDiag("Test capped pool");
    vector_allocator<int> A(pool_builder()
                            .name("testCapped")
                            .fixed(16)
                            .capped(2)
                            .build<int>());

    testOk1(A.info().fixedsize);
    testOk1(A.info().num_free==1); // initial defaults to 1
    testOk1(A.info().allocsize==sizeof(int)*16);

    try{
        shared_vector<int> X(A.malloc(17));
        testFail("Missed expected exception");
    }catch(std::bad_alloc&){
        testPass("Got expected exception");
    }

    {
        shared_vector<int> X(A.malloc(16));
        testOk1(X.unique());
        testOk1(X.data()!=NULL);
        testOk1(X.size()==16);

        shared_vector<int> Y(A.malloc(8));
        testOk1(Y.unique());
        testOk1(Y.data()!=NULL);
        testOk1(Y.size()==8);

        try{
            shared_vector<int> Z(A.malloc(8));
            testFail("Missed expected exception");
        }catch(std::bad_alloc&){
            testPass("Got expected exception");
        }

        testOk1(X.data()!=Y.data());

        allocator_info I(A.info());
        testOk1(I.has_stats);
        testOk1(I.num_allocs==2);
        testOk1(I.num_free==0);
    }

    // of the 2 allocations made above,
    // all go into the free list
    allocator_info I(A.info());
    testOk1(I.has_stats);
    testOk1(I.num_allocs==0);
    testOk1(I.num_free==2);
}

static const char expect[] =
        "# Allocator info\n"
        "Name: Default Allocator\n"
        " Size: dynamic\n"
        "Name: capped pool 1\n"
        " Size: 64\n"
        " Alloc: 0 0\n"
        " Free : 1 64\n"
        "Name: cached pool 2\n"
        " Size: 64\n"
        " Alloc: 1 64\n"
        " Free : 2 128\n"
        "Name: <unnamed>\n"
        " Size: 64\n"
        " Alloc: 0 0\n"
        " Free : 1 64\n"
        "# End Allocator info\n";

static void testShowInfo(void)
{
    testDiag("Test pool info printing");

    vector_allocator<int> A(pool_builder()
                            .name("capped pool 1")
                            .fixed(16)
                            .capped(2)
                            .build<int>());

    vector_allocator<int> B(pool_builder()
                            .name("cached pool %d", 2)
                            .fixed(16)
                            .initial(3)
                            .cached(3)
                            .build<int>());

    vector_allocator<int> C(pool_builder()
                            .fixed(16)
                            .cached(2)
                            .build<int>());

    shared_vector<int> X(B.malloc(16));

    std::ostringstream strm;
    print_allocator_info(strm);
    if(strm.str()==expect)
        testPass("Strings match");
    else {
        testFail("Mismatch");
        testDiag("Got:\n%s", strm.str().c_str());
        testDiag("Expect:\n%s", expect);
    }
}

MAIN(testVectorPool)
{
    testPlan(47);
    testDynamic();
    testCached();
    testCapped();
    testShowInfo();
    return testDone();
}
