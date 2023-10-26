//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef COERCIBLEVARIANT_T_H
#define COERCIBLEVARIANT_T_H
#ifdef _WIN32
#pragma once
#endif


#include "ehandle.h"

class CBaseEntity;


//
// A variant class for use in response rule contexts.
// It provides implicit conversion between types.
// So, you can construct it as a string, but fetch it
// as an int, and so on.
// It caches these conversions to make them faster afterwards.
// If you construct from a string, it will make a copy of 
// that string, so it's okay to pass in extemporaneous stack strings.
//
class coerciblevariant_t
{
	// each of the types we store
	bool bVal;
	int iVal;
	float flVal;
	char * szVal; // generally this one is only computed on demand (because it eats memory)
	CHandle<CBaseEntity> eVal; // this can't be in the union because it has a constructor.

	// my native type -- but I can be implicitly converted to others.
	fieldtype_t fieldType;

	// store which types have been initialized
	enum 
	{
		kINIT_BOOLEAN = ( 1 << 0 ),
		kINIT_INT	  = ( 1 << 1 ),
		kINIT_FLOAT	  = ( 1 << 2 ),
		kINIT_STRING  = ( 1 << 3 ),
		kINIT_EHANDLE = ( 1 << 4 ),
	};
	unsigned int m_bvInitFields; 

	enum
	{
		DEFAULT_VARIANT_STRING_SIZE = 64 // how big my allocated strings are, when I make them.
	};

public:

	// constructor
	coerciblevariant_t() : fieldType(FIELD_VOID), iVal(0), bVal(false), szVal(NULL), flVal(0), m_bvInitFields(0) {}
	~coerciblevariant_t();
	// assignment ctors (private because not used yet, please don't use them
private:
	coerciblevariant_t(const coerciblevariant_t &src);
	coerciblevariant_t& operator=(const coerciblevariant_t &src);
public:

	// convenience constructors
	coerciblevariant_t( bool b );
	coerciblevariant_t( const char * str );
	coerciblevariant_t( int i );
	coerciblevariant_t( float f );
	coerciblevariant_t( const EHANDLE &handle );
	coerciblevariant_t( CBaseEntity *ent );

	inline bool Bool( void ) ;// const						{ return( fieldType == FIELD_BOOLEAN ) ? bVal : false; }
	const char *String( void ) ;// const				{ return( fieldType == FIELD_STRING ) ? STRING(iszVal) : ToString(); }
	inline int Int( void ) ;// const						{ return( fieldType == FIELD_INTEGER ) ? iVal : 0; }
	inline float Float( void ) ;// const					{ return( fieldType == FIELD_FLOAT ) ? flVal : 0; }
	inline const CHandle<CBaseEntity> &Entity(void) ;// const;

	fieldtype_t FieldType( void ) { return fieldType; }

	void SetBool( bool b );
	void SetString( const char * str );
	void SetInt( int val );
	void SetFloat( float val );
	void SetEntity( CBaseEntity *val );

protected:

	// from my native type, make a float value if possible
	float ConvertFloat() const;

	// from my native type, make an int value if possible
	int ConvertInt() const;

	// from my native type, make a bool value if possible
	bool ConvertBool() const;

	// from my native type, make an entity if possible, NULL otherwise
	CBaseEntity * ConvertEntity() const;

private: 
	// zero out my contents -- called at top of each Set
	inline void Void( void );
};

// mark me as having no conversions, dump the string
// if there is one
void coerciblevariant_t::Void( void )
{
	m_bvInitFields = 0;

	if (szVal)
	{
		delete[] szVal;
		szVal = NULL;
	}
}

// get my bool contents
bool coerciblevariant_t::Bool( void )
{
	if ( (m_bvInitFields & kINIT_BOOLEAN) == 0 )
	{
		// we need to convert
		bVal = ConvertBool();
		m_bvInitFields |= kINIT_BOOLEAN ;
	}

	return bVal;
}

// get my int contents
int coerciblevariant_t::Int( void )
{
	if ( (m_bvInitFields & kINIT_INT) == 0 )
	{
		// we need to convert
		iVal = ConvertInt();
		m_bvInitFields |= kINIT_INT ;
	}

	return iVal;
}

// get my float contents
float coerciblevariant_t::Float( void )
{
	if ( (m_bvInitFields & kINIT_FLOAT) == 0 )
	{
		// we need to convert
		flVal = ConvertFloat();
		m_bvInitFields |= kINIT_FLOAT ;
	}

	return flVal;
}


// get me as an entity
const CHandle<CBaseEntity> &coerciblevariant_t::Entity( void )
{
	if ( (m_bvInitFields & kINIT_EHANDLE) == 0 )
	{
		// we need to convert
		eVal = ConvertEntity();
		m_bvInitFields |= kINIT_EHANDLE ;
	}

	return eVal;
}


typedef coerciblevariant_t cvariant_t; // easier typing!

#endif // VARIANT_T_H
