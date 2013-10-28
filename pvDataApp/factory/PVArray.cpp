/*PVArray.cpp*/
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 *  @author mrk
 */

#include <cstddef>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <pv/pvData.h>
#include <pv/factory.h>

using std::size_t;

namespace epics { namespace pvData {

PVArray::PVArray(FieldConstPtr const & field)
: PVField(field),capacityMutable(true)
{ }

 void PVArray::setImmutable()
 {
     capacityMutable = false;
     PVField::setImmutable();
 } 

 bool PVArray::isCapacityMutable() const
 {
      if(PVField::isImmutable()) {
          return false;
      }
      return capacityMutable;
 }

 void PVArray::setCapacityMutable(bool isMutable)
 {
    if(isMutable && PVField::isImmutable()) {
       throw std::runtime_error("field is immutable");
    }
    capacityMutable = isMutable;
 }


std::ostream& operator<<(format::array_at_internal const& manip, const PVArray& array)
{
	return array.dumpValue(manip.stream, manip.index);
}

}}
