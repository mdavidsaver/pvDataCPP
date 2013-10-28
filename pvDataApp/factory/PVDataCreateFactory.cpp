/*PVDataCreateFactory.cpp*/
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 *  @author mrk
 */
#ifdef _WIN32
#define NOMINMAX
#endif
#include <cstddef>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <pv/lock.h>
#include <pv/pvIntrospect.h>
#include <pv/pvData.h>
#include <pv/convert.h>
#include <pv/factory.h>
#include <pv/serializeHelper.h>

using std::tr1::static_pointer_cast;
using std::size_t;
using std::min;

namespace epics { namespace pvData {


template<> const ScalarType PVBoolean::typeCode = pvBoolean;
template<> const ScalarType PVByte::typeCode = pvByte;
template<> const ScalarType PVShort::typeCode = pvShort;
template<> const ScalarType PVInt::typeCode = pvInt;
template<> const ScalarType PVLong::typeCode = pvLong;
template<> const ScalarType PVUByte::typeCode = pvUByte;
template<> const ScalarType PVUShort::typeCode = pvUShort;
template<> const ScalarType PVUInt::typeCode = pvUInt;
template<> const ScalarType PVULong::typeCode = pvULong;
template<> const ScalarType PVFloat::typeCode = pvFloat;
template<> const ScalarType PVDouble::typeCode = pvDouble;
template<> const ScalarType PVScalarValue<String>::typeCode = pvString;

template<> const ScalarType PVBooleanArray::typeCode = pvBoolean;
template<> const ScalarType PVByteArray::typeCode = pvByte;
template<> const ScalarType PVShortArray::typeCode = pvShort;
template<> const ScalarType PVIntArray::typeCode = pvInt;
template<> const ScalarType PVLongArray::typeCode = pvLong;
template<> const ScalarType PVUByteArray::typeCode = pvUByte;
template<> const ScalarType PVUShortArray::typeCode = pvUShort;
template<> const ScalarType PVUIntArray::typeCode = pvUInt;
template<> const ScalarType PVULongArray::typeCode = pvULong;
template<> const ScalarType PVFloatArray::typeCode = pvFloat;
template<> const ScalarType PVDoubleArray::typeCode = pvDouble;
template<> const ScalarType PVStringArray::typeCode = pvString;

/** Default storage for scalar values
 */
template<typename T>
class BasePVScalar : public PVScalarValue<T> {
public:
    typedef T  value_type;
    typedef T* pointer;
    typedef const T* const_pointer;

    BasePVScalar(ScalarConstPtr const & scalar);
    virtual ~BasePVScalar();
    virtual T get() const ;
    virtual void put(T val);
    virtual void serialize(ByteBuffer *pbuffer,
        SerializableControl *pflusher) const;
    virtual void deserialize(ByteBuffer *pbuffer,
        DeserializableControl *pflusher);
private:
    T value;
};

template<typename T>
BasePVScalar<T>::BasePVScalar(ScalarConstPtr const & scalar)
    : PVScalarValue<T>(scalar),value(0)
{}
//Note: '0' is a suitable default for all POD types (not String)

template<typename T>
BasePVScalar<T>::~BasePVScalar() {}

template<typename T>
T BasePVScalar<T>::get() const  { return value;}

template<typename T>
void BasePVScalar<T>::put(T val)
{
    value = val;
    PVField::postPut();
}

template<typename T>
void BasePVScalar<T>::serialize(ByteBuffer *pbuffer,
    SerializableControl *pflusher) const {
    pflusher->ensureBuffer(sizeof(T));
    pbuffer->put(value);
}

template<typename T>
void BasePVScalar<T>::deserialize(ByteBuffer *pbuffer,
    DeserializableControl *pflusher)
{
    pflusher->ensureData(sizeof(T));
    value = pbuffer->GET(T);
}

typedef BasePVScalar<boolean> BasePVBoolean;
typedef BasePVScalar<int8> BasePVByte;
typedef BasePVScalar<int16> BasePVShort;
typedef BasePVScalar<int32> BasePVInt;
typedef BasePVScalar<int64> BasePVLong;
typedef BasePVScalar<uint8> BasePVUByte;
typedef BasePVScalar<uint16> BasePVUShort;
typedef BasePVScalar<uint32> BasePVUInt;
typedef BasePVScalar<uint64> BasePVULong;
typedef BasePVScalar<float> BasePVFloat;
typedef BasePVScalar<double> BasePVDouble;

// BasePVString is special case, since it implements SerializableArray
class BasePVString : public PVString {
public:
    typedef String value_type;
    typedef String* pointer;
    typedef const String* const_pointer;

    BasePVString(ScalarConstPtr const & scalar);
    virtual ~BasePVString();
    virtual String get() const ;
    virtual void put(String val);
    virtual void serialize(ByteBuffer *pbuffer,
        SerializableControl *pflusher) const;
    virtual void deserialize(ByteBuffer *pbuffer,
        DeserializableControl *pflusher);
    virtual void serialize(ByteBuffer *pbuffer,
        SerializableControl *pflusher, size_t offset, size_t count) const;
private:
    String value;
};

BasePVString::BasePVString(ScalarConstPtr const & scalar)
    : PVString(scalar),value()
{}

BasePVString::~BasePVString() {}

String BasePVString::get() const  { return value;}

void BasePVString::put(String val)
{
    value = val;
    postPut();
}

void BasePVString::serialize(ByteBuffer *pbuffer,
    SerializableControl *pflusher) const
{
    SerializeHelper::serializeString(value, pbuffer, pflusher);
}

void BasePVString::deserialize(ByteBuffer *pbuffer,
    DeserializableControl *pflusher)
{
    value = SerializeHelper::deserializeString(pbuffer, pflusher);
}

void BasePVString::serialize(ByteBuffer *pbuffer,
    SerializableControl *pflusher, size_t offset, size_t count) const
{
	// check bounds
	const size_t length = /*(value == null) ? 0 :*/ value.length();
	/*if (offset < 0) offset = 0;
	else*/ if (offset > length) offset = length;
	//if (count < 0) count = length;

	const size_t maxCount = length - offset;
	if (count > maxCount)
		count = maxCount;
	
	// write
	SerializeHelper::serializeSubstring(value, offset, count, pbuffer, pflusher);
}

/** Default storage for arrays
 */
template<typename T>
class DefaultPVArray : public PVValueArray<T> {
public:
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef std::vector<T> vector;
    typedef const std::vector<T> const_vector;
    typedef std::tr1::shared_ptr<vector> shared_vector;

    typedef ::epics::pvData::shared_vector<T> svector;
    typedef ::epics::pvData::shared_vector<const T> const_svector;

    DefaultPVArray(ScalarArrayConstPtr const & scalarArray);
    virtual ~DefaultPVArray();

    virtual size_t getLength() const {return value.size();}
    virtual size_t getCapacity() const {return value.capacity();}

    virtual void setCapacity(size_t capacity);
    virtual void setLength(size_t length);

    virtual const_svector view() const {return value;}
    virtual void swap(const_svector &other);
    virtual void replace(const const_svector& next);

    // from Serializable
    virtual void serialize(ByteBuffer *pbuffer,SerializableControl *pflusher) const;
    virtual void deserialize(ByteBuffer *pbuffer,DeserializableControl *pflusher);
    virtual void serialize(ByteBuffer *pbuffer,
         SerializableControl *pflusher, size_t offset, size_t count) const;
private:
    const_svector value;
};

template<typename T>
DefaultPVArray<T>::DefaultPVArray(ScalarArrayConstPtr const & scalarArray)
: PVValueArray<T>(scalarArray),
    value()
  
{ }

template<typename T>
DefaultPVArray<T>::~DefaultPVArray()
{ }
template<typename T>
void DefaultPVArray<T>::setCapacity(size_t capacity)
{
    if(this->isCapacityMutable()) {
        value.reserve(capacity);
    }
}

template<typename T>
void DefaultPVArray<T>::setLength(size_t length)
{
    if(this->isImmutable())
        THROW_EXCEPTION2(std::logic_error,"Immutable");
    if(length == value.size())
        return;
    else if(length < value.size())
        value.slice(0, length);
    else
        value.resize(length);
}

template<typename T>
void DefaultPVArray<T>::replace(const const_svector& next)
{
    value = next;
    this->postPut();
}

template<typename T>
void DefaultPVArray<T>::swap(const_svector &other)
{
    if(this->isImmutable())
        THROW_EXCEPTION2(std::logic_error,"Immutable");

    value.swap(other);
}


template<typename T>
void DefaultPVArray<T>::serialize(ByteBuffer *pbuffer,
            SerializableControl *pflusher) const {
    serialize(pbuffer, pflusher, 0, this->getLength());
}

template<typename T>
void DefaultPVArray<T>::deserialize(ByteBuffer *pbuffer,
        DeserializableControl *pcontrol) {
    size_t size = SerializeHelper::readSize(pbuffer, pcontrol);

    svector nextvalue(thaw(value));
    nextvalue.resize(size); // TODO: avoid copy of stuff we will then overwrite

    T* cur = nextvalue.data();

    // try to avoid deserializing from the buffer
    // this is only possible if we do not need to do endian-swapping
    if (!pbuffer->reverse<T>())
        if (pcontrol->directDeserialize(pbuffer, (char*)cur, size, sizeof(T)))
        {
        // inform about the change?
        PVField::postPut();
        return;
    }

    // retrieve value from the buffer
    size_t remaining = size;
    while(remaining) {
        const size_t have_bytes = pbuffer->getRemaining();

        // correctly rounds down in an element is partially received
        const size_t available = have_bytes/sizeof(T);

        if(available == 0) {
            size_t want = sizeof(T);
            if(remaining==1 && sizeof(T)>1) {
                // Need to wait for the last few bytes
                // of the final element.
                // available==0 implies have_bytes<sizeof(T)
                want = sizeof(T) - have_bytes;
            }
            // recv() at least one element, or remaining buffer
            pcontrol->ensureData(want);
            continue;
        }

        const size_t n2read = std::min(remaining, available);

        pbuffer->getArray(cur, n2read);
        cur += n2read;
        remaining -= n2read;
    }
    value = freeze(nextvalue);
    // inform about the change?
    PVField::postPut();
}

template<typename T>
void DefaultPVArray<T>::serialize(ByteBuffer *pbuffer,
        SerializableControl *pflusher, size_t offset, size_t count) const
{
    //TODO: avoid incrementing the ref counter...
    const_svector temp(value);
    temp.slice(offset, count);
    count = temp.size();

    SerializeHelper::writeSize(temp.size(), pbuffer, pflusher);

    const T* cur = temp.data();

    // try to avoid copying into the buffer
    // this is only possible if we do not need to do endian-swapping
    if (!pbuffer->reverse<T>())
        if (pflusher->directSerialize(pbuffer, (const char*)cur, count, sizeof(T)))
            return;

    while(count) {
        const size_t empty = pbuffer->getRemaining();
        const size_t space_for = empty/sizeof(T);

        if(space_for==0) {
            pflusher->flushSerializeBuffer();
            // Can we be certain that more space is now free???
            // If not then we spinnnnnnnnn
            continue;
        }

        const size_t n2send = std::min(count, space_for);

        pbuffer->putArray(cur, n2send);
        cur += n2send;
        count -= n2send;
    }

    pflusher->flushSerializeBuffer();
}

// specializations for String

template<>
void DefaultPVArray<String>::deserialize(ByteBuffer *pbuffer,
        DeserializableControl *pcontrol) {
    size_t size = SerializeHelper::readSize(pbuffer, pcontrol);

    svector nextvalue(thaw(value));

    // Decide if we must re-allocate
    if(size > nextvalue.size() || !nextvalue.unique())
        nextvalue.resize(size);
    else if(size < nextvalue.size())
        nextvalue.slice(0, size);


    String * pvalue = nextvalue.data();
    for(size_t i = 0; i<size; i++) {
        pvalue[i] = SerializeHelper::deserializeString(pbuffer,
                                                       pcontrol);
    }
    value = freeze(nextvalue);
    // inform about the change?
    postPut();
}

template<>
void DefaultPVArray<String>::serialize(ByteBuffer *pbuffer,
        SerializableControl *pflusher, size_t offset, size_t count) const {

    const_svector temp(value);
    temp.slice(offset, count);

    SerializeHelper::writeSize(temp.size(), pbuffer, pflusher);

    const String * pvalue = temp.data();
    for(size_t i = 0; i<temp.size(); i++) {
        SerializeHelper::serializeString(pvalue[i], pbuffer, pflusher);
    }
}

typedef DefaultPVArray<boolean> DefaultPVBooleanArray;
typedef DefaultPVArray<int8> BasePVByteArray;
typedef DefaultPVArray<int16> BasePVShortArray;
typedef DefaultPVArray<int32> BasePVIntArray;
typedef DefaultPVArray<int64> BasePVLongArray;
typedef DefaultPVArray<uint8> BasePVUByteArray;
typedef DefaultPVArray<uint16> BasePVUShortArray;
typedef DefaultPVArray<uint32> BasePVUIntArray;
typedef DefaultPVArray<uint64> BasePVULongArray;
typedef DefaultPVArray<float> BasePVFloatArray;
typedef DefaultPVArray<double> BasePVDoubleArray;
typedef DefaultPVArray<String> BasePVStringArray;

// Factory

PVDataCreate::PVDataCreate()
: fieldCreate(getFieldCreate())
{ }

PVFieldPtr PVDataCreate::createPVField(FieldConstPtr const & field)
{
     switch(field->getType()) {
     case scalar: {
         ScalarConstPtr xx = static_pointer_cast<const Scalar>(field);
         return createPVScalar(xx);
     }
     case scalarArray: {
         ScalarArrayConstPtr xx = static_pointer_cast<const ScalarArray>(field);
         return createPVScalarArray(xx);
     }
     case structure: {
         StructureConstPtr xx = static_pointer_cast<const Structure>(field);
         return createPVStructure(xx);
     }
     case structureArray: {
         StructureArrayConstPtr xx = static_pointer_cast<const StructureArray>(field);
         return createPVStructureArray(xx);
     }
     }
     throw std::logic_error("PVDataCreate::createPVField should never get here");
}

PVFieldPtr PVDataCreate::createPVField(PVFieldPtr const & fieldToClone)
{
     switch(fieldToClone->getField()->getType()) {
     case scalar:
        {
            PVScalarPtr pvScalar = static_pointer_cast<PVScalar>(fieldToClone);
            return createPVScalar(pvScalar);
        }
     case scalarArray:
        {
             PVScalarArrayPtr pvScalarArray
                 = static_pointer_cast<PVScalarArray>(fieldToClone);
             return createPVScalarArray(pvScalarArray);
        }
     case structure:
         {
             PVStructurePtr pvStructure
                   = static_pointer_cast<PVStructure>(fieldToClone);
             StringArray const & fieldNames = pvStructure->getStructure()->getFieldNames();
             PVFieldPtrArray pvFieldPtrArray = pvStructure->getPVFields();
             return createPVStructure(fieldNames,pvFieldPtrArray);
         }
     case structureArray:
         {
             PVStructureArrayPtr from
                 = static_pointer_cast<PVStructureArray>(fieldToClone);
             StructureArrayConstPtr structureArray = from->getStructureArray();
             PVStructureArrayPtr to = createPVStructureArray(
                 structureArray);
             getConvert()->copyStructureArray(from, to);
             return to;
         }
     }
     throw std::logic_error("PVDataCreate::createPVField should never get here");
}

PVScalarPtr PVDataCreate::createPVScalar(ScalarConstPtr const & scalar)
{
     ScalarType scalarType = scalar->getScalarType();
     switch(scalarType) {
     case pvBoolean:
         return PVScalarPtr(new BasePVBoolean(scalar));
     case pvByte:
         return PVScalarPtr(new BasePVByte(scalar));
     case pvShort:
         return PVScalarPtr(new BasePVShort(scalar));
     case pvInt:
         return PVScalarPtr(new BasePVInt(scalar));
     case pvLong:
         return PVScalarPtr(new BasePVLong(scalar));
     case pvUByte:
         return PVScalarPtr(new BasePVUByte(scalar));
     case pvUShort:
         return PVScalarPtr(new BasePVUShort(scalar));
     case pvUInt:
         return PVScalarPtr(new BasePVUInt(scalar));
     case pvULong:
         return PVScalarPtr(new BasePVULong(scalar));
     case pvFloat:
         return PVScalarPtr(new BasePVFloat(scalar));
     case pvDouble:
         return PVScalarPtr(new BasePVDouble(scalar));
     case pvString:
         return PVScalarPtr(new BasePVString(scalar));
     }
     throw std::logic_error("PVDataCreate::createPVScalar should never get here");
}

PVScalarPtr PVDataCreate::createPVScalar(ScalarType scalarType)
{
     ScalarConstPtr scalar = fieldCreate->createScalar(scalarType);
     return createPVScalar(scalar);
}


PVScalarPtr PVDataCreate::createPVScalar(PVScalarPtr const & scalarToClone)
{
     ScalarType scalarType = scalarToClone->getScalar()->getScalarType();
     PVScalarPtr pvScalar = createPVScalar(scalarType);
     getConvert()->copyScalar(scalarToClone, pvScalar);
     PVAuxInfoPtr from = scalarToClone->getPVAuxInfo();
     PVAuxInfoPtr to = pvScalar->getPVAuxInfo();
     PVAuxInfo::PVInfoMap & map = from->getInfoMap();
     for(PVAuxInfo::PVInfoIter iter = map.begin(); iter!= map.end(); ++iter) {
         String key = iter->first;
         PVScalarPtr pvFrom = iter->second;
         ScalarConstPtr scalar = pvFrom->getScalar();
         PVScalarPtr pvTo = to->createInfo(key,scalar->getScalarType());
         getConvert()->copyScalar(pvFrom,pvTo);
     }
     return pvScalar;
}

PVScalarArrayPtr PVDataCreate::createPVScalarArray(
        ScalarArrayConstPtr const & scalarArray)
{
     switch(scalarArray->getElementType()) {
     case pvBoolean:
           return PVScalarArrayPtr(new DefaultPVBooleanArray(scalarArray));
     case pvByte:
           return PVScalarArrayPtr(new BasePVByteArray(scalarArray));
     case pvShort:
           return PVScalarArrayPtr(new BasePVShortArray(scalarArray));
     case pvInt:
           return PVScalarArrayPtr(new BasePVIntArray(scalarArray));
     case pvLong:
           return PVScalarArrayPtr(new BasePVLongArray(scalarArray));
     case pvUByte:
           return PVScalarArrayPtr(new BasePVUByteArray(scalarArray));
     case pvUShort:
           return PVScalarArrayPtr(new BasePVUShortArray(scalarArray));
     case pvUInt:
           return PVScalarArrayPtr(new BasePVUIntArray(scalarArray));
     case pvULong:
           return PVScalarArrayPtr(new BasePVULongArray(scalarArray));
     case pvFloat:
           return PVScalarArrayPtr(new BasePVFloatArray(scalarArray));
     case pvDouble:
           return PVScalarArrayPtr(new BasePVDoubleArray(scalarArray));
     case pvString:
           return PVScalarArrayPtr(new BasePVStringArray(scalarArray));
     }
     throw std::logic_error("PVDataCreate::createPVScalarArray should never get here");
     
}

PVScalarArrayPtr PVDataCreate::createPVScalarArray(
        ScalarType elementType)
{
     ScalarArrayConstPtr scalarArray = fieldCreate->createScalarArray(elementType);
     return createPVScalarArray(scalarArray);
}

PVScalarArrayPtr PVDataCreate::createPVScalarArray(
        PVScalarArrayPtr const & arrayToClone)
{
     PVScalarArrayPtr pvArray = createPVScalarArray(
          arrayToClone->getScalarArray()->getElementType());
     pvArray->assign(*arrayToClone.get());
     PVAuxInfoPtr from = arrayToClone->getPVAuxInfo();
     PVAuxInfoPtr to = pvArray->getPVAuxInfo();
     PVAuxInfo::PVInfoMap & map = from->getInfoMap();
     for(PVAuxInfo::PVInfoIter iter = map.begin(); iter!= map.end(); ++iter) {
         String key = iter->first;
         PVScalarPtr pvFrom = iter->second;
         ScalarConstPtr scalar = pvFrom->getScalar();
         PVScalarPtr pvTo = to->createInfo(key,scalar->getScalarType());
         getConvert()->copyScalar(pvFrom,pvTo);
     }
    return pvArray;
}

PVStructureArrayPtr PVDataCreate::createPVStructureArray(
        StructureArrayConstPtr const & structureArray)
{
     return PVStructureArrayPtr(new PVStructureArray(structureArray));
}

PVStructurePtr PVDataCreate::createPVStructure(
        StructureConstPtr const & structure)
{
     return PVStructurePtr(new PVStructure(structure));
}

PVStructurePtr PVDataCreate::createPVStructure(
        StringArray const & fieldNames,PVFieldPtrArray const & pvFields)
{
     size_t num = fieldNames.size();
     FieldConstPtrArray fields(num);
     for (size_t i=0;i<num;i++) fields[i] = pvFields[i]->getField();
     StructureConstPtr structure = fieldCreate->createStructure(fieldNames,fields);
     PVStructurePtr pvStructure(new PVStructure(structure,pvFields));
     return pvStructure;
}

PVStructurePtr PVDataCreate::createPVStructure(PVStructurePtr const & structToClone)
{
    FieldConstPtrArray field;
    if(structToClone==0) {
        FieldConstPtrArray fields(0);
        StringArray fieldNames(0);
        StructureConstPtr structure = fieldCreate->createStructure(fieldNames,fields);
        return PVStructurePtr(new PVStructure(structure));
    }
    StructureConstPtr structure = structToClone->getStructure();
    PVStructurePtr pvStructure(new PVStructure(structure));
    getConvert()->copyStructure(structToClone,pvStructure);
    return pvStructure;
}

PVDataCreatePtr PVDataCreate::getPVDataCreate()
{
    static PVDataCreatePtr pvDataCreate;
    static Mutex mutex;
    Lock xx(mutex);

    if(pvDataCreate.get()==0) pvDataCreate = PVDataCreatePtr(new PVDataCreate());
    return pvDataCreate;
}

PVDataCreatePtr getPVDataCreate() {
     return PVDataCreate::getPVDataCreate();
}

}}
