//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "coerciblevariant_t.h"
#include "entitylist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#pragma warning(disable: 4800) // yes, we really do want to coerce ints to bools, 
							   // that's the whole point of this class.

coerciblevariant_t::~coerciblevariant_t()
{
	if (szVal)
		delete[] szVal;
}

// Note: the constructor and SetXXX() functions are
// almost identical, except that the Sets() have a Void()
// step, because they may need to wipe out a string that's 
// been allocated. 

coerciblevariant_t::coerciblevariant_t( bool b ) : szVal(NULL)
{
	// do the bool, int, and float conversions right now, set the flags
	bVal = b;
	iVal = b ? 1 : 0;
	flVal = b ? 1.0f : 0.0f;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_BOOLEAN;
}

void coerciblevariant_t::SetBool( bool b )
{
	Void();

	// do the bool, int, and float conversions right now, set the flags
	bVal = b;
	iVal = b ? 1 : 0;
	flVal = b ? 1.0f : 0.0f;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_BOOLEAN;
}

coerciblevariant_t::coerciblevariant_t( int i ) : szVal(NULL)
{
	// do the bool, int, and float conversions right now, set the flags
	bVal = i;
	iVal = i;
	flVal = i;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_INTEGER;
}

void coerciblevariant_t::SetInt( int i )
{
	Void();

	// do the bool, int, and float conversions right now, set the flags
	bVal = i;
	iVal = i;
	flVal = i;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_INTEGER;
}

coerciblevariant_t::coerciblevariant_t( float f ) : szVal(NULL)
{
	// do the bool, int, and float conversions right now, set the flags
	bVal = f;
	iVal = f;
	flVal = f;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_FLOAT;
}


void coerciblevariant_t::SetFloat( float f )
{
	Void();

	// do the bool, int, and float conversions right now, set the flags
	bVal = f;
	iVal = f;
	flVal = f;
	m_bvInitFields = kINIT_BOOLEAN | kINIT_INT | kINIT_FLOAT ;
	fieldType = FIELD_FLOAT;
}

coerciblevariant_t::coerciblevariant_t( const char * str )
{
	SetString(str);
}

void coerciblevariant_t::SetString( const char * str )
{
	Void();

	// make a copy of the string for me.
	szVal = new char[DEFAULT_VARIANT_STRING_SIZE];
	Q_strncpy( szVal, str, DEFAULT_VARIANT_STRING_SIZE );
	m_bvInitFields = kINIT_STRING;
	fieldType = FIELD_STRING;
}

coerciblevariant_t::coerciblevariant_t( const EHANDLE &handle )
	: eVal(handle), fieldType(FIELD_EHANDLE), m_bvInitFields(kINIT_EHANDLE), szVal(NULL)
{};

coerciblevariant_t::coerciblevariant_t( CBaseEntity *ent )
	: eVal(ent), fieldType(FIELD_EHANDLE), m_bvInitFields(kINIT_EHANDLE), szVal(NULL)
{};

void coerciblevariant_t::SetEntity( CBaseEntity *val )
{
	Void();

	eVal = val;
	m_bvInitFields = kINIT_EHANDLE;
	fieldType = FIELD_EHANDLE;
}


// Get me as a string
const char * coerciblevariant_t::String( void )
{
	if ( (m_bvInitFields & kINIT_STRING) == 0 )
	{
		// need to initialize the string.
		AssertMsg(szVal == NULL, "A variant tried to convert itself to a string when it already was one.");

		// convert from the appropriate native type
		switch (fieldType)
		{
		case FIELD_BOOLEAN:
			szVal = new char[2];
			szVal[0] = bVal ? '1' : '0';
			szVal[1] = '\0';
			break;
		case FIELD_INTEGER:
			szVal = new char[24];
			Q_snprintf( szVal, 24, "%d", iVal );
			break;
		case FIELD_FLOAT:
			szVal = new char[32];
			Q_snprintf( szVal, 32, "%.3f", flVal );
			break;
		case FIELD_EHANDLE: // entity name, or ""
		{
			
			CBaseEntity *ent = eVal.Get();
			if (ent)
			{
				szVal = new char[DEFAULT_VARIANT_STRING_SIZE];
				Q_strncpy( szVal, ent->GetEntityName().ToCStr(), DEFAULT_VARIANT_STRING_SIZE );
			}
			else
			{
				szVal = new char[1];
				szVal[0] = 0;
			}
			break;
		}
		case FIELD_STRING:
			AssertMsg( false, "Tried to convert a variant from string to string?!" );
			break;
		default:
			AssertMsg1( false, "Tried to convert a variant from unknown field type %d", fieldType );
			szVal = new char[1];
			szVal[0] = 0;
			break;
		}

		m_bvInitFields |= kINIT_STRING;
	}

	return szVal;
}

float coerciblevariant_t::ConvertFloat() const
{
	// convert from salient native type to a float.
	switch (fieldType)
	{
	case FIELD_STRING:
		// try to turn the string into a float
		return atof(szVal);

	case FIELD_EHANDLE:
		AssertMsg(false, "Variant tried to convert from EHANDLE to float?!" );
		return eVal.Get() ? 1 : 0;

	case FIELD_FLOAT:
	case FIELD_INTEGER:
	case FIELD_BOOLEAN:
		AssertMsg( false, "Can't-happen: coercible variant tried to convert from float to a number, which should already have been done" );
		return flVal;
	default:
		AssertMsg1( false, "Tried to convert a variant from unknown field type %d", fieldType );
		return 0;
	}
}

int coerciblevariant_t::ConvertInt() const
{
	// convert from salient native type to an int.
	switch (fieldType)
	{
	case FIELD_STRING:
		// try to turn the string into an int
		return atoi(szVal);

	case FIELD_EHANDLE:
		AssertMsg(false, "Variant tried to convert from EHANDLE to int?!" );
		return eVal.Get() ? 1 : 0;

	case FIELD_FLOAT:
	case FIELD_INTEGER:
	case FIELD_BOOLEAN:
		AssertMsg( false, "Can't-happen: coercible variant tried to convert from int to a number, which should already have been done" );
		return iVal;
	default:
		AssertMsg1( false, "Tried to convert a variant from unknown field type %d", fieldType );
		return 0;
	}
}

bool coerciblevariant_t::ConvertBool() const
{
	// convert from salient native type to an bool.
	switch (fieldType)
	{
	case FIELD_STRING:
		// try to turn the string into an int
		return atoi(szVal);

	case FIELD_EHANDLE:
		return eVal.Get();

	case FIELD_FLOAT:
	case FIELD_INTEGER:
	case FIELD_BOOLEAN:
		AssertMsg( false, "Can't-happen: coercible variant tried to convert from int to a number, which should already have been done" );
		return iVal;
	default:
		AssertMsg1( false, "Tried to convert a variant from unknown field type %d", fieldType );
		return 0;
	}
}

CBaseEntity *coerciblevariant_t::ConvertEntity() const
{
	// convert from salient native type to an entity pointer.
	switch (fieldType)
	{
	case FIELD_STRING:
		// try to look up the entity by name
		return gEntList.FindEntityByName(NULL, szVal);

	case FIELD_EHANDLE:
		return eVal.Get();

	case FIELD_FLOAT:
	case FIELD_INTEGER:
	case FIELD_BOOLEAN:
		AssertMsg( false, "Coercible variant tried to turn a number into an entity, which is just plain wierd." );
		return NULL;
	default:
		AssertMsg1( false, "Tried to convert a variant from unknown field type %d", fieldType );
		return NULL;
	}
}
