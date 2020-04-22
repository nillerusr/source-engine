//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef GCRECORD_H
#define GCRECORD_H
#ifdef _WIN32
#pragma once
#endif

#include "schema.h"

#include "tier0/memdbgon.h"
namespace GCSDK
{
#pragma pack( push, 1 )

class CRecordInfo;

//-----------------------------------------------------------------------------
// VarFieldBlockInfo_t
// Tracks a block of memory used to hold all the variable-length
// fields for a record. 
//-----------------------------------------------------------------------------
struct VarFieldBlockInfo_t
{
	union
	{
		// Take up 64-bits of space now even though
		// pointers are still 32 bits
		uint8* m_pubBlock;
		uint64 _unused;
	};
	uint32 m_cubBlock;		// how much is in this block?
	uint32 m_cubBlockFree;	// how much in this block is free?
};

//-----------------------------------------------------------------------------
// VarField_t
// Data format for a variable field entry in a DS record
// For leaf code, is hidden inside a CVarField or CVarCharField
//-----------------------------------------------------------------------------
struct VarField_t
{
	uint32 m_cubField;					// Size of the field
	uint32 m_dubOffset;					// Offset of the field within the block
};

//-----------------------------------------------------------------------------
// CVarField
// Defines a class to encompass a variable-length field - opaque
//-----------------------------------------------------------------------------
class CVarField : private VarField_t
{
public:
	friend class CRecordVar;
private:
};


//-----------------------------------------------------------------------------
// CVarCharField
// Defines a class to encompass a variable-length string field - opaque
//-----------------------------------------------------------------------------
class CVarCharField : public CVarField
{
public:
	friend class CRecordVar;
};


//-----------------------------------------------------------------------------
// CVarCharField
// Defines a class to encompass a variable-length string field - opaque
//-----------------------------------------------------------------------------
class CVarBinaryField : public CVarField
{
public:
	friend class CRecordVar;
};


//-----------------------------------------------------------------------------
// CImageField
// Defines a class to encompass a long variable-length binary field - opaque
//-----------------------------------------------------------------------------
class CImageField : public CVarField
{
public:
	friend class CRecordVar;
};


#pragma pack( pop )

// fix the size of this just to be safe
#pragma pack( push, 4 )

class CSchema;

//-----------------------------------------------------------------------------
// CRecordBase
// Defines a class which arbitrates access to a fixed-length record
//
// This is used as a base class for the CSchTable wrapper classes emitted
// by the schema compiler when the involved table has no variable length data.
//-----------------------------------------------------------------------------
class CRecordBase
{
public:
	// These both allocate new space and COPY the record data into it
	CRecordBase( const CRecordBase &that );
	CRecordBase &operator=(const CRecordBase &that);

	// Init from general memory
	int InitFromBytes( uint8 *pubRecord );

	virtual ~CRecordBase();

	// Use these when sending records over the wire
	uint32 CubSerialized();

	virtual uint8 *PubRecordFixed();
	const uint8 *PubRecordFixed() const;
	uint32 CubRecordFixed() const;

	virtual uint8 *PubRecordVarBlock();
	virtual const uint8 *PubRecordVarBlock() const;
	uint32 CubRecordVarBlock() const;
	bool BAssureRecordVarStorage( uint32 cVariableBytes );

	// generic data accessors
	bool BGetField( int iField, uint8 **ppubData, uint32 *pcubField ) const;
	virtual bool BSetField( int iField, void * pvData, uint32 cubData );
	virtual void WipeField( int iField );

	// data accessors
	const char * GetStringField( int iField, uint32 *pcubField );
	int GetInt( int iField );
	uint16 GetUint16( int iField );
	uint32 GetUint32( int iField );
	uint64 GetUint64( int iField );

	// variable length data accessors
	// (not implemented by this class)
	virtual const char *ReadVarCharField( const CVarCharField &field ) const;
	virtual const uint8 *ReadVarDataField( const CVarField &field, uint32 *pcubField ) const;
	virtual bool SetVarCharField( CVarCharField &field, const char *pchString, bool bTruncate, int32 iField );
	virtual void SetVarDataField( CVarField &field, const void *pvData, uint32 cubData );

	virtual const CSchema *GetPSchema() const
	{
		return const_cast<CRecordBase*>(this)->GetPSchema();
	}
	virtual CSchema *GetPSchema();
	const CRecordInfo *GetPRecordInfo() const;

	// implemented by CSch-something derivatives
	virtual int GetITable() const = 0;

	void RenderField( uint32 unColumn, int cchBuffer, char *pchBuffer ) const;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );		// Validate our internal structures
	static void ValidateStatics( CValidator &validator, const char *pchName );
#endif // DBGFLAG_VALIDATE

protected:
	// copies the contents of the record. The assignement operator uses this internally
	virtual void Copy( const CRecordBase & that );

	CSchema *GetPSchemaImpl();
	void Cleanup();
	bool BSetField( int iField, void *pvData, uint32 cubData, bool *pbRealloced );


	// ctor for derived classes, CSch*
	CRecordBase( ) { }

};


//-----------------------------------------------------------------------------
// CRecordVar
// Defines a class which arbitrates access to a variable-length record
//
// This is used as a base class for the CSchTable wrapper classes emitted
// by the schema compiler when the involved table *does* have variable-length data
//-----------------------------------------------------------------------------
class CRecordVar : public CRecordBase
{
public:
	CRecordVar( )
	{
		m_pSchema = NULL;
		m_nFlags = 0;
	}

	virtual ~CRecordVar()
	{
		Cleanup();
	}

	virtual uint8* PubRecordFixed();
	const uint8 *PubRecordFixed() const;

	virtual CSchema *GetPSchema()
	{
		return m_pSchema;
	}
	virtual const CSchema *GetPSchema() const
	{
		return m_pSchema;
	}

	// Init from general memory
	int InitFromBytes( uint8 *pubRecord );

	// generic data accessors
	virtual bool BSetField( int iField, void * pvData, uint32 cubData );
	virtual void WipeField( int iField );

	// variable-length data accessors
	virtual const char *ReadVarCharField( const CVarCharField &field ) const;
	virtual const uint8 *ReadVarDataField( const CVarField &field, uint32 *pcubField ) const;
	virtual bool SetVarCharField( CVarCharField &field, const char *pchString, bool bTruncate, int32 iField );
	virtual void SetVarDataField( CVarField &field, const void *pvData, uint32 cubData );

	// allocated fixed means we've got our own memory for the fixed record
	// allocated var block means we've allocated a block for the variable part of this record
	enum EFlags { k_EAllocatedFixed = 0x1, k_EAllocatedVarBlock = 0x2  };
	bool BFlagSet( int eFlag ) const;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );		// Validate our internal structures
#endif // DBGFLAG_VALIDATE

protected:
	// copies the contents of the record. The assignement operator uses this internally
	virtual void Copy( const CRecordBase & that );

	// initialization helper
	void DoInit()
	{
		m_pSchema = CRecordBase::GetPSchema();
	}

	void SetFlag( int eFlag, bool bSet );

	void Cleanup();
	CSchema *m_pSchema;							// Corresponding Schema
	int m_nFlags;								// Flags about the record memory allocations / location
};


//-----------------------------------------------------------------------------
// CRecordExternal
// Defines a class which arbitrates access to a variable-length record
//
// This is used as an accessor for a polymorphic record. It can be used to
// read CSchTable records when the type is unknown, manipulate stats records,
// or touch data that isn't preallocated. Its use is relatively rare.
//-----------------------------------------------------------------------------
class CRecordExternal : public CRecordVar
{
public:
	CRecordExternal()
	{
		m_pubRecordFixedExternal = NULL;
		m_nFlags = 0;
	}

	virtual ~CRecordExternal()
	{
		Cleanup();
	}

	virtual uint8* PubRecordFixed();
	const uint8 *PubRecordFixed() const;

	void DeSerialize( uint8 *pubData );

	int GetITable() const
	{
		return -1;
	}

	void Init( CSchema *pSchema );
	int Init( CSchema *pSchema, uint8 *pubRecord, bool bTakeOwnership );

	// test helpers
	void InitRecordRandom( uint32 unPrimaryIndex );
	void SetFieldRandom( int iField );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );		// Validate our internal structures
#endif // DBGFLAG_VALIDATE

protected:
	// copies the contents of the record. The assignement operator uses this internally
	virtual void Copy( const CRecordBase & that );

	void Cleanup();

	void DoInit()
	{
		m_pSchema = CRecordBase::GetPSchema();
	}

	uint8 *m_pubRecordFixedExternal;			// If the fixed record is not a part of this object,
												// this points to where it is

	CSchema *GetPSchemaImpl();
};


//-----------------------------------------------------------------------------
// Accessors for variable length character data.
// These goofy little macros implement some syntax sugar
//-----------------------------------------------------------------------------

#define READ_VAR_CHAR_FIELD( record, field )\
	(record).ReadVarCharField( (record).field )

#define WRITE_VAR_CHAR_FIELD( record, field, text )\
	(record).SetVarCharField( (record).m_##field, text, false, (record).k_iField_##field )

#define WRITE_VAR_CHAR_FIELD_TRUNC( record, field, text )\
	(record).SetVarCharField( (record).m_##field, text, true, (record).k_iField_##field )

#pragma pack( pop )


//-----------------------------------------------------------------------------
// Template classes that get a LessFunc that sorts CRecordBases by a field
// within them
//-----------------------------------------------------------------------------
template <class T, int I, typename F>
class CDefSchOps
{
public:
	static bool LessFunc( const T &lhs, const T &rhs )
	{
		// Check that the field number is valid
		COMPILE_TIME_ASSERT( I >= 0 && I < T::k_iFieldMax );

		// Check to make sure this is a fixed field
		const Field_t &fieldInfo = lhs.GetPSchema()->GetField( I );
		Assert( !fieldInfo.BIsStringType() && !fieldInfo.BIsVariableLength() );
		if ( fieldInfo.BIsStringType() || fieldInfo.BIsVariableLength() )
			return false;

		// Read the data and make sure the sizes are correct for the field type we expect
		uint8 *pubLhs;
		uint8 *pubRhs;
		bool bRet;
		uint32 cubRead;

		bRet = lhs.BGetField( I, &pubLhs, &cubRead );
		Assert( bRet && cubRead == sizeof( F ) );
		if ( !bRet || cubRead != sizeof( F ) )
			return false;

		bRet = rhs.BGetField( I, &pubRhs, &cubRead );
		Assert( bRet && cubRead == sizeof( F ) );
		if ( !bRet || cubRead != sizeof( F ) )
			return false;

		// Finally do the comparison
		return ( *( (F *)pubLhs ) ) < ( *( (F *)pubRhs ) );
	}

	static bool LessFuncCtx( const T &lhs, const T &rhs, void *pCtx )
	{
		return LessFunc( lhs, rhs );
	}
};


#define DefSchLessFunc( RecordType, FieldIndex, FieldType ) CDefSchOps<RecordType, FieldIndex, FieldType>::LessFunc
#define DefSchLessFuncCtx( RecordType, FieldIndex, FieldType ) CDefSchOps<RecordType, FieldIndex, FieldType>::LessFuncCtx


//-----------------------------------------------------------------------------
// Specializations for string fields
//-----------------------------------------------------------------------------
template <class T, int I> 
class CDefSchOps<T, I, char *>
{
public:
	static bool LessFunc( const T &lhs, const T &rhs )
	{
		// Check that the field number is valid
		COMPILE_TIME_ASSERT( I >= 0 && I < T::k_iFieldMax );

		// Check to make sure this is indeed a string field
		Field_t &fieldInfo = lhs.GetPSchema()->GetField( I );
		Assert( fieldInfo.BIsStringType() );
		if ( !fieldInfo.BIsStringType() )
			return false;

		// Read the data
		uint32 cubRead;
		const char *pchLhs = lhs.GetStringField( I, &cubRead );
		const char *pchRhs = rhs.GetStringField( I, &cubRead );

		// Finally do the comparison
		return CDefOps<const char *>::LessFunc( lhs, rhs );
	}

	static bool LessFuncCtx( const T &lhs, const T &rhs, void *pCtx )
	{
		return LessFunc( lhs, rhs );
	}
};


template <class T, int I> 
class CDefSchOps<T, I, const char *>
{
public:
	static bool LessFunc( const T &lhs, const T &rhs )
	{
		return CDefSchOps<T, I, char *>::LessFunc( lhs, rhs );
	}

	static bool LessFuncCtx( const T &lhs, const T &rhs, void *pCtx )
	{
		return LessFunc( lhs, rhs );
	}
};


//-----------------------------------------------------------------------------
// Purpose: Provide a convenient object to pass around to represent a type
//			of record
//-----------------------------------------------------------------------------
class CRecordType
{
public:
	virtual int GetITable() const = 0;
	virtual CRecordBase *Create() const = 0;

	CSchema *GetSchema() const;
	CRecordInfo *GetRecordInfo() const;
protected:
private:
};

template <typename TRecord>
class CRecordTypeConcrete: public CRecordType
{
public:
	virtual int GetITable() const { return TRecord::k_iTable; }
	virtual CRecordBase *Create() const { return new TRecord(); }
};

#define RTYPE( recordClass )  CRecordTypeConcrete<recordClass>()

} // namespace GCSDK

#include "tier0/memdbgoff.h"

#endif // GCRECORD_H
