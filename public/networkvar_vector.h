//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// Included by networkvar.h
//
//==================================================================================================

#ifndef NETWORKVAR_VECTOR_H
#define NETWORKVAR_VECTOR_H
#ifdef _WIN32
#pragma once
#endif


// This is the normal case.. you've got a SendPropVector to match your CNetworkVector
#define CNetworkVector( name ) CNetworkVectorInternal( Vector, name, NetworkStateChanged, CNetworkVectorBase )

// This variant of a CNetworkVector should be used if you want to use SendPropFloat
// on each individual component of the vector.
#define CNetworkVectorXYZ( name ) CNetworkVectorInternal( Vector, name, NetworkStateChanged, CNetworkVectorXYZBase )

// This variant of a CNetworkVector should be used if you want to use SendPropVectorXY
// for the XY components and SendPropFloat for the Z component.
#define CNetworkVectorXY_SeparateZ( name ) CNetworkVectorInternal( Vector, name, NetworkStateChanged, CNetworkVectorXY_SeparateZBase )



// This is the normal case.. you've got a SendPropQAngle to match your CNetworkQAngle
#define CNetworkQAngle( name ) CNetworkVectorInternal( QAngle, name, NetworkStateChanged, CNetworkVectorBase  )

// This variant of a CNetworkQAngle should be used if you want to use SendPropFloat
// on each individual component of the vector.
#define CNetworkQAngleXYZ( name ) CNetworkVectorInternal( QAngle, name, NetworkStateChanged, CNetworkVectorXYZBase )



//
// Use these variants if you want the networkvar to not trigger a change in the baseclass
// version but you might want it to trigger changes in derived classes that do network that variable.
//
#define CNetworkVectorForDerived( name ) \
	virtual void NetworkStateChanged_##name() {} \
	virtual void NetworkStateChanged_##name( void *pVar ) {} \
	CNetworkVectorInternal( Vector, name, NetworkStateChanged_##name, CNetworkVectorBase )
		
#define CNetworkVectorXYZForDerived( name ) \
	virtual void NetworkStateChanged_##name() {} \
	virtual void NetworkStateChanged_##name( void *pVar ) {} \
	CNetworkVectorInternal( Vector, name, NetworkStateChanged_##name, CNetworkVectorXYZBase )




#define CNetworkVectorInternal( type, name, stateChangedFn, baseClass ) \
	NETWORK_VAR_START( type, name ) \
	NETWORK_VAR_END( type, name, baseClass, stateChangedFn )



// Network vector wrapper.
//
// The common base is shared between all CNetworkVectors.
// It includes everything but the Set() and operator=() functions,
// because the behavior of each of those is different for each vector type.
template< class Type, class Changer >
class CNetworkVectorCommonBase : public CNetworkVarBase< Type, Changer >
{
	typedef CNetworkVarBase< Type, Changer > base;
public:

	FORCEINLINE  void Init( float ix=0, float iy=0, float iz=0 ) 
	{
		base::Set( Type( ix, iy, iz ) );
	}
	
	FORCEINLINE float GetX() const { return this->m_Value.x; }
	FORCEINLINE float GetY() const { return this->m_Value.y; }
	FORCEINLINE float GetZ() const { return this->m_Value.z; }
	FORCEINLINE float operator[]( int i ) const { return this->m_Value[i]; }

	FORCEINLINE bool operator==( const Type &val ) const 
	{ 
		return this->m_Value == (Type)val; 
	}

	FORCEINLINE bool operator!=( const Type &val ) const 
	{
		return this->m_Value != (Type)val; 
	}

	FORCEINLINE const Type operator+( const Type &val ) const 
	{
		return this->m_Value + val; 
	}

	FORCEINLINE const Type operator-( const Type &val ) const
	{ 
		return this->m_Value - val; 
	}

	FORCEINLINE const Type operator*( const Type &val ) const
	{
		return this->m_Value * val; 
	}

	FORCEINLINE const Type& operator*=( float val )
	{
		return base::Set( this->m_Value * val );
	}

	FORCEINLINE const Type operator*( float val ) const
	{
		return this->m_Value * val; 
	}

	FORCEINLINE const Type operator/( const Type &val ) const
	{
		return this->m_Value / val; 
	}

protected:
	FORCEINLINE void DetectChange( float &out, float in ) 
	{
		if ( out != in ) 
		{
			this->NetworkStateChanged();
			out = in;
		}
	}
};



//
// This is for a CNetworkVector that only generates one change offset.
// It should only ever be used with SendPropVector/QAngle.
//
// Single-component things like SendPropFloat should never refer to it because
// they require the network var to report an offset for each component.
//
template< class Type, class Changer >
class CNetworkVectorBase : public CNetworkVectorCommonBase< Type, Changer >
{
	typedef CNetworkVarBase< Type, Changer > base;
public:
	static FORCEINLINE int GetNetworkVarFlags() { return NETWORKVAR_IS_A_VECTOR; }

	FORCEINLINE const Type& operator=( const Type &val ) 
	{ 
		return base::Set( val ); 
	}

	FORCEINLINE const Type& operator=( const CNetworkVectorBase<Type,Changer> &val ) 
	{ 
		return base::Set( val.m_Value );
	}
	FORCEINLINE void SetX( float val ) { this->DetectChange( this->m_Value.x, val ); }
	FORCEINLINE void SetY( float val ) { this->DetectChange( this->m_Value.y, val ); }
	FORCEINLINE void SetZ( float val ) { this->DetectChange( this->m_Value.z, val ); }
	FORCEINLINE void Set( int i, float val ) { this->DetectChange( this->m_Value[i], val ); }

	FORCEINLINE const Type& operator*=( float val )
	{
		return base::Set( this->m_Value * val );
	}
};


//
// This variant of a CNetworkVector should be used if you want to use SendPropFloat
// on each individual component of the vector.
//
template< class Type, class Changer >
class CNetworkVectorXYZBase : public CNetworkVectorCommonBase< Type, Changer >
{
	typedef CNetworkVectorCommonBase< Type, Changer > base;
public:
	
	static FORCEINLINE int GetNetworkVarFlags() { return NETWORKVAR_IS_A_VECTOR | NETWORKVAR_VECTOR_XYZ_FLAG; }

	FORCEINLINE const Type& operator=( const Type &val ) 
	{ 
		return Set( val ); 
	}

	FORCEINLINE const Type& operator=( const CNetworkVectorBase<Type,Changer> &val ) 
	{ 
		return Set( val.m_Value );
	}

	FORCEINLINE const Type& Set( const Type &val )
	{
		SetX( val.x );
		SetY( val.y );
		SetZ( val.z );

		return this->m_Value;
	}
	
	FORCEINLINE Type& GetForModify()
	{
		this->NetworkStateChanged( &((float*)this)[0] );
		this->NetworkStateChanged( &((float*)this)[1] );
		this->NetworkStateChanged( &((float*)this)[2] );
		return this->m_Value;
	}

	FORCEINLINE const Type& SetDirect( const Type &val )
	{
		GetForModify() = val;
		return this->m_Value;
	}

	FORCEINLINE void SetX( float val ) { DetectChange( 0, val ); }
	FORCEINLINE void SetY( float val ) { DetectChange( 1, val ); }
	FORCEINLINE void SetZ( float val ) { DetectChange( 2, val ); }
	FORCEINLINE void Set( int i, float val ) { DetectChange( i, val ); }

	FORCEINLINE const Type& operator+=( const Type &val )
	{
		return Set( this->m_Value + val );
	}

	FORCEINLINE const Type& operator-=( const Type &val )
	{
		return Set( this->m_Value - val );
	}

	FORCEINLINE const Type& operator*=( float val )
	{
		return Set( this->m_Value * val );
	}

	FORCEINLINE const Type& operator/=( float val )
	{
		return Set( this->m_Value / val );
	}

private:
	FORCEINLINE void DetectChange( int nComponent, float in ) 
	{
		float *pVar = &((float*)this)[nComponent];
		if ( *pVar != in ) 
		{
			if ( pVar != &((float*)this)[0] )
			{
				this->NetworkStateChanged( &((float*)this)[0] ); // Always mark the start of the vector as changed
			}
			this->NetworkStateChanged( pVar );
			*pVar = in;
		}
	}
};



//
// This variant of a CNetworkVector should be used if you want to use SendPropVectorXY
// for the XY components and SendPropFloat for the Z component.
//
template< class Type, class Changer >
class CNetworkVectorXY_SeparateZBase : public CNetworkVectorCommonBase< Type, Changer >
{
	typedef CNetworkVectorCommonBase< Type, Changer > base;
public:
	
	static FORCEINLINE int GetNetworkVarFlags() { return NETWORKVAR_IS_A_VECTOR | NETWORKVAR_VECTOR_XY_SEPARATEZ_FLAG; }

	FORCEINLINE const Type& operator=( const Type &val ) 
	{ 
		return Set( val ); 
	}

	FORCEINLINE const Type& operator=( const CNetworkVectorBase<Type,Changer> &val ) 
	{ 
		return Set( val.m_Value );
	}

	FORCEINLINE const Type& Set( const Type &val )
	{
		SetX( val.x );
		SetY( val.y );
		SetZ( val.z );

		return this->m_Value;
	}
	
	FORCEINLINE Type& GetForModify()
	{
		this->NetworkStateChanged( &((float*)this)[0] );	// Mark the offset of our XY SendProp as changed.
		this->NetworkStateChanged( &((float*)this)[2] );	// Mark the offset of our Z SendProp as changed.
		return this->m_Value;
	}

	FORCEINLINE const Type& SetDirect( const Type &val )
	{
		GetForModify() = val;
		return this->m_Value;
	}

	FORCEINLINE void SetX( float val ) { DetectChange( 0, val ); }
	FORCEINLINE void SetY( float val ) { DetectChange( 1, val ); }
	FORCEINLINE void SetZ( float val ) { DetectChange( 2, val ); }
	FORCEINLINE void Set( int i, float val ) { DetectChange( i, val ); }

	FORCEINLINE const Type& operator+=( const Type &val )
	{
		return Set( this->m_Value + val );
	}

	FORCEINLINE const Type& operator-=( const Type &val )
	{
		return Set( this->m_Value - val );
	}

	FORCEINLINE const Type& operator*=( float val )
	{
		return Set( this->m_Value * val );
	}

	FORCEINLINE const Type& operator/=( float val )
	{
		return Set( this->m_Value / val );
	}

private:
	FORCEINLINE void DetectChange( int nComponent, float in ) 
	{
		float *pVar = &((float*)this)[nComponent];
		if ( *pVar != in ) 
		{
			this->NetworkStateChanged( &((float*)this)[0] );	// Mark the offset of our XY SendProp as changed.
			this->NetworkStateChanged( &((float*)this)[2] );	// Mark the offset of our Z SendProp as changed.

			*pVar = in;
		}
	}
};


#endif // NETWORKVAR_VECTOR_H

