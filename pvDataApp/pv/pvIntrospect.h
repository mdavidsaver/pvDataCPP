/* pvIntrospect.h */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvDataCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 * Author - Marty Kraimer
 */
#ifndef PVINTROSPECT_H
#define PVINTROSPECT_H
#include <string>
#include <stdexcept>

#include <pv/noDefaultMethods.h>
#include <pv/sharedPtr.h>
#include <pv/pvType.h>
namespace epics { namespace pvData { 

class Field;
class Scalar;
class ScalarArray;
class Structure;
class StructureArray;

/**
 * typedef for a shared pointer to an immutable Field.
 */
typedef std::tr1::shared_ptr<const Field> FieldConstPtr;
/**
 * typedef for an array of shared pointer to an immutable Field.
 */
typedef FieldConstPtr * FieldConstPtrArray;
/**
 * typedef for a shared pointer to an immutable Scalar.
 */
typedef std::tr1::shared_ptr<const Scalar> ScalarConstPtr;
/**
 * typedef for a shared pointer to an immutable ScalarArray.
 */
typedef std::tr1::shared_ptr<const ScalarArray> ScalarArrayConstPtr;
/**
 * typedef for a shared pointer to an immutable Structure.
 */
typedef std::tr1::shared_ptr<const Structure> StructureConstPtr;
/**
 * typedef for a shared pointer to an immutable StructureArray.
 */
typedef std::tr1::shared_ptr<const StructureArray> StructureArrayConstPtr;

/**
 * Definition of support field types.
 */
enum Type {
    /**
     * The type is scalar. It has a scalarType
     */
    scalar,
    /**
     * The type is scalarArray. Each element is a scalar of the same scalarType.
     */
    scalarArray,
    /**
     * The type is structure.
     */
    structure,
    /**
     * The type is structureArray. Each element is a structure.
     */
    structureArray
};

/**
 * Convenience functions for Type.
 */
namespace TypeFunc {
    /**
     * Get a name for the type.
     * @param  type The type.
     * @return The name for the type.
     */
    const char* name(Type type);
    /**
     * Convert the type to a string and add it to builder.
     * @param  builder The string builder.
     * @param  type    The type.
     */
    void toString(StringBuilder builder,const Type type);
};

/**
 * Definition of support scalar types.
 */
enum ScalarType {
    /**
     * The type is boolean, i. e. value can be {@code false} or {@code true}
     */
    pvBoolean,
    /**
     * The type is byte, i. e. a 8 bit signed integer.
     */
    pvByte,
    /**
     * The type is short, i. e. a 16 bit signed integer.
     */
    pvShort,
    /**
     * The type is int, i. e. a 32 bit signed integer.
     */
    pvInt,
    /**
     * The type is long, i. e. a 64 bit signed integer.
     */
    pvLong,
    /**
     * The type is float, i. e. 32 bit IEEE floating point,
     */
    pvFloat,
    /**
     * The type is float, i. e. 64 bit IEEE floating point,
     */
    pvDouble,
    /**
     * The type is string, i. e. a UTF8 character string.
     */
    pvString
};

/**
 * Convenience functions for ScalarType.
 */
namespace ScalarTypeFunc {
    /**
     * Is the type an integer, i. e. is it one of byte,...long
     * @param  scalarType The type.
     * @return (false,true) if the scalarType is an integer.
     */
    bool isInteger(ScalarType scalarType);
    /**
     * Is the type numeric, i. e. is it one of byte,...,double
     * @param  scalarType The type.
     * @return (false,true) if the scalarType is a numeric
     */
    bool isNumeric(ScalarType scalarType);
    /**
     * Is the type primitive, i. e. not string
     * @param  scalarType The type.
     * @return (false,true) if the scalarType is primitive.
     */
    bool isPrimitive(ScalarType scalarType);
    /**
     * Get the scalarType for value.
     * @param  value The name of the scalar type.
     * @return The scalarType.
     * An exception is thrown if the name is not the name of a scalar type.
     */
    ScalarType getScalarType(String value);
    /**
     * Get a name for the scalarType.
     * @param  scalarType The type.
     * @return The name for the scalarType.
     */
    const char* name(ScalarType scalarType);
    /**
     * Convert the scalarType to a string and add it to builder.
     * @param  builder The string builder.
     * @param  scalarType    The type.
     */
    void toString(StringBuilder builder,ScalarType scalarType);
};

/**
 * This class implements introspection object for field.
 */
class Field :  public std::tr1::enable_shared_from_this<Field> {
public:
   POINTER_DEFINITIONS(Field);
    /**
     * Destructor.
     */
   virtual ~Field();
    /**
     * Get the name of the field.
     * @return The field name.
     */
   String getFieldName() const{return m_fieldName;}
    /**
     * Get the field type.
     * @return The type.
     */
   Type getType() const{return m_type;}
    /**
     * Convert the scalarType to a string and add it to builder.
     * @param  builder The string builder.
     */
   virtual void toString(StringBuilder builder) const{toString(builder,0);}
    /**
     * Convert the scalarType to a string and add it to builder.
     * @param  builder The string builder.
     * @param  indentLevel The number of blanks at the beginning of new lines.
     */
   virtual void toString(StringBuilder builder,int indentLevel) const;
    /**
     * Rename the field.
     * @param  newName The new name.
     * This MUST not be called after the field is put into use!!!
     */
   void renameField(String  newName);
protected:
    /**
     * Constructor
     * @param  fieldName The field name.
     * @param  fieldName The field type.
     */
   Field(String fieldName,Type type);
private:
   String m_fieldName;
   Type m_type;

   friend class StructureArray;
   friend class Structure;
   friend class PVFieldPvt;
   friend class StandardField;
   friend class BasePVStructureArray;
   friend class FieldCreate;

   struct Deleter{void operator()(Field *p){delete p;}};
};


/**
 * This class implements introspection object for Scalar.
 */
class Scalar : public Field{
public:
    POINTER_DEFINITIONS(Scalar);
    /**
     * Destructor.
     */
    virtual ~Scalar();
    typedef Scalar& reference;
    typedef const Scalar& const_reference;
    /**
     * Get the scalarType
     * @return the scalarType
     */
    ScalarType getScalarType() const {return scalarType;}
    /**
     * Convert the scalar to a string and add it to builder.
     * @param  builder The string builder.
     */
    virtual void toString(StringBuilder buf) const{toString(buf,0);}
    /**
     * Convert the scalar to a string and add it to builder.
     * @param  builder The string builder.
     * @param  indentLevel The number of blanks at the beginning of new lines.
     */
    virtual void toString(StringBuilder buf,int indentLevel) const;
protected:
    Scalar(String fieldName,ScalarType scalarType);
private:
    ScalarType scalarType;
    friend class FieldCreate;
};

/**
 * This class implements introspection object for field.
 */
class ScalarArray : public Field{
public:
    POINTER_DEFINITIONS(ScalarArray);
    ScalarArray(String fieldName,ScalarType scalarType);
    typedef ScalarArray& reference;
    typedef const ScalarArray& const_reference;

    /**
     * Get the scalarType for the elements.
     * @return the scalarType
     */
    ScalarType  getElementType() const {return elementType;}
    /**
     * Convert the scalarType to a string and add it to builder.
     * @param  builder The string builder.
     */
    virtual void toString(StringBuilder buf) const{toString(buf,0);}
    /**
     * Convert the scalarType to a string and add it to builder.
     * @param  builder The string builder.
     * @param  indentLevel The number of blanks at the beginning of new lines.
     */
    virtual void toString(StringBuilder buf,int indentLevel) const;
protected:
    /**
     * Destructor.
     */
    virtual ~ScalarArray();
private:
    ScalarType elementType;
    friend class FieldCreate;
};

/**
 * This class implements introspection object for a structureArray
 */
class StructureArray : public Field{
public:
    POINTER_DEFINITIONS(StructureArray);
    typedef StructureArray& reference;
    typedef const StructureArray& const_reference;

    const Structure& structure() const {return *pstructure;}
    /**
     * Get the introspection interface for the array elements.
     * @return The introspection interface.
     */
    StructureConstPtr  getStructure() const {return pstructure;}

    /**
     * Convert the scalarType to a string and add it to builder.
     * @param  builder The string builder.
     * @param  indentLevel The number of blanks at the beginning of new lines.
     */
    virtual void toString(StringBuilder buf,int indentLevel=0) const;
protected:
    /**
     * Constructor.
     * @param fieldName The name for the field.
     * @param structure The introspection interface for the elements.
     */
    StructureArray(String fieldName,StructureConstPtr structure);
    /**
     * Destructor.
     */
    virtual ~StructureArray();
private:
    StructureConstPtr pstructure;
    friend class FieldCreate;
};

/**
 * This class implements introspection object for a structure.
 */
class Structure : public Field {
public:
    POINTER_DEFINITIONS(Structure);
    /**
     * Destructor.
     */
    virtual ~Structure();
    typedef Structure& reference;
    typedef const Structure& const_reference;

    /**
     * Get the number of immediate subfields in the structure/
     * @return The number of fields.
     */
    int getNumberFields() const {return numberFields;}
    /**
     * Get the field for the specified fieldName.
     * @return The introspection interface.
     * This will hold a null pointer if the field is not in the structure.
     */
    FieldConstPtr getField(String fieldName) const;
    /**
     * Get the field index for the specified fieldName.
     * @return The introspection interface.
     * This will be -1 if the field is not in the structure.
     */
    int getFieldIndex(String fieldName) const;
    /**
     * Get the fields in the structure.
     * @return The array of fields.
     */
    FieldConstPtrArray getFields() const {return fields;}
    /**
     * Append a field to the structure.
     * @param field The field to append.
     */
    void appendField(FieldConstPtr field);
    /**
     * Append an array of  fields to the structure.
     * @param field The fields to append.
     * The array MUST be allocated on the heap.
     * The structure takes ownership of the field array.
     */
    void appendFields(int numberFields,FieldConstPtrArray fields);
    /**
     * Remove a field from the structure.
     * @param field The field to remove.
     */
    void removeField(int index);
    /**
     * Convert the structure to a string and add it to builder.
     * @param  builder The string builder.
     */
    virtual void toString(StringBuilder buf) const{toString(buf,0);}
    /**
     * Convert the structure to a string and add it to builder.
     * @param  builder The string builder.
     * @param  indentLevel The number of blanks at the beginning of new lines.
     */
    virtual void toString(StringBuilder buf,int indentLevel) const;
protected:
   Structure(String fieldName, int numberFields,FieldConstPtrArray fields);
private:
    int numberFields;
    FieldConstPtrArray  fields;
   friend class FieldCreate;
};

/**
 * This is a singlton class for creating introspection interfaces.
 */
class FieldCreate : NoDefaultMethods {
public:
    /**
     * Create a new Field like an existing field but with a different name.
     * @param fieldName The field name.
     * @param field An existing field
     * @return a {@code Field} interface for the newly created object.
     */
    FieldConstPtr  create(String fieldName,FieldConstPtr  field) const;
    /**
     * Create a {@code ScalarField}.
     * @param fieldName The field name.
     * @param scalarType The scalar type.
     * @return a {@code Scalar} interface for the newly created object.
     * @throws An {@code IllegalArgumentException} if an illegal type is specified.
     */
    ScalarConstPtr  createScalar(String fieldName,ScalarType scalarType) const;
    /**
     * Create an {@code Array} field.
     * @param fieldName The field name
     * @param elementType The {@code scalarType} for array elements
     * @return An {@code Array} Interface for the newly created object.
     */
    ScalarArrayConstPtr createScalarArray(String fieldName,
        ScalarType elementType) const;
     /**
      * Create an {@code Array} field that is has element type <i>Structure</i>
      * @param fieldName The field name
      * @param elementStructure The {@code Structure} for each array element.
      * @return An {@code Array} Interface for the newly created object.
      */
    StructureConstPtr createStructure (String fieldName,
        int numberFields,FieldConstPtrArray fields) const;
    /**
     * Create a {@code Structure} field.
     * @param fieldName The field name
     * @param fields The array of {@code Field}s for the structure.
     * @return a {@code Structure} interface for the newly created object.
     */
    StructureArrayConstPtr createStructureArray(String fieldName,
        StructureConstPtr structure) const;
private:
   FieldCreate();
   friend FieldCreate * getFieldCreate();
};

/**
 * Get the single class that implemnents FieldCreate,
 * @param The fieldCreate factory.
 */
extern FieldCreate * getFieldCreate();

}}
#endif  /* PVINTROSPECT_H */
