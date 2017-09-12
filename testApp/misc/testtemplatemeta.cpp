/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
/* Author:  Michael Davidsaver */

#include <typeinfo>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <pv/templateMeta.h>

namespace meta = epics::pvData::meta;

MAIN(testtemplatemeta)
{
    testPlan(6);

    testOk1(typeid(char)==typeid(meta::strip_const<char>::type));
    testOk1(typeid(char)==typeid(meta::strip_const<const char>::type));

    testOk1(typeid(char*)==typeid(meta::strip_const<char*>::type));
    testOk1(typeid(const char*)==typeid(meta::strip_const<const char*>::type));

    testOk1(typeid(char*)==typeid(meta::strip_const<char* const >::type));
    testOk1(typeid(const char*)==typeid(meta::strip_const<const char* const>::type));

    return testDone();
}
