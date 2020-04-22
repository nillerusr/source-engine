//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef TRANSFORMATIONS_H
#define TRANSFORMATIONS_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/mathlib.h"
#include "mathlib/vmatrix.h"
#include "tier1/utlbuffer.h"
#include "panorama.h"
#include "panoramatypes.h"
#include "layout/uilength.h"

namespace panorama
{

class CTransform3D
{
public:
	virtual ~CTransform3D() {}

	virtual ETransform3DType GetType() const = 0;
	virtual VMatrix GetTransformMatrix( float flParentWidth, float flParentHeight ) const = 0;
	virtual CTransform3D *Clone() const = 0;
	virtual void ScaleLengthValues( float flScaleFactor ) = 0;
	virtual bool BOnlyImpacts2DValues() = 0;

	virtual bool operator==( const CTransform3D &rhs ) const = 0;
	bool operator!=( const CTransform3D &rhs ) const { return !(*this == rhs); }
};

// Helpers for interpolating 3d transform matrixes
struct DecomposedMatrix_t
{
	DecomposedMatrix_t() 
	{ 
		m_flTranslationXYZ[0] = 0.0f;
		m_flTranslationXYZ[1] = 0.0f;
		m_flTranslationXYZ[2] = 0.0f;
		m_flScaleXYZ[0] = 1.0;
		m_flScaleXYZ[1] = 1.0;
		m_flScaleXYZ[2] = 1.0;
		m_flSkewXY = 0.0f;
		m_flSkewXZ = 0.0f;
		m_flSkewYZ = 0.0f;

		m_quatTransform.Init();
	}
	float m_flTranslationXYZ[3];
	float m_flScaleXYZ[3];
	float m_flSkewXY;
	float m_flSkewXZ;
	float m_flSkewYZ;
	Quaternion m_quatTransform;


};
DecomposedMatrix_t DecomposeTransformMatrix( VMatrix matrix );
VMatrix RecomposeTransformMatrix( const DecomposedMatrix_t &decomposed );
VMatrix InterpolateTransformMatrix( VMatrix from, VMatrix to, float flTimeProgress );

class CTransformRotate3D : public CTransform3D
{
public:
	// Transform in 3D space:
	// PITCH: Clockwise rotation around the Y axis.
	// YAW: Counterclockwise rotation around the Z axis.
	// ROLL: Counterclockwise rotation around the X axis.
	CTransformRotate3D( float flDegreesPitch, float flDegreesYaw, float flDegreesRoll )
	{
		QAngle angle( flDegreesPitch, flDegreesYaw, flDegreesRoll );
		AngleQuaternion( angle, m_quatTransform );
	}

	ETransform3DType GetType() const { return k_ETransform3DRotate; }

	VMatrix GetTransformMatrix( float flParentWidth, float flParentHeight ) const
	{ 
		matrix3x4_t mat;
		QuaternionMatrix( m_quatTransform, mat );
		return VMatrix( mat );
	}

	bool BOnlyImpacts2DValues()
	{
		return false;
	}

	virtual CTransform3D *Clone() const
	{
		CTransformRotate3D *pRet = new CTransformRotate3D();
		pRet->m_quatTransform = m_quatTransform;

		return pRet;
	}

	QAngle GetAngles() const
	{
		QAngle angle;
		QuaternionAngles( m_quatTransform, angle );

		return angle;
	}

	virtual void ScaleLengthValues( float flScaleFactor ) { }

	virtual bool operator==( const CTransform3D &other ) const
	{
		if ( other.GetType() != k_ETransform3DRotate )
			return false;

		const CTransformRotate3D &rhs = (const CTransformRotate3D &)other;
		return (m_quatTransform == rhs.m_quatTransform);
	}

protected:
	CTransformRotate3D() {}

private:

	Quaternion m_quatTransform;
};

class CTransformTranslate3D : public CTransform3D
{
public:
	// Translate in 3D space
	CTransformTranslate3D( float x, float y, float z )
	{
		m_x.SetLength( x );
		m_y.SetLength( y );
		m_z.SetLength( z );
	}

	CTransformTranslate3D( const CUILength &x, const CUILength &y, const CUILength &z )
	{
		m_x = x;
		m_y = y;
		m_z = z;
	}

	bool BOnlyImpacts2DValues()
	{
		if( fabs( m_z.GetValueAsLength( 100 ) - 0.0f ) < 0.0001f )
			return true;

		return false;
	}

	ETransform3DType GetType() const { return k_ETransform3DTranslate; }

	VMatrix GetTransformMatrix( float flParentWidth, float flParentHeight ) const
	{ 

		VMatrix mat = VMatrix::GetIdentityMatrix();
		mat.SetTranslation( Vector( m_x.GetValueAsLength( flParentWidth ), m_y.GetValueAsLength( flParentHeight ), m_z.GetValueAsLength( 0 ) ) );
		return mat;
	}

	virtual CTransform3D *Clone() const
	{
		return new CTransformTranslate3D( m_x, m_y, m_z );
	}

	virtual void ScaleLengthValues( float flScaleFactor ) 
	{
		m_x.ScaleLengthValue( flScaleFactor );
		m_y.ScaleLengthValue( flScaleFactor );
		m_z.ScaleLengthValue( flScaleFactor );
	}

	CUILength GetX() const { return m_x; }
	CUILength GetY() const { return m_y; }
	CUILength GetZ() const { return m_z; }

	virtual bool operator==( const CTransform3D &other ) const
	{
		if ( other.GetType() != k_ETransform3DTranslate )
			return false;

		const CTransformTranslate3D &rhs = (const CTransformTranslate3D &)other;
		return ( m_x == rhs.m_x && m_y == rhs.m_y && m_z == rhs.m_z );
	}

private:
	CUILength m_x;
	CUILength m_y;
	CUILength m_z;
};


class CTransformScale3D : public CTransform3D
{
public:
	// Scale in 3D space
	CTransformScale3D( float x, float y, float z ) : m_VecScale( x, y, z )
	{
	}

	ETransform3DType GetType() const { return k_ETransform3DScale; }

	bool BOnlyImpacts2DValues()
	{
		return false;
	}

	VMatrix GetTransformMatrix( float flParentWidth, float flParentHeight ) const
	{ 
		VMatrix mat;
		MatrixBuildScale( mat, m_VecScale.x, m_VecScale.y, m_VecScale.z );
		return mat;
	}

	virtual CTransform3D *Clone() const
	{
		return new CTransformScale3D( m_VecScale.x, m_VecScale.y, m_VecScale.z );
	}

	virtual void ScaleLengthValues( float flScaleFactor ) { }

	float GetX() const { return m_VecScale.x; }
	float GetY() const { return m_VecScale.y; }
	float GetZ() const { return m_VecScale.z; }

	virtual bool operator==( const CTransform3D &other ) const
	{
		if ( other.GetType() != k_ETransform3DScale )
			return false;

		const CTransformScale3D &rhs = (const CTransformScale3D &)other;
		return ( m_VecScale == rhs.m_VecScale );
	}

private:
	Vector m_VecScale;
};

} // namespace panorama

#endif // TRANSFORMATIONS_H