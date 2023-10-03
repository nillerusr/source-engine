//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SHAREDVAR_H
#define SHAREDVAR_H
#ifdef _WIN32
#pragma once
#endif


#include "convar.h"
#include "ispsharedmemory.h"
#include "basehandle.h"
#include "isaverestore.h"


#pragma warning( disable : 4284 ) // warning C4284: return type for 'CNetworkVarT<int>::operator ->' is 'int *' (ie; not a UDT or reference to a UDT.  Will produce errors if applied using infix notation)


#if defined( GAME_DLL )
	static int Server_EHandleToInt( const EHANDLE &hIn )
	{
		if ( hIn.Get() )
		{
			int iSerialNum = hIn.GetSerialNumber() & (1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS) - 1;
			return ( hIn.GetEntryIndex() | (iSerialNum << MAX_EDICT_BITS) );
		}
		else
		{
			return INVALID_NETWORKED_EHANDLE_VALUE;
		}
	}
#endif

#if defined( CLIENT_DLL )
	static EHANDLE Client_IntToEHandle( int iIn )
	{
		if ( iIn == INVALID_NETWORKED_EHANDLE_VALUE )
		{
			return INVALID_EHANDLE_INDEX;
		}
		else
		{
			int iEntity = iIn & ((1 << MAX_EDICT_BITS) - 1);
			int iSerialNum = iIn >> MAX_EDICT_BITS;

			EHANDLE hOut;
			hOut.Init( iEntity, iSerialNum );
			return hOut;
		}
	}
#endif


class CSharedVarSaveDataOps;


// Templated save/restore functions for shared vars (add specializations as needed)
template< class Type >
static void SharedVar_Save( ISave *pSave, Type *pValue, int iCount = 1 )
{
	int iNumBytes = sizeof( Type ) * iCount;
	pSave->WriteInt( &iNumBytes );
	pSave->WriteData( (char*)( (void*)(pValue) ), iNumBytes );
}

template< class Type >
static void SharedVar_Restore( IRestore *pRestore, Type *pValue )
{
	int iNumBytes = pRestore->ReadInt();
	pRestore->ReadData( (char*)( (void*)(pValue) ), iNumBytes, 0 );
}

template< class Type >
static bool SharedVar_IsEmpty( Type *pValue, int iCount = 1 )
{
	char *pChar = (char*)( (void*)(pValue) );
	int iNumBytes = sizeof( Type ) * iCount;

	for ( int i = 0; i < iNumBytes; ++i )
	{
		if ( pChar[i] != NULL )
		{
			return false;
		}
	}

	return true;
}

template< class Type >
static void SharedVar_MakeEmpty( Type *pValue, int iCount = 1 )
{
	memset( pValue, 0, sizeof( Type ) * iCount );
}


// EHANDLE Save/Restore specializations
template<>
TEMPLATE_STATIC void SharedVar_Save<EHANDLE>( ISave *pSave, EHANDLE *pValue, int iCount )
{
	pSave->WriteInt( &iCount );
	pSave->WriteEHandle( pValue, iCount );
}

template<>
TEMPLATE_STATIC void SharedVar_Restore<EHANDLE>( IRestore *pRestore, EHANDLE *pValue )
{
	int iCount = pRestore->ReadInt();
	pRestore->ReadEHandle( pValue, iCount );
}

// UtlVector Save/Restore specializations
template< class Type > void SharedVar_SaveUtlVector( ISave *pSave, CUtlVector<Type> *pValue, int iCount = 1 )
{
	Assert( iCount == 1 );

	int iNumBytes = sizeof( Type ) * pValue->Count();
	pSave->WriteInt( &iNumBytes );
	pSave->WriteData( (char*)( (void*)(pValue->Base()) ), iNumBytes );
}

template< class Type > void SharedVar_RestoreUtlVector( IRestore *pRestore, CUtlVector<Type> *pValue )
{
	int iNumBytes = pRestore->ReadInt();
	pValue->SetCount( iNumBytes / sizeof( Type ) );
	pRestore->ReadData( (char*)( (void*)(pValue->Base()) ), iNumBytes, 0 );
}

template< class Type >
static bool SharedVar_IsEmptyUtlVector( CUtlVector<Type> *pValue, int iCount = 1 )
{
	Assert( iCount == 1 );

	return ( pValue->Count() == 0 );
}

template< class Type >
static void SharedVar_MakeEmptyUtlVector( CUtlVector<Type> *pValue, int iCount = 1 )
{
	Assert( iCount == 1 );

	pValue->SetCount( 0 );
}


abstract_class ISharedVarBase
{
private:
	virtual void _Save( ISave *pSave ) = 0;
	virtual void _Restore( IRestore *pRestore ) = 0;
	virtual bool _IsEmpty( void ) = 0;
	virtual void _MakeEmpty( void ) = 0;

public:
	friend class CSharedVarSaveDataOps;
};


template< class Type, class Changer >
class CSharedVarBase : public ISharedVarBase
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = NULL;
		m_pValue = NULL;
	}

	~CSharedVarBase( void )
	{
		m_pSharedMemory->Release();
		m_pSharedMemory = NULL;
		m_pValue = NULL;
	}

	void Initialize( char *pchName, int iEntNum )
	{
		Assert( !m_pSharedMemory && !m_pValue );

		m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		m_pSharedMemory->Init( sizeof( Type ) );
		m_pValue = (Type*)m_pSharedMemory->Base();
	}

	#if defined( GAME_DLL )
		// Data can only be set on the server
		template< class C >
		const Type& operator=( const C &val ) 
		{ 
			return Set( static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator=( const CNetworkVarBase< C, Changer > &val ) 
		{ 
			return Set( static_cast<const Type>( *val.m_pValue ) ); 
		}

		const Type& Set( const Type &val )
		{
			*m_pValue = val;
			return *m_pValue;
		}

		template< class C >
		const Type& operator+=( const C &val ) 
		{
			return Set( *m_pValue + static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator-=( const C &val ) 
		{
			return Set( *m_pValue - static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator/=( const C &val ) 
		{
			return Set( *m_pValue / static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator*=( const C &val ) 
		{
			return Set( *m_pValue * static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator^=( const C &val ) 
		{
			return Set( *m_pValue ^ static_cast<const Type>( val ) ); 
		}

		template< class C >
		const Type& operator|=( const C &val ) 
		{
			return Set( *m_pValue | static_cast<const Type>( val ) ); 
		}

		const Type& operator++()
		{
			return (*this += 1);
		}

		Type operator--()
		{
			return (*this -= 1);
		}

		Type operator++( int ) // postfix version..
		{
			Type val = *m_pValue;
			(*this += 1);
			return val;
		}

		Type operator--( int ) // postfix version..
		{
			Type val = *m_pValue;
			(*this -= 1);
			return val;
		}

		// For some reason the compiler only generates type conversion warnings for this operator when used like 
		// CNetworkVarBase<unsigned char> = 0x1
		// (it warns about converting from an int to an unsigned char).
		template< class C >
		const Type& operator&=( const C &val ) 
		{	
			return Set( *m_pValue & static_cast<const Type>( val ) ); 
		}

		operator Type&()
		{
			return *m_pValue; 
		}

		Type& GetForModify()
		{
			return *m_pValue;
		}

		Type* operator->()
		{
			return m_pValue; 
		}
	#endif

	operator const Type&() const 
	{
		return *m_pValue; 
	}

	const Type& Get() const 
	{
		return *m_pValue; 
	}

	const Type* operator->() const 
	{
		return m_pValue; 
	}

private:
	virtual void _Save( ISave *pSave ) { SharedVar_Save<Type>( pSave, m_pValue ); }
	virtual void _Restore( IRestore *pRestore ) { SharedVar_Restore<Type>( pRestore, m_pValue ); }
	virtual bool _IsEmpty( void ) { return SharedVar_IsEmpty<Type>( m_pValue ); }
	virtual void _MakeEmpty( void ) { SharedVar_MakeEmpty<Type>( m_pValue ); }

public:
	ISPSharedMemory		*m_pSharedMemory;
	Type				*m_pValue;
};


// Shared color wrapper.
template< class Type, class Changer >
class CSharedColor32Base : public CSharedVarBase< Type, Changer >
{
public:
	#if defined( GAME_DLL )
		// Data can only be set on the server
		inline void Init( byte rVal, byte gVal, byte bVal )
		{
			SetR( rVal );
			SetG( gVal );
			SetB( bVal );
		}
		inline void Init( byte rVal, byte gVal, byte bVal, byte aVal )
		{
			SetR( rVal );
			SetG( gVal );
			SetB( bVal );
			SetA( aVal );
		}

		const Type& operator=( const Type &val ) 
		{ 
			return Set( val ); 
		}

		const Type& operator=( const CSharedColor32Base<Type,Changer> &val ) 
		{ 
			return CSharedVarBase<Type,Changer>::Set( *val.m_pValue );
		}

		inline void SetR( byte val ) { SetVal( CSharedColor32Base<Type,Changer>::m_pValue->r, val ); }
		inline void SetG( byte val ) { SetVal( CSharedColor32Base<Type,Changer>::m_pValue->g, val ); }
		inline void SetB( byte val ) { SetVal( CSharedColor32Base<Type,Changer>::m_pValue->b, val ); }
		inline void SetA( byte val ) { SetVal( CSharedColor32Base<Type,Changer>::m_pValue->a, val ); }
#endif

	inline byte GetR() const { return CSharedColor32Base<Type,Changer>::m_pValue->r; }
	inline byte GetG() const { return CSharedColor32Base<Type,Changer>::m_pValue->g; }
	inline byte GetB() const { return CSharedColor32Base<Type,Changer>::m_pValue->b; }
	inline byte GetA() const { return CSharedColor32Base<Type,Changer>::m_pValue->a; }

#if defined( GAME_DLL )
	// Data can only be set on the server
	private:
		inline void SetVal( byte &out, const byte &in )
		{
			out = in;
		}
#endif
};


// Shared vector wrapper.
template< class Type, class Changer >
class CSharedVectorBase : public CSharedVarBase< Type, Changer >
{
public:
	#if defined( GAME_DLL )
		// Data can only be set on the server
		inline void Init( float ix=0, float iy=0, float iz=0 ) 
		{
			SetX( ix );
			SetY( iy );
			SetZ( iz );
		}

		const Type& operator=( const Type &val ) 
		{ 
			return CSharedVarBase< Type, Changer >::Set( val ); 
		}

		const Type& operator=( const CSharedVectorBase<Type,Changer> &val ) 
		{ 
			return CSharedVarBase<Type,Changer>::Set( *val.m_pValue );
		}

		inline void SetX( float val ) { SetVal( CSharedVectorBase<Type,Changer>::m_pValue->x, val ); }
		inline void SetY( float val ) { SetVal( CSharedVectorBase<Type,Changer>::m_pValue->y, val ); }
		inline void SetZ( float val ) { SetVal( CSharedVectorBase<Type,Changer>::m_pValue->z, val ); }
		inline void Set( int i, float val ) { SetVal( (*CSharedVectorBase<Type,Changer>::m_pValue)[i], val ); }
		inline float& operator[]( int i ) { return (*CSharedVectorBase<Type,Changer>::m_pValue)[i]; }

		const Type& operator*=( float val )
		{
			return CSharedVarBase< Type, Changer >::Set( (*CSharedVectorBase<Type,Changer>::m_pValue) * val );
		}
	#endif

	inline float GetX() const { return CSharedVectorBase<Type,Changer>::m_pValue->x; }
	inline float GetY() const { return CSharedVectorBase<Type,Changer>::m_pValue->y; }
	inline float GetZ() const { return CSharedVectorBase<Type,Changer>::m_pValue->z; }
	inline const float& operator[]( int i ) const { return (*CSharedVectorBase<Type,Changer>::m_pValue)[i]; }

	bool operator==( const Type &val ) const 
	{ 
		return (*CSharedVectorBase<Type,Changer>::m_pValue) == (Type)val; 
	}

	bool operator!=( const Type &val ) const 
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) != (Type)val; 
	}

	const Type operator+( const Type &val ) const 
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) + val; 
	}

	const Type operator-( const Type &val ) const
	{ 
		return (*CSharedVectorBase<Type,Changer>::m_pValue) - val; 
	}

	const Type operator*( const Type &val ) const
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator*( float val ) const
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator/( const Type &val ) const
	{
		return (*CSharedVectorBase<Type,Changer>::m_pValue) / val; 
	}

#if defined( GAME_DLL )
	// Data can only be set on the server
	private:
		inline void SetVal( float &out, float in ) 
		{
			out = in;
		}
#endif
};


// Shared quaternion wrapper.
template< class Type, class Changer >
class CSharedQuaternionBase : public CSharedVarBase< Type, Changer >
{
public:
	#if defined( GAME_DLL )
		// Data can only be set on the server
		inline void Init( float ix=0, float iy=0, float iz=0, float iw = 0 ) 
		{
			SetX( ix );
			SetY( iy );
			SetZ( iz );
			SetZ( iw );
		}

		const Type& operator=( const Type &val ) 
		{ 
			return CSharedVarBase< Type, Changer >::Set( val ); 
		}

		const Type& operator=( const CSharedQuaternionBase<Type,Changer> &val ) 
		{ 
			return CSharedVarBase<Type,Changer>::Set( *val.m_pValue );
		}

		const Type& operator*=( float val )
		{
			return CSharedVarBase< Type, Changer >::Set( (*CSharedQuaternionBase<Type,Changer>::m_pValue) * val );
		}

		inline void SetX( float val ) { SetVar( CSharedQuaternionBase<Type,Changer>::m_pValue->x, val ); }
		inline void SetY( float val ) { SetVar( CSharedQuaternionBase<Type,Changer>::m_pValue->y, val ); }
		inline void SetZ( float val ) { SetVar( CSharedQuaternionBase<Type,Changer>::m_pValue->z, val ); }
		inline void SetW( float val ) { SetVar( CSharedQuaternionBase<Type,Changer>::m_pValue->w, val ); }
		inline void Set( int i, float val ) { SetVar( (*CSharedQuaternionBase<Type,Changer>::m_pValue)[i], val ); }
		inline float& operator[]( int i ) { return (*CSharedQuaternionBase<Type,Changer>::m_pValue)[i]; }
	#endif

	inline float GetX() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->x; }
	inline float GetY() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->y; }
	inline float GetZ() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->z; }
	inline float GetW() const { return CSharedQuaternionBase<Type,Changer>::m_pValue->w; }
	inline const float& operator[]( int i ) const { return (*CSharedQuaternionBase<Type,Changer>::m_pValue)[i]; }

	bool operator==( const Type &val ) const 
	{ 
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) == (Type)val; 
	}

	bool operator!=( const Type &val ) const 
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) != (Type)val; 
	}

	const Type operator+( const Type &val ) const 
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) + val; 
	}

	const Type operator-( const Type &val ) const
	{ 
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) - val; 
	}

	const Type operator*( const Type &val ) const
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator*( float val ) const
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) * val; 
	}

	const Type operator/( const Type &val ) const
	{
		return (*CSharedQuaternionBase<Type,Changer>::m_pValue) / val; 
	}

#if defined( GAME_DLL )
	// Data can only be set on the server
	private:
		inline void SetVar( float &out, float in ) 
		{
			out = in;
		}
#endif
};


// Prevent Shared Vars from using handle templates without the handle wrapper
template< class Type, class Changer >
class CSharedVarBase< CHandle<Type>, Changer > : public ISharedVarBase
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = NULL;
		m_pValue = NULL;
	}

	~CSharedVarBase( void )
	{
		m_pSharedMemory->Release();
		m_pSharedMemory = NULL;
		m_pValue = NULL;
	}

protected:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() = 0;

public:
	ISPSharedMemory		*m_pSharedMemory;
	CHandle<Type>		*m_pValue;
};

// Prevent Shared Vars from using handle templates without the handle wrapper
template< class Changer >
class CSharedVarBase< CBaseHandle, Changer > : public ISharedVarBase
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = NULL;
		m_pValue = NULL;
	}

	~CSharedVarBase( void )
	{
		m_pSharedMemory->Release();
		m_pSharedMemory = NULL;
		m_pValue = NULL;
	}

protected:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() = 0;

public:
	ISPSharedMemory		*m_pSharedMemory;
	CBaseHandle			*m_pValue;
};


// Shared handle wrapper.
template< class Type, class Changer >
class CSharedHandleBase : public CSharedVarBase< EHANDLE, Changer >
{
public:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() {}

	void Initialize( char *pchName, int iEntNum )
	{
		Assert( !this->m_pSharedMemory && !this->m_pValue );

		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( EHANDLE ) + sizeof( int ) );		// Also allocate memory for the int that converts to a Client handle
		this->m_pValue = (EHANDLE*)this->m_pSharedMemory->Base();
		m_piClientIndex = (int*)((void*)(this->m_pValue + 1));
	}

	bool operator !() const 
	{ 
		return !GetHandle_Internal()->Get(); 
	}

	#if defined( GAME_DLL )
		// Data can only be set on the server
		const Type* operator=( const Type *val ) 
		{ 
			return Set( val ); 
		}

		const Type& operator=( const CSharedHandleBase<Type,Changer> &val ) 
		{ 
			const EHANDLE &handle = CSharedVarBase<EHANDLE,Changer>::Set( *val.m_pValue );
			return *(const Type*)handle.Get();
		}

		const Type* Set( const Type *val )
		{
			(*CSharedHandleBase<Type,Changer>::m_pValue) = val;

			// Store the int that we'll need to translate to the EHandle on the client
			*m_piClientIndex = Server_EHandleToInt( *CSharedHandleBase<Type,Changer>::m_pValue );
			return val;
		}
	#endif

	operator const Type*() const 
	{ 
		return Get_Internal();
	}

	const Type* Get() const 
	{ 
		return Get_Internal();
	}

	const Type* operator->() const 
	{ 
		return Get_Internal();
	}

	bool operator==( const Type *val ) const 
	{
		return Get_Internal() == val; 
	}

	bool operator!=( const Type *val ) const 
	{
		return Get_Internal() != val;
	}

private:
	EHANDLE GetHandle_Internal() const 
	{
		#if defined( GAME_DLL )
			// Server can get the handle directly
			return (*CSharedHandleBase<Type,Changer>::m_pValue);
		#else
			// Client needs to get the handle translated from the stroed int
			EHANDLE hClient = Client_IntToEHandle( *m_piClientIndex );
			return hClient;
		#endif
	}

	Type* Get_Internal() const 
	{
		return static_cast< Type* >( GetHandle_Internal().Get() );
	}

	void UpdateClientIndex( void )
	{
		*m_piClientIndex = Server_EHandleToInt( *(CSharedHandleBase<Type,Changer>::m_pValue) );
	}

	virtual void _Save( ISave *pSave )
	{
		SharedVar_Save<EHANDLE>( pSave, this->m_pValue );
	}

	virtual void _Restore( IRestore *pRestore )
	{
		SharedVar_Restore<EHANDLE>( pRestore, this->m_pValue );
		UpdateClientIndex();
	}

	virtual bool _IsEmpty( void )
	{
		return SharedVar_IsEmpty<EHANDLE>( this->m_pValue );
	}

	virtual void _MakeEmpty( void )
	{
		SharedVar_MakeEmpty<EHANDLE>( this->m_pValue );
	}

public:
	int		*m_piClientIndex;	// Pointer to ints for Client handle translations
};


// Shared array wrapper.
template< class Type, class Changer >
class CSharedArrayBase : public CSharedVarBase< Type, Changer >
{
public:
	CSharedArrayBase( void )
	{
		m_iCount = 0;
	}

	void Initialize( char *pchName, int iEntNum, int iCount )
	{
		Assert( !this->m_pSharedMemory && !(CSharedArrayBase<Type,Changer>::m_pValue) && iCount > 0 );

		m_iCount = iCount;
		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( Type ) * m_iCount );
		CSharedArrayBase<Type,Changer>::m_pValue = (Type*)this->m_pSharedMemory->Base();
	}

	const int& Count() const
	{
		return m_iCount;
	}

	const Type* Get() const 
	{
		return CSharedArrayBase<Type,Changer>::m_pValue; 
	}

	const Type& Get( int i ) const
	{
		Assert( i >= 0 && i < m_iCount );
		return CSharedArrayBase<Type,Changer>::m_pValue[i];
	}

	const Type& operator[]( int i ) const
	{
		return Get( i );
	}

	#if defined( GAME_DLL )
		// Data can only be set on the server
		Type& operator[]( int i )
		{
			return GetForModify( i );
		}

		Type& GetForModify( int i )
		{
			Assert( i >= 0 && i < m_iCount );
			return CSharedArrayBase<Type,Changer>::m_pValue[i];
		}

		void Set( int i, const Type &val )
		{
			Assert( i >= 0 && i < m_iCount );
			CSharedArrayBase<Type,Changer>::m_pValue[i] = val;
		}
	#endif

private:
	virtual void _Save( ISave *pSave )
	{
		SharedVar_Save<Type>( pSave, CSharedArrayBase<Type,Changer>::m_pValue, m_iCount );
	}

	virtual void _Restore( IRestore *pRestore )
	{
		SharedVar_Restore<Type>( pRestore, CSharedArrayBase<Type,Changer>::m_pValue );
	}

	virtual bool _IsEmpty( void )
	{
		return SharedVar_IsEmpty<Type>( CSharedArrayBase<Type,Changer>::m_pValue, m_iCount );
	}

	virtual void _MakeEmpty( void )
	{
		SharedVar_MakeEmpty<Type>( CSharedArrayBase<Type,Changer>::m_pValue, m_iCount );
	}

public:
	int		m_iCount;
};


// Shared handle array wrapper.
template< class Type, class Changer >
class CSharedHandleArrayBase : public CSharedArrayBase< EHANDLE, Changer >
{
public:
	virtual void Shared_Handle_Vars_Must_Use_CSharedHandle_Or_CSharedHandleArray() {}

	void Initialize( char *pchName, int iEntNum, int iCount )
	{
		Assert( !this->m_pSharedMemory && !(CSharedHandleArrayBase<Type,Changer>::m_pValue) && iCount > 0 );

		this->m_iCount = iCount;
		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( EHANDLE ) * this->m_iCount + sizeof( int ) * this->m_iCount );	// Also allocate memory for the ints that convert to Client handles
		this->m_pValue = (EHANDLE*)this->m_pSharedMemory->Base();
		m_piClientIndex = (int*)((void*)(CSharedHandleArrayBase<Type,Changer>::m_pValue + this->m_iCount));
	}

	const int& Count() const
	{
		return this->m_iCount;
	}

	const Type& Get( int i ) const
	{
		Assert( i >= 0 && i < this->m_iCount );
		return Get_Internal( i );
	}

	const Type* operator[]( int i ) const
	{
		return Get_Internal( i );
	}

	#if defined( GAME_DLL )
		// Data can only be set on the server
		const Type* Set( int i, const Type *val )
		{
			Assert( i >= 0 && i < this->m_iCount );
			CSharedHandleArrayBase<Type,Changer>::m_pValue[i] = val;

			// Store the int that we'll need to translate to the EHandle on the client
			m_piClientIndex[i] = Server_EHandleToInt( CSharedHandleArrayBase<Type,Changer>::m_pValue[i] );
			return val;
		}
	#endif

private:
	EHANDLE GetHandle_Internal( int i ) const 
	{
		#if defined( GAME_DLL )
			// Server can get the handle directly
			return CSharedHandleArrayBase<Type,Changer>::m_pValue[i];
		#else
			// Client needs to get the handle translated from the stroed int
			EHANDLE hClient = Client_IntToEHandle( m_piClientIndex[i] );
			return hClient;
		#endif
	}

	Type* Get_Internal( int i ) const 
	{
		return static_cast< Type* >( GetHandle_Internal(i).Get() );
	}

private:
	void UpdateClientIndices( void )
	{
		for ( int i = 0; i < this->m_iConcept; ++i )
		{
			m_piClientIndex[i] = Server_EHandleToInt( CSharedHandleBase<Type,Changer>::m_pValue[i] );
		}
	}

	virtual void _Restore( IRestore *pRestore )
	{
		SharedVar_Restore<EHANDLE>( pRestore, CSharedHandleArrayBase<Type,Changer>::m_pValue );
		UpdateClientIndices();
	}

public:
	int		*m_piClientIndex;	// Pointer to ints for Client handle translations
};


// Prevent Shared Vars from using UtlVector templates without the UtlVector wrapper
template< class Type, class Changer >
class CSharedVarBase< CUtlVector<Type>, Changer > : public ISharedVarBase
{
public:
	CSharedVarBase( void )
	{
		m_pSharedMemory = NULL;
		m_pValue = NULL;
	}

	~CSharedVarBase( void )
	{
		m_pSharedMemory->Release();
		m_pSharedMemory = NULL;
		m_pValue = NULL;
	}

protected:
	virtual void Shared_CUtlVector_Vars_Must_Use_CSharedUtlVector() = 0;

public:
	ISPSharedMemory		*m_pSharedMemory;
	CUtlVector<Type>	*m_pValue;
};


// Shared UtlVector wrapper.
template< class Type, class Changer >
class CSharedUtlVectorBase : public CSharedVarBase< CUtlVector<Type>, Changer >
{
public:
	void Initialize( char *pchName, int iEntNum )
	{
		Assert( !this->m_pSharedMemory && !(CSharedUtlVectorBase<Type,Changer>::m_pValue) );

		this->m_pSharedMemory = engine->GetSinglePlayerSharedMemorySpace( pchName, iEntNum );
		this->m_pSharedMemory->Init( sizeof( CUtlVector<Type> ) );
		CSharedUtlVectorBase<Type,Changer>::m_pValue = (CUtlVector<Type>*)this->m_pSharedMemory->Base();
	}

#if defined( GAME_DLL )
	// Data can only be set on the server
	operator CUtlVector<Type>&()
	{
		return *CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	CUtlVector<Type>& GetForModify()
	{
		return *CSharedUtlVectorBase<Type,Changer>::m_pValue;
	}

	CUtlVector<Type>* operator->()
	{
		return CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	Type& operator[]( int i )
	{
		return GetForModify( i );
	}

	Type& GetForModify( int i )
	{
		Assert( i >= 0 && i < (CSharedUtlVectorBase<Type,Changer>::m_pValue)->Count() );
		return (*CSharedUtlVectorBase<Type,Changer>::m_pValue)[i];
	}

	void Set( int i, const Type &val )
	{
		Assert( i >= 0 && i < (CSharedUtlVectorBase<Type,Changer>::m_pValue)->Count() );
		(*CSharedUtlVectorBase<Type,Changer>::m_pValue)[i] = val;
	}
#endif

	operator const CUtlVector<Type>&() const 
	{
		return *CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	const CUtlVector<Type>& Get() const 
	{
		return *CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	const CUtlVector<Type>* operator->() const 
	{
		return CSharedUtlVectorBase<Type,Changer>::m_pValue; 
	}

	const Type& operator[]( int i ) const
	{
		return Get( i );
	}

	const Type& Get( int i ) const
	{
		Assert( i >= 0 && i < (CSharedUtlVectorBase<Type,Changer>::m_pValue)->Count() );
		return (*CSharedUtlVectorBase<Type,Changer>::m_pValue)[i];
	}

	const int Count() const
	{
		return CSharedUtlVectorBase<Type,Changer>::m_pValue->Count();
	}

protected:
	virtual void Shared_CUtlVector_Vars_Must_Use_CSharedUtlVector() {}

private:
	virtual void _Save( ISave *pSave )
	{
		SharedVar_SaveUtlVector<Type>( pSave, CSharedUtlVectorBase<Type,Changer>::m_pValue, 1 );
	}

	virtual void _Restore( IRestore *pRestore )
	{
		SharedVar_RestoreUtlVector<Type>( pRestore, CSharedUtlVectorBase<Type,Changer>::m_pValue );
	}

	virtual bool _IsEmpty( void )
	{
		return SharedVar_IsEmptyUtlVector<Type>( CSharedUtlVectorBase<Type,Changer>::m_pValue, 1 );
	}

	virtual void _MakeEmpty( void )
	{
		SharedVar_MakeEmptyUtlVector<Type>( CSharedUtlVectorBase<Type,Changer>::m_pValue, 1 );
	}
};


// Use this macro to define a network variable.
#define CSharedVar( type, name ) \
	SHARED_VAR_START( type, name ) \
	SHARED_VAR_END( type, name, CSharedVarBase )

// Helper for color32's. Contains GetR(), SetR(), etc.. functions.
#define CSharedColor32( name ) \
	SHARED_VAR_START( color32, name ) \
	SHARED_VAR_END( color32, name, CSharedColor32Base )

// Vectors + some convenient helper functions.
#define CSharedVector( name ) CSharedVectorInternal( Vector, name )
#define CSharedQAngle( name ) CSharedVectorInternal( QAngle, name )

#define CSharedVectorInternal( type, name ) \
	SHARED_VAR_START( type, name ) \
	SHARED_VAR_END( type, name, CSharedVectorBase )

#define CSharedQuaternion( name ) \
	SHARED_VAR_START( Quaternion, name ) \
	SHARED_VAR_END( Quaternion, name, CSharedQuaternionBase )

#define CSharedHandle( handletype, name ) \
	SHARED_VAR_START( handletype, name ) \
	SHARED_VAR_END( handletype, name, CSharedHandleBase )

#define CSharedArray( type, name, count ) \
	SHARED_VAR_START( type, name ) \
	SHARED_VAR_END( type, name, CSharedArrayBase ) \
	enum { m_SharedArray_count_##name = count };

#define CSharedString( name, length ) \
	CSharedArray( char, name, length )

#define CSharedHandleArray( handletype, name, count ) \
	SHARED_VAR_START( handletype, name ) \
	SHARED_VAR_END( handletype, name, CSharedHandleArrayBase ) \
	enum { m_SharedArray_count_##name = count };

#define CSharedUtlVector( utlvectortype, name ) \
	SHARED_VAR_START( utlvectortype, name ) \
	SHARED_VAR_END( utlvectortype, name, CSharedUtlVectorBase ) \


// Internal macros used in definitions of network vars.
#define SHARED_VAR_START( type, name ) \
	class SharedVar_##name; \
	friend class SharedVar_##name; \
	typedef ThisClass MakeASharedVar_##name; \
	class SharedVar_##name \
	{ \
	public: \
		template <typename T> friend int ServerClassInit(T *);


#define SHARED_VAR_END( type, name, base ) \
	}; \
	base< type, SharedVar_##name > name;


#define DECLARE_SHAREDCLASS() \
	virtual void InitSharedVars( void );

#define IMPLEMENT_SHAREDCLASS_DT( type ) \
	void type::InitSharedVars( void ) \
	{ \
		BaseClass::InitSharedVars();

#define END_SHARED_TABLE() \
	}

#define SharedProp( var ) \
	var.Initialize( "SharedVar_"#var, entindex() );

#define SharedPropArray( var ) \
	var.Initialize( "SharedVar_"#var, entindex(), m_SharedArray_count_##var );


// Save/Restore handling
class CSharedVarSaveDataOps : public CDefSaveRestoreOps
{
	// saves the entire array of variables
	virtual void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave )
	{
		ISharedVarBase *pSharedVar = (ISharedVarBase *)fieldInfo.pField;
		pSharedVar->_Save( pSave );
	}

	// restores a single instance of the variable
	virtual void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore )
	{
		ISharedVarBase *pSharedVar = (ISharedVarBase *)fieldInfo.pField;
		pSharedVar->_Restore( pRestore );
	}


	virtual bool IsEmpty( const SaveRestoreFieldInfo_t &fieldInfo )
	{
		ISharedVarBase *pSharedVar = (ISharedVarBase *)fieldInfo.pField;
		return pSharedVar->_IsEmpty();
	}

	virtual void MakeEmpty( const SaveRestoreFieldInfo_t &fieldInfo )
	{
		ISharedVarBase *pSharedVar = (ISharedVarBase *)fieldInfo.pField;
		return pSharedVar->_MakeEmpty();
	}
};

static CSharedVarSaveDataOps g_SharedVarSaveDataOps;


#define DEFINE_SHARED_FIELD(name)	\
	{ FIELD_CUSTOM, #name, offsetof(classNameTypedef, name), 1, FTYPEDESC_SAVE, NULL, &g_SharedVarSaveDataOps, NULL }


#endif // SHAREDVAR_H
