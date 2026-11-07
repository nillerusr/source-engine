//========= Copyright Valve Corporation, All rights reserved. ============//
//
// DXVK-native builds: the D3DX9 functions the D3D9 shader API uses.
//
// On Windows these come from d3dx9.dll; DXVK only implements d3d9 itself, so the few
// D3DX helpers the engine calls (matrix math, a matrix stack, shader version) are
// implemented here. Conventions follow D3DX: row vectors, v' = v * M, M[row][col].
// (ToGL has its own versions of these in togl/linuxwin/dxabstract.cpp.)
//
//=============================================================================//

#ifdef DXVK

#include "togl/rendermechanism.h"
#include "tier0/dbg.h"
#include <math.h>
#include <new>

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// ---------------------------------------------------------------------------------------
// Matrices / vectors / planes
// ---------------------------------------------------------------------------------------

static void MatIdentity( D3DXMATRIX *pOut )
{
	for ( int i = 0; i < 4; i++ )
		for ( int j = 0; j < 4; j++ )
			pOut->m[i][j] = ( i == j ) ? 1.0f : 0.0f;
}

D3DXMATRIX* WINAPI D3DXMatrixMultiply( D3DXMATRIX *pOut, const D3DXMATRIX *pM1, const D3DXMATRIX *pM2 )
{
	D3DXMATRIX temp;
	for ( int i = 0; i < 4; i++ )
	{
		for ( int j = 0; j < 4; j++ )
		{
			temp.m[i][j] = pM1->m[i][0] * pM2->m[0][j] + pM1->m[i][1] * pM2->m[1][j]
			             + pM1->m[i][2] * pM2->m[2][j] + pM1->m[i][3] * pM2->m[3][j];
		}
	}
	*pOut = temp;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixTranslation( D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z )
{
	MatIdentity( pOut );
	pOut->m[3][0] = x;
	pOut->m[3][1] = y;
	pOut->m[3][2] = z;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixScaling( D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz )
{
	MatIdentity( pOut );
	pOut->m[0][0] = sx;
	pOut->m[1][1] = sy;
	pOut->m[2][2] = sz;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixTranspose( D3DXMATRIX *pOut, const D3DXMATRIX *pM )
{
	D3DXMATRIX temp;
	for ( int i = 0; i < 4; i++ )
		for ( int j = 0; j < 4; j++ )
			temp.m[i][j] = pM->m[j][i];
	*pOut = temp;
	return pOut;
}

// General 4x4 inverse (cofactors). Returns NULL if the matrix is singular, like D3DX.
D3DXMATRIX* WINAPI D3DXMatrixInverse( D3DXMATRIX *pOut, FLOAT *pDeterminant, const D3DXMATRIX *pM )
{
	const float *m = &pM->m[0][0];
	float inv[16];

	inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
	inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
	inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
	inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
	inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
	inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
	inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
	inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
	inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]  - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
	inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]  + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
	inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]  - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
	inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]  + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];
	inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]  + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]   + m[9]*m[3]*m[6];
	inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]  - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]   - m[8]*m[3]*m[6];
	inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]   + m[4]*m[1]*m[11] - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7]   + m[8]*m[3]*m[5];
	inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]   - m[4]*m[1]*m[10] + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6]   - m[8]*m[2]*m[5];

	float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
	if ( pDeterminant )
		*pDeterminant = det;
	if ( det == 0.0f )
		return NULL;

	float invDet = 1.0f / det;
	float *o = &pOut->m[0][0];
	for ( int i = 0; i < 16; i++ )
		o[i] = inv[i] * invDet;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixRotationAxis( D3DXMATRIX *pOut, const D3DXVECTOR3 *pV, FLOAT angle )
{
	float len = sqrtf( pV->x * pV->x + pV->y * pV->y + pV->z * pV->z );
	float x = len ? pV->x / len : 0.0f, y = len ? pV->y / len : 0.0f, z = len ? pV->z / len : 0.0f;
	float s = sinf( angle ), c = cosf( angle ), t = 1.0f - c;

	MatIdentity( pOut );
	pOut->m[0][0] = t * x * x + c;     pOut->m[0][1] = t * x * y + s * z; pOut->m[0][2] = t * x * z - s * y;
	pOut->m[1][0] = t * x * y - s * z; pOut->m[1][1] = t * y * y + c;     pOut->m[1][2] = t * y * z + s * x;
	pOut->m[2][0] = t * x * z + s * y; pOut->m[2][1] = t * y * z - s * x; pOut->m[2][2] = t * z * z + c;
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixRotationYawPitchRoll( D3DXMATRIX *pOut, FLOAT yaw, FLOAT pitch, FLOAT roll )
{
	// roll (z), then pitch (x), then yaw (y)
	D3DXMATRIX mz, mx, my;
	D3DXVECTOR3 ax( 1, 0, 0 ), ay( 0, 1, 0 ), az( 0, 0, 1 );
	D3DXMatrixRotationAxis( &mz, &az, roll );
	D3DXMatrixRotationAxis( &mx, &ax, pitch );
	D3DXMatrixRotationAxis( &my, &ay, yaw );
	D3DXMatrixMultiply( pOut, &mz, &mx );
	D3DXMatrixMultiply( pOut, pOut, &my );
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixOrthoOffCenterLH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf )
{
	MatIdentity( pOut );
	pOut->m[0][0] = 2.0f / ( r - l );
	pOut->m[1][1] = 2.0f / ( t - b );
	pOut->m[2][2] = 1.0f / ( zf - zn );
	pOut->m[3][0] = ( l + r ) / ( l - r );
	pOut->m[3][1] = ( t + b ) / ( b - t );
	pOut->m[3][2] = zn / ( zn - zf );
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixOrthoOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf )
{
	MatIdentity( pOut );
	pOut->m[0][0] = 2.0f / ( r - l );
	pOut->m[1][1] = 2.0f / ( t - b );
	pOut->m[2][2] = 1.0f / ( zn - zf );
	pOut->m[3][0] = ( l + r ) / ( l - r );
	pOut->m[3][1] = ( t + b ) / ( b - t );
	pOut->m[3][2] = zn / ( zn - zf );
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixPerspectiveRH( D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf )
{
	memset( pOut, 0, sizeof( *pOut ) );
	pOut->m[0][0] = 2.0f * zn / w;
	pOut->m[1][1] = 2.0f * zn / h;
	pOut->m[2][2] = zf / ( zn - zf );
	pOut->m[2][3] = -1.0f;
	pOut->m[3][2] = zn * zf / ( zn - zf );
	return pOut;
}

D3DXMATRIX* WINAPI D3DXMatrixPerspectiveOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf )
{
	memset( pOut, 0, sizeof( *pOut ) );
	pOut->m[0][0] = 2.0f * zn / ( r - l );
	pOut->m[1][1] = 2.0f * zn / ( t - b );
	pOut->m[2][0] = ( l + r ) / ( r - l );
	pOut->m[2][1] = ( t + b ) / ( t - b );
	pOut->m[2][2] = zf / ( zn - zf );
	pOut->m[2][3] = -1.0f;
	pOut->m[3][2] = zn * zf / ( zn - zf );
	return pOut;
}

D3DXVECTOR3* WINAPI D3DXVec3TransformCoord( D3DXVECTOR3 *pOut, const D3DXVECTOR3 *pV, const D3DXMATRIX *pM )
{
	D3DXVECTOR3 v;
	float w = pM->m[0][3] * pV->x + pM->m[1][3] * pV->y + pM->m[2][3] * pV->z + pM->m[3][3];
	float invW = w ? 1.0f / w : 0.0f;
	v.x = ( pM->m[0][0] * pV->x + pM->m[1][0] * pV->y + pM->m[2][0] * pV->z + pM->m[3][0] ) * invW;
	v.y = ( pM->m[0][1] * pV->x + pM->m[1][1] * pV->y + pM->m[2][1] * pV->z + pM->m[3][1] ) * invW;
	v.z = ( pM->m[0][2] * pV->x + pM->m[1][2] * pV->y + pM->m[2][2] * pV->z + pM->m[3][2] ) * invW;
	*pOut = v;
	return pOut;
}

D3DXVECTOR4* WINAPI D3DXVec4Transform( D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV, const D3DXMATRIX *pM )
{
	const float in[4] = { pV->x, pV->y, pV->z, pV->w };
	float out[4];
	for ( int j = 0; j < 4; j++ )
		out[j] = in[0] * pM->m[0][j] + in[1] * pM->m[1][j] + in[2] * pM->m[2][j] + in[3] * pM->m[3][j];
	pOut->x = out[0]; pOut->y = out[1]; pOut->z = out[2]; pOut->w = out[3];
	return pOut;
}

D3DXVECTOR4* WINAPI D3DXVec4Normalize( D3DXVECTOR4 *pOut, const D3DXVECTOR4 *pV )
{
	float len = sqrtf( pV->x * pV->x + pV->y * pV->y + pV->z * pV->z + pV->w * pV->w );
	if ( len )
	{
		float inv = 1.0f / len;
		pOut->x = pV->x * inv; pOut->y = pV->y * inv; pOut->z = pV->z * inv; pOut->w = pV->w * inv;
	}
	else
	{
		pOut->x = pOut->y = pOut->z = pOut->w = 0.0f;
	}
	return pOut;
}

D3DXPLANE* WINAPI D3DXPlaneNormalize( D3DXPLANE *pOut, const D3DXPLANE *pP )
{
	float len = sqrtf( pP->a * pP->a + pP->b * pP->b + pP->c * pP->c );
	if ( len )
	{
		float inv = 1.0f / len;
		pOut->a = pP->a * inv; pOut->b = pP->b * inv; pOut->c = pP->c * inv; pOut->d = pP->d * inv;
	}
	else
	{
		pOut->a = pOut->b = pOut->c = pOut->d = 0.0f;
	}
	return pOut;
}

// Like D3DX: the plane is transformed as a row vector (pass the inverse transpose of the
// point transform to move a plane).
D3DXPLANE* WINAPI D3DXPlaneTransform( D3DXPLANE *pOut, const D3DXPLANE *pP, const D3DXMATRIX *pM )
{
	const float in[4] = { pP->a, pP->b, pP->c, pP->d };
	float out[4];
	for ( int j = 0; j < 4; j++ )
		out[j] = in[0] * pM->m[0][j] + in[1] * pM->m[1][j] + in[2] * pM->m[2][j] + in[3] * pM->m[3][j];
	pOut->a = out[0]; pOut->b = out[1]; pOut->c = out[2]; pOut->d = out[3];
	return pOut;
}

// ---------------------------------------------------------------------------------------
// Matrix stack
// ---------------------------------------------------------------------------------------

class CDxvkMatrixStack : public ID3DXMatrixStack
{
public:
	CDxvkMatrixStack() : m_nRef( 1 ), m_nTop( 0 ) { MatIdentity( &m_Stack[0] ); }

	STDMETHOD( QueryInterface )( REFIID riid, void **out ) { *out = NULL; return E_NOINTERFACE; }
	STDMETHOD_( DXVK_ULONG, AddRef )() { return ++m_nRef; }
	STDMETHOD_( DXVK_ULONG, Release )()
	{
		DXVK_ULONG n = --m_nRef;
		if ( !n )
			delete this;
		return n;
	}

	STDMETHOD( Pop )()
	{
		if ( m_nTop == 0 )
			return D3DERR_INVALIDCALL;
		m_nTop--;
		return D3D_OK;
	}
	STDMETHOD( Push )()
	{
		if ( m_nTop + 1 >= MAX_DEPTH )
		{
			Warning( "D3DX matrix stack overflow\n" );
			return E_OUTOFMEMORY;
		}
		m_Stack[m_nTop + 1] = m_Stack[m_nTop];
		m_nTop++;
		return D3D_OK;
	}
	STDMETHOD( LoadIdentity )() { MatIdentity( Top() ); return D3D_OK; }
	STDMETHOD( LoadMatrix )( const D3DXMATRIX *pM ) { *Top() = *pM; return D3D_OK; }
	STDMETHOD( MultMatrix )( const D3DXMATRIX *pM ) { D3DXMatrixMultiply( Top(), Top(), pM ); return D3D_OK; }
	STDMETHOD( MultMatrixLocal )( const D3DXMATRIX *pM ) { D3DXMatrixMultiply( Top(), pM, Top() ); return D3D_OK; }
	STDMETHOD( RotateAxis )( const D3DXVECTOR3 *pV, FLOAT angle )
	{
		D3DXMATRIX t; D3DXMatrixRotationAxis( &t, pV, angle ); return MultMatrix( &t );
	}
	STDMETHOD( RotateAxisLocal )( const D3DXVECTOR3 *pV, FLOAT angle )
	{
		D3DXMATRIX t; D3DXMatrixRotationAxis( &t, pV, angle ); return MultMatrixLocal( &t );
	}
	STDMETHOD( RotateYawPitchRoll )( FLOAT yaw, FLOAT pitch, FLOAT roll )
	{
		D3DXMATRIX t; D3DXMatrixRotationYawPitchRoll( &t, yaw, pitch, roll ); return MultMatrix( &t );
	}
	STDMETHOD( RotateYawPitchRollLocal )( FLOAT yaw, FLOAT pitch, FLOAT roll )
	{
		D3DXMATRIX t; D3DXMatrixRotationYawPitchRoll( &t, yaw, pitch, roll ); return MultMatrixLocal( &t );
	}
	STDMETHOD( Scale )( FLOAT x, FLOAT y, FLOAT z )
	{
		D3DXMATRIX t; D3DXMatrixScaling( &t, x, y, z ); return MultMatrix( &t );
	}
	STDMETHOD( ScaleLocal )( FLOAT x, FLOAT y, FLOAT z )
	{
		D3DXMATRIX t; D3DXMatrixScaling( &t, x, y, z ); return MultMatrixLocal( &t );
	}
	STDMETHOD( Translate )( FLOAT x, FLOAT y, FLOAT z )
	{
		D3DXMATRIX t; D3DXMatrixTranslation( &t, x, y, z ); return MultMatrix( &t );
	}
	STDMETHOD( TranslateLocal )( FLOAT x, FLOAT y, FLOAT z )
	{
		D3DXMATRIX t; D3DXMatrixTranslation( &t, x, y, z ); return MultMatrixLocal( &t );
	}
	STDMETHOD_( D3DXMATRIX *, GetTop )() { return Top(); }

private:
	enum { MAX_DEPTH = 64 };
	D3DXMATRIX *Top() { return &m_Stack[m_nTop]; }

	DXVK_ULONG m_nRef;
	int m_nTop;
	D3DXMATRIX m_Stack[MAX_DEPTH];
};

HRESULT WINAPI D3DXCreateMatrixStack( DWORD flags, ID3DXMatrixStack **ppStack )
{
	*ppStack = new CDxvkMatrixStack;
	return D3D_OK;
}

// ---------------------------------------------------------------------------------------
// Shaders: the game ships compiled shaders (.vcs); runtime HLSL compilation is a
// development feature and not available here.
// ---------------------------------------------------------------------------------------

DWORD WINAPI D3DXGetShaderVersion( const DWORD *pByteCode )
{
	return pByteCode ? *pByteCode : 0;
}

HRESULT WINAPI D3DXCompileShader( const char *src_data, UINT data_len, const D3DXMACRO *defines,
	ID3DXInclude *include, const char *function_name, const char *profile, DWORD flags,
	ID3DXBuffer **shader, ID3DXBuffer **error_messages, ID3DXConstantTable **constant_table )
{
	Warning( "D3DXCompileShader is not available with DXVK-native (profile %s)\n", profile ? profile : "?" );
	if ( shader )
		*shader = NULL;
	if ( error_messages )
		*error_messages = NULL;
	if ( constant_table )
		*constant_table = NULL;
	return E_NOTIMPL;
}

// Used by CShaderDeviceMgrDx8::GetVidMemBytes (wmi.cpp on Windows). D3D9 has no way to ask
// for the amount of video memory per adapter; the engine only uses it to pick default
// texture quality settings.
uint64 GetVidMemBytes( void )
{
	return 2048ull * 1024 * 1024;
}

#endif // DXVK
