//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// VMatrix always postmultiply vectors as in Ax = b.
// Given a set of basis vectors ((F)orward, (L)eft, (U)p), and a (T)ranslation, 
// a matrix to transform a vector into that space looks like this:
// Fx Lx Ux Tx
// Fy Ly Uy Ty
// Fz Lz Uz Tz
// 0   0  0  1

// Note that concatenating matrices needs to multiply them in reverse order.
// ie: if I want to apply matrix A, B, then C, the equation needs to look like this:
// C * B * A * v
// ie:
// v = A * v;
// v = B * v;
// v = C * v;
//=============================================================================

#ifndef VMATRIX_H
#define VMATRIX_H

#ifdef _WIN32
#pragma once
#endif

#include <math.h>
#include <string.h>
#include "mathlib/vector.h"
#include "mathlib/vplane.h"
#include "mathlib/vector4d.h"
#include "mathlib/mathlib.h"

struct cplane_t;

#ifndef M_PI
	#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

class alignas(16) VMatrix
{
public:

	VMatrix();
	VMatrix(
		vec_t m00, vec_t m01, vec_t m02, vec_t m03,
		vec_t m10, vec_t m11, vec_t m12, vec_t m13,
		vec_t m20, vec_t m21, vec_t m22, vec_t m23,
		vec_t m30, vec_t m31, vec_t m32, vec_t m33
		);

	// Creates a matrix where the X axis = forward
	// the Y axis = left, and the Z axis = up
	VMatrix( const Vector& forward, const Vector& left, const Vector& up );
	VMatrix( const Vector& forward, const Vector& left, const Vector& up, const Vector& translation );
	
	// Construct from a 3x4 matrix
	VMatrix( const matrix3x4_t& matrix3x4 );
	VMatrix( const VMatrix& ) = default;

	// Set the values in the matrix.
	void		Init( 
		vec_t m00, vec_t m01, vec_t m02, vec_t m03,
		vec_t m10, vec_t m11, vec_t m12, vec_t m13,
		vec_t m20, vec_t m21, vec_t m22, vec_t m23,
		vec_t m30, vec_t m31, vec_t m32, vec_t m33 
		);


	// Initialize from a 3x4
	void		Init( const matrix3x4_t& matrix3x4 );

	// array access
	inline float* operator[](int i)
	{ 
		return m[i]; 
	}

	inline const float* operator[](int i) const
	{ 
		return m[i]; 
	}

	// Get a pointer to m[0][0]
	inline float *Base()
	{
		return &m[0][0];
	}

	inline const float *Base() const
	{
		return &m[0][0];
	}

	void		SetLeft(const Vector &vLeft);
	void		SetUp(const Vector &vUp);
	void		SetForward(const Vector &vForward);

	void		GetBasisVectors(Vector &vForward, Vector &vLeft, Vector &vUp) const;
	void		SetBasisVectors(const Vector &vForward, const Vector &vLeft, const Vector &vUp);

	// Get/set the translation.
	Vector &	GetTranslation( Vector &vTrans ) const;
	void		SetTranslation(const Vector &vTrans);

	void		PreTranslate(const Vector &vTrans);
	void		PostTranslate(const Vector &vTrans);

	const matrix3x4_t& As3x4() const;
	void		CopyFrom3x4( const matrix3x4_t &m3x4 );
	void		Set3x4( const matrix3x4_t& matrix3x4 );

	bool		operator==( const VMatrix& src ) const {
		return src.m[0][0] == m[0][0] && src.m[0][1] == m[0][1] && src.m[0][2] == m[0][2] && src.m[0][3] == m[0][3] &&
			src.m[1][0] == m[1][0] && src.m[1][1] == m[1][1] && src.m[1][2] == m[1][2] && src.m[1][3] == m[1][3] &&
			src.m[2][0] == m[2][0] && src.m[2][1] == m[2][1] && src.m[2][2] == m[2][2] && src.m[2][3] == m[2][3] &&
			src.m[3][0] == m[3][0] && src.m[3][1] == m[3][1] && src.m[3][2] == m[3][2] && src.m[3][3] == m[3][3];
	}
	
	bool		operator!=( const VMatrix& src ) const { return !( *this == src ); }

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// Access the basis vectors.
	Vector		GetLeft() const;
	Vector		GetUp() const;
	Vector		GetForward() const;
	Vector		GetTranslation() const;
#endif


// Matrix->vector operations.
public:
	// Multiply by a 3D vector (same as operator*).
	void		V3Mul(const Vector &vIn, Vector &vOut) const;

	// Multiply by a 4D vector.
	void		V4Mul(const Vector4D &vIn, Vector4D &vOut) const;

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// Applies the rotation (ignores translation in the matrix). (This just calls VMul3x3).
	Vector		ApplyRotation(const Vector &vVec) const;

	// Multiply by a vector (divides by w, assumes input w is 1).
	Vector		operator*(const Vector &vVec) const;

	// Multiply by the upper 3x3 part of the matrix (ie: only apply rotation).
	Vector		VMul3x3(const Vector &vVec) const;

	// Apply the inverse (transposed) rotation (only works on pure rotation matrix)
	Vector		VMul3x3Transpose(const Vector &vVec) const;

	// Multiply by the upper 3 rows.
	Vector		VMul4x3(const Vector &vVec) const;

	// Apply the inverse (transposed) transformation (only works on pure rotation/translation)
	Vector		VMul4x3Transpose(const Vector &vVec) const;
#endif


// Matrix->plane operations.
public:
	// Transform the plane. The matrix can only contain translation and rotation.
	void		TransformPlane( const VPlane &inPlane, VPlane &outPlane ) const;

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// Just calls TransformPlane and returns the result.
	VPlane		operator*(const VPlane &thePlane) const;
#endif

// Matrix->matrix operations.
public:

	VMatrix&	operator=(const VMatrix &mOther);
	
	// Multiply two matrices (out = this * vm).
	void		MatrixMul( const VMatrix &vm, VMatrix &out ) const;

	// Add two matrices.
	const VMatrix& operator+=(const VMatrix &other);

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// Just calls MatrixMul and returns the result.	
	VMatrix		operator*(const VMatrix &mOther) const;

	// Add/Subtract two matrices.
	VMatrix		operator+(const VMatrix &other) const;
	VMatrix		operator-(const VMatrix &other) const;

	// Negation.
	VMatrix		operator-() const;

	// Return inverse matrix. Be careful because the results are undefined 
	// if the matrix doesn't have an inverse (ie: InverseGeneral returns false).
	VMatrix		operator~() const;
#endif

// Matrix operations.
public:
	// Set to identity.
	void		Identity();

	bool		IsIdentity() const;

	// Setup a matrix for origin and angles.
	void		SetupMatrixOrgAngles( const Vector &origin, const QAngle &vAngles );
	
	// Setup a matrix for angles and no translation.
	void		SetupMatrixAngles( const QAngle &vAngles );

	// General inverse. This may fail so check the return!
	bool		InverseGeneral(VMatrix &vInverse) const;
	
	// Does a fast inverse, assuming the matrix only contains translation and rotation.
	void		InverseTR( VMatrix &mRet ) const;

	// Usually used for debug checks. Returns true if the upper 3x3 contains
	// unit vectors and they are all orthogonal.
	bool		IsRotationMatrix() const;
	
#ifndef VECTOR_NO_SLOW_OPERATIONS
	// This calls the other InverseTR and returns the result.
	VMatrix		InverseTR() const;

	// Get the scale of the matrix's basis vectors.
	Vector		GetScale() const;

	// (Fast) multiply by a scaling matrix setup from vScale.
	VMatrix		Scale(const Vector &vScale);	

	// Normalize the basis vectors.
	VMatrix		NormalizeBasisVectors() const;

	// Transpose.
	VMatrix		Transpose() const;

	// Transpose upper-left 3x3.
	VMatrix		Transpose3x3() const;
#endif

public:
	// The matrix.
	vec_t m[4][4];
};

inline void MatrixSetColumn( VMatrix &src, int nCol, const Vector &column )
{
	Assert( (nCol >= 0) && (nCol <= 3) );

	src.m[0][nCol] = column.x;
	src.m[1][nCol] = column.y;
	src.m[2][nCol] = column.z;
}

//-----------------------------------------------------------------------------
// Vector3DMultiplyPosition treats src2 as if it's a point (adds the translation)
//-----------------------------------------------------------------------------
// NJS: src2 is passed in as a full vector rather than a reference to prevent the need
// for 2 branches and a potential copy in the body.  (ie, handling the case when the src2
// reference is the same as the dst reference ).
inline void Vector3DMultiplyPosition( const VMatrix& src1, const VectorByValue src2, Vector& dst )
{
	dst[0] = src1[0][0] * src2.x + src1[0][1] * src2.y + src1[0][2] * src2.z + src1[0][3];
	dst[1] = src1[1][0] * src2.x + src1[1][1] * src2.y + src1[1][2] * src2.z + src1[1][3];
	dst[2] = src1[2][0] * src2.x + src1[2][1] * src2.y + src1[2][2] * src2.z + src1[2][3];
}

//-----------------------------------------------------------------------------
// Sets matrix to identity
//-----------------------------------------------------------------------------
inline void MatrixSetIdentity( VMatrix &dst )
{
	dst[0][0] = 1.0f; dst[0][1] = 0.0f; dst[0][2] = 0.0f; dst[0][3] = 0.0f;
	dst[1][0] = 0.0f; dst[1][1] = 1.0f; dst[1][2] = 0.0f; dst[1][3] = 0.0f;
	dst[2][0] = 0.0f; dst[2][1] = 0.0f; dst[2][2] = 1.0f; dst[2][3] = 0.0f;
	dst[3][0] = 0.0f; dst[3][1] = 0.0f; dst[3][2] = 0.0f; dst[3][3] = 1.0f;
}

//-----------------------------------------------------------------------------
// Matrix/vector multiply
//-----------------------------------------------------------------------------
inline void Vector3DMultiply( const VMatrix &src1, const Vector &src2, Vector &dst )
{
	// Make sure it works if src2 == dst
	Vector tmp;
	const Vector &v = (&src2 == &dst) ?  static_cast<const Vector&>(tmp) : src2;

	if( &src2 == &dst )
		VectorCopy( src2, tmp );

	dst[0] = src1[0][0] * v[0] + src1[0][1] * v[1] + src1[0][2] * v[2];
	dst[1] = src1[1][0] * v[0] + src1[1][1] * v[1] + src1[1][2] * v[2];
	dst[2] = src1[2][0] * v[0] + src1[2][1] * v[1] + src1[2][2] * v[2];
}


inline bool MatrixInverseGeneral(const VMatrix& src, VMatrix& dst)
{
	int iRow, i, j, iTemp, iTest;
	vec_t mul, fTest, fLargest;
	vec_t mat[4][8];
	int rowMap[4], iLargest;
	vec_t *pOut, *pRow, *pScaleRow;


	// How it's done.
	// AX = I
	// A = this
	// X = the matrix we're looking for
	// I = identity

	// Setup AI
	for(i=0; i < 4; i++)
	{
		const vec_t *pIn = src[i];
		pOut = mat[i];

		for(j=0; j < 4; j++)
		{
			pOut[j] = pIn[j];
		}

		pOut[4] = 0.0f;
		pOut[5] = 0.0f;
		pOut[6] = 0.0f;
		pOut[7] = 0.0f;
		pOut[i+4] = 1.0f;

		rowMap[i] = i;
	}

	// Use row operations to get to reduced row-echelon form using these rules:
	// 1. Multiply or divide a row by a nonzero number.
	// 2. Add a multiple of one row to another.
	// 3. Interchange two rows.

	for(iRow=0; iRow < 4; iRow++)
	{
		// Find the row with the largest element in this column.
		fLargest = 0.00001f;
		iLargest = -1;
		for(iTest=iRow; iTest < 4; iTest++)
		{
			fTest = (vec_t)FloatMakePositive(mat[rowMap[iTest]][iRow]);
			if(fTest > fLargest)
			{
				iLargest = iTest;
				fLargest = fTest;
			}
		}

		// They're all too small.. sorry.
		if(iLargest == -1)
		{
			return false;
		}

		// Swap the rows.
		iTemp = rowMap[iLargest];
		rowMap[iLargest] = rowMap[iRow];
		rowMap[iRow] = iTemp;

		pRow = mat[rowMap[iRow]];

		// Divide this row by the element.
		mul = 1.0f / pRow[iRow];
		for(j=0; j < 8; j++)
			pRow[j] *= mul;

		pRow[iRow] = 1.0f; // Preserve accuracy...
		
		// Eliminate this element from the other rows using operation 2.
		for(i=0; i < 4; i++)
		{
			if(i == iRow)
				continue;

			pScaleRow = mat[rowMap[i]];
		
			// Multiply this row by -(iRow*the element).
			mul = -pScaleRow[iRow];
			for(j=0; j < 8; j++)
			{
				pScaleRow[j] += pRow[j] * mul;
			}

			pScaleRow[iRow] = 0.0f; // Preserve accuracy...
		}
	}

	// The inverse is on the right side of AX now (the identity is on the left).
	for(i=0; i < 4; i++)
	{
		const vec_t *pIn = mat[rowMap[i]] + 4;
		pOut = dst.m[i];

		for(j=0; j < 4; j++)
		{
			pOut[j] = pIn[j];
		}
	}

	return true;
}

static inline void SetupMatrixAnglesInternal( vec_t m[4][4], const QAngle & vAngles )
{
	float		sr, sp, sy, cr, cp, cy;

	SinCos( DEG2RAD( vAngles[YAW] ), &sy, &cy );
	SinCos( DEG2RAD( vAngles[PITCH] ), &sp, &cp );
	SinCos( DEG2RAD( vAngles[ROLL] ), &sr, &cr );

	// matrix = (YAW * PITCH) * ROLL
	m[0][0] = cp*cy;
	m[1][0] = cp*sy;
	m[2][0] = -sp;
	m[0][1] = sr*sp*cy+cr*-sy;
	m[1][1] = sr*sp*sy+cr*cy;
	m[2][1] = sr*cp;
	m[0][2] = (cr*sp*cy+-sr*-sy);
	m[1][2] = (cr*sp*sy+-sr*cy);
	m[2][2] = cr*cp;
	m[0][3] = 0.f;
	m[1][3] = 0.f;
	m[2][3] = 0.f;
}


//-----------------------------------------------------------------------------
// Transpose
//-----------------------------------------------------------------------------
inline void Swap( float& a, float& b )
{
	float tmp = a;
	a = b;
	b = tmp;
}

inline void MatrixTranspose( const VMatrix& src, VMatrix& dst )
{
	if (&src == &dst)
	{
		Swap( dst[0][1], dst[1][0] );
		Swap( dst[0][2], dst[2][0] );
		Swap( dst[0][3], dst[3][0] );
		Swap( dst[1][2], dst[2][1] );
		Swap( dst[1][3], dst[3][1] );
		Swap( dst[2][3], dst[3][2] );
	}
	else
	{
		dst[0][0] = src[0][0]; dst[0][1] = src[1][0]; dst[0][2] = src[2][0]; dst[0][3] = src[3][0];
		dst[1][0] = src[0][1]; dst[1][1] = src[1][1]; dst[1][2] = src[2][1]; dst[1][3] = src[3][1];
		dst[2][0] = src[0][2]; dst[2][1] = src[1][2]; dst[2][2] = src[2][2]; dst[2][3] = src[3][2];
		dst[3][0] = src[0][3]; dst[3][1] = src[1][3]; dst[3][2] = src[2][3]; dst[3][3] = src[3][3];
	}
}

//-----------------------------------------------------------------------------
// Does a fast inverse, assuming the matrix only contains translation and rotation.
//-----------------------------------------------------------------------------
inline void MatrixInverseTR( const VMatrix& src, VMatrix &dst )
{
	Vector vTrans, vNewTrans;

	// Transpose the upper 3x3.
	dst.m[0][0] = src.m[0][0];  dst.m[0][1] = src.m[1][0]; dst.m[0][2] = src.m[2][0];
	dst.m[1][0] = src.m[0][1];  dst.m[1][1] = src.m[1][1]; dst.m[1][2] = src.m[2][1];
	dst.m[2][0] = src.m[0][2];  dst.m[2][1] = src.m[1][2]; dst.m[2][2] = src.m[2][2];

	// Transform the translation.
	vTrans.Init( -src.m[0][3], -src.m[1][3], -src.m[2][3] );
	Vector3DMultiply( dst, vTrans, vNewTrans );
	MatrixSetColumn( dst, 3, vNewTrans );

	// Fill in the bottom row.
	dst.m[3][0] = dst.m[3][1] = dst.m[3][2] = 0.0f;
	dst.m[3][3] = 1.0f;
}

inline void VMatrix::Init(
	vec_t m00, vec_t m01, vec_t m02, vec_t m03,
	vec_t m10, vec_t m11, vec_t m12, vec_t m13,
	vec_t m20, vec_t m21, vec_t m22, vec_t m23,
	vec_t m30, vec_t m31, vec_t m32, vec_t m33
	)
{
	m[0][0] = m00;
	m[0][1] = m01;
	m[0][2] = m02;
	m[0][3] = m03;

	m[1][0] = m10;
	m[1][1] = m11;
	m[1][2] = m12;
	m[1][3] = m13;

	m[2][0] = m20;
	m[2][1] = m21;
	m[2][2] = m22;
	m[2][3] = m23;

	m[3][0] = m30;
	m[3][1] = m31;
	m[3][2] = m32;
	m[3][3] = m33;
}


//-----------------------------------------------------------------------------
// Initialize from a 3x4
//-----------------------------------------------------------------------------
inline void VMatrix::Init( const matrix3x4_t& _m )
{
	m[0][0] = _m[0][0]; m[0][1] = _m[0][1]; m[0][2] = _m[0][2]; m[0][3] = _m[0][3]; 
	m[1][0] = _m[1][0]; m[1][1] = _m[1][1]; m[1][2] = _m[1][2]; m[1][3] = _m[1][3]; 
	m[2][0] = _m[2][0]; m[2][1] = _m[2][1]; m[2][2] = _m[2][2]; m[2][3] = _m[2][3]; 
	m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;	
}

//-----------------------------------------------------------------------------
// VMatrix inlines.
//-----------------------------------------------------------------------------
inline VMatrix::VMatrix()
{
	Init(
		0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f
		);
}

inline VMatrix::VMatrix(
	vec_t m00, vec_t m01, vec_t m02, vec_t m03,
	vec_t m10, vec_t m11, vec_t m12, vec_t m13,
	vec_t m20, vec_t m21, vec_t m22, vec_t m23,
	vec_t m30, vec_t m31, vec_t m32, vec_t m33)
{
	Init(
		m00, m01, m02, m03,
		m10, m11, m12, m13,
		m20, m21, m22, m23,
		m30, m31, m32, m33
		);
}


inline VMatrix::VMatrix( const matrix3x4_t& matrix3x4 )
{
	Init( matrix3x4 );
}


//-----------------------------------------------------------------------------
// Creates a matrix where the X axis = forward
// the Y axis = left, and the Z axis = up
//-----------------------------------------------------------------------------
inline VMatrix::VMatrix( const Vector& xAxis, const Vector& yAxis, const Vector& zAxis )
{
	Init(
		xAxis.x, yAxis.x, zAxis.x, 0.0f,
		xAxis.y, yAxis.y, zAxis.y, 0.0f,
		xAxis.z, yAxis.z, zAxis.z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
}

inline VMatrix::VMatrix( const Vector& xAxis, const Vector& yAxis, const Vector& zAxis, const Vector& translation )
{
	Init(
		xAxis.x, yAxis.x, zAxis.x, translation.x,
		xAxis.y, yAxis.y, zAxis.y, translation.y,
		xAxis.z, yAxis.z, zAxis.z, translation.z,
		0.0f, 0.0f, 0.0f, 1.0f
		);
}

//-----------------------------------------------------------------------------
// Methods related to the basis vectors of the matrix
//-----------------------------------------------------------------------------

inline VMatrix& VMatrix::operator=(const VMatrix &mOther)
{
	m[0][0] = mOther.m[0][0];
	m[0][1] = mOther.m[0][1];
	m[0][2] = mOther.m[0][2];
	m[0][3] = mOther.m[0][3];

	m[1][0] = mOther.m[1][0];
	m[1][1] = mOther.m[1][1];
	m[1][2] = mOther.m[1][2];
	m[1][3] = mOther.m[1][3];

	m[2][0] = mOther.m[2][0];
	m[2][1] = mOther.m[2][1];
	m[2][2] = mOther.m[2][2];
	m[2][3] = mOther.m[2][3];

	m[3][0] = mOther.m[3][0];
	m[3][1] = mOther.m[3][1];
	m[3][2] = mOther.m[3][2];
	m[3][3] = mOther.m[3][3];

	return *this;
}


#ifndef VECTOR_NO_SLOW_OPERATIONS

inline VMatrix VMatrix::operator+(const VMatrix &other) const
{
	VMatrix ret;
	for(int i=0; i < 16; i++)
	{
		((float*)ret.m)[i] = ((float*)m)[i] + ((float*)other.m)[i];
	}
	return ret;
}

inline VMatrix VMatrix::operator-(const VMatrix &other) const
{
	VMatrix ret;
	for(int i=0; i < 16; i++)
	{
		((float*)ret.m)[i] = ((float*)m)[i] - ((float*)other.m)[i];
	}
	return ret;
}

inline VMatrix VMatrix::operator-() const
{
	VMatrix ret;
	for( int i=0; i < 16; i++ )
	{
		((float*)ret.m)[i] = ((float*)m)[i];
	}
	return ret;
}

//-----------------------------------------------------------------------------
// Matrix math operations
//-----------------------------------------------------------------------------
inline const VMatrix& VMatrix::operator+=(const VMatrix &other)
{
	for(int i=0; i < 16; i++)
	{
		((float*)m)[i] += ((float*)other.m)[i];
	}
	return *this;
}


inline Vector VMatrix::operator*(const Vector &vVec) const
{
	Vector vRet;
	vRet.x = m[0][0]*vVec.x + m[0][1]*vVec.y + m[0][2]*vVec.z + m[0][3];
	vRet.y = m[1][0]*vVec.x + m[1][1]*vVec.y + m[1][2]*vVec.z + m[1][3];
	vRet.z = m[2][0]*vVec.x + m[2][1]*vVec.y + m[2][2]*vVec.z + m[2][3];

	return vRet;
}

//-----------------------------------------------------------------------------
// Plane transformation
//-----------------------------------------------------------------------------
inline void	VMatrix::TransformPlane( const VPlane &inPlane, VPlane &outPlane ) const
{
	Vector vTrans;
	Vector3DMultiply( *this, inPlane.m_Normal, outPlane.m_Normal );
	outPlane.m_Dist = inPlane.m_Dist * DotProduct( outPlane.m_Normal, outPlane.m_Normal );
	outPlane.m_Dist += DotProduct( outPlane.m_Normal, GetTranslation( vTrans ) );
}

inline VPlane VMatrix::operator*(const VPlane &thePlane) const
{
	VPlane ret;
	TransformPlane( thePlane, ret );
	return ret;
}

inline bool VMatrix::InverseGeneral(VMatrix &vInverse) const
{
	return MatrixInverseGeneral( *this, vInverse );
}

inline VMatrix VMatrix::operator~() const
{
	VMatrix mRet;
	InverseGeneral(mRet);
	return mRet;
}

inline void VMatrix::MatrixMul( const VMatrix &vm, VMatrix &out ) const
{
	out.Init(
		m[0][0]*vm.m[0][0] + m[0][1]*vm.m[1][0] + m[0][2]*vm.m[2][0] + m[0][3]*vm.m[3][0],
		m[0][0]*vm.m[0][1] + m[0][1]*vm.m[1][1] + m[0][2]*vm.m[2][1] + m[0][3]*vm.m[3][1],
		m[0][0]*vm.m[0][2] + m[0][1]*vm.m[1][2] + m[0][2]*vm.m[2][2] + m[0][3]*vm.m[3][2],
		m[0][0]*vm.m[0][3] + m[0][1]*vm.m[1][3] + m[0][2]*vm.m[2][3] + m[0][3]*vm.m[3][3],

		m[1][0]*vm.m[0][0] + m[1][1]*vm.m[1][0] + m[1][2]*vm.m[2][0] + m[1][3]*vm.m[3][0],
		m[1][0]*vm.m[0][1] + m[1][1]*vm.m[1][1] + m[1][2]*vm.m[2][1] + m[1][3]*vm.m[3][1],
		m[1][0]*vm.m[0][2] + m[1][1]*vm.m[1][2] + m[1][2]*vm.m[2][2] + m[1][3]*vm.m[3][2],
		m[1][0]*vm.m[0][3] + m[1][1]*vm.m[1][3] + m[1][2]*vm.m[2][3] + m[1][3]*vm.m[3][3],

		m[2][0]*vm.m[0][0] + m[2][1]*vm.m[1][0] + m[2][2]*vm.m[2][0] + m[2][3]*vm.m[3][0],
		m[2][0]*vm.m[0][1] + m[2][1]*vm.m[1][1] + m[2][2]*vm.m[2][1] + m[2][3]*vm.m[3][1],
		m[2][0]*vm.m[0][2] + m[2][1]*vm.m[1][2] + m[2][2]*vm.m[2][2] + m[2][3]*vm.m[3][2],
		m[2][0]*vm.m[0][3] + m[2][1]*vm.m[1][3] + m[2][2]*vm.m[2][3] + m[2][3]*vm.m[3][3],

		m[3][0]*vm.m[0][0] + m[3][1]*vm.m[1][0] + m[3][2]*vm.m[2][0] + m[3][3]*vm.m[3][0],
		m[3][0]*vm.m[0][1] + m[3][1]*vm.m[1][1] + m[3][2]*vm.m[2][1] + m[3][3]*vm.m[3][1],
		m[3][0]*vm.m[0][2] + m[3][1]*vm.m[1][2] + m[3][2]*vm.m[2][2] + m[3][3]*vm.m[3][2],
		m[3][0]*vm.m[0][3] + m[3][1]*vm.m[1][3] + m[3][2]*vm.m[2][3] + m[3][3]*vm.m[3][3]
		);
}

inline VMatrix VMatrix::operator*(const VMatrix &vm) const
{
	VMatrix ret;
	MatrixMul( vm, ret );
	return ret;
}

inline Vector VMatrix::GetForward() const
{
	return Vector(m[0][0], m[1][0], m[2][0]);
}

inline Vector VMatrix::GetLeft() const
{
	return Vector(m[0][1], m[1][1], m[2][1]);
}

inline Vector VMatrix::GetUp() const
{
	return Vector(m[0][2], m[1][2], m[2][2]);
}

#endif

inline void VMatrix::SetForward(const Vector &vForward)
{
	m[0][0] = vForward.x;
	m[1][0] = vForward.y;
	m[2][0] = vForward.z;
}

inline void VMatrix::SetLeft(const Vector &vLeft)
{
	m[0][1] = vLeft.x;
	m[1][1] = vLeft.y;
	m[2][1] = vLeft.z;
}

inline void VMatrix::SetUp(const Vector &vUp)
{
	m[0][2] = vUp.x;
	m[1][2] = vUp.y;
	m[2][2] = vUp.z;
}

inline void VMatrix::GetBasisVectors(Vector &vForward, Vector &vLeft, Vector &vUp) const
{
	vForward.Init( m[0][0], m[1][0], m[2][0] );
	vLeft.Init( m[0][1], m[1][1], m[2][1] );
	vUp.Init( m[0][2], m[1][2], m[2][2] );
}

inline void VMatrix::SetBasisVectors(const Vector &vForward, const Vector &vLeft, const Vector &vUp)
{
	SetForward(vForward);
	SetLeft(vLeft);
	SetUp(vUp);
}

//-----------------------------------------------------------------------------
// Methods related to the translation component of the matrix
//-----------------------------------------------------------------------------
#ifndef VECTOR_NO_SLOW_OPERATIONS

inline Vector VMatrix::GetTranslation() const
{
	return Vector(m[0][3], m[1][3], m[2][3]);
}

#endif

inline Vector& VMatrix::GetTranslation( Vector &vTrans ) const
{
	vTrans.x = m[0][3];
	vTrans.y = m[1][3];
	vTrans.z = m[2][3];
	return vTrans;
}

inline void VMatrix::SetTranslation(const Vector &vTrans)
{
	m[0][3] = vTrans.x;
	m[1][3] = vTrans.y;
	m[2][3] = vTrans.z;
}

		  
//-----------------------------------------------------------------------------
// appply translation to this matrix in the input space
//-----------------------------------------------------------------------------
inline void VMatrix::PreTranslate(const Vector &vTrans)
{
	Vector tmp;
	Vector3DMultiplyPosition( *this, vTrans, tmp );
	m[0][3] = tmp.x;
	m[1][3] = tmp.y;
	m[2][3] = tmp.z;
}


//-----------------------------------------------------------------------------
// appply translation to this matrix in the output space
//-----------------------------------------------------------------------------
inline void VMatrix::PostTranslate(const Vector &vTrans)
{
	m[0][3] += vTrans.x;
	m[1][3] += vTrans.y;
	m[2][3] += vTrans.z;
}

inline const matrix3x4_t& VMatrix::As3x4() const
{
	return *((const matrix3x4_t*)this);
}

inline void VMatrix::CopyFrom3x4( const matrix3x4_t &m3x4 )
{
	Init(m3x4);
}	

inline void	VMatrix::Set3x4( const matrix3x4_t& _m )
{
	m[0][0] = _m[0][0]; m[0][1] = _m[0][1]; m[0][2] = _m[0][2]; m[0][3] = _m[0][3]; 
	m[1][0] = _m[1][0]; m[1][1] = _m[1][1]; m[1][2] = _m[1][2]; m[1][3] = _m[1][3]; 
	m[2][0] = _m[2][0]; m[2][1] = _m[2][1]; m[2][2] = _m[2][2]; m[2][3] = _m[2][3]; 
}

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline Vector VMatrix::VMul4x3(const Vector &vVec) const
{
	Vector vResult;
	Vector3DMultiplyPosition( *this, vVec, vResult );
	return vResult;
}


inline Vector VMatrix::VMul4x3Transpose(const Vector &vVec) const
{
	Vector tmp = vVec;
	tmp.x -= m[0][3];
	tmp.y -= m[1][3];
	tmp.z -= m[2][3];

	return Vector(
		m[0][0]*tmp.x + m[1][0]*tmp.y + m[2][0]*tmp.z,
		m[0][1]*tmp.x + m[1][1]*tmp.y + m[2][1]*tmp.z,
		m[0][2]*tmp.x + m[1][2]*tmp.y + m[2][2]*tmp.z
		);
}

inline Vector VMatrix::VMul3x3(const Vector &vVec) const
{
	return Vector(
		m[0][0]*vVec.x + m[0][1]*vVec.y + m[0][2]*vVec.z,
		m[1][0]*vVec.x + m[1][1]*vVec.y + m[1][2]*vVec.z,
		m[2][0]*vVec.x + m[2][1]*vVec.y + m[2][2]*vVec.z
		);
}

inline Vector VMatrix::VMul3x3Transpose(const Vector &vVec) const
{
	return Vector(
		m[0][0]*vVec.x + m[1][0]*vVec.y + m[2][0]*vVec.z,
		m[0][1]*vVec.x + m[1][1]*vVec.y + m[2][1]*vVec.z,
		m[0][2]*vVec.x + m[1][2]*vVec.y + m[2][2]*vVec.z
		);
}

#endif // VECTOR_NO_SLOW_OPERATIONS


inline void VMatrix::V3Mul(const Vector &vIn, Vector &vOut) const
{
	vec_t rw;

	rw = 1.0f / (m[3][0]*vIn.x + m[3][1]*vIn.y + m[3][2]*vIn.z + m[3][3]);
	vOut.x = (m[0][0]*vIn.x + m[0][1]*vIn.y + m[0][2]*vIn.z + m[0][3]) * rw;
	vOut.y = (m[1][0]*vIn.x + m[1][1]*vIn.y + m[1][2]*vIn.z + m[1][3]) * rw;
	vOut.z = (m[2][0]*vIn.x + m[2][1]*vIn.y + m[2][2]*vIn.z + m[2][3]) * rw;
}

inline void VMatrix::V4Mul(const Vector4D &vIn, Vector4D &vOut) const
{
	vOut[0] = m[0][0]*vIn[0] + m[0][1]*vIn[1] + m[0][2]*vIn[2] + m[0][3]*vIn[3];
	vOut[1] = m[1][0]*vIn[0] + m[1][1]*vIn[1] + m[1][2]*vIn[2] + m[1][3]*vIn[3];
	vOut[2] = m[2][0]*vIn[0] + m[2][1]*vIn[1] + m[2][2]*vIn[2] + m[2][3]*vIn[3];
	vOut[3] = m[3][0]*vIn[0] + m[3][1]*vIn[1] + m[3][2]*vIn[2] + m[3][3]*vIn[3];
}


//-----------------------------------------------------------------------------
// Other random stuff
//-----------------------------------------------------------------------------
inline void VMatrix::Identity()
{
	MatrixSetIdentity( *this );
}


inline bool VMatrix::IsIdentity() const
{
	return 
		m[0][0] == 1.0f && m[0][1] == 0.0f && m[0][2] == 0.0f && m[0][3] == 0.0f &&
		m[1][0] == 0.0f && m[1][1] == 1.0f && m[1][2] == 0.0f && m[1][3] == 0.0f &&
		m[2][0] == 0.0f && m[2][1] == 0.0f && m[2][2] == 1.0f && m[2][3] == 0.0f &&
		m[3][0] == 0.0f && m[3][1] == 0.0f && m[3][2] == 0.0f && m[3][3] == 1.0f;
}

#ifndef VECTOR_NO_SLOW_OPERATIONS

inline Vector VMatrix::ApplyRotation(const Vector &vVec) const
{
	return VMul3x3(vVec);
}

#endif

inline void VMatrix::InverseTR( VMatrix &ret ) const
{
	MatrixInverseTR( *this, ret );
}

inline void MatrixInverseTranspose( const VMatrix& src, VMatrix& dst )
{
	src.InverseGeneral( dst );
	MatrixTranspose( dst, dst );
}

//-----------------------------------------------------------------------------
// Computes the inverse transpose
//-----------------------------------------------------------------------------
inline void MatrixInverseTranspose( const matrix3x4_t& src, matrix3x4_t& dst )
{
	VMatrix tmp, out;
	tmp.CopyFrom3x4( src );
	::MatrixInverseTranspose( tmp, out );
	out.Set3x4( dst );
}


#ifndef VECTOR_NO_SLOW_OPERATIONS

inline VMatrix VMatrix::InverseTR() const
{
	VMatrix ret;
	MatrixInverseTR( *this, ret );
	return ret;
}

inline Vector VMatrix::GetScale() const
{
	Vector vecs[3];

	GetBasisVectors(vecs[0], vecs[1], vecs[2]);

	return Vector(
		vecs[0].Length(),
		vecs[1].Length(),
		vecs[2].Length()
		);
}

inline VMatrix VMatrix::Scale(const Vector &vScale)
{
	return VMatrix(
		m[0][0]*vScale.x, m[0][1]*vScale.y, m[0][2]*vScale.z, m[0][3],
		m[1][0]*vScale.x, m[1][1]*vScale.y, m[1][2]*vScale.z, m[1][3],
		m[2][0]*vScale.x, m[2][1]*vScale.y, m[2][2]*vScale.z, m[2][3],
		m[3][0]*vScale.x, m[3][1]*vScale.y, m[3][2]*vScale.z, 1.0f
		);
}

inline VMatrix VMatrix::NormalizeBasisVectors() const
{
	Vector vecs[3];
	VMatrix mRet;

	GetBasisVectors(vecs[0], vecs[1], vecs[2]);
	
	VectorNormalize( vecs[0] );
	VectorNormalize( vecs[1] );
	VectorNormalize( vecs[2] );

	mRet.SetBasisVectors(vecs[0], vecs[1], vecs[2]);
	
	// Set everything but basis vectors to identity.
	mRet.m[3][0] = mRet.m[3][1] = mRet.m[3][2] = 0.0f;
	mRet.m[3][3] = 1.0f;

	return mRet;
}

inline VMatrix VMatrix::Transpose() const
{
	return VMatrix(
		m[0][0], m[1][0], m[2][0], m[3][0],
		m[0][1], m[1][1], m[2][1], m[3][1],
		m[0][2], m[1][2], m[2][2], m[3][2],
		m[0][3], m[1][3], m[2][3], m[3][3]);
}

// Transpose upper-left 3x3.
inline VMatrix VMatrix::Transpose3x3() const
{
	return VMatrix(
		m[0][0], m[1][0], m[2][0], m[0][3],
		m[0][1], m[1][1], m[2][1], m[1][3],
		m[0][2], m[1][2], m[2][2], m[2][3],
		m[3][0], m[3][1], m[3][2], m[3][3]);
}

#endif // VECTOR_NO_SLOW_OPERATIONS


inline bool VMatrix::IsRotationMatrix() const
{
	Vector &v1 = (Vector&)m[0][0];
	Vector &v2 = (Vector&)m[1][0];
	Vector &v3 = (Vector&)m[2][0];

	return 
		FloatMakePositive( 1 - v1.Length() ) < 0.01f && 
		FloatMakePositive( 1 - v2.Length() ) < 0.01f && 
		FloatMakePositive( 1 - v3.Length() ) < 0.01f && 
		FloatMakePositive( v1.Dot(v2) ) < 0.01f &&
		FloatMakePositive( v1.Dot(v3) ) < 0.01f &&
		FloatMakePositive( v2.Dot(v3) ) < 0.01f;
}

inline void VMatrix::SetupMatrixOrgAngles( const Vector &origin, const QAngle &vAngles )
{
	SetupMatrixAnglesInternal( m, vAngles );
	
	// Add translation
	m[0][3] = origin.x;
	m[1][3] = origin.y;
	m[2][3] = origin.z;
	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = 0.0f;
	m[3][3] = 1.0f;
}


inline void VMatrix::SetupMatrixAngles( const QAngle &vAngles )
{
	SetupMatrixAnglesInternal( m, vAngles );

	// Zero everything else
	m[0][3] = 0.0f;
	m[1][3] = 0.0f;
	m[2][3] = 0.0f;
	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = 0.0f;
	m[3][3] = 1.0f;
}

//-----------------------------------------------------------------------------
// Creates euler angles from a matrix 
//-----------------------------------------------------------------------------
inline void MatrixToAngles( const VMatrix& src, QAngle& vAngles )
{
	float forward[3];
	float left[3];
	float up[3];

	// Extract the basis vectors from the matrix. Since we only need the Z
	// component of the up vector, we don't get X and Y.
	forward[0] = src[0][0];
	forward[1] = src[1][0];
	forward[2] = src[2][0];
	left[0] = src[0][1];
	left[1] = src[1][1];
	left[2] = src[2][1];
	up[2] = src[2][2];

	float xyDist = sqrtf( forward[0] * forward[0] + forward[1] * forward[1] );
	
	// enough here to get angles?
	if ( xyDist > 0.001f )
	{
		// (yaw)	y = ATAN( forward.y, forward.x );		-- in our space, forward is the X axis
		vAngles[1] = RAD2DEG( atan2f( forward[1], forward[0] ) );

		// The engine does pitch inverted from this, but we always end up negating it in the DLL
		// UNDONE: Fix the engine to make it consistent
		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		vAngles[0] = RAD2DEG( atan2f( -forward[2], xyDist ) );

		// (roll)	z = ATAN( left.z, up.z );
		vAngles[2] = RAD2DEG( atan2f( left[2], up[2] ) );
	}
	else	// forward is mostly Z, gimbal lock-
	{
		// (yaw)	y = ATAN( -left.x, left.y );			-- forward is mostly z, so use right for yaw
		vAngles[1] = RAD2DEG( atan2f( -left[0], left[1] ) );

		// The engine does pitch inverted from this, but we always end up negating it in the DLL
		// UNDONE: Fix the engine to make it consistent
		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		vAngles[0] = RAD2DEG( atan2f( -forward[2], xyDist ) );

		// Assume no roll in this case as one degree of freedom has been lost (i.e. yaw == roll)
		vAngles[2] = 0;
	}
}

//-----------------------------------------------------------------------------
// Transform a plane
//-----------------------------------------------------------------------------
inline void MatrixTransformPlane( const VMatrix &src, const cplane_t &inPlane, cplane_t &outPlane )
{
	// What we want to do is the following:
	// 1) transform the normal into the new space.
	// 2) Determine a point on the old plane given by plane dist * plane normal
	// 3) Transform that point into the new space
	// 4) Plane dist = DotProduct( new normal, new point )

	// An optimized version, which works if the plane is orthogonal.
	// 1) Transform the normal into the new space
	// 2) Realize that transforming the old plane point into the new space
	// is given by [ d * n'x + Tx, d * n'y + Ty, d * n'z + Tz ]
	// where d = old plane dist, n' = transformed normal, Tn = translational component of transform
	// 3) Compute the new plane dist using the dot product of the normal result of #2

	// For a correct result, this should be an inverse-transpose matrix
	// but that only matters if there are nonuniform scale or skew factors in this matrix.
	Vector vTrans;
	Vector3DMultiply( src, inPlane.normal, outPlane.normal );
	outPlane.dist = inPlane.dist * DotProduct( outPlane.normal, outPlane.normal );
	outPlane.dist += DotProduct( outPlane.normal, src.GetTranslation(vTrans) );
}


//-----------------------------------------------------------------------------
// Helper functions.
//-----------------------------------------------------------------------------

#define VMatToString(mat)	(static_cast<const char *>(CFmtStr("[ (%f, %f, %f), (%f, %f, %f), (%f, %f, %f), (%f, %f, %f) ]", mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3], mat.m[1][0], mat.m[1][1], mat.m[1][2], mat.m[1][3], mat.m[2][0], mat.m[2][1], mat.m[2][2], mat.m[2][3], mat.m[3][0], mat.m[3][1], mat.m[3][2], mat.m[3][3] ))) // ** Note: this generates a temporary, don't hold reference!

//-----------------------------------------------------------------------------
// Matrix multiply
//-----------------------------------------------------------------------------
typedef ALIGN16 float VMatrixRaw_t[4];

//-----------------------------------------------------------------------------
// Matrix/vector multiply
//-----------------------------------------------------------------------------

inline void Vector4DMultiply( const VMatrix& src1, Vector4D const& src2, Vector4D& dst )
{
	// Make sure it works if src2 == dst
	Vector4D tmp;
	Vector4D const&v = (&src2 == &dst) ? tmp : src2;

	if (&src2 == &dst)
	{
		Vector4DCopy( src2, tmp );
	}

	dst[0] = src1[0][0] * v[0] + src1[0][1] * v[1] + src1[0][2] * v[2] + src1[0][3] * v[3];
	dst[1] = src1[1][0] * v[0] + src1[1][1] * v[1] + src1[1][2] * v[2] + src1[1][3] * v[3];
	dst[2] = src1[2][0] * v[0] + src1[2][1] * v[1] + src1[2][2] * v[2] + src1[2][3] * v[3];
	dst[3] = src1[3][0] * v[0] + src1[3][1] * v[1] + src1[3][2] * v[2] + src1[3][3] * v[3];
}

//-----------------------------------------------------------------------------
// Matrix/vector multiply
//-----------------------------------------------------------------------------

inline void Vector4DMultiplyPosition( const VMatrix& src1, Vector const& src2, Vector4D& dst )
{
	// Make sure it works if src2 == dst
	Vector tmp;
	Vector const&v = ( &src2 == &dst.AsVector3D() ) ? static_cast<const Vector&>(tmp) : src2;

	if (&src2 == &dst.AsVector3D())
	{
		VectorCopy( src2, tmp );
	}

	dst[0] = src1[0][0] * v[0] + src1[0][1] * v[1] + src1[0][2] * v[2] + src1[0][3];
	dst[1] = src1[1][0] * v[0] + src1[1][1] * v[1] + src1[1][2] * v[2] + src1[1][3];
	dst[2] = src1[2][0] * v[0] + src1[2][1] * v[1] + src1[2][2] * v[2] + src1[2][3];
	dst[3] = src1[3][0] * v[0] + src1[3][1] * v[1] + src1[3][2] * v[2] + src1[3][3];
}

//-----------------------------------------------------------------------------
// Vector3DMultiplyPositionProjective treats src2 as if it's a point 
// and does the perspective divide at the end
//-----------------------------------------------------------------------------
inline void Vector3DMultiplyPositionProjective( const VMatrix& src1, const Vector &src2, Vector& dst )
{
	// Make sure it works if src2 == dst
	Vector tmp;
	const Vector &v = (&src2 == &dst) ? static_cast<const Vector&>(tmp): src2;
	if( &src2 == &dst )
	{
		VectorCopy( src2, tmp );
	}

	float w = src1[3][0] * v[0] + src1[3][1] * v[1] + src1[3][2] * v[2] + src1[3][3];
	if ( w != 0.0f ) 
	{
		w = 1.0f / w;
	}

	dst[0] = src1[0][0] * v[0] + src1[0][1] * v[1] + src1[0][2] * v[2] + src1[0][3];
	dst[1] = src1[1][0] * v[0] + src1[1][1] * v[1] + src1[1][2] * v[2] + src1[1][3];
	dst[2] = src1[2][0] * v[0] + src1[2][1] * v[1] + src1[2][2] * v[2] + src1[2][3];
	dst *= w;
}


//-----------------------------------------------------------------------------
// Vector3DMultiplyProjective treats src2 as if it's a direction 
// and does the perspective divide at the end
//-----------------------------------------------------------------------------
inline void Vector3DMultiplyProjective( const VMatrix& src1, const Vector &src2, Vector& dst )
{
	// Make sure it works if src2 == dst
	Vector tmp;
	const Vector &v = (&src2 == &dst) ? static_cast<const Vector&>(tmp) : src2;
	if( &src2 == &dst )
	{
		VectorCopy( src2, tmp );
	}

	float w;
	dst[0] = src1[0][0] * v[0] + src1[0][1] * v[1] + src1[0][2] * v[2];
	dst[1] = src1[1][0] * v[0] + src1[1][1] * v[1] + src1[1][2] * v[2];
	dst[2] = src1[2][0] * v[0] + src1[2][1] * v[1] + src1[2][2] * v[2];
	w = src1[3][0] * v[0] + src1[3][1] * v[1] + src1[3][2] * v[2];
	if (w != 0.0f)
	{
		dst /= w;
	}
	else
	{
		dst = vec3_origin;
	}
}


//-----------------------------------------------------------------------------
// Multiplies the vector by the transpose of the matrix
//-----------------------------------------------------------------------------
inline void Vector4DMultiplyTranspose( const VMatrix& src1, Vector4D const& src2, Vector4D& dst )
{
	// Make sure it works if src2 == dst
	bool srcEqualsDst = (&src2 == &dst);

	Vector4D tmp;
	Vector4D const&v = srcEqualsDst ? tmp : src2;

	if (srcEqualsDst)
	{
		Vector4DCopy( src2, tmp );
	}

	dst[0] = src1[0][0] * v[0] + src1[1][0] * v[1] + src1[2][0] * v[2] + src1[3][0] * v[3];
	dst[1] = src1[0][1] * v[0] + src1[1][1] * v[1] + src1[2][1] * v[2] + src1[3][1] * v[3];
	dst[2] = src1[0][2] * v[0] + src1[1][2] * v[1] + src1[2][2] * v[2] + src1[3][2] * v[3];
	dst[3] = src1[0][3] * v[0] + src1[1][3] * v[1] + src1[2][3] * v[2] + src1[3][3] * v[3];
}

//-----------------------------------------------------------------------------
// Multiplies the vector by the transpose of the matrix
//-----------------------------------------------------------------------------
inline void Vector3DMultiplyTranspose( const VMatrix& src1, const Vector& src2, Vector& dst )
{
	// Make sure it works if src2 == dst
	bool srcEqualsDst = (&src2 == &dst);

	Vector tmp;
	const Vector&v = srcEqualsDst ? static_cast<const Vector&>(tmp) : src2;

	if (srcEqualsDst)
	{
		VectorCopy( src2, tmp );
	}

	dst[0] = src1[0][0] * v[0] + src1[1][0] * v[1] + src1[2][0] * v[2];
	dst[1] = src1[0][1] * v[0] + src1[1][1] * v[1] + src1[2][1] * v[2];
	dst[2] = src1[0][2] * v[0] + src1[1][2] * v[1] + src1[2][2] * v[2];
}


//-----------------------------------------------------------------------------
// Matrix copy
//-----------------------------------------------------------------------------
inline void MatrixCopy( const VMatrix& src, VMatrix& dst )
{
	if (&src != &dst)
		dst = src;
}

inline void MatrixMultiply( const VMatrix& src1, const VMatrix& src2, VMatrix& dst )
{
	// Make sure it works if src1 == dst or src2 == dst
	VMatrix tmp1, tmp2;
	const VMatrixRaw_t* s1 = (&src1 == &dst) ? tmp1.m : src1.m;
	const VMatrixRaw_t* s2 = (&src2 == &dst) ? tmp2.m : src2.m;

	if (&src1 == &dst)
		MatrixCopy( src1, tmp1 );

	if (&src2 == &dst)
		MatrixCopy( src2, tmp2 );

	dst[0][0] = s1[0][0] * s2[0][0] + s1[0][1] * s2[1][0] + s1[0][2] * s2[2][0] + s1[0][3] * s2[3][0];
	dst[0][1] = s1[0][0] * s2[0][1] + s1[0][1] * s2[1][1] + s1[0][2] * s2[2][1] + s1[0][3] * s2[3][1];
	dst[0][2] = s1[0][0] * s2[0][2] + s1[0][1] * s2[1][2] + s1[0][2] * s2[2][2] + s1[0][3] * s2[3][2];
	dst[0][3] = s1[0][0] * s2[0][3] + s1[0][1] * s2[1][3] + s1[0][2] * s2[2][3] + s1[0][3] * s2[3][3];

	dst[1][0] = s1[1][0] * s2[0][0] + s1[1][1] * s2[1][0] + s1[1][2] * s2[2][0] + s1[1][3] * s2[3][0];
	dst[1][1] = s1[1][0] * s2[0][1] + s1[1][1] * s2[1][1] + s1[1][2] * s2[2][1] + s1[1][3] * s2[3][1];
	dst[1][2] = s1[1][0] * s2[0][2] + s1[1][1] * s2[1][2] + s1[1][2] * s2[2][2] + s1[1][3] * s2[3][2];
	dst[1][3] = s1[1][0] * s2[0][3] + s1[1][1] * s2[1][3] + s1[1][2] * s2[2][3] + s1[1][3] * s2[3][3];

	dst[2][0] = s1[2][0] * s2[0][0] + s1[2][1] * s2[1][0] + s1[2][2] * s2[2][0] + s1[2][3] * s2[3][0];
	dst[2][1] = s1[2][0] * s2[0][1] + s1[2][1] * s2[1][1] + s1[2][2] * s2[2][1] + s1[2][3] * s2[3][1];
	dst[2][2] = s1[2][0] * s2[0][2] + s1[2][1] * s2[1][2] + s1[2][2] * s2[2][2] + s1[2][3] * s2[3][2];
	dst[2][3] = s1[2][0] * s2[0][3] + s1[2][1] * s2[1][3] + s1[2][2] * s2[2][3] + s1[2][3] * s2[3][3];

	dst[3][0] = s1[3][0] * s2[0][0] + s1[3][1] * s2[1][0] + s1[3][2] * s2[2][0] + s1[3][3] * s2[3][0];
	dst[3][1] = s1[3][0] * s2[0][1] + s1[3][1] * s2[1][1] + s1[3][2] * s2[2][1] + s1[3][3] * s2[3][1];
	dst[3][2] = s1[3][0] * s2[0][2] + s1[3][1] * s2[1][2] + s1[3][2] * s2[2][2] + s1[3][3] * s2[3][2];
	dst[3][3] = s1[3][0] * s2[0][3] + s1[3][1] * s2[1][3] + s1[3][2] * s2[2][3] + s1[3][3] * s2[3][3];
}


//-----------------------------------------------------------------------------
// Purpose: Builds the matrix for a counterclockwise rotation about an arbitrary axis.
//
//		   | ax2 + (1 - ax2)cosQ		axay(1 - cosQ) - azsinQ		azax(1 - cosQ) + aysinQ |
// Ra(Q) = | axay(1 - cosQ) + azsinQ	ay2 + (1 - ay2)cosQ			ayaz(1 - cosQ) - axsinQ |
//		   | azax(1 - cosQ) - aysinQ	ayaz(1 - cosQ) + axsinQ		az2 + (1 - az2)cosQ     |
//	   
// Input  : mat - 
//			vAxisOrRot - 
//			angle - 
//-----------------------------------------------------------------------------
inline void MatrixBuildRotationAboutAxis( const Vector &vAxisOfRot, float angleDegrees, matrix3x4_t &dst )
{
	float radians;
	float axisXSquared;
	float axisYSquared;
	float axisZSquared;
	float fSin;
	float fCos;

	radians = angleDegrees * ( M_PI / 180.0 );
	fSin = sinf( radians );
	fCos = cosf( radians );

	axisXSquared = vAxisOfRot[0] * vAxisOfRot[0];
	axisYSquared = vAxisOfRot[1] * vAxisOfRot[1];
	axisZSquared = vAxisOfRot[2] * vAxisOfRot[2];

	// Column 0:
	dst[0][0] = axisXSquared + (1 - axisXSquared) * fCos;
	dst[1][0] = vAxisOfRot[0] * vAxisOfRot[1] * (1 - fCos) + vAxisOfRot[2] * fSin;
	dst[2][0] = vAxisOfRot[2] * vAxisOfRot[0] * (1 - fCos) - vAxisOfRot[1] * fSin;

	// Column 1:
	dst[0][1] = vAxisOfRot[0] * vAxisOfRot[1] * (1 - fCos) - vAxisOfRot[2] * fSin;
	dst[1][1] = axisYSquared + (1 - axisYSquared) * fCos;
	dst[2][1] = vAxisOfRot[1] * vAxisOfRot[2] * (1 - fCos) + vAxisOfRot[0] * fSin;

	// Column 2:
	dst[0][2] = vAxisOfRot[2] * vAxisOfRot[0] * (1 - fCos) + vAxisOfRot[1] * fSin;
	dst[1][2] = vAxisOfRot[1] * vAxisOfRot[2] * (1 - fCos) - vAxisOfRot[0] * fSin;
	dst[2][2] = axisZSquared + (1 - axisZSquared) * fCos;

	// Column 3:
	dst[0][3] = 0;
	dst[1][3] = 0;
	dst[2][3] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Builds the matrix for a counterclockwise rotation about an arbitrary axis.
//
//		   | ax2 + (1 - ax2)cosQ		axay(1 - cosQ) - azsinQ		azax(1 - cosQ) + aysinQ |
// Ra(Q) = | axay(1 - cosQ) + azsinQ	ay2 + (1 - ay2)cosQ			ayaz(1 - cosQ) - axsinQ |
//		   | azax(1 - cosQ) - aysinQ	ayaz(1 - cosQ) + axsinQ		az2 + (1 - az2)cosQ     |
//	   
// Input  : mat - 
//			vAxisOrRot - 
//			angle - 
//-----------------------------------------------------------------------------
inline void MatrixBuildRotationAboutAxis( VMatrix &dst, const Vector &vAxisOfRot, float angleDegrees )
{
	MatrixBuildRotationAboutAxis( vAxisOfRot, angleDegrees, const_cast< matrix3x4_t &> ( dst.As3x4() ) );
	dst[3][0] = 0;
	dst[3][1] = 0;
	dst[3][2] = 0;
	dst[3][3] = 1;
}


//-----------------------------------------------------------------------------
// Builds a rotation matrix that rotates one direction vector into another
//-----------------------------------------------------------------------------
inline void MatrixBuildTranslation( VMatrix& dst, float x, float y, float z )
{
	MatrixSetIdentity( dst );
	dst[0][3] = x;
	dst[1][3] = y;
	dst[2][3] = z;
}

inline void MatrixBuildTranslation( VMatrix& dst, const Vector &translation )
{
	MatrixSetIdentity( dst );
	dst[0][3] = translation[0];
	dst[1][3] = translation[1];
	dst[2][3] = translation[2];
}


inline void MatrixTranslate( VMatrix& dst, const Vector &translation )
{
	VMatrix matTranslation, temp;
	MatrixBuildTranslation( matTranslation, translation );
	MatrixMultiply( dst, matTranslation, temp );
	dst = temp;
}

inline void MatrixRotate( VMatrix& dst, const Vector& vAxisOfRot, float angleDegrees )
{
	VMatrix rotation, temp;
	MatrixBuildRotationAboutAxis( rotation, vAxisOfRot, angleDegrees );
	MatrixMultiply( dst, rotation, temp );
	dst = temp;
}


//-----------------------------------------------------------------------------
// Accessors
//-----------------------------------------------------------------------------
inline void MatrixGetColumn( const VMatrix &src, int nCol, Vector *pColumn )
{
	Assert( (nCol >= 0) && (nCol <= 3) );

	pColumn->x = src[0][nCol];
	pColumn->y = src[1][nCol];
	pColumn->z = src[2][nCol];
}

inline void MatrixGetRow( const VMatrix &src, int nRow, Vector *pRow )
{
	Assert( (nRow >= 0) && (nRow <= 3) );
	*pRow = *(Vector*)src[nRow];
}

inline void MatrixSetRow( VMatrix &dst, int nRow, const Vector &row )
{
	Assert( (nRow >= 0) && (nRow <= 3) );
	*(Vector*)dst[nRow] = row;
}

//-----------------------------------------------------------------------------
// Transform a plane that has an axis-aligned normal
//-----------------------------------------------------------------------------
inline void MatrixTransformAxisAlignedPlane( const VMatrix &src, int nDim, float flSign, float flDist, cplane_t &outPlane )
{
	// See MatrixTransformPlane in the .cpp file for an explanation of the algorithm.
	MatrixGetColumn( src, nDim, &outPlane.normal );
	outPlane.normal *= flSign;
	outPlane.dist = flDist * DotProduct( outPlane.normal, outPlane.normal );

	// NOTE: Writing this out by hand because it doesn't inline (inline depth isn't large enough)
	// This should read outPlane.dist += DotProduct( outPlane.normal, src.GetTranslation );
	outPlane.dist += outPlane.normal.x * src.m[0][3] + outPlane.normal.y * src.m[1][3] + outPlane.normal.z * src.m[2][3];
}


//-----------------------------------------------------------------------------
// Matrix equality test
//-----------------------------------------------------------------------------
inline bool MatricesAreEqual( const VMatrix &src1, const VMatrix &src2, float flTolerance )
{
	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			if ( fabs( src1[i][j] - src2[i][j] ) > flTolerance )
				return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void MatrixBuildOrtho( VMatrix& dst, double left, double top, double right, double bottom, double zNear, double zFar );
void MatrixBuildPerspectiveX( VMatrix& dst, double flFovX, double flAspect, double flZNear, double flZFar );
void MatrixBuildPerspectiveOffCenterX( VMatrix& dst, double flFovX, double flAspect, double flZNear, double flZFar, double bottom, double top, double left, double right );
void MatrixBuildPerspectiveZRange( VMatrix& dst, double flZNear, double flZFar );

inline void MatrixOrtho( VMatrix& dst, double left, double top, double right, double bottom, double zNear, double zFar )
{
	VMatrix mat;
	MatrixBuildOrtho( mat, left, top, right, bottom, zNear, zFar );

	VMatrix temp;
	MatrixMultiply( dst, mat, temp );
	dst = temp;
}

inline void MatrixPerspectiveX( VMatrix& dst, double flFovX, double flAspect, double flZNear, double flZFar )
{
	VMatrix mat;
	MatrixBuildPerspectiveX( mat, flFovX, flAspect, flZNear, flZFar );

	VMatrix temp;
	MatrixMultiply( dst, mat, temp );
	dst = temp;
}

inline void MatrixPerspectiveOffCenterX( VMatrix& dst, double flFovX, double flAspect, double flZNear, double flZFar, double bottom, double top, double left, double right )
{
	VMatrix mat;
	MatrixBuildPerspectiveOffCenterX( mat, flFovX, flAspect, flZNear, flZFar, bottom, top, left, right );

	VMatrix temp;
	MatrixMultiply( dst, mat, temp );
	dst = temp;
}


#ifndef VECTOR_NO_SLOW_OPERATIONS

inline VMatrix SetupMatrixIdentity()
{
	return VMatrix(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}

inline VMatrix SetupMatrixTranslation(const Vector &vTranslation)
{
	return VMatrix(
		1.0f, 0.0f, 0.0f, vTranslation.x,
		0.0f, 1.0f, 0.0f, vTranslation.y,
		0.0f, 0.0f, 1.0f, vTranslation.z,
		0.0f, 0.0f, 0.0f, 1.0f
		);
}

inline VMatrix SetupMatrixScale(const Vector &vScale)
{
	return VMatrix(
		vScale.x, 0.0f,     0.0f,     0.0f,
		0.0f,     vScale.y, 0.0f,     0.0f,
		0.0f,     0.0f,     vScale.z, 0.0f,
		0.0f,     0.0f,     0.0f,     1.0f
		);
}

inline VMatrix SetupMatrixReflection(const VPlane &thePlane)
{
	VMatrix mReflect, mBack, mForward;
	Vector vOrigin, N;

	N = thePlane.m_Normal;

	mReflect.Init( 
		-2.0f*N.x*N.x + 1.0f,	-2.0f*N.x*N.y,			-2.0f*N.x*N.z,			0.0f,
		-2.0f*N.y*N.x,			-2.0f*N.y*N.y + 1.0f,	-2.0f*N.y*N.z,			0.0f,
		-2.0f*N.z*N.x,			-2.0f*N.z*N.y,			-2.0f*N.z*N.z + 1.0f,	0.0f,
		0.0f,					0.0f,					0.0f,					1.0f
		);

	vOrigin = thePlane.GetPointOnPlane();

	mBack.Identity();
	mBack.SetTranslation(-vOrigin);

	mForward.Identity();
	mForward.SetTranslation(vOrigin);

	// (multiplied in reverse order, so it translates to the origin point,
	// reflects, and translates back).
	return mForward * mReflect * mBack;
}

inline VMatrix SetupMatrixProjection(const Vector &vOrigin, const VPlane &thePlane)
{
	vec_t dot;
	VMatrix mRet;


	#define PN thePlane.m_Normal
	#define PD thePlane.m_Dist;

		dot = PN[0]*vOrigin.x + PN[1]*vOrigin.y + PN[2]*vOrigin.z - PD;

		mRet.m[0][0] = dot - vOrigin.x * PN[0];
		mRet.m[0][1] = -vOrigin.x * PN[1];
		mRet.m[0][2] = -vOrigin.x * PN[2];
		mRet.m[0][3] = -vOrigin.x * -PD;

		mRet.m[1][0] = -vOrigin.y * PN[0];
		mRet.m[1][1] = dot - vOrigin.y * PN[1];
		mRet.m[1][2] = -vOrigin.y * PN[2];
		mRet.m[1][3] = -vOrigin.y * -PD;

		mRet.m[2][0] = -vOrigin.z * PN[0];
		mRet.m[2][1] = -vOrigin.z * PN[1];
		mRet.m[2][2] = dot - vOrigin.z * PN[2];
		mRet.m[2][3] = -vOrigin.z * -PD;

		mRet.m[3][0] = -PN[0];
		mRet.m[3][1] = -PN[1];
		mRet.m[3][2] = -PN[2];
		mRet.m[3][3] = dot + PD;

	#undef PN
	#undef PD	

	return mRet;
}

inline VMatrix SetupMatrixAxisRot(const Vector &vAxis, vec_t fDegrees)
{
	vec_t s, c, t;
	vec_t tx, ty, tz;
	vec_t sx, sy, sz;
	vec_t fRadians;


	fRadians = fDegrees * (M_PI / 180.0f);
	
	s = (vec_t)sin(fRadians);
	c = (vec_t)cos(fRadians);
	t = 1.0f - c;

	tx = t * vAxis.x;	ty = t * vAxis.y;	tz = t * vAxis.z;
	sx = s * vAxis.x;	sy = s * vAxis.y;	sz = s * vAxis.z;

	return VMatrix(
		tx*vAxis.x + c,  tx*vAxis.y - sz, tx*vAxis.z + sy, 0.0f,
		tx*vAxis.y + sz, ty*vAxis.y + c,  ty*vAxis.z - sx, 0.0f,
		tx*vAxis.z - sy, ty*vAxis.z + sx, tz*vAxis.z + c,  0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}

//-----------------------------------------------------------------------------
// Setup a matrix from euler angles. 
//-----------------------------------------------------------------------------
inline void MatrixFromAngles( const QAngle& vAngles, VMatrix& dst )
{
	dst.SetupMatrixOrgAngles( vec3_origin, vAngles );
}

inline VMatrix SetupMatrixAngles(const QAngle &vAngles)
{
	VMatrix mRet;
	MatrixFromAngles( vAngles, mRet );
	return mRet;
}

inline VMatrix SetupMatrixOrgAngles(const Vector &origin, const QAngle &vAngles)
{
	VMatrix mRet;
	mRet.SetupMatrixOrgAngles( origin, vAngles );
	return mRet;
}

#endif // VECTOR_NO_SLOW_OPERATIONS


inline bool PlaneIntersection( const VPlane &vp1, const VPlane &vp2, const VPlane &vp3, Vector &vOut )
{
	VMatrix mMat, mInverse;

	mMat.Init(
		vp1.m_Normal.x, vp1.m_Normal.y, vp1.m_Normal.z, -vp1.m_Dist,
		vp2.m_Normal.x, vp2.m_Normal.y, vp2.m_Normal.z, -vp2.m_Dist,
		vp3.m_Normal.x, vp3.m_Normal.y, vp3.m_Normal.z, -vp3.m_Dist,
		0.0f, 0.0f, 0.0f, 1.0f
		);
	
	if(mMat.InverseGeneral(mInverse))
	{
		//vOut = mInverse * Vector(0.0f, 0.0f, 0.0f);
		mInverse.GetTranslation( vOut );
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Builds a rotation matrix that rotates one direction vector into another
//-----------------------------------------------------------------------------
inline void MatrixBuildRotation( VMatrix &dst, const Vector& initialDirection, const Vector& finalDirection )
{
	float angle = DotProduct( initialDirection, finalDirection );
	Assert( IsFinite(angle) );
	
	Vector axis;

	// No rotation required
	if (angle - 1.0 > -1e-3)
	{
		// parallel case
		MatrixSetIdentity(dst);
		return;
	}
	else if (angle + 1.0 < 1e-3)
	{
		// antiparallel case, pick any axis in the plane
		// perpendicular to the final direction. Choose the direction (x,y,z)
		// which has the minimum component of the final direction, use that
		// as an initial guess, then subtract out the component which is 
		// parallel to the final direction
		int idx = 0;
		if (FloatMakePositive(finalDirection[1]) < FloatMakePositive(finalDirection[idx]))
			idx = 1;
		if (FloatMakePositive(finalDirection[2]) < FloatMakePositive(finalDirection[idx]))
			idx = 2;

		axis.Init( 0, 0, 0 );
		axis[idx] = 1.0f;
		VectorMA( axis, -DotProduct( axis, finalDirection ), finalDirection, axis );
		VectorNormalize(axis);
		angle = 180.0f;
	}
	else
	{
		CrossProduct( initialDirection, finalDirection, axis );
		VectorNormalize( axis );
		angle = acos(angle) * 180 / M_PI;
	}

	MatrixBuildRotationAboutAxis( dst, axis, angle );

#ifdef _DEBUG
	Vector test;
	Vector3DMultiply( dst, initialDirection, test );
	test -= finalDirection;
	Assert( test.LengthSqr() < 1e-3 );
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void MatrixBuildRotateZ( VMatrix &dst, float angleDegrees )
{
	float radians = angleDegrees * ( M_PI / 180.0f );

	float fSin = ( float )sin( radians );
	float fCos = ( float )cos( radians );

	dst[0][0] = fCos; dst[0][1] = -fSin; dst[0][2] = 0.0f; dst[0][3] = 0.0f;
	dst[1][0] = fSin; dst[1][1] =  fCos; dst[1][2] = 0.0f; dst[1][3] = 0.0f;
	dst[2][0] = 0.0f; dst[2][1] =  0.0f; dst[2][2] = 1.0f; dst[2][3] = 0.0f;
	dst[3][0] = 0.0f; dst[3][1] =  0.0f; dst[3][2] = 0.0f; dst[3][3] = 1.0f;
}

// Builds a scale matrix
inline void MatrixBuildScale( VMatrix &dst, float x, float y, float z )
{
	dst[0][0] = x;		dst[0][1] = 0.0f;	dst[0][2] = 0.0f;	dst[0][3] = 0.0f;
	dst[1][0] = 0.0f;	dst[1][1] = y;		dst[1][2] = 0.0f;	dst[1][3] = 0.0f;
	dst[2][0] = 0.0f;	dst[2][1] = 0.0f;	dst[2][2] = z;		dst[2][3] = 0.0f;
	dst[3][0] = 0.0f;	dst[3][1] = 0.0f;	dst[3][2] = 0.0f;	dst[3][3] = 1.0f;
}

inline void MatrixBuildScale( VMatrix &dst, const Vector& scale )
{
	MatrixBuildScale( dst, scale.x, scale.y, scale.z );
}

// nillerusr: optimize this bruh later
inline void MatrixBuildPerspective( VMatrix &dst, float fovX, float fovY, float zNear, float zFar )
{
	// FIXME: collapse all of this into one matrix after we figure out what all should be in here.
	float width = 2 * zNear * tan( fovX * ( M_PI/180.0f ) * 0.5f );
	float height = 2 * zNear * tan( fovY * ( M_PI/180.0f ) * 0.5f );

	dst.	Init(
		2.0f * zNear / width, 0.f, 0.f, 0.f,
		0.f, 2.0f * zNear / height, 0.f, 0.f,
		0.f, 0.f, -zFar / ( zNear - zFar ), zNear * zFar / ( zNear - zFar ),
		0.f, 0.f, 1.f, 0.f
		);

	// negate X and Y so that X points right, and Y points up.
	VMatrix negateXY;
	negateXY.Identity();
	negateXY[0][0] = -1.0f;
	negateXY[1][1] = -1.0f;
	MatrixMultiply( negateXY, dst, dst );

	VMatrix addW;
	addW.Identity();
	addW[0][3] = 1.0f;
	addW[1][3] = 1.0f;
	addW[2][3] = 0.0f;
	MatrixMultiply( addW, dst, dst );
	
	VMatrix scaleHalf;
	scaleHalf.Identity();
	scaleHalf[0][0] = 0.5f;
	scaleHalf[1][1] = 0.5f;
	MatrixMultiply( scaleHalf, dst, dst );
}

static inline void CalculateAABBForNormalizedFrustum_Helper( float x, float y, float z, const VMatrix &volumeToWorld, Vector &mins, Vector &maxs )
{
	Vector volumeSpacePos( x, y, z );

	// Make sure it's been clipped
	Assert( volumeSpacePos[0] >= -1e-3f );
	Assert( volumeSpacePos[0] - 1.0f <= 1e-3f );
	Assert( volumeSpacePos[1] >= -1e-3f );
	Assert( volumeSpacePos[1] - 1.0f <= 1e-3f );
	Assert( volumeSpacePos[2] >= -1e-3f );
	Assert( volumeSpacePos[2] - 1.0f <= 1e-3f );

	Vector worldPos;
	Vector3DMultiplyPositionProjective( volumeToWorld, volumeSpacePos, worldPos );
	AddPointToBounds( worldPos, mins, maxs );
}

//-----------------------------------------------------------------------------
// Given an inverse projection matrix, take the extremes of the space in transformed into world space and
// get a bounding box.
//-----------------------------------------------------------------------------
inline void CalculateAABBFromProjectionMatrixInverse( const VMatrix &volumeToWorld, Vector *pMins, Vector *pMaxs )
{
	// FIXME: Could maybe do better than the compile with all of these multiplies by 0 and 1.
	ClearBounds( *pMins, *pMaxs );
	CalculateAABBForNormalizedFrustum_Helper( 0, 0, 0, volumeToWorld, *pMins, *pMaxs );
	CalculateAABBForNormalizedFrustum_Helper( 0, 0, 1, volumeToWorld, *pMins, *pMaxs );
	CalculateAABBForNormalizedFrustum_Helper( 0, 1, 0, volumeToWorld, *pMins, *pMaxs );
	CalculateAABBForNormalizedFrustum_Helper( 0, 1, 1, volumeToWorld, *pMins, *pMaxs );
	CalculateAABBForNormalizedFrustum_Helper( 1, 0, 0, volumeToWorld, *pMins, *pMaxs );
	CalculateAABBForNormalizedFrustum_Helper( 1, 0, 1, volumeToWorld, *pMins, *pMaxs );
	CalculateAABBForNormalizedFrustum_Helper( 1, 1, 0, volumeToWorld, *pMins, *pMaxs );
	CalculateAABBForNormalizedFrustum_Helper( 1, 1, 1, volumeToWorld, *pMins, *pMaxs );
}

inline void CalculateAABBFromProjectionMatrix( const VMatrix &worldToVolume, Vector *pMins, Vector *pMaxs )
{
	VMatrix volumeToWorld;
	MatrixInverseGeneral( worldToVolume, volumeToWorld );
	CalculateAABBFromProjectionMatrixInverse( volumeToWorld, pMins, pMaxs );
}

//-----------------------------------------------------------------------------
// Given an inverse projection matrix, take the extremes of the space in transformed into world space and
// get a bounding sphere.
//-----------------------------------------------------------------------------
inline void CalculateSphereFromProjectionMatrixInverse( const VMatrix &volumeToWorld, Vector *pCenter, float *pflRadius )
{
	// FIXME: Could maybe do better than the compile with all of these multiplies by 0 and 1.

	// Need 3 points: the endpoint of the line through the center of the near + far planes,
	// and one point on the far plane. From that, we can derive a point somewhere on the center	line
	// which would produce the smallest bounding sphere.
	Vector vecCenterNear, vecCenterFar, vecNearEdge, vecFarEdge;
	Vector3DMultiplyPositionProjective( volumeToWorld, Vector( 0.5f, 0.5f, 0.0f ), vecCenterNear );
	Vector3DMultiplyPositionProjective( volumeToWorld, Vector( 0.5f, 0.5f, 1.0f ), vecCenterFar );
	Vector3DMultiplyPositionProjective( volumeToWorld, Vector( 0.0f, 0.0f, 0.0f ), vecNearEdge );
	Vector3DMultiplyPositionProjective( volumeToWorld, Vector( 0.0f, 0.0f, 1.0f ), vecFarEdge );

	// Let the distance between the near + far center points = l
	// Let the distance between the near center point + near edge point = h1
	// Let the distance between the far center point + far edge point = h2
	// Let the distance along the center line from the near point to the sphere center point = x
	// Then let the distance between the sphere center point + near edge point == 
	//	the distance between the sphere center point + far edge point == r == radius of sphere
	// Then h1^2 + x^2 == r^2 == (l-x)^2 + h2^2
	// h1^x + x^2 = l^2 - 2 * l * x + x^2 + h2^2
	// 2 * l * x = l^2 + h2^2 - h1^2
	// x = (l^2 + h2^2 - h1^2) / (2 * l)
	// r = sqrt( hl^1 + x^2 )
	Vector vecDelta;
	VectorSubtract( vecCenterFar, vecCenterNear, vecDelta );
	float l = vecDelta.Length();
	float h1Sqr = vecCenterNear.DistToSqr( vecNearEdge );
	float h2Sqr = vecCenterFar.DistToSqr( vecFarEdge );
	float x = (l*l + h2Sqr - h1Sqr) / (2.0f * l);
	VectorMA( vecCenterNear, (x / l), vecDelta, *pCenter );
	*pflRadius = sqrt( h1Sqr + x*x );
}

//-----------------------------------------------------------------------------
// Given a projection matrix, take the extremes of the space in transformed into world space and
// get a bounding sphere.
//-----------------------------------------------------------------------------
inline void CalculateSphereFromProjectionMatrix( const VMatrix &worldToVolume, Vector *pCenter, float *pflRadius )
{
	VMatrix volumeToWorld;
	MatrixInverseGeneral( worldToVolume, volumeToWorld );
	CalculateSphereFromProjectionMatrixInverse( volumeToWorld, pCenter, pflRadius );
}


static inline void FrustumPlanesFromMatrixHelper( const VMatrix &shadowToWorld, const Vector &p1, const Vector &p2, const Vector &p3, 
												 Vector &normal, float &dist )
{
	Vector world1, world2, world3;
	Vector3DMultiplyPositionProjective( shadowToWorld, p1, world1 );
	Vector3DMultiplyPositionProjective( shadowToWorld, p2, world2 );
	Vector3DMultiplyPositionProjective( shadowToWorld, p3, world3 );

	Vector v1, v2;
	VectorSubtract( world2, world1, v1 );
	VectorSubtract( world3, world1, v2 );

	CrossProduct( v1, v2, normal );
	VectorNormalize( normal );
	dist = DotProduct( normal, world1 );	
}

inline void FrustumPlanesFromMatrix( const VMatrix &clipToWorld, Frustum_t &frustum )
{
	Vector normal;
	float dist;

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 0.0f, 0.0f, 0.0f ), Vector( 1.0f, 0.0f, 0.0f ), Vector( 0.0f, 1.0f, 0.0f ), normal, dist );
	frustum.SetPlane( FRUSTUM_NEARZ, PLANE_ANYZ, normal, dist );

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 0.0f, 0.0f, 1.0f ), Vector( 0.0f, 1.0f, 1.0f ), Vector( 1.0f, 0.0f, 1.0f ), normal, dist );
	frustum.SetPlane( FRUSTUM_FARZ, PLANE_ANYZ, normal, dist );

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 1.0f, 0.0f, 0.0f ), Vector( 1.0f, 1.0f, 1.0f ), Vector( 1.0f, 1.0f, 0.0f ), normal, dist );
	frustum.SetPlane( FRUSTUM_RIGHT, PLANE_ANYZ, normal, dist );

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 0.0f, 0.0f, 0.0f ), Vector( 0.0f, 1.0f, 1.0f ), Vector( 0.0f, 0.0f, 1.0f ), normal, dist );
	frustum.SetPlane( FRUSTUM_LEFT, PLANE_ANYZ, normal, dist );

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 1.0f, 1.0f, 0.0f ), Vector( 1.0f, 1.0f, 1.0f ), Vector( 0.0f, 1.0f, 1.0f ), normal, dist );
	frustum.SetPlane( FRUSTUM_TOP, PLANE_ANYZ, normal, dist );

	FrustumPlanesFromMatrixHelper( clipToWorld, 
		Vector( 1.0f, 0.0f, 0.0f ), Vector( 0.0f, 0.0f, 1.0f ), Vector( 1.0f, 0.0f, 1.0f ), normal, dist );
	frustum.SetPlane( FRUSTUM_BOTTOM, PLANE_ANYZ, normal, dist );
}

inline void MatrixBuildOrtho( VMatrix& dst, double left, double top, double right, double bottom, double zNear, double zFar )
{
	// FIXME: This is being used incorrectly! Should read:
	// D3DXMatrixOrthoOffCenterRH( &matrix, left, right, bottom, top, zNear, zFar );
	// Which is certainly why we need these extra -1 scales in y. Bleah

	// NOTE: The camera can be imagined as the following diagram:
	//		/z
	//	   /
	//	  /____ x	Z is going into the screen
	//	  |
	//	  |
	//	  |y
	//
	// (0,0,z) represents the upper-left corner of the screen.
	// Our projection transform needs to transform from this space to a LH coordinate
	// system that looks thusly:
	// 
	//	y|  /z
	//	 | /
	//	 |/____ x	Z is going into the screen
	//
	// Where x,y lies between -1 and 1, and z lies from 0 to 1
	// This is because the viewport transformation from projection space to pixels
	// introduces a -1 scale in the y coordinates
	//		D3DXMatrixOrthoOffCenterRH( &matrix, left, right, top, bottom, zNear, zFar );

	dst.Init(	 2.0f / ( right - left ),						0.0f,						0.0f, ( left + right ) / ( left - right ),
				0.0f,	 2.0f / ( bottom - top ),						0.0f, ( bottom + top ) / ( top - bottom ),
				0.0f,						0.0f,	 1.0f / ( zNear - zFar ),			 zNear / ( zNear - zFar ),
				0.0f,						0.0f,						0.0f,								1.0f );
}

inline void MatrixBuildPerspectiveZRange( VMatrix& dst, double flZNear, double flZFar )
{
	dst.m[2][0] = 0.0f;
	dst.m[2][1] = 0.0f;
	dst.m[2][2] = flZFar / ( flZNear - flZFar );
	dst.m[2][3] = flZNear * flZFar / ( flZNear - flZFar );
}

inline void MatrixBuildPerspectiveX( VMatrix& dst, double flFovX, double flAspect, double flZNear, double flZFar )
{
	float flWidthScale = 1.0f / tanf( flFovX * M_PI / 360.0f );
	float flHeightScale = flAspect * flWidthScale;
	dst.Init(   flWidthScale,				0.0f,							0.0f,										0.0f,
				0.0f,						flHeightScale,					0.0f,										0.0f,
				0.0f,						0.0f,							0.0f,										0.0f,
				0.0f,						0.0f,						   -1.0f,										0.0f );

	MatrixBuildPerspectiveZRange ( dst, flZNear, flZFar );
}

inline void MatrixBuildPerspectiveOffCenterX( VMatrix& dst, double flFovX, double flAspect, double flZNear, double flZFar, double bottom, double top, double left, double right )
{
	float flWidth = tanf( flFovX * M_PI / 360.0f );
	float flHeight = flWidth / flAspect;

	// bottom, top, left, right are 0..1 so convert to -<val>/2..<val>/2
	float flLeft   = -(flWidth/2.0f)  * (1.0f - left)   + left   * (flWidth/2.0f);
	float flRight  = -(flWidth/2.0f)  * (1.0f - right)  + right  * (flWidth/2.0f);
	float flBottom = -(flHeight/2.0f) * (1.0f - bottom) + bottom * (flHeight/2.0f);
	float flTop    = -(flHeight/2.0f) * (1.0f - top)    + top    * (flHeight/2.0f);

	dst.Init(   1.0f / (flRight-flLeft),	 0.0f,			      (flLeft+flRight)/(flRight-flLeft),  0.0f,
				0.0f,			      1.0f /(flTop-flBottom),	  (flTop+flBottom)/(flTop-flBottom),  0.0f,
				0.0f,			      0.0f,							0.0f,								0.0f,
				0.0f,			      0.0f,			      -1.0f,								0.0f );

	MatrixBuildPerspectiveZRange ( dst, flZNear, flZFar );
}

#endif


