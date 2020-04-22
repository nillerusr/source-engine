//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//------------------------------------------------------------------------------
// DX9AsmToGL2.cpp
//------------------------------------------------------------------------------
// Immediately include gl.h, etc. here to avoid compilation warnings.
#include <GL/gl.h>
#include <GL/glext.h>

#include "togl/rendermechanism.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "dx9asmtogl2.h"

#include "materialsystem/IShader.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef POSIX
#define strcat_s( a, b, c) V_strcat( a, c, b )
#endif

#define DST_REGISTER		0
#define SRC_REGISTER		1

// Flags to PrintUsageAndIndexToString.
#define SEMANTIC_OUTPUT		0x01
#define SEMANTIC_INPUT		0x02

#define UNDECLARED_OUTPUT	0xFFFFFFFF
#define UNDECLARED_INPUT	0xFFFFFFFF

#ifndef POSIX
#define Debugger() Assert(0)
#endif

//#define Assert(n) if( !(n) ){ TranslationError(); }


static char *g_szVecZeros[] = { NULL, "0.0", "vec2( 0.0, 0.0 )", "vec3( 0.0, 0.0, 0.0 )", "vec4( 0.0, 0.0, 0.0, 0.0 )" };
static char *g_szVecOnes[] = { NULL, "1.0", "vec2( 1.0, 1.0 )", "vec3( 1.0, 1.0, 1.0 )", "vec4( 1.0, 1.0, 1.0, 1.0 )" };
static char *g_szDefaultSwizzle = "xyzw";
static char *g_szDefaultSwizzleStrings[] = { "x", "y", "z", "w" };
static char *g_szSamplerStrings[] = { "2D", "CUBE", "3D" };

static const char *g_pAtomicTempVarName = "atomic_temp_var";
static const char *g_pTangentAttributeName = "g_tangent";

int __cdecl SortInts( const int *a, const int *b )
{
	if ( *a < *b )
		return -1;
	else if ( *a > *b )
		return 1;
	else
		return 0;
}

void StripExtraTrailingZeros( char *pStr )
{
	int len = (int)V_strlen( pStr );
	while ( len >= 2 && pStr[len-1] == '0' && pStr[len-2] != '.' )
	{
		pStr[len-1] = 0;
		--len;
	}
}

void D3DToGL::PrintToBufWithIndents( CUtlBuffer &buf, const char *pFormat, ... )
{
	va_list marker;
	va_start( marker, pFormat );

	char szTemp[1024];
	V_vsnprintf( szTemp, sizeof( szTemp ), pFormat, marker );
	va_end( marker );

	PrintIndentation( (char*)buf.Base(), buf.Size() );
	strcat_s( (char*)buf.Base(), buf.Size(), szTemp );
}

void PrintToBuf( CUtlBuffer &buf, const char *pFormat, ... )
{
	va_list marker;
	va_start( marker, pFormat );
	
	char szTemp[1024];
	V_vsnprintf( szTemp, sizeof( szTemp ), pFormat, marker );
	va_end( marker );

	strcat_s( (char*)buf.Base(), buf.Size(), szTemp );
}

void PrintToBuf( char *pOut, int nOutSize, const char *pFormat, ... )
{
	int nStrlen = V_strlen( pOut );
	pOut += nStrlen;
	nOutSize -= nStrlen;
	
	va_list marker;
	va_start( marker, pFormat );
	V_vsnprintf( pOut, nOutSize, pFormat, marker );
	va_end( marker );
}

// Return the number of letters following the dot.
// Returns 4 if there is no dot.
// (So "r0.xy" returns 2 and "r0" returns 4).
int GetNumWriteMaskEntries( const char *pParam )
{
	const char *pDot = strchr( pParam, '.' );
	if ( pDot )
		return V_strlen( pDot + 1 );
	else
		return 4;
}

const char* GetSwizzleDot( const char *pParam )
{
	const char *pDot = strrchr( pParam, '.' );

	const char *pSquareClose = strrchr( pParam, ']' );

	if ( pSquareClose )
	{
		// The test against ']' catches cases like, so we point to the last dot vc[int(va_r.x) + 29].x
		if ( pDot && ( pSquareClose < pDot  ) )
			return pDot;
		else
			return NULL;
	}

	// Make sure the next character is a valid swizzle since we want to treat strings like vec4( gl_Normal, 0.0 ) as a whole param name.
	if ( pDot && ( ( *(pDot+1) == 'x' ) || ( *(pDot+1) == 'y' ) || ( *(pDot+1) == 'z' ) || ( *(pDot+1) == 'w' ) ||
				   ( *(pDot+1) == 'r' ) || ( *(pDot+1) == 'g' ) || ( *(pDot+1) == 'b' ) || ( *(pDot+1) == 'z' ) ) )
	{
		return pDot;
	}

	return NULL;
}

int GetNumSwizzleComponents( const char *pParam )
{
	// Special scalar output which won't accept a swizzle
	if ( !V_stricmp( pParam, "gl_FogFragCoord" ) )
		return 1;

	// Special scalar output which won't accept a swizzle
	if ( !V_stricmp( pParam, "gl_FragDepth" ) )
		return 1;	
	
	// Special scalar output which won't accept a swizzle
	if ( !V_stricmp( pParam, "a0" ) )
		return 1;
	
	const char *pDot = GetSwizzleDot( pParam );
	if ( pDot )
	{
		pDot++; // Step over the dot

		int nNumSwizzleComponents = 0;
		while ( ( *pDot == 'x' ) || ( *pDot == 'y' ) || ( *pDot == 'z' ) || ( *pDot == 'w' ) ||
				( *pDot == 'r' ) || ( *pDot == 'g' ) || ( *pDot == 'b' ) || ( *pDot == 'z' ) )
		{
			nNumSwizzleComponents++;
			pDot++;
		}

		return nNumSwizzleComponents;
	}

	return 0;
}

char GetSwizzleComponent( const char *pParam, int n )
{
	Assert( n < 4 );

	const char *pDot = GetSwizzleDot( pParam );
	if ( pDot )
	{
		++pDot;
		int nComponents = (int)V_strlen( pDot );
		Assert( nComponents > 0 );

		if ( n < nComponents )
			return pDot[n];
		else
			return pDot[nComponents-1];
	}

	return g_szDefaultSwizzle[n];
}

// Replace the parameter name and leave the swizzle intact.
// So "somevar.xyz" becomes "othervar.xyz".
void ReplaceParamName( const char *pSrc, const char *pNewParamName, char *pOut, int nOutLen )
{
	// Start with the new parameter name.
	V_strncpy( pOut, pNewParamName, nOutLen );

	// Now add the swizzle if necessary.
	const char *pDot = GetSwizzleDot( pSrc );
	if ( pDot )
	{
		V_strncat( pOut, pDot, nOutLen );
	}
}

void GetParamNameWithoutSwizzle( const char *pParam, char *pOut, int nOutLen )
{
	char *pParamStart = (char *) pParam;
	const char *pDot = GetSwizzleDot( pParam );			// dot followed by valid swizzle characters
	bool bAbsWrapper = false;

	// Check for abs() or -abs() wrapper and strip it off during the fixup
	if ( !V_strncmp( pParam, "abs(", 4 ) || !V_strncmp( pParam, "-abs(", 5 ) )
	{
		const char *pOpenParen = strchr( pParam, '(' );		// FIRST opening paren
		const char *pClosingParen = strrchr( pParam, ')' ); // LAST closing paren

		Assert ( pOpenParen && pClosingParen );
		pClosingParen; // hush compiler

		pParamStart = (char *) pOpenParen;
		pParamStart++;
		bAbsWrapper = true;
	}

	if ( pDot  )
	{
		int nToCopy = MIN( nOutLen-1, pDot - pParamStart );
		memcpy( pOut, pParamStart, nToCopy );
		pOut[nToCopy] = 0;
	}
	else
	{
		V_strncpy( pOut, pParamStart, bAbsWrapper ? nOutLen - 1 : nOutLen );
	}
}

bool DoParamNamesMatch( const char *pParam1, const char *pParam2 )
{
	char szTemp[2][256];
	GetParamNameWithoutSwizzle( pParam1, szTemp[0], sizeof( szTemp[0] ) );
	GetParamNameWithoutSwizzle( pParam2, szTemp[1], sizeof( szTemp[1] ) );
	return ( V_stricmp( szTemp[0], szTemp[1] ) == 0 );
}



// Extract the n'th component of the swizzle mask.
// If n would exceed the length of the swizzle mask, then it looks up into "xyzw".
void WriteParamWithSingleMaskEntry( const char *pParam, int n, char *pOut, int nOutLen )
{
	bool bCloseParen = false;
	if ( !V_strncmp( pParam, "-abs(", 5 ) )
	{
		V_strcpy( pOut, "-abs(" );
		bCloseParen = true;
		
		pOut += 5; nOutLen -= 5;
	}
	else if ( !V_strncmp( pParam, "abs(", 4 ) )
	{
		V_strcpy( pOut, "abs(" );
		bCloseParen = true;
		
		pOut += 4; nOutLen -= 4;
	}
	
	GetParamNameWithoutSwizzle( pParam, pOut, nOutLen );
	PrintToBuf( pOut, nOutLen, "." );
	PrintToBuf( pOut, nOutLen, "%c", GetSwizzleComponent( pParam, n ) );

	if ( bCloseParen )
	{
		PrintToBuf( pOut, nOutLen, ")" );
	}
}


float uint32ToFloat( uint32 dw )
{
	return *((float*)&dw);
}

CUtlString EnsureNumSwizzleComponents( const char *pSrcRegisterName, int nComponents )
{
	int nExisting = GetNumSwizzleComponents( pSrcRegisterName );
	if ( nExisting == nComponents )
		return pSrcRegisterName;

	bool bAbsWrapper = false; // Parameter wrapped in an abs()
	bool bAbsNegative = false; // -abs()
	char szSrcRegister[128];
	V_strncpy( szSrcRegister, pSrcRegisterName, sizeof(szSrcRegister) );

	// Check for abs() or -abs() wrapper and strip it off during the fixup
	if ( !V_strncmp( pSrcRegisterName, "abs(", 4 ) || !V_strncmp( pSrcRegisterName, "-abs(", 5 ) )
	{
		bAbsWrapper = true;
		bAbsNegative = pSrcRegisterName[0] == '-';

		const char *pOpenParen = strchr( pSrcRegisterName, '(' );		// FIRST opening paren
		const char *pClosingParen = strrchr( pSrcRegisterName, ')' ); // LAST closing paren

		Assert ( pOpenParen && pClosingParen );	// If we start with abs( and don't get both parens, something is very wrong

		// Copy out just the register name with no abs()
		int nRegNameLength = pClosingParen - pOpenParen - 1;
		V_strncpy( szSrcRegister, pOpenParen+1, nRegNameLength + 1 ); // Kind of a weird function...copy more than you need and slam the last char to NULL-terminate
	}

	char szReg[256];
	GetParamNameWithoutSwizzle( szSrcRegister, szReg, sizeof( szReg ) );
	if ( nComponents == 0 )
		return szReg;

	PrintToBuf( szReg, sizeof( szReg ), "." );
	if ( nExisting > nComponents )
	{
		// DX ASM will sometimes have statements like "NRM r0.xyz, r1.yzww", where it just doesn't use the last part of r1. So we won't either.
		for ( int i=0; i < nComponents; i++ )
		{
			PrintToBuf( szReg, sizeof( szReg ), "%c", GetSwizzleComponent( szSrcRegister, i ) );
		}
	}
	else
	{
		if ( nExisting == 0 )
		{
			// We've got something like r0 and need N more components, so add as much of "xyzw" is needed.
			for ( int i=0; i < nComponents; i++ )
				PrintToBuf( szReg, sizeof( szReg ), "%c", g_szDefaultSwizzle[i] );
		}
		else
		{
			// We've got something like r0.x and need N more components, so replicate the X so it looks like r0.xxx
			V_strncpy( szReg, szSrcRegister, sizeof( szReg ) );
			char cLast = szSrcRegister[ V_strlen( szSrcRegister ) - 1 ];
			for ( int i=nExisting; i < nComponents; i++ )
			{
				PrintToBuf( szReg, sizeof( szReg ), "%c", cLast );
			}
		}
	}

	if ( bAbsWrapper )
	{
		char szTemp[128];
		V_strncpy( szTemp, szReg, sizeof(szTemp) );
		V_snprintf( szReg, sizeof( szReg ), "%sabs(%s)", bAbsNegative ? "-" : "", szTemp ) ;
	}

	return szReg;	
}

static void TranslationError()
{
	GLMDebugPrintf( "D3DToGL: GLSL translation error!\n" );
	DebuggerBreakIfDebugging();
	
	Error( "D3DToGL: GLSL translation error!\n" );
}

D3DToGL::D3DToGL()
{
}

uint32 D3DToGL::GetNextToken( void )
{
	uint32 dwToken = *m_pdwNextToken;
	m_pdwNextToken++;
	return dwToken;
}

void D3DToGL::SkipTokens( uint32 numToSkip )
{
	m_pdwNextToken += numToSkip;
}

uint32 D3DToGL::Opcode( uint32 dwToken )
{
	return ( dwToken & D3DSI_OPCODE_MASK );
}

uint32 D3DToGL::OpcodeSpecificData (uint32 dwToken)
{
	return ( ( dwToken & D3DSP_OPCODESPECIFICCONTROL_MASK ) >> D3DSP_OPCODESPECIFICCONTROL_SHIFT );
}

uint32 D3DToGL::TextureType ( uint32 dwToken )
{
	return ( dwToken & D3DSP_TEXTURETYPE_MASK ); // Note this one doesn't shift due to weird D3DSAMPLER_TEXTURE_TYPE enum
}



// Print GLSL intrinsic corresponding to particular instruction
bool D3DToGL::OpenIntrinsic( uint32 inst, char* buff, int nBufLen, uint32 destDimension, uint32 nArgumentDimension )
{
	// Some GLSL intrinsics need type conversion, which we do in this routine
	// As a result, the caller must sometimes close both parentheses, not just one
	bool bDoubleClose = false;
	
	if ( nArgumentDimension == 0 )
	{
		nArgumentDimension = 4;
	}
	
	switch ( inst )
	{
		case D3DSIO_RSQ:
			V_snprintf( buff, nBufLen, "inversesqrt( " );
			break;
		case D3DSIO_DP3:
		case D3DSIO_DP4:
			if ( destDimension == 1 )
			{
				V_snprintf( buff, nBufLen, "dot( " );
			}
			else
			{
				if ( !destDimension )
					destDimension = 4;
				V_snprintf( buff, nBufLen, "vec%d( dot( ", destDimension );
				bDoubleClose = true;
			}
			break;
		case D3DSIO_MIN:
			V_snprintf( buff, nBufLen, "min( " );
			break;
		case D3DSIO_MAX:
			V_snprintf( buff, nBufLen, "max( " );
			break;
		case D3DSIO_SLT:
			if ( nArgumentDimension == 1 )
			{
				V_snprintf( buff, nBufLen, "float( " ); // lessThan doesn't have a scalar version
			}
			else
			{
				Assert( nArgumentDimension > 1 );
				V_snprintf( buff, nBufLen, "vec%d( lessThan( ", nArgumentDimension );
				bDoubleClose = true;
			}
			break;
		case D3DSIO_SGE:
			if ( nArgumentDimension == 1 )
			{
				V_snprintf( buff, nBufLen, "float( " ); // greaterThanEqual doesn't have a scalar version
			}
			else
			{
				Assert( nArgumentDimension > 1 );
				V_snprintf( buff, nBufLen, "vec%d( greaterThanEqual( ", nArgumentDimension );
				bDoubleClose = true;
			}					
			break;
		case D3DSIO_EXP:
			V_snprintf( buff, nBufLen, "exp( " );  // exp2 ?
			break;
		case D3DSIO_LOG:
			V_snprintf( buff, nBufLen, "log( " );	// log2 ?
			break;
		case D3DSIO_LIT:
			TranslationError();
			V_snprintf( buff, nBufLen, "lit( " ); // gonna have to write this one
			break;
		case D3DSIO_DST:
			V_snprintf( buff, nBufLen, "dst( " ); // gonna have to write this one
			break;
		case D3DSIO_LRP:
			Assert( !m_bVertexShader );
			V_snprintf( buff, nBufLen, "mix( " );
			break;
		case D3DSIO_FRC:
			V_snprintf( buff, nBufLen, "fract( " );
			break;
		case D3DSIO_M4x4:
			TranslationError();
			V_snprintf( buff, nBufLen, "m4x4" );
			break;
		case D3DSIO_M4x3:
		case D3DSIO_M3x4:
		case D3DSIO_M3x3:
		case D3DSIO_M3x2:
		case D3DSIO_CALL:
		case D3DSIO_CALLNZ:
		case D3DSIO_LOOP:
		case D3DSIO_RET:
		case D3DSIO_ENDLOOP:
		case D3DSIO_LABEL:
		case D3DSIO_DCL:
			TranslationError();
			break;
		case D3DSIO_POW:
			V_snprintf( buff, nBufLen, "pow( " );
			break;
		case D3DSIO_CRS:
			V_snprintf( buff, nBufLen, "cross( " );
			break;
		case D3DSIO_SGN:
			TranslationError();
			V_snprintf( buff, nBufLen, "sign( " );
			break;
		case D3DSIO_ABS:
			V_snprintf( buff, nBufLen, "abs( " );
			break;
		case D3DSIO_NRM:
			TranslationError();
			V_snprintf( buff, nBufLen, "normalize( " );
			break;
		case D3DSIO_SINCOS:
			TranslationError();
			V_snprintf( buff, nBufLen, "sincos( " ); // gonna have to write this one
			break;
		case D3DSIO_REP:
		case D3DSIO_ENDREP:
		case D3DSIO_IF:
		case D3DSIO_IFC:
		case D3DSIO_ELSE:
		case D3DSIO_ENDIF:
		case D3DSIO_BREAK:
		case D3DSIO_BREAKC:		// TODO: these are the reason we even need GLSL...gotta make these work
			TranslationError();
			break;
		case D3DSIO_DEFB:
		case D3DSIO_DEFI:
			TranslationError();
			break;
		case D3DSIO_TEXCOORD:
			V_snprintf( buff, nBufLen, "texcoord" );
			break;
		case D3DSIO_TEXKILL:
			V_snprintf( buff, nBufLen, "kill( " ); // wrap the discard instruction?
			break;
		case D3DSIO_TEX:
			TranslationError();
			V_snprintf( buff, nBufLen, "TEX" );		// We shouldn't get here
			break;
		case D3DSIO_TEXBEM:
		case D3DSIO_TEXBEML:
		case D3DSIO_TEXREG2AR:
		case D3DSIO_TEXREG2GB:
		case D3DSIO_TEXM3x2PAD:
		case D3DSIO_TEXM3x2TEX:
		case D3DSIO_TEXM3x3PAD:
		case D3DSIO_TEXM3x3TEX:
		case D3DSIO_TEXM3x3SPEC:
		case D3DSIO_TEXM3x3VSPEC:
			TranslationError();
			break;
		case D3DSIO_EXPP:
			V_snprintf( buff, nBufLen, "exp( " );
			break;
		case D3DSIO_LOGP:
			V_snprintf( buff, nBufLen, "log( " );
			break;
		case D3DSIO_CND:
			TranslationError();
			break;
		case D3DSIO_DEF:
			TranslationError();
			V_snprintf( buff, nBufLen, "DEF" );
			break;
		case D3DSIO_TEXREG2RGB:
		case D3DSIO_TEXDP3TEX:
		case D3DSIO_TEXM3x2DEPTH:
		case D3DSIO_TEXDP3:
		case D3DSIO_TEXM3x3:
			TranslationError();
			break;
		case D3DSIO_TEXDEPTH:
			V_snprintf( buff, nBufLen, "texdepth" );
			break;
		case D3DSIO_CMP:
			TranslationError();
			Assert( !m_bVertexShader );
			V_snprintf( buff, nBufLen, "CMP" );
			break;
		case D3DSIO_BEM:
			TranslationError();
			break;
		case D3DSIO_DP2ADD:
			TranslationError();
			break;
		case D3DSIO_DSX:
		case D3DSIO_DSY:
			TranslationError();
			break;
		case D3DSIO_TEXLDD:
			V_snprintf( buff, nBufLen, "texldd" );
			break;
		case D3DSIO_SETP:
			TranslationError();
			break;
		case D3DSIO_TEXLDL:
			V_snprintf( buff, nBufLen, "texldl" );
			break;
		case D3DSIO_BREAKP:
		case D3DSIO_PHASE:
			TranslationError();
			break;
	}
	
	return bDoubleClose;
}


const char* D3DToGL::GetGLSLOperatorString( uint32 inst )
{
	if ( inst == D3DSIO_ADD )
		return "+";
	else if ( inst == D3DSIO_SUB )
		return "-";
	else if ( inst == D3DSIO_MUL )
		return "*";
	
	Error( "GetGLSLOperatorString: unknown operator" );
	return "zzzz";
}


// Print ASM opcode
void D3DToGL::PrintOpcode( uint32 inst, char* buff, int nBufLen )
{
	switch ( inst )
	{
		case D3DSIO_NOP:
			V_snprintf( buff, nBufLen, "NOP" );
			TranslationError();
			break;
		case D3DSIO_MOV:
			V_snprintf( buff, nBufLen, "MOV" );
			break;
		case D3DSIO_ADD:
			V_snprintf( buff, nBufLen, "ADD" );
			break;
		case D3DSIO_SUB:
			V_snprintf( buff, nBufLen, "SUB" );
			break;
		case D3DSIO_MAD:
			V_snprintf( buff, nBufLen, "MAD" );
			break;
		case D3DSIO_MUL:
			V_snprintf( buff, nBufLen, "MUL" );
			break;
		case D3DSIO_RCP:
			V_snprintf( buff, nBufLen, "RCP" );
			break;
		case D3DSIO_RSQ:
			V_snprintf( buff, nBufLen, "RSQ" );
			break;
		case D3DSIO_DP3:
			V_snprintf( buff, nBufLen, "DP3" );
			break;
		case D3DSIO_DP4:
			V_snprintf( buff, nBufLen, "DP4" );
			break;
		case D3DSIO_MIN:
			V_snprintf( buff, nBufLen, "MIN" );
			break;
		case D3DSIO_MAX:
			V_snprintf( buff, nBufLen, "MAX" );
			break;
		case D3DSIO_SLT:
			V_snprintf( buff, nBufLen, "SLT" );
			break;
		case D3DSIO_SGE:
			V_snprintf( buff, nBufLen, "SGE" );
			break;
		case D3DSIO_EXP:
			V_snprintf( buff, nBufLen, "EX2" );
			break;
		case D3DSIO_LOG:
			V_snprintf( buff, nBufLen, "LG2" );
			break;
		case D3DSIO_LIT:
			V_snprintf( buff, nBufLen, "LIT" );
			break;
		case D3DSIO_DST:
			V_snprintf( buff, nBufLen, "DST" );
			break;
		case D3DSIO_LRP:
			Assert( !m_bVertexShader );
			V_snprintf( buff, nBufLen, "LRP" );
			break;
		case D3DSIO_FRC:
			V_snprintf( buff, nBufLen, "FRC" );
			break;
		case D3DSIO_M4x4:
			V_snprintf( buff, nBufLen, "m4x4" );
			break;
		case D3DSIO_M4x3:
		case D3DSIO_M3x4:
		case D3DSIO_M3x3:
		case D3DSIO_M3x2:
		case D3DSIO_CALL:
		case D3DSIO_CALLNZ:
		case D3DSIO_LOOP:
		case D3DSIO_RET:
		case D3DSIO_ENDLOOP:
		case D3DSIO_LABEL:
			TranslationError();
			break;
		case D3DSIO_DCL:
			V_snprintf( buff, nBufLen, "DCL" );
			break;
		case D3DSIO_POW:
			V_snprintf( buff, nBufLen, "POW" );
			break;
		case D3DSIO_CRS:
			V_snprintf( buff, nBufLen, "XPD" );
			break;
		case D3DSIO_SGN:
			TranslationError();
			V_snprintf( buff, nBufLen, "SGN" );
			break;
		case D3DSIO_ABS:
			V_snprintf( buff, nBufLen, "ABS" );
			break;
		case D3DSIO_NRM:
			TranslationError();
			V_snprintf( buff, nBufLen, "NRM" );
			break;
		case D3DSIO_SINCOS:
			Assert( !m_bVertexShader );
			V_snprintf( buff, nBufLen, "SCS" );
			break;
		case D3DSIO_REP:
		case D3DSIO_ENDREP:
		case D3DSIO_IF:
		case D3DSIO_IFC:
		case D3DSIO_ELSE:
		case D3DSIO_ENDIF:
		case D3DSIO_BREAK:
		case D3DSIO_BREAKC:
			TranslationError();
			break;
		case D3DSIO_MOVA:
			Assert( m_bVertexShader );
			V_snprintf( buff, nBufLen, "MOV" ); // We're always moving into a temp instead, so this is MOV instead of ARL
			break;
		case D3DSIO_DEFB:
		case D3DSIO_DEFI:
			TranslationError();
			break;
		case D3DSIO_TEXCOORD:
			V_snprintf( buff, nBufLen, "texcoord" );
			break;
		case D3DSIO_TEXKILL:
			V_snprintf( buff, nBufLen, "KIL" );
			break;
		case D3DSIO_TEX:
			V_snprintf( buff, nBufLen, "TEX" );
			break;
		case D3DSIO_TEXBEM:
		case D3DSIO_TEXBEML:
		case D3DSIO_TEXREG2AR:
		case D3DSIO_TEXREG2GB:
		case D3DSIO_TEXM3x2PAD:
		case D3DSIO_TEXM3x2TEX:
		case D3DSIO_TEXM3x3PAD:
		case D3DSIO_TEXM3x3TEX:
		case D3DSIO_TEXM3x3SPEC:
		case D3DSIO_TEXM3x3VSPEC:
			TranslationError();
			break;
		case D3DSIO_EXPP:
			V_snprintf( buff, nBufLen, "EXP" );
			break;
		case D3DSIO_LOGP:
			V_snprintf( buff, nBufLen, "LOG" );
			break;
		case D3DSIO_CND:
			TranslationError();
			break;
		case D3DSIO_DEF:
			V_snprintf( buff, nBufLen, "DEF" );
			break;
		case D3DSIO_TEXREG2RGB:
		case D3DSIO_TEXDP3TEX:
		case D3DSIO_TEXM3x2DEPTH:
		case D3DSIO_TEXDP3:
		case D3DSIO_TEXM3x3:
			TranslationError();
			break;
		case D3DSIO_TEXDEPTH:
			V_snprintf( buff, nBufLen, "texdepth" );
			break;
		case D3DSIO_CMP:
			Assert( !m_bVertexShader );
			V_snprintf( buff, nBufLen, "CMP" );
			break;
		case D3DSIO_BEM:
			TranslationError();
			break;
		case D3DSIO_DP2ADD:
			TranslationError();
			break;
		case D3DSIO_DSX:
		case D3DSIO_DSY:
			TranslationError();
			break;
		case D3DSIO_TEXLDD:
			V_snprintf( buff, nBufLen, "texldd" );
			break;
		case D3DSIO_SETP:
			TranslationError();
			break;
		case D3DSIO_TEXLDL:
			V_snprintf( buff, nBufLen, "texldl" );
			break;
		case D3DSIO_BREAKP:
		case D3DSIO_PHASE:
			TranslationError();
			break;
	}
}

CUtlString D3DToGL::GetUsageAndIndexString( uint32 dwToken, int fSemanticFlags )
{
	char szTemp[1024];
	PrintUsageAndIndexToString( dwToken, szTemp, sizeof( szTemp ), fSemanticFlags );
	return szTemp;
}

//------------------------------------------------------------------------------
// Helper function which prints ASCII representation of usage-usageindex pair to string
//
// Strictly used by vertex shaders
// not used any more now that we have attribmap metadata
//------------------------------------------------------------------------------
void D3DToGL::PrintUsageAndIndexToString( uint32 dwToken, char* strUsageUsageIndexName, int nBufLen, int fSemanticFlags )
{
	uint32 dwUsage = ( dwToken & D3DSP_DCL_USAGE_MASK );
	uint32 dwUsageIndex = ( dwToken & D3DSP_DCL_USAGEINDEX_MASK ) >> D3DSP_DCL_USAGEINDEX_SHIFT;

	switch ( dwUsage )
	{
		case D3DDECLUSAGE_POSITION:
			if ( m_bVertexShader )
			{
				if ( fSemanticFlags & SEMANTIC_OUTPUT )
					V_snprintf( strUsageUsageIndexName, nBufLen, "vTempPos" ); // effectively gl_Position
				else
					V_snprintf( strUsageUsageIndexName, nBufLen, "gl_Vertex" );
			}
			else
			{
				// .xy = position in viewport coordinates
				// .z  = depth
				V_snprintf( strUsageUsageIndexName, nBufLen, "gl_FragCoord" );
			}
			
			break;
		case D3DDECLUSAGE_BLENDWEIGHT:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[1]" );	// "vertex.attrib[12]" );			// or [1]
			break;
		case D3DDECLUSAGE_BLENDINDICES:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[13]" );	// "vertex.attrib[13]" );			// or [ 7 ]
			break;
		case D3DDECLUSAGE_NORMAL:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vec4( gl_Normal, 0.0 )" );
			break;
		case D3DDECLUSAGE_PSIZE:
			TranslationError();
			V_snprintf( strUsageUsageIndexName, nBufLen, "_psize" );					// no analog
			break;
		case D3DDECLUSAGE_TEXCOORD:
			V_snprintf( strUsageUsageIndexName, nBufLen, "oT%d", dwUsageIndex );
			break;
		case D3DDECLUSAGE_TANGENT:
			
			NoteTangentInputUsed();
			V_strncpy( strUsageUsageIndexName, g_pTangentAttributeName, nBufLen );
			
			break;
		case D3DDECLUSAGE_BINORMAL:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[14]" );			// aka texc[6]
			break;
//		case D3DDECLUSAGE_TESSFACTOR:
//			TranslationError();
//			V_snprintf( strUsageUsageIndexName, nBufLen, "_position" );					// no analog
//			break;
//		case D3DDECLUSAGE_POSITIONT:
//			TranslationError();
//			V_snprintf( strUsageUsageIndexName, nBufLen, "_positiont" );				// no analog
//			break;
		case D3DDECLUSAGE_COLOR:
			
			Assert( dwUsageIndex <= 1 );
//			if ( fSemanticFlags & SEMANTIC_OUTPUT )
//				V_snprintf( strUsageUsageIndexName, nBufLen, dwUsageIndex != 0 ? "gl_BackColor" : "gl_FrontColor" );
//			else
			V_snprintf( strUsageUsageIndexName, nBufLen, dwUsageIndex != 0 ? "gl_SecondaryColor" : "gl_Color" );
			
			break;
		case D3DDECLUSAGE_FOG:
			TranslationError();
			break;
		case D3DDECLUSAGE_DEPTH:
			TranslationError();
			V_snprintf( strUsageUsageIndexName, nBufLen, "_depth" );					// no analog
			break;
		case D3DDECLUSAGE_SAMPLE:
			TranslationError();
			V_snprintf( strUsageUsageIndexName, nBufLen, "_sample" );					// no analog
			break;
		default:
			Debugger();
		break;
	}
}

uint32 D3DToGL::GetRegType( uint32 dwRegToken )
{
	return ( ( dwRegToken & D3DSP_REGTYPE_MASK2 ) >> D3DSP_REGTYPE_SHIFT2 ) | ( ( dwRegToken & D3DSP_REGTYPE_MASK ) >> D3DSP_REGTYPE_SHIFT );
}

void D3DToGL::PrintIndentation( char *pBuf, int nBufLen )
{
	for( int i=0; i<m_NumIndentTabs; i++ )
	{
		strcat_s( pBuf, nBufLen, "\t" );
	}
}

CUtlString D3DToGL::GetParameterString( uint32 dwToken, uint32 dwSourceOrDest, bool bForceScalarSource, int *pARLDestReg )
{
	char szTemp[1024];
	PrintParameterToString( dwToken, dwSourceOrDest, szTemp, sizeof( szTemp ), bForceScalarSource, pARLDestReg );
	return szTemp;
}


// If the register happens to end with ".xyzw", then this strips off the mask.
void SimplifyFourParamRegister( char *pRegister )
{
	int nLen = V_strlen( pRegister );
	if ( nLen > 5 && V_strcmp( &pRegister[nLen-5], ".xyzw" ) == 0 )
		pRegister[nLen-5] = 0;
}


// This returns 0 for x, 1 for y, 2 for z, and 3 for w.
int GetSwizzleComponentVectorIndex( char chMask )
{
	if ( chMask == 'x' )
		return 0;
	else if ( chMask == 'y' )
		return 1;
	else if ( chMask == 'z' )
		return 2;
	else if ( chMask == 'w' )
		return 3;

	Error( "GetSwizzleComponentVectorIndex( '%c' ) - invalid parameter.\n", chMask );
	return 0;
}


// GLSL needs the # of src masks to match the dest write mask.
//
// So this:
//		r0.xy = r1 + r2;
// becomes:
//		r0.xy = r1.xy + r2.xy;
//
//
// Also, and this is the trickier one: GLSL reads the source registers from their first component on
// whereas D3D reads them as referenced in the dest register mask!
//
//		So this code in D3D:
//			r0.yz = c0.x + c1.wxyz
//		Really means:
//			r0.y = c0.x + c1.x
//			r0.z = c0.x + c1.y
//		So we translate it to this in GLSL:
//			r0.yz = c0.xx + c1.wx
//			r0.yz = c0.xx + c1.xy
//
CUtlString D3DToGL::FixGLSLSwizzle( const char *pDestRegisterName, const char *pSrcRegisterName )
{
	bool bAbsWrapper = false; // Parameter wrapped in an abs()
	bool bAbsNegative = false; // -abs()
	char szSrcRegister[128];
	V_strncpy( szSrcRegister, pSrcRegisterName, sizeof(szSrcRegister) );

	// Check for abs() or -abs() wrapper and strip it off during the fixup
	if ( !V_strncmp( pSrcRegisterName, "abs(", 4 ) || !V_strncmp( pSrcRegisterName, "-abs(", 5 ) )
	{
		bAbsWrapper = true;
		bAbsNegative = pSrcRegisterName[0] == '-';

		const char *pOpenParen = strchr( pSrcRegisterName, '(' );		// FIRST opening paren
		const char *pClosingParen = strrchr( pSrcRegisterName, ')' ); // LAST closing paren

		Assert ( pOpenParen && pClosingParen );	// If we start with abs( and don't get both parens, something is very wrong

		// Copy out just the register name with no abs()
		int nRegNameLength = pClosingParen - pOpenParen - 1;
		V_strncpy( szSrcRegister, pOpenParen+1, nRegNameLength + 1 ); // Kind of a weird function...copy more than you need and slam the last char to NULL-terminate

	}
	
	int nSwizzlesInDest = GetNumSwizzleComponents( pDestRegisterName );
	if ( nSwizzlesInDest == 0 )
		nSwizzlesInDest = 4;

	char szFixedSrcRegister[128];
	GetParamNameWithoutSwizzle( szSrcRegister, szFixedSrcRegister, sizeof( szFixedSrcRegister ) );
	V_strncat( szFixedSrcRegister, ".", sizeof( szFixedSrcRegister ) );
	for ( int i=0; i < nSwizzlesInDest; i++ )
	{
		char chDestWriteMask = GetSwizzleComponent( pDestRegisterName, i );
		int nVectorIndex = GetSwizzleComponentVectorIndex( chDestWriteMask );

		char ch[2];
		ch[0] = GetSwizzleComponent( szSrcRegister, nVectorIndex );
		ch[1] = 0;
		V_strncat( szFixedSrcRegister, ch, sizeof( szFixedSrcRegister ) );
	}

	SimplifyFourParamRegister( szFixedSrcRegister );

	if ( bAbsWrapper )
	{
		char szTempSrcRegister[128];
		V_strncpy( szTempSrcRegister, szFixedSrcRegister, sizeof(szTempSrcRegister) );
		V_snprintf( szFixedSrcRegister, sizeof( szFixedSrcRegister ), "%sabs(%s)", bAbsNegative ? "-" : "", szTempSrcRegister ) ;
	}

	return szFixedSrcRegister;
}

// Weird encoding...bits are split apart in the dwToken
inline uint32 GetRegTypeFromToken( uint32 dwToken )
{
	return ( ( dwToken & D3DSP_REGTYPE_MASK2 ) >> D3DSP_REGTYPE_SHIFT2 ) | ( ( dwToken & D3DSP_REGTYPE_MASK ) >> D3DSP_REGTYPE_SHIFT );
}

void D3DToGL::FlagIndirectRegister( uint32 dwToken, int *pARLDestReg )
{
	if ( !pARLDestReg )
		return;

	switch ( dwToken & D3DVS_SWIZZLE_MASK & D3DVS_X_W )
	{
		case D3DVS_X_X:
			*pARLDestReg = ARL_DEST_X;
			break;
		case D3DVS_X_Y:
			*pARLDestReg = ARL_DEST_Y;
			break;
		case D3DVS_X_Z:
			*pARLDestReg = ARL_DEST_Z;
			break;
		case D3DVS_X_W:
			*pARLDestReg = ARL_DEST_W;
			break;
	}
}


//------------------------------------------------------------------------------
// PrintParameterToString()
//
// Helper function which prints ASCII representation of passed Parameter dwToken
// to string. Token defines parameter details. The dwSourceOrDest parameter says 
// whether or not this is a source or destination register
//------------------------------------------------------------------------------
void D3DToGL::PrintParameterToString ( uint32 dwToken, uint32 dwSourceOrDest, char *pRegisterName, int nBufLen, bool bForceScalarSource, int *pARLDestReg )
{
	char buff[32];
	bool bAllowWriteMask = true;
	bool bAllowSwizzle = true;

	uint32 dwRegNum = dwToken & D3DSP_REGNUM_MASK;

	uint32 dwRegType, dwSwizzle;
	uint32 dwSrcModifier = D3DSPSM_NONE;

	// Clear string to zero length
	pRegisterName[ 0 ] = 0;

	dwRegType = GetRegTypeFromToken( dwToken );

	// If this is a dest register
	if ( dwSourceOrDest == DST_REGISTER )
	{
		// Instruction modifiers
		if ( dwToken & D3DSPDM_PARTIALPRECISION )
		{
//			strcat_s( pRegisterName, nBufLen, "_pp" );
		}
				
		if ( dwToken & D3DSPDM_MSAMPCENTROID)
		{
//			strcat_s( pRegisterName, nBufLen, "_centroid" );
		}
	}

	// If this is a source register
	if ( dwSourceOrDest == SRC_REGISTER )
	{
		dwSrcModifier = dwToken & D3DSP_SRCMOD_MASK;

		// If there are any source modifiers, check to see if they're at
		// least partially "prefix" and prepend appropriately
		if ( dwSrcModifier != D3DSPSM_NONE )
		{
			switch ( dwSrcModifier )
			{
				// These four start with just minus... (some may result in "postfix" notation as well later on)
				case D3DSPSM_NEG:							 // negate
					strcat_s( pRegisterName, nBufLen, "-" );
					break;
				case D3DSPSM_BIASNEG:						// bias and negate
				case D3DSPSM_SIGNNEG:						// sign and negate
				case D3DSPSM_X2NEG:						  // *2 and negate
					TranslationError();
					strcat_s( pRegisterName, nBufLen, "-" );
					break;
				case D3DSPSM_COMP:							// complement
					TranslationError();
					strcat_s( pRegisterName, nBufLen, "1-" );
					break;
				case D3DSPSM_ABS:							 // abs()
					strcat_s( pRegisterName, nBufLen, "abs(" );
					
					break;
				case D3DSPSM_ABSNEG:						 // -abs()
					strcat_s( pRegisterName, nBufLen, "-abs(" );
					
					break;
				case D3DSPSM_NOT:							 // for predicate register: "!p0"
					TranslationError();
					strcat_s( pRegisterName, nBufLen, "!" );
					break;
			}
		}
	}

	// Register name (from type and number)
	switch ( dwRegType )
	{
		case D3DSPR_TEMP:
			V_snprintf( buff, sizeof( buff ), "r%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			m_dwTempUsageMask |= 0x00000001 << dwRegNum;			// Keep track of the use of this temp
			break;
		case D3DSPR_INPUT:
			if ( !m_bVertexShader && ( dwSourceOrDest == SRC_REGISTER ) )
			{
				if ( m_dwMajorVersion == 3 )
				{
					V_snprintf( buff, sizeof( buff ), "oTempT%d", dwRegNum );
				}
				else
				{
					V_snprintf( buff, sizeof( buff ), dwRegNum == 0 ? "gl_Color" : "gl_SecondaryColor" );
				}
				strcat_s( pRegisterName, nBufLen, buff );				
			}
			else 
			{
				V_snprintf( buff, sizeof( buff ), "v%d", dwRegNum );
				strcat_s( pRegisterName, nBufLen, buff );
			}
			break;
		case D3DSPR_CONST:
			if ( m_bConstantRegisterDefined[dwRegNum] )
			{
				char szConstantRegName[3];
				if ( m_bVertexShader )
				{
					V_snprintf( szConstantRegName, 3, "vd" );
				}
				else
				{
					V_snprintf( szConstantRegName, 3, "pd" );
				}

				// Put defined constants into their own namespace "d"
				V_snprintf( buff, sizeof( buff ), "%s%d", szConstantRegName, dwRegNum );
				strcat_s( pRegisterName, nBufLen, buff );
			}
			else if ( dwToken & D3DSHADER_ADDRESSMODE_MASK )  // Indirect addressing (e.g. skinning in a vertex shader)
			{
				char szConstantRegName[16];
				if ( m_bVertexShader )
				{
					V_snprintf( szConstantRegName, 3, "vc" );
				}
				else  // No indirect addressing in PS, this shouldn't happen
				{
					TranslationError();
					V_snprintf( szConstantRegName, 3, "pc" );
				}
								
				if ( ( m_bGenerateBoneUniformBuffer ) && ( dwRegNum >= DXABSTRACT_VS_FIRST_BONE_SLOT ) )
				{
					if( dwRegNum < DXABSTRACT_VS_LAST_BONE_SLOT )
					{
						dwRegNum -= DXABSTRACT_VS_FIRST_BONE_SLOT;
						V_strcpy( szConstantRegName, "vcbones" );

						m_nHighestBoneRegister = ( DXABSTRACT_VS_PARAM_SLOTS - 1 ) - DXABSTRACT_VS_FIRST_BONE_SLOT;
					}
					else
					{
						dwRegNum -= ( DXABSTRACT_VS_LAST_BONE_SLOT + 1 ) - DXABSTRACT_VS_FIRST_BONE_SLOT;
						m_nHighestRegister = m_bGenerateBoneUniformBuffer ? ( ( DXABSTRACT_VS_PARAM_SLOTS - 1 ) - ( ( DXABSTRACT_VS_LAST_BONE_SLOT + 1 ) -  DXABSTRACT_VS_FIRST_BONE_SLOT )  ): ( DXABSTRACT_VS_PARAM_SLOTS - 1 );
					}
				}
				else
				{
					m_nHighestRegister = m_bGenerateBoneUniformBuffer ? ( ( DXABSTRACT_VS_PARAM_SLOTS - 1 ) - ( ( DXABSTRACT_VS_LAST_BONE_SLOT + 1 ) -  DXABSTRACT_VS_FIRST_BONE_SLOT )  ): ( DXABSTRACT_VS_PARAM_SLOTS - 1 );
				}

				// Index into single pc/vc[] register array with relative addressing
				int nDstReg = -1;
				FlagIndirectRegister( GetNextToken(), &nDstReg );
				if ( pARLDestReg ) 
					*pARLDestReg = nDstReg;

				Assert( nDstReg != ARL_DEST_NONE );
				int nSrcSwizzle = 'x';
				if ( nDstReg == ARL_DEST_Y )
					nSrcSwizzle = 'y';
				else if ( nDstReg == ARL_DEST_Z )
					nSrcSwizzle = 'z';
				else if ( nDstReg == ARL_DEST_W )
					nSrcSwizzle = 'w';
				V_snprintf( buff, sizeof( buff ), "%s[int(va_r.%c) + %d]", szConstantRegName, nSrcSwizzle, dwRegNum );
								
				strcat_s( pRegisterName, nBufLen, buff );
				
				// Must allow swizzling, otherwise this example doesn't compile right: mad r3.xyz, c27[a0.w].w, r3, r7
				//bAllowSwizzle = false;
			}
			else // Direct addressing of constant array
			{
				char szConstantRegName[16];
				V_snprintf( szConstantRegName, 3, m_bVertexShader ? "vc" : "pc" );

				if ( ( m_bGenerateBoneUniformBuffer ) && ( dwRegNum >= DXABSTRACT_VS_FIRST_BONE_SLOT ) )
				{
					if( dwRegNum < DXABSTRACT_VS_LAST_BONE_SLOT )
					{
						dwRegNum -= DXABSTRACT_VS_FIRST_BONE_SLOT;
						V_strcpy( szConstantRegName, "vcbones" );

						m_nHighestBoneRegister = MAX( m_nHighestBoneRegister, (int)dwRegNum );
					}
					else
					{
						// handles case where constants after the bones are used (c217 onwards), these are to be concatenated with those before the bones (c0-c57) 
						// keep track of regnum for concatenated array
						dwRegNum -= ( DXABSTRACT_VS_LAST_BONE_SLOT + 1 ) - DXABSTRACT_VS_FIRST_BONE_SLOT;
						m_nHighestRegister = MAX( m_nHighestRegister, dwRegNum );	
					}
				}
				else
				{
					//// NOGO if (dwRegNum != 255)	// have seen cases where dwRegNum is 0xFF... need to figure out where those opcodes are coming from
					{
						m_nHighestRegister = MAX( m_nHighestRegister, dwRegNum );	
					}

					Assert( m_nHighestRegister < DXABSTRACT_VS_PARAM_SLOTS );
				}

				// Index into single pc/vc[] register array with absolute addressing, same for GLSL and ASM
				V_snprintf( buff, sizeof( buff ), "%s[%d]", szConstantRegName, dwRegNum );
				strcat_s( pRegisterName, nBufLen, buff );
			}
			break;
		case D3DSPR_ADDR: // aliases to D3DSPR_TEXTURE
			if ( m_bVertexShader )
			{
				Assert( dwRegNum == 0 );

				V_snprintf( buff, sizeof( buff ), "va_r" );
			}
			else // D3DSPR_TEXTURE in the pixel shader
			{
				// If dest reg, this is an iterator/varying declaration
				if ( dwSourceOrDest == DST_REGISTER )
				{
					// Is this iterator centroid?
					if ( m_nCentroidMask & ( 0x00000001 << dwRegNum ) )
					{
						V_snprintf( buff, sizeof( buff ), "centroid varying vec4 oT%d", dwRegNum ); // centroid varying
					}
					else
					{
						V_snprintf( buff, sizeof( buff ), "varying vec4 oT%d", dwRegNum );
					}
					
					bAllowWriteMask = false;
				}
				else // source register
				{
					V_snprintf( buff, sizeof( buff ), "oT%d", dwRegNum );
				}
			}
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_RASTOUT: // vertex shader oPos
			Assert( m_bVertexShader );
			Assert( m_dwMajorVersion == 2 );
			switch( dwRegNum )
			{
				case D3DSRO_POSITION:
					strcat_s( pRegisterName, nBufLen, "vTempPos" ); // In GLSL, this ends up in gl_Position later on
					m_bDeclareVSOPos = true;
				break;
				
				case D3DSRO_FOG:
					strcat_s( pRegisterName, nBufLen, "gl_FogFragCoord" );
					m_bDeclareVSOFog = true;
				break;

				default:
					printf( "\nD3DSPR_RASTOUT: dwRegNum is %08x and token is %08x", dwRegNum, dwToken );  
					TranslationError();
				break;
			}
			break;
		case D3DSPR_ATTROUT:
			Assert( m_bVertexShader );
			Assert( m_dwMajorVersion == 2 );

			if ( dwRegNum == 0 )
			{
				V_snprintf( buff, sizeof( buff ), "gl_FrontColor" );
			}
			else if ( dwRegNum == 1 )
			{
				V_snprintf( buff, sizeof( buff ), "gl_FrontSecondaryColor" );
			}
			else
			{
				Error( "Invalid D3DSPR_ATTROUT index" );
			}
			
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_TEXCRDOUT: // aliases to D3DSPR_OUTPUT
			if ( m_bVertexShader )
			{
				if ( m_nVSPositionOutput == (int32) dwRegNum )
				{
					V_snprintf( buff, sizeof( buff ), "vTempPos" ); // This output varying is the position
				}
				else if ( m_dwMajorVersion == 3 )
				{
					V_snprintf( buff, sizeof( buff ), "oTempT%d", dwRegNum );
				}
				else
				{
					V_snprintf( buff, sizeof( buff ), "oT%d", dwRegNum );
				}
				
				m_dwTexCoordOutMask |= ( 0x00000001 << dwRegNum );
			}
			else
			{
				V_snprintf( buff, sizeof( buff ), "oC%d", dwRegNum );
			}
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_CONSTINT:
			V_snprintf( buff, sizeof( buff ), "i%d", dwRegNum );	// Loops use these
			strcat_s( pRegisterName, nBufLen, buff );
			m_dwConstIntUsageMask |= 0x00000001 << dwRegNum;		// Keep track of the use of this integer constant
			break;
		case D3DSPR_COLOROUT:
			V_snprintf( buff, sizeof( buff ), "gl_FragData[%d]", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			m_bOutputColorRegister[dwRegNum] = true;
			break;
		case D3DSPR_DEPTHOUT:
			V_snprintf( buff, sizeof( buff ), "gl_FragDepth" );
			strcat_s( pRegisterName, nBufLen, buff );
			m_bOutputDepthRegister = true;
			break;
		case D3DSPR_SAMPLER:
			V_snprintf( buff, sizeof( buff ), "sampler%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_CONST2:
			TranslationError();
			V_snprintf( buff, sizeof( buff ), "c%d", dwRegNum+2048);
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_CONST3:
			TranslationError();
			V_snprintf( buff, sizeof( buff ), "c%d", dwRegNum+4096);
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_CONST4:
			TranslationError();
			V_snprintf( buff, sizeof( buff ), "c%d", dwRegNum+6144);
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_CONSTBOOL:
			V_snprintf( buff, sizeof( buff ), m_bVertexShader ? "b%d" : "fb%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			m_dwConstBoolUsageMask |= 0x00000001 << dwRegNum;		// Keep track of the use of this bool constant
			break;
		case D3DSPR_LOOP:
			TranslationError();
			V_snprintf( buff, sizeof( buff ), "aL%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_TEMPFLOAT16:
			TranslationError();
			V_snprintf( buff, sizeof( buff ), "temp_float16_xxx%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_MISCTYPE:
			Assert( dwRegNum == 0 ); // So far, we know that MISC[0] is gl_FragCoord (aka vPos in DX ASM parlance), but we don't know about any other MISC registers
			V_snprintf( buff, sizeof( buff ), "gl_FragCoord" );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_LABEL:
			TranslationError();
			V_snprintf( buff, sizeof( buff ), "label%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_PREDICATE:
			TranslationError();
			V_snprintf( buff, sizeof( buff ), "p%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
	}

	// If this is a dest register
	if ( dwSourceOrDest == DST_REGISTER )
	{
		//
		// Write masks
		//
		// If some (not all, not none) of the write masks are set, we should include them
		//
		if ( bAllowWriteMask && ( !((dwToken & D3DSP_WRITEMASK_ALL) == D3DSP_WRITEMASK_ALL) || ((dwToken & D3DSP_WRITEMASK_ALL) == 0x00000000) ) )
		{
			// Put the dot on there
			strcat_s( pRegisterName, nBufLen, "." );

			// Optionally put on the x, y, z or w
			int nMasksWritten = 0;
			if ( dwToken & D3DSP_WRITEMASK_0 )
			{
				strcat_s( pRegisterName, nBufLen, "x" );
				++nMasksWritten;
			}
			if ( dwToken & D3DSP_WRITEMASK_1 )
			{
				strcat_s( pRegisterName, nBufLen, "y" );
				++nMasksWritten;
			}
			if ( dwToken & D3DSP_WRITEMASK_2 )
			{
				strcat_s( pRegisterName, nBufLen, "z" );
				++nMasksWritten;
			}
			if ( dwToken & D3DSP_WRITEMASK_3 )
			{
				strcat_s( pRegisterName, nBufLen, "w" );
				++nMasksWritten;
			}
		}
	}
	else // must be a source register
	{
		if ( bAllowSwizzle ) // relative addressing hard-codes the swizzle on a0.x
		{
			uint32 dwXSwizzle, dwYSwizzle, dwZSwizzle, dwWSwizzle;

			// Mask out the swizzle modifier
			dwSwizzle = dwToken & D3DVS_SWIZZLE_MASK;

			// If there are any swizzles at all, tack on the appropriate notation
			if ( dwSwizzle != D3DVS_NOSWIZZLE )
			{
				// Separate out the two-bit codes for each component swizzle
				dwXSwizzle = dwSwizzle & D3DVS_X_W;
				dwYSwizzle = dwSwizzle & D3DVS_Y_W;
				dwZSwizzle = dwSwizzle & D3DVS_Z_W;
				dwWSwizzle = dwSwizzle & D3DVS_W_W;

				// Put on the dot
				strcat_s( pRegisterName, nBufLen, "." );

				// See where X comes from
				switch ( dwXSwizzle )
				{
					case D3DVS_X_X:
						strcat_s( pRegisterName, nBufLen, "x" );
						break;
					case D3DVS_X_Y:
						strcat_s( pRegisterName, nBufLen, "y" );
						break;
					case D3DVS_X_Z:
						strcat_s( pRegisterName, nBufLen, "z" );
						break;
					case D3DVS_X_W:
						strcat_s( pRegisterName, nBufLen, "w" );
						break;
				}

				if ( !bForceScalarSource )
				{
					// If the source of the remaining components are aren't
					// identical to the source of x, continue with swizzle
					if ( ((dwXSwizzle >> D3DVS_SWIZZLE_SHIFT) != (dwYSwizzle >> (D3DVS_SWIZZLE_SHIFT + 2))) ||	// X and Y sources match?
					   ((dwXSwizzle >> D3DVS_SWIZZLE_SHIFT) != (dwZSwizzle >> (D3DVS_SWIZZLE_SHIFT + 4))) ||	// X and Z sources match?
					   ((dwXSwizzle >> D3DVS_SWIZZLE_SHIFT) != (dwWSwizzle >> (D3DVS_SWIZZLE_SHIFT + 6))))	  // X and W sources match?
					{

						// OpenGL seems to want us to have either 1 or 4 components in a swizzle, so just plow on through the rest
						switch ( dwYSwizzle )
						{
							case D3DVS_Y_X:
								strcat_s( pRegisterName, nBufLen, "x" );
								break;
							case D3DVS_Y_Y:
								strcat_s( pRegisterName, nBufLen, "y" );
								break;
							case D3DVS_Y_Z:
								strcat_s( pRegisterName, nBufLen, "z" );
								break;
							case D3DVS_Y_W:
								strcat_s( pRegisterName, nBufLen, "w" );
								break;
						}

						switch ( dwZSwizzle )
						{
							case D3DVS_Z_X:
								strcat_s( pRegisterName, nBufLen, "x" );
								break;
							case D3DVS_Z_Y:
								strcat_s( pRegisterName, nBufLen, "y" );
								break;
							case D3DVS_Z_Z:
								strcat_s( pRegisterName, nBufLen, "z" );
								break;
							case D3DVS_Z_W:
								strcat_s( pRegisterName, nBufLen, "w" );
								break;
						}

						switch ( dwWSwizzle )
						{
							case D3DVS_W_X:
								strcat_s( pRegisterName, nBufLen, "x" );
								break;
							case D3DVS_W_Y:
								strcat_s( pRegisterName, nBufLen, "y" );
								break;
							case D3DVS_W_Z:
								strcat_s( pRegisterName, nBufLen, "z" );
								break;
							case D3DVS_W_W:
								strcat_s( pRegisterName, nBufLen, "w" );
								break;
						}

					}

				} // end !bForceScalarSource 
			}
			else // dwSwizzle == D3DVS_NOSWIZZLE
			{
				// If this is a MOVA / ARL, GL on the Mac requires us to tack the .x onto the source register
				if ( bForceScalarSource )
				{
					strcat_s( pRegisterName, nBufLen, ".x" );
				}
			}
		} // bAllowSwizzle

		// If there are any source modifiers, check to see if they're at
		// least partially "postfix" and tack them on as appropriate
		if ( dwSrcModifier != D3DSPSM_NONE )
		{
			switch ( dwSrcModifier )
			{
				case D3DSPSM_BIAS:							// bias
				case D3DSPSM_BIASNEG:						// bias and negate
					TranslationError();
					strcat_s( pRegisterName, nBufLen, "_bx2" );
					break;
				case D3DSPSM_SIGN:							// sign
				case D3DSPSM_SIGNNEG:						// sign and negate
					TranslationError();
					strcat_s( pRegisterName, nBufLen, "_sgn" );
					break;
				case D3DSPSM_X2:							  // *2
				case D3DSPSM_X2NEG:						  // *2 and negate
					TranslationError();
					strcat_s( pRegisterName, nBufLen, "_x2" );
					break;
				case D3DSPSM_ABS:							 // abs()
				case D3DSPSM_ABSNEG:						 // -abs()
					strcat_s( pRegisterName, nBufLen, ")" );
					break;
				case D3DSPSM_DZ:							  // divide through by z component
					TranslationError();
					strcat_s( pRegisterName, nBufLen, "_dz" );
					break;
				case D3DSPSM_DW:							  // divide through by w component
					TranslationError();
					strcat_s( pRegisterName, nBufLen, "_dw" );
					break;
			}
		} // end postfix modifiers (really only ps.1.x)
	}
}

void D3DToGL::RecordInputAndOutputPositions()
{
	// Remember where we are in the token stream.
	m_pRecordedInputTokenStart = m_pdwNextToken;

	// Remember where our outputs are.
	m_nRecordedParamCodeStrlen = V_strlen( (char*)m_pBufParamCode->Base() );
	m_nRecordedALUCodeStrlen = V_strlen( (char*)m_pBufALUCode->Base() );
	m_nRecordedAttribCodeStrlen = V_strlen( (char*)m_pBufAttribCode->Base() );
}
void D3DToGL::AddTokenHexCodeToBuffer( char *pBuffer, int nSize, int nLastStrlen )
{
	int nCurStrlen = V_strlen( pBuffer );
	if ( nCurStrlen == nLastStrlen )
		return;

	// Build a string with all the hex codes of the tokens since last time.
	char szHex[512];
	szHex[0] = '\n';
	V_snprintf( &szHex[1], sizeof( szHex )-1, HEXCODE_HEADER );
	int nTokens = MIN( 10, m_pdwNextToken - m_pRecordedInputTokenStart );
	for ( int i=0; i < nTokens; i++ )
	{
		char szTemp[32];
		V_snprintf( szTemp, sizeof( szTemp ), "0x%x ", m_pRecordedInputTokenStart[i] );
		V_strncat( szHex, szTemp, sizeof( szHex ) );
	}
	V_strncat( szHex, "\n", sizeof( szHex ) );

	// Insert the hex codes into the string.
	int nBytesToInsert = V_strlen( szHex );
	if ( nCurStrlen + nBytesToInsert + 1 >= nSize )
		Error( "Buffer overflow writing token hex codes" );

	if ( m_bPutHexCodesAfterLines )
	{
		// Put it at the end of the last line.
		if ( pBuffer[nCurStrlen-1] == '\n' )
			pBuffer[nCurStrlen-1] = 0;

		V_strncat( pBuffer, &szHex[1], nSize );
	}
	else
	{
		memmove( pBuffer + nLastStrlen + nBytesToInsert, pBuffer + nLastStrlen, nCurStrlen - nLastStrlen + 1 );
		memcpy( pBuffer + nLastStrlen, szHex, nBytesToInsert );
	}
}

void D3DToGL::AddTokenHexCode()
{
	if ( m_pdwNextToken > m_pRecordedInputTokenStart )
	{
		AddTokenHexCodeToBuffer( (char*)m_pBufParamCode->Base(), m_pBufParamCode->Size(), m_nRecordedParamCodeStrlen );
		AddTokenHexCodeToBuffer( (char*)m_pBufALUCode->Base(), m_pBufALUCode->Size(), m_nRecordedALUCodeStrlen );
		AddTokenHexCodeToBuffer( (char*)m_pBufAttribCode->Base(), m_pBufAttribCode->Size(), m_nRecordedAttribCodeStrlen );
	}
}

uint32 D3DToGL::MaintainAttributeMap( uint32 dwToken, uint32 dwRegToken )
{
	// Check that this reg index has not been used before - if it has, let Houston know
	uint dwRegIndex = dwRegToken & D3DSP_REGNUM_MASK;
	if ( m_dwAttribMap[ dwRegIndex ] == 0xFFFFFFFF )
	{
		// log it
		// semantic/usage in the higher nibble
		// usage index in the low nibble

		uint usage		= dwToken & D3DSP_DCL_USAGE_MASK;
		uint usageindex	= ( dwToken & D3DSP_DCL_USAGEINDEX_MASK ) >> D3DSP_DCL_USAGEINDEX_SHIFT;

		m_dwAttribMap[ dwRegIndex ] = ( usage << 4 ) | usageindex;

		// avoid writing 0xBB since runtime code uses that for an 'unused' marker
		if ( m_dwAttribMap[ dwRegIndex ] == 0xBB )
		{
			Debugger();
		}
	}
	else
	{
		//not OK
		Debugger();
	}

	return dwRegIndex;
}

void D3DToGL::Handle_DCL()
{
	uint32 dwToken = GetNextToken();		// What kind of dcl is this...
	uint32 dwRegToken = GetNextToken();		// Look ahead to register token

	uint32 dwUsage = ( dwToken & D3DSP_DCL_USAGE_MASK );
	uint32 dwUsageIndex = ( dwToken & D3DSP_DCL_USAGEINDEX_MASK ) >> D3DSP_DCL_USAGEINDEX_SHIFT;
		
	uint32 dwRegNum = dwRegToken & D3DSP_REGNUM_MASK;
	uint32 nRegType = GetRegTypeFromToken( dwRegToken );

	if ( m_bVertexShader )
	{
		// If this is an output, remember the index (what the ASM code calls o0, o1, o2..) and the semantic.
		// When GetParameterString( DST_REGISTER ) hits this one, we'll return "oN".
		// At the end of the main() function, we'll insert a bunch of statements like "gl_Color = o2" based on what we remembered here.
		if ( ( m_dwMajorVersion >= 3 ) && ( nRegType == D3DSPR_OUTPUT ) )
		{
//				uint32 dwRegComponents = ( dwRegToken & D3DSP_WRITEMASK_ALL ) >> 16; // Components used by the output register (1 means float, 3 means vec2, 7 means vec3, f means vec4)
				
			if ( dwRegNum >= MAX_DECLARED_OUTPUTS )
				Error( "Output register number (%d) too high (only %d supported).", dwRegNum, MAX_DECLARED_OUTPUTS );

			if ( m_DeclaredOutputs[dwRegNum] != UNDECLARED_OUTPUT )
				Error( "Output dcl_ hit for register #%d more than once!", dwRegNum );

			Assert( dwToken != UNDECLARED_OUTPUT );
			m_DeclaredOutputs[dwRegNum] = dwToken;

			//uint32 dwUsage = ( dwToken & D3DSP_DCL_USAGE_MASK );
			//uint32 dwUsageIndex = ( dwToken & D3DSP_DCL_USAGEINDEX_MASK ) >> D3DSP_DCL_USAGEINDEX_SHIFT;

			// Flag which o# output register maps to gl_Position
			if ( dwUsage == D3DDECLUSAGE_POSITION )
			{
				m_nVSPositionOutput = dwUsageIndex;
				m_bDeclareVSOPos = true;
			}

			if ( m_bAddHexCodeComments )
			{
				CUtlString sParam2 = GetUsageAndIndexString( dwToken, SEMANTIC_OUTPUT );
				PrintToBuf( *m_pBufHeaderCode, "// [GL remembering that oT%d maps to %s]\n", dwRegNum, sParam2.String() );
			}

		}
		else if ( GetRegType( dwRegToken ) == D3DSPR_SAMPLER )
		{
			// We can support vertex texturing if necessary, but I can't find a use case in any branch. (HW morphing in L4D2 isn't enabled, and the comments indicate that r_hwmorph isn't compatible with mat_queue_mode anyway, and CS:GO/DoTA don't use vertex shader texturing.)
			TranslationError();

			int nRegNum = dwRegToken & D3DSP_REGNUM_MASK;
			switch ( TextureType( dwToken ) )
			{
			default:
			case D3DSTT_UNKNOWN:
			case D3DSTT_2D:
				m_dwSamplerTypes[nRegNum] = SAMPLER_TYPE_2D;
				break;
			case D3DSTT_CUBE:
				m_dwSamplerTypes[nRegNum] = SAMPLER_TYPE_CUBE;
				break;
			case D3DSTT_VOLUME:
				m_dwSamplerTypes[nRegNum] = SAMPLER_TYPE_3D;
				break;
			}

			// Track sampler declarations
			m_dwSamplerUsageMask |= 1 << nRegNum;
		}
		else
		{
			Assert( GetRegType( dwRegToken ) == D3DSPR_INPUT);

			CUtlString sParam1 = GetParameterString( dwRegToken, DST_REGISTER, false, NULL );
			CUtlString sParam2 = GetUsageAndIndexString( dwToken, SEMANTIC_INPUT );

			sParam2 = FixGLSLSwizzle( sParam1, sParam2 );
			PrintToBuf( *m_pBufHeaderCode, "attribute vec4 %s; // ", sParam1.String() );

			MaintainAttributeMap( dwToken, dwRegToken );

			char temp[128];
			// regnum goes straight into the vertex.attrib[n] index
			sprintf( temp, "%08x %08x\n", dwToken, dwRegToken );
			StrcatToHeaderCode( temp );
		}
	}
	else // Pixel shader
	{
		// If the register is a sampler, the dcl has a dimension decorator that we have to save for subsequent TEX instructions
		uint32 nRegType = GetRegType( dwRegToken );
		if ( nRegType == D3DSPR_SAMPLER )
		{
			int nRegNum = dwRegToken & D3DSP_REGNUM_MASK;
			switch ( TextureType( dwToken ) )
			{
				default:
				case D3DSTT_UNKNOWN:
				case D3DSTT_2D:
					m_dwSamplerTypes[nRegNum] = SAMPLER_TYPE_2D;
					break;
				case D3DSTT_CUBE:
					m_dwSamplerTypes[nRegNum] = SAMPLER_TYPE_CUBE;
					break;
				case D3DSTT_VOLUME:
					m_dwSamplerTypes[nRegNum] = SAMPLER_TYPE_3D;
					break;
			}
			
			// Track sampler declarations
			m_dwSamplerUsageMask |= 1 << nRegNum;
		}
		else // Not a sampler, we're going to generate varying declaration code
		{
			// In pixel shaders we only declare texture coordinate varyings since they may be using centroid
			if ( ( m_dwMajorVersion == 3 ) && ( nRegType == D3DSPR_INPUT ) )
			{
				Assert( m_DeclaredInputs[dwRegNum] == UNDECLARED_INPUT );
				m_DeclaredInputs[dwRegNum] = dwToken;
								
				if ( ( dwUsage != D3DDECLUSAGE_COLOR ) && ( dwUsage != D3DDECLUSAGE_TEXCOORD ) )
				{
					TranslationError(); // Not supported yet, but can be if we need it.
				}

				if ( dwUsage == D3DDECLUSAGE_TEXCOORD )
				{
					char buf[256];
					if ( m_nCentroidMask & ( 0x00000001 << dwUsageIndex ) )
					{
						V_snprintf( buf, sizeof( buf ), "centroid varying vec4 oT%d;\n", dwUsageIndex ); // centroid varying
					}
					else
					{
						V_snprintf( buf, sizeof( buf ), "varying vec4 oT%d;\n", dwUsageIndex );
					}
					StrcatToHeaderCode( buf );
				}
			}
			else if ( nRegType == D3DSPR_TEXTURE )
			{
				char buff[256];
				PrintParameterToString( dwRegToken, DST_REGISTER, buff, sizeof( buff ), false, NULL );
				PrintToBuf( *m_pBufHeaderCode, "%s;\n",buff );
			}
			else
			{
				// No need to declare anything (probably D3DSPR_MISCTYPE either VPOS or VFACE)
			}
		}
	}
}

static bool IsFloatNaN( float f )
{
	const uint nBits = *reinterpret_cast<uint*>(&f);
	const uint nExponent = ( nBits >> 23 ) & 0xFF;

	return ( nExponent == 255 );
}

static inline bool EqualTol( double a, double b, double t )
{
	return fabs( a - b ) <= ( ( MAX( fabs( a ), fabs( b ) ) + 1.0 ) * t );
}

// Originally written by Bruce Dawson, see:
// See http://randomascii.wordpress.com/2012/03/08/float-precisionfrom-zero-to-100-digits-2/
// This class represents a very limited high-precision number with 'count' 32-bit
// unsigned elements.
template <int count>
struct HighPrec
{
	typedef unsigned T;
	typedef unsigned long long Product_t;
	static const int kWordShift = 32;
	HighPrec()
	{
		memset(m_data, 0, sizeof(m_data));
		m_nLowestNonZeroIndex = ARRAYSIZE(m_data);
	}

	// Insert the bits from value into m_data, shifted in from the bottom (least
	// significant end) by the specified number of bits. A shift of zero or less
	// means that none of the bits will be shifted in. A shift of one means that
	// the high bit of value will be in the bottom of the last element of m_data -
	// the least significant bit. A shift of kWordShift means that value will be
	// in the least significant element of m_data, and so on.
	void InsertLowBits(T value, int shiftAmount)
	{
		if (shiftAmount <= 0)
			return;

		int subShift = shiftAmount & (kWordShift - 1);
		int bigShift = shiftAmount / kWordShift;
		Product_t result = (Product_t)value << subShift;
		T resultLow = (T)result;
		T resultHigh = result >> kWordShift;

		// Use an unsigned type so that negative numbers will become large,
		// which makes the range checking below simpler.
		unsigned highIndex = ARRAYSIZE(m_data) - 1 - bigShift;
		// Write the results to the data array. If the index is too large
		// then that means that the data was shifted off the edge.
		if ( (highIndex < ARRAYSIZE(m_data)) && ( resultHigh ) )
		{
			m_data[highIndex] |= resultHigh;
			m_nLowestNonZeroIndex = MIN( m_nLowestNonZeroIndex, highIndex );
		}

		if ( ( highIndex + 1 < ARRAYSIZE(m_data)) && ( resultLow ) )
		{
			m_data[highIndex + 1] |= resultLow;
			m_nLowestNonZeroIndex = MIN( m_nLowestNonZeroIndex, highIndex + 1 );
		}
	}

	// Insert the bits from value into m_data, shifted in from the top (most
	// significant end) by the specified number of bits. A shift of zero or less
	// means that none of the bits will be shifted in. A shift of one means that
	// the low bit of value will be in the top of the first element of m_data -
	// the most significant bit. A shift of kWordShift means that value will be
	// in the most significant element of m_data, and so on.
	void InsertTopBits(T value, int shiftAmount)
	{
		InsertLowBits(value, (ARRAYSIZE(m_data) + 1) * kWordShift - shiftAmount);
	}

	// Return true if all elements of m_data are zero.
	bool IsZero() const
	{
		bool bIsZero = ( m_nLowestNonZeroIndex == ARRAYSIZE(m_data) );

#ifdef DEBUG
		for (int i = 0; i < ARRAYSIZE(m_data); ++i)
		{
			if (m_data[i])
			{
				Assert( !bIsZero );
				return false;
			}
		}
		Assert( bIsZero );
#endif

		return bIsZero;
	}

	// Divide by div and return the remainder, from 0 to div-1.
	// Standard long-division algorithm.
	T DivReturnRemainder(T divisor)
	{
		T remainder = 0;

#ifdef DEBUG
		for (uint j = 0; j < m_nLowestNonZeroIndex; ++j)
		{
			Assert( m_data[j] == 0 );
		}
#endif
		
		int nNewLowestNonZeroIndex = ARRAYSIZE(m_data);
		for (int i = m_nLowestNonZeroIndex; i < ARRAYSIZE(m_data); ++i)
		{
			Product_t dividend = ((Product_t)remainder << kWordShift) + m_data[i];
			Product_t result = dividend / divisor;
			remainder = T(dividend % divisor);

			m_data[i] = T(result);

			if ( ( result ) && ( nNewLowestNonZeroIndex == ARRAYSIZE(m_data) ) )
				nNewLowestNonZeroIndex = i;
		}
		m_nLowestNonZeroIndex = nNewLowestNonZeroIndex;

		return remainder;
	}

	// The individual 'digits' (32-bit unsigned integers actually) that
	// make up the number. The most-significant digit is in m_data[0].
	T m_data[count];
	
	uint m_nLowestNonZeroIndex;
};

union Double_t
{
	Double_t(double num = 0.0f) : f(num) {}
	// Portable extraction of components.
	bool Negative() const { return (i >> 63) != 0; }
	int64_t RawMantissa() const { return i & ((1LL << 52) - 1); }
	int64_t RawExponent() const { return (i >> 52) & 0x7FF; }

	int64_t i;
	double f;
};

static uint PrintDoubleInt( char *pBuf, uint nBufSize, double f, uint nMinChars )
{
	static const char *pDigits = "00010203040506070809101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899";

	Assert( !nMinChars || ( ( nMinChars % 6 ) == 0 ) );

	char *pLastChar = pBuf + nBufSize - 1;
	char *pDst = pLastChar;
	*pDst-- = '\0';

	// Put the double in our magic union so we can grab the components.
	union Double_t num(f);

	// Get the character that represents the sign.
	// Check for NaNs or infinity.
	if (num.RawExponent() == 2047)
	{
		TranslationError();
	}

	// Adjust for the exponent bias.
	int exponentValue = int(num.RawExponent() - 1023);
	// Add the implied one to the mantissa.
	uint64_t mantissaValue = (1ll << 52) + num.RawMantissa();
	// Special-case for denormals - no special exponent value and
	// no implied one.
	if (num.RawExponent() == 0)
	{
		exponentValue = -1022;
		mantissaValue = num.RawMantissa();
	}
	uint32_t mantissaHigh = mantissaValue >> 32;
	uint32_t mantissaLow = mantissaValue & 0xFFFFFFFF;

	// The first bit of the mantissa has an implied value of one and this can
	// be shifted 1023 positions to the left, so that's 1024 bits to the left
	// of the binary point, or 32 32-bit words for the integer part.
	HighPrec<32> intPart;
	// When our exponentValue is zero (a number in the 1.0 to 2.0 range)
	// we have a 53-bit mantissa and the implied value of the highest bit
	// is 1. We need to shift 12 bits in from the bottom to get that 53rd bit
	// into the ones spot in the integral portion.
	// To complicate it a bit more we have to insert the mantissa as two parts.
	intPart.InsertLowBits(mantissaHigh, 12 + exponentValue);
	intPart.InsertLowBits(mantissaLow, 12 + exponentValue - 32);

	bool bAnyDigitsLeft;
	do
	{
		uint remainder = intPart.DivReturnRemainder( 1000000 ); // 10^6
		uint origRemainer = remainder; (void)origRemainer;

		bAnyDigitsLeft = !intPart.IsZero();

		if ( bAnyDigitsLeft )
		{
			uint n = remainder % 100U; remainder /= 100U; *reinterpret_cast<uint16*>(pDst - 1) = reinterpret_cast<const uint16*>(pDigits)[n]; 
			n = remainder % 100U; remainder /= 100U; *reinterpret_cast<uint16*>(pDst - 1 - 2) = reinterpret_cast<const uint16*>(pDigits)[n]; 
			Assert( remainder < 100U );
			*reinterpret_cast<uint16*>(pDst - 1 - 4) = reinterpret_cast<const uint16*>(pDigits)[remainder]; 
			pDst -= 6;
		}
		else
		{
			uint n = remainder % 100U; remainder /= 100U; *reinterpret_cast<uint16*>(pDst - 1) = reinterpret_cast<const uint16*>(pDigits)[n]; --pDst; if ( ( n >= 10 ) || ( remainder ) ) --pDst;
			if ( remainder )
			{
				n = remainder % 100U; remainder /= 100U; *reinterpret_cast<uint16*>(pDst - 1) = reinterpret_cast<const uint16*>(pDigits)[n]; --pDst; if ( ( n >= 10 ) || ( remainder ) ) --pDst;

				if ( remainder )
				{
					Assert( remainder < 100U );
					*reinterpret_cast<uint16*>(pDst - 1) = reinterpret_cast<const uint16*>(pDigits)[remainder]; --pDst; if ( remainder >= 10 ) --pDst;
				}
			}
		}

	} while ( bAnyDigitsLeft );

	uint l = pLastChar - pDst;
	
	while ( ( l - 1 ) < nMinChars )
	{
		*pDst-- = '0';
		l++;
	}
	
	Assert( (int)l == ( pLastChar - pDst ) );

	Assert( l <= nBufSize );
			
	memmove( pBuf, pDst + 1, l );
	return l - 1;
}

// FloatToString is equivalent to sprintf( "%.12f" ), but doesn't have any dependencies on the current locale setting.
// Unfortunately, high accuracy radix conversion is actually pretty tricky to do right. 
// Most importantly, this function has the same max roundtrip (IEEE->ASCII->IEEE) error as the MS CRT functions and can reliably handle extremely large inputs.
static void FloatToString( char *pBuf, uint nBufSize, double fConst )
{
	char *pEnd = pBuf + nBufSize;
	char *pDst = pBuf;

	double flVal = fConst;
	if ( IsFloatNaN( flVal ) )
	{
		flVal = 0;
	}

	if ( flVal < 0.0f )
	{
		*pDst++ = '-';
		flVal = -flVal;
	}

	double flInt;
	double flFract = modf( flVal, &flInt );

	flFract = floor( flFract * 1000000000000.0 + .5 );

	if ( !flInt )
	{
		*pDst++ = '0';
	}
	else
	{
		uint l = PrintDoubleInt( pDst, pEnd - pDst, flInt, 0 );
		pDst += l;
	}

	*pDst++ = '.';
	if ( !flFract )
	{
		*pDst++ = '0';
		*pDst++ = '\0';
	}
	else
	{
		uint l = PrintDoubleInt( pDst, pEnd - pDst, flFract, 12 );
		pDst += l;
					
		StripExtraTrailingZeros( pBuf );	// Turn 1.00000 into 1.0
	}
}

#if 0
#include "vstdlib/random.h"
static void TestFloatConversion()
{
	for ( ; ; )
	{
		double fConst;
		switch ( rand() % 4 )
		{
		case 0:
			fConst = RandomFloat( -1e-30, 1e+30 ); break;
		case 1:
			fConst = RandomFloat( -1e-10, 1e+10 ); break;
		case 2: 
			fConst = RandomFloat( -1e-5, 1e+5 ); break;
		default:
			fConst = RandomFloat( -1, 1 ); break;
		}

		char szTemp[1024];

		// FloatToString does not rely on V_snprintf(), so it can't be affected by the current locale setting.
		FloatToString( szTemp, sizeof( szTemp ), fConst );

		static double flMaxErr1;
		static double flMaxErr2;

		// Compare FloatToString()'s results vs. V_snprintf()'s, also track maximum error of each.
		double flCheck = atof( szTemp );
		double flErr = fabs( flCheck - fConst );
		flMaxErr1 = MAX( flMaxErr1, flErr );
		Assert( EqualTol( flCheck, fConst, .000000125 ) );

		char szTemp2[256];
		V_snprintf( szTemp2, sizeof( szTemp2 ), "%.12f", fConst );
		StripExtraTrailingZeros( szTemp2 );

		if ( !strchr( szTemp2, '.' ) )
		{
			V_strncat( szTemp2, ".0", sizeof( szTemp2 ) );
		}
		double flCheck2 = atof( szTemp2 );
		double flErr2 = fabs( flCheck2 - fConst );
		flMaxErr2 = MAX( flMaxErr2, flErr2 );
		Assert( EqualTol( flCheck2, fConst, .000000125 ) );

		if ( flMaxErr1 > flMaxErr2 )
		{
			GLMDebugPrintf( "!\n" );
		}
	}
}
#endif

void D3DToGL::Handle_DEFIB( uint32 instruction )
{
	Assert( ( instruction == D3DSIO_DEFI ) || ( instruction == D3DSIO_DEFB ) );

	// which register is being defined
	uint32 dwToken = GetNextToken();

	uint32 nRegNum = dwToken & D3DSP_REGNUM_MASK;

	uint32 regType = GetRegTypeFromToken( dwToken );


	if ( regType == D3DSPR_CONSTINT )
	{
		m_dwDefConstIntUsageMask |= ( 1 << nRegNum );

		uint x = GetNextToken();
		uint y = GetNextToken();
		uint z = GetNextToken();
		uint w = GetNextToken();
		NOTE_UNUSED(y); NOTE_UNUSED(z);	NOTE_UNUSED(w);

		Assert( nRegNum < 32 );
		if ( nRegNum < 32 )
		{
			m_dwDefConstIntIterCount[nRegNum] = x;
		}
	}
	else
	{
		TranslationError();
	}

}

void D3DToGL::Handle_DEF()
{
	//TestFloatConversion();

	//
	// JasonM TODO: catch D3D's sincos-specific D3DSINCOSCONST1 and D3DSINCOSCONST2 constants and filter them out here
	//

	// Which register is being defined
	uint32 dwToken = GetNextToken();

	// Note that this constant was explicitly defined
	m_bConstantRegisterDefined[dwToken & D3DSP_REGNUM_MASK] = true;
	CUtlString sParamName = GetParameterString( dwToken, DST_REGISTER, false, NULL );

	PrintIndentation( (char*)m_pBufParamCode->Base(), m_pBufParamCode->Size() );
	PrintToBuf( *m_pBufParamCode, "vec4 %s = vec4( ", sParamName.String() );

	// Run through the 4 floats
	for ( int i=0; i < 4; i++ )
	{
		float fConst = uint32ToFloat( GetNextToken() );

		char szTemp[1024];

		FloatToString( szTemp, sizeof( szTemp ), fConst );

#if 0
		static double flMaxErr1;
		static double flMaxErr2;

		// Compare FloatToString()'s results vs. V_snprintf()'s, also track maximum error of each.
		double flCheck = atof( szTemp );
		double flErr = fabs( flCheck - fConst );
		flMaxErr1 = MAX( flMaxErr1, flErr );
		Assert( EqualTol( flCheck, fConst, .000000125 ) );

		char szTemp2[256];
		V_snprintf( szTemp2, sizeof( szTemp2 ), "%.12f", fConst );
		StripExtraTrailingZeros( szTemp2 );

		if ( !strchr( szTemp2, '.' ) )
		{
			V_strncat( szTemp2, ".0", sizeof( szTemp2 ) );
		}
		double flCheck2 = atof( szTemp2 );
		double flErr2 = fabs( flCheck2 - fConst );
		flMaxErr2 = MAX( flMaxErr2, flErr2 );
		Assert( EqualTol( flCheck2, fConst, .000000125 ) );

		if ( flMaxErr1 > flMaxErr2 )
		{
			GLMDebugPrintf( "!\n" );
		}
#endif

		PrintToBuf( *m_pBufParamCode, i != 3 ? "%s, " : "%s", szTemp ); // end with comma-space
	}

	PrintToBuf( *m_pBufParamCode, " );\n" );
}

void D3DToGL::Handle_MAD( uint32 nInstruction )
{
	uint32 nDestToken = GetNextToken();
	CUtlString sParam1 = GetParameterString( nDestToken, DST_REGISTER, false, NULL );
	int nARLComp0 = ARL_DEST_NONE;
	CUtlString sParam2 = GetParameterString( GetNextToken(), SRC_REGISTER, false, &nARLComp0 );
	int nARLComp1 = ARL_DEST_NONE;
	CUtlString sParam3 = GetParameterString( GetNextToken(), SRC_REGISTER, false, &nARLComp1 );
	int nARLComp2 = ARL_DEST_NONE;
	CUtlString sParam4 = GetParameterString( GetNextToken(), SRC_REGISTER, false, &nARLComp2 );

	// This optionally inserts a move from our dummy address register to the .x component of the real one
	InsertMoveFromAddressRegister( m_pBufALUCode, nARLComp0, nARLComp1, nARLComp2 );

	sParam2 = FixGLSLSwizzle( sParam1, sParam2 );
	sParam3 = FixGLSLSwizzle( sParam1, sParam3 );
	sParam4 = FixGLSLSwizzle( sParam1, sParam4 );
	PrintToBufWithIndents( *m_pBufALUCode, "%s = %s * %s + %s;\n", sParam1.String(), sParam2.String(), sParam3.String(), sParam4.String() );
		
	// If the _SAT instruction modifier is used, then do a saturate here.
	if ( nDestToken & D3DSPDM_SATURATE )
	{
		int nComponents = GetNumSwizzleComponents( sParam1.String() );
		if ( nComponents == 0 )
			nComponents = 4;
			
		PrintToBufWithIndents( *m_pBufALUCode, "%s = clamp( %s, %s, %s );\n", sParam1.String(), sParam1.String(), g_szVecZeros[nComponents], g_szVecOnes[nComponents] );
	}
}


void D3DToGL::Handle_DP2ADD()
{
	char pDestReg[64], pSrc0Reg[64], pSrc1Reg[64], pSrc2Reg[64];
	uint32 nDestToken = GetNextToken();
	PrintParameterToString( nDestToken, DST_REGISTER, pDestReg, sizeof( pDestReg ), false, NULL );
	PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false, NULL );
	PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc1Reg, sizeof( pSrc1Reg ), false, NULL );
	PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc2Reg, sizeof( pSrc2Reg ), false, NULL );

	// We should only be assigning to a single component of the dest.
	Assert( GetNumSwizzleComponents( pDestReg ) == 1 );
	Assert( GetNumSwizzleComponents( pSrc2Reg ) == 1 );

	// This is a 2D dot product, so we only want two entries from the middle components.
	CUtlString sArg0 = EnsureNumSwizzleComponents( pSrc0Reg, 2 );
	CUtlString sArg1 = EnsureNumSwizzleComponents( pSrc1Reg, 2 );

	PrintToBufWithIndents( *m_pBufALUCode, "%s = dot( %s, %s ) + %s;\n", pDestReg, sArg0.String(), sArg1.String(), pSrc2Reg );
		
	// If the _SAT instruction modifier is used, then do a saturate here.
	if ( nDestToken & D3DSPDM_SATURATE )
	{
		int nComponents = GetNumSwizzleComponents( pDestReg );
		if ( nComponents == 0 )
			nComponents = 4;
			
		PrintToBufWithIndents( *m_pBufALUCode, "%s = clamp( %s, %s, %s );\n", pDestReg, pDestReg, g_szVecZeros[nComponents], g_szVecOnes[nComponents] );
	}
}


void D3DToGL::Handle_SINCOS()
{
	char pDestReg[64], pSrc0Reg[64];
	PrintParameterToString( GetNextToken(), DST_REGISTER, pDestReg, sizeof( pDestReg ), false, NULL );
	PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), true, NULL );
	m_bNeedsSinCosDeclarations = true;

	
	CUtlString sDest( pDestReg );
	CUtlString sArg0 = EnsureNumSwizzleComponents( pSrc0Reg, 1 );// Ensure input is scalar
	CUtlString sResult( "vSinCosTmp.xy" );			// Always going to populate this
	sResult = FixGLSLSwizzle( sDest, sResult );		// Make sure we match the desired output reg
			
	PrintToBufWithIndents( *m_pBufALUCode, "vSinCosTmp.z = %s * %s;\n", sArg0.String(), sArg0.String() );
		
	PrintToBufWithIndents( *m_pBufALUCode, "vSinCosTmp.xy = vSinCosTmp.zz * scA.xy + scA.wz;\n" );
	PrintToBufWithIndents( *m_pBufALUCode, "vSinCosTmp.xy = vSinCosTmp.xy * vSinCosTmp.zz + scB.xy;\n" );
	PrintToBufWithIndents( *m_pBufALUCode, "vSinCosTmp.xy = vSinCosTmp.xy * vSinCosTmp.zz + scB.wz;\n" );

	PrintToBufWithIndents( *m_pBufALUCode, "vSinCosTmp.x = vSinCosTmp.x * %s;\n", sArg0.String() );
		
	PrintToBufWithIndents( *m_pBufALUCode, "vSinCosTmp.xy = vSinCosTmp.xy * vSinCosTmp.xx;\n" );
	PrintToBufWithIndents( *m_pBufALUCode, "vSinCosTmp.xy = vSinCosTmp.xy + vSinCosTmp.xy;\n" );
	PrintToBufWithIndents( *m_pBufALUCode, "vSinCosTmp.x = -vSinCosTmp.x + scB.z;\n" );
		
	PrintToBufWithIndents( *m_pBufALUCode, "%s = %s;\n", sDest.String(), sResult.String() );
	
	if ( m_dwMajorVersion < 3 )
	{
		// Eat two more tokens since D3D defines Taylor series constants that we won't need
		// Only valid for pixel and vertex shader version earlier than 3_0
		// (http://msdn.microsoft.com/en-us/library/windows/hardware/ff569710(v=vs.85).aspx)
		SkipTokens( 2 );
	}
}


void D3DToGL::Handle_LRP( uint32 nInstruction )
{
	uint32 nDestToken = GetNextToken();
	CUtlString sDest = GetParameterString( nDestToken, DST_REGISTER, false, NULL );
	int nARLComp0 = ARL_DEST_NONE;
	CUtlString sParam0 = GetParameterString( GetNextToken(), SRC_REGISTER, false, &nARLComp0 );
	int nARLComp1 = ARL_DEST_NONE;
	CUtlString sParam1 = GetParameterString( GetNextToken(), SRC_REGISTER, false, &nARLComp1 );
	int nARLComp2 = ARL_DEST_NONE;
	CUtlString sParam2 = GetParameterString( GetNextToken(), SRC_REGISTER, false, &nARLComp2 );
		
	// This optionally inserts a move from our dummy address register to the .x component of the real one
	InsertMoveFromAddressRegister( m_pBufALUCode, nARLComp0, nARLComp1, nARLComp2 );
		
	sParam0 = FixGLSLSwizzle( sDest, sParam0 );
	sParam1 = FixGLSLSwizzle( sDest, sParam1 );
	sParam2 = FixGLSLSwizzle( sDest, sParam2 );

	// dest = src0 * (src1 - src2) + src2;
	PrintToBufWithIndents( *m_pBufALUCode, "%s = %s * ( %s - %s ) + %s;\n", sDest.String(), sParam0.String(), sParam1.String(), sParam2.String(), sParam2.String() );

	// If the _SAT instruction modifier is used, then do a saturate here.
	if ( nDestToken & D3DSPDM_SATURATE )
	{
		int nComponents = GetNumSwizzleComponents( sDest.String() );
		if ( nComponents == 0 )
			nComponents = 4;
			
		PrintToBufWithIndents( *m_pBufALUCode, "%s = clamp( %s, %s, %s );\n", sDest.String(), sDest.String(), g_szVecZeros[nComponents], g_szVecOnes[nComponents] );
	}
}


void D3DToGL::Handle_TEX( uint32 dwToken, bool bIsTexLDL )
{
	char pDestReg[64], pSrc0Reg[64], pSrc1Reg[64];
	PrintParameterToString( GetNextToken(), DST_REGISTER, pDestReg, sizeof( pDestReg ), false, NULL );
	PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false, NULL );
	
	DWORD dwSrc1Token = GetNextToken();
	PrintParameterToString( dwSrc1Token, SRC_REGISTER, pSrc1Reg, sizeof( pSrc1Reg ), false, NULL );
	
	Assert( (dwSrc1Token & D3DSP_REGNUM_MASK) < ARRAYSIZE( m_dwSamplerTypes ) );
	uint32 nSamplerType = m_dwSamplerTypes[dwSrc1Token & D3DSP_REGNUM_MASK];
	if ( nSamplerType == SAMPLER_TYPE_2D )
	{
		const bool bIsShadowSampler = ( ( 1 << ( (int) ( dwSrc1Token & D3DSP_REGNUM_MASK ) ) ) & m_nShadowDepthSamplerMask ) != 0;
			
		if ( bIsTexLDL )
		{
			CUtlString sCoordVar = EnsureNumSwizzleComponents( pSrc0Reg, bIsShadowSampler ? 3 : 2 );

			// Strip out the W component of the pSrc0Reg and pass that as the LOD to texture2DLod.
			char szLOD[128], szExtra[8];
			GetParamNameWithoutSwizzle( pSrc0Reg, szLOD, sizeof( szLOD ) );
			V_snprintf( szExtra, sizeof( szExtra ), ".%c", GetSwizzleComponent( pSrc0Reg, 3 ) );
			V_strncat( szLOD, szExtra, sizeof( szLOD ) );

			PrintToBufWithIndents( *m_pBufALUCode, "%s = %s( %s, %s, %s );\n", pDestReg, bIsShadowSampler ? "shadow2DLod" : "texture2DLod", pSrc1Reg, sCoordVar.String(), szLOD );
		}
		else if ( bIsShadowSampler )
		{
			// .z is meant to contain the object depth, while .xy contains the 2D tex coords
			CUtlString sCoordVar3D = EnsureNumSwizzleComponents( pSrc0Reg, 3 );

			PrintToBufWithIndents( *m_pBufALUCode, "%s = shadow2D( %s, %s );\n", pDestReg, pSrc1Reg, sCoordVar3D.String() );
			Assert( m_dwSamplerTypes[dwSrc1Token & D3DSP_REGNUM_MASK] == SAMPLER_TYPE_2D );
		}
		else if( ( OpcodeSpecificData( dwToken ) << D3DSP_OPCODESPECIFICCONTROL_SHIFT ) == D3DSI_TEXLD_PROJECT )
		{
			// This projective case is after the shadow case intentionally, due to the way that "projective"
			// loads are overloaded in our D3D shaders for shadow lookups.
			//
			// We use the vec4 variant of texture2DProj() intentionally here, since it lines up well with Direct3D.

			CUtlString s4DProjCoords = EnsureNumSwizzleComponents( pSrc0Reg, 4 ); // Ensure vec4 variant
			PrintToBufWithIndents( *m_pBufALUCode, "%s = texture2DProj( %s, %s );\n", pDestReg, pSrc1Reg, s4DProjCoords.String() );
		}
		else				
		{
			CUtlString sCoordVar = EnsureNumSwizzleComponents( pSrc0Reg, bIsShadowSampler ? 3 : 2 );
			PrintToBufWithIndents( *m_pBufALUCode, "%s = texture2D( %s, %s );\n", pDestReg, pSrc1Reg, sCoordVar.String() );
		}
	}
	else if ( nSamplerType == SAMPLER_TYPE_3D )
	{
		if ( bIsTexLDL )
		{
			TranslationError();
		}

		CUtlString sCoordVar = EnsureNumSwizzleComponents( pSrc0Reg, 3 );
		PrintToBufWithIndents( *m_pBufALUCode, "%s = texture3D( %s, %s );\n", pDestReg, pSrc1Reg, sCoordVar.String() );
	}
	else if ( nSamplerType == SAMPLER_TYPE_CUBE )
	{
		if ( bIsTexLDL )
		{
			TranslationError();
		}

		CUtlString sCoordVar = EnsureNumSwizzleComponents( pSrc0Reg, 3 );
		PrintToBufWithIndents( *m_pBufALUCode, "%s = textureCube( %s, %s );\n", pDestReg, pSrc1Reg, sCoordVar.String() );
	}
	else
	{
		Error( "TEX instruction: unsupported sampler type used" );
	}
}

void D3DToGL::StrcatToHeaderCode( const char *pBuf )
{
	strcat_s( (char*)m_pBufHeaderCode->Base(), m_pBufHeaderCode->Size(), pBuf );
}

void D3DToGL::StrcatToALUCode( const char *pBuf )
{
	PrintIndentation( (char*)m_pBufALUCode->Base(), m_pBufALUCode->Size() );

	strcat_s( (char*)m_pBufALUCode->Base(), m_pBufALUCode->Size(), pBuf );
}

void D3DToGL::StrcatToParamCode( const char *pBuf )
{
	strcat_s( (char*)m_pBufParamCode->Base(), m_pBufParamCode->Size(), pBuf );
}

void D3DToGL::StrcatToAttribCode( const char *pBuf )
{
	strcat_s( (char*)m_pBufAttribCode->Base(), m_pBufAttribCode->Size(), pBuf );
}

void D3DToGL::Handle_TexLDD( uint32 nInstruction )
{
	TranslationError(); // Not supported yet, but can be if we need it.
}


void D3DToGL::Handle_TexCoord()
{
	TranslationError();

	// If ps_1_4, this is texcrd
	if ( (m_dwMajorVersion == 1) && (m_dwMinorVersion == 4) && (!m_bVertexShader) )
	{
		StrcatToALUCode( "texcrd" );
	}
	else // else it's texcoord
	{
		TranslationError();
		StrcatToALUCode( "texcoord" );
	}

	char buff[256];
	PrintParameterToString( GetNextToken(), DST_REGISTER, buff, sizeof( buff ), false, NULL );
	StrcatToALUCode( buff );

	// If ps_1_4, texcrd also has a source parameter
	if ((m_dwMajorVersion == 1) && (m_dwMinorVersion == 4) && (!m_bVertexShader))
	{
		StrcatToALUCode( ", " );
		PrintParameterToString( GetNextToken(), SRC_REGISTER, buff, sizeof( buff ), false, NULL );
		StrcatToALUCode( buff );
	}

	StrcatToALUCode( ";\n" );
}

void D3DToGL::Handle_BREAKC( uint32 dwToken )
{
	uint nComparison = ( dwToken & D3DSHADER_COMPARISON_MASK ) >> D3DSHADER_COMPARISON_SHIFT;

	const char *pComparison = "?";
	switch ( nComparison )
	{
	case D3DSPC_GT: pComparison = ">"; break;
	case D3DSPC_EQ: pComparison = "=="; break;
	case D3DSPC_GE: pComparison = ">="; break;
	case D3DSPC_LT: pComparison = "<"; break;
	case D3DSPC_NE: pComparison = "!="; break;
	case D3DSPC_LE: pComparison = "<="; break;
	default:
		TranslationError();
	}

	char src0[256];
	uint32 src0Token = GetNextToken(); 
	PrintParameterToString( src0Token, SRC_REGISTER, src0, sizeof( src0 ), false, NULL );

	char src1[256];
	uint32 src1Token = GetNextToken(); 
	PrintParameterToString( src1Token, SRC_REGISTER, src1, sizeof( src1 ), false, NULL );

	PrintToBufWithIndents( *m_pBufALUCode, "if (%s %s %s) break;\n", src0, pComparison, src1 );
}

void D3DToGL::HandleBinaryOp_GLSL( uint32 nInstruction )
{
	uint32 nDestToken = GetNextToken();
	CUtlString sParam1 = GetParameterString( nDestToken, DST_REGISTER, false, NULL );
	int nARLComp0 = ARL_DEST_NONE;
	CUtlString sParam2 = GetParameterString( GetNextToken(), SRC_REGISTER, false, &nARLComp0 );
	int nARLComp1 = ARL_DEST_NONE;
	CUtlString sParam3 = GetParameterString( GetNextToken(), SRC_REGISTER, false, &nARLComp1 );

	// This optionally inserts a move from our dummy address register to the .x component of the real one
	InsertMoveFromAddressRegister( m_pBufALUCode, nARLComp0, nARLComp1 );

	// Since DP3 and DP4 have a scalar as the dest and vectors as the src, don't screw with the swizzle specifications.
	if ( nInstruction == D3DSIO_DP3 )
	{
		sParam2 = EnsureNumSwizzleComponents( sParam2, 3 );
		sParam3 = EnsureNumSwizzleComponents( sParam3, 3 );
	}
	else if ( nInstruction == D3DSIO_DP4 )
	{
		sParam2 = EnsureNumSwizzleComponents( sParam2, 4 );
		sParam3 = EnsureNumSwizzleComponents( sParam3, 4 );
	}
	else if ( nInstruction == D3DSIO_DST )
	{
		m_bUsesDSTInstruction = true;
		sParam2 = EnsureNumSwizzleComponents( sParam2, 4 );
		sParam3 = EnsureNumSwizzleComponents( sParam3, 4 );
	}
	else
	{
		sParam2 = FixGLSLSwizzle( sParam1, sParam2 );
		sParam3 = FixGLSLSwizzle( sParam1, sParam3 );
	}

	char buff[256];
	if ( nInstruction == D3DSIO_ADD || nInstruction == D3DSIO_SUB || nInstruction == D3DSIO_MUL )
	{
		// These all look like x = y op z
		PrintToBufWithIndents( *m_pBufALUCode, "%s = %s %s %s;\n", sParam1.String(), sParam2.String(), GetGLSLOperatorString( nInstruction ), sParam3.String() );
	}
	else
	{
		int nDestComponents = GetNumSwizzleComponents( sParam1.String() );
		int nSrcComponents = GetNumSwizzleComponents( sParam2.String() );
		
		// All remaining instructions can use GLSL intrinsics like dot() and cross().
		bool bDoubleClose = OpenIntrinsic( nInstruction, buff, sizeof( buff ), nDestComponents, nSrcComponents );

		if ( ( nSrcComponents == 1 ) && ( nInstruction == D3DSIO_SGE ) )
		{
			PrintToBufWithIndents( *m_pBufALUCode, "%s = %s%s >= %s );\n", sParam1.String(), buff, sParam2.String(), sParam3.String() );
		}
		else if ( ( nSrcComponents == 1 ) && ( nInstruction == D3DSIO_SLT ) )
		{
			PrintToBufWithIndents( *m_pBufALUCode, "%s = %s%s < %s );\n", sParam1.String(), buff, sParam2.String(), sParam3.String() );
		}
		else
		{
			PrintToBufWithIndents( *m_pBufALUCode, "%s = %s%s, %s %s;\n", sParam1.String(), buff, sParam2.String(), sParam3.String(), bDoubleClose ? ") )" : ")" );
		}
	}

	// If the _SAT instruction modifier is used, then do a saturate here.
	if ( nDestToken & D3DSPDM_SATURATE )
	{
		int nComponents = GetNumSwizzleComponents( sParam1.String() );
		if ( nComponents == 0 )
			nComponents = 4;

		PrintToBufWithIndents( *m_pBufALUCode, "%s = clamp( %s, %s, %s );\n", sParam1.String(), sParam1.String(), g_szVecZeros[nComponents], g_szVecOnes[nComponents] );
	}
}

void D3DToGL::HandleBinaryOp_ASM( uint32 nInstruction )
{
	CUtlString sParam1 = GetParameterString( GetNextToken(), DST_REGISTER, false, NULL );
	int nARLComp0 = ARL_DEST_NONE;
	CUtlString sParam2 = GetParameterString( GetNextToken(), SRC_REGISTER, false, &nARLComp0 );
	int nARLComp1 = ARL_DEST_NONE;
	CUtlString sParam3 = GetParameterString( GetNextToken(), SRC_REGISTER, false, &nARLComp1 );

	// This optionally inserts a move from our dummy address register to the .x component of the real one
	InsertMoveFromAddressRegister( m_pBufALUCode, nARLComp0, nARLComp1 );

	char buff[256];
	PrintOpcode( nInstruction, buff, sizeof( buff ) );
	PrintToBufWithIndents( *m_pBufALUCode, "%s%s, %s, %s;\n", buff, sParam1.String(), sParam2.String(), sParam3.String() );
}

void D3DToGL::WriteGLSLCmp( const char *pDestReg, const char *pSrc0Reg, const char *pSrc1Reg, const char *pSrc2Reg )
{
	int nWriteMaskEntries = GetNumWriteMaskEntries( pDestReg );
	for ( int i=0; i < nWriteMaskEntries; i++ )
	{
		char params[4][256];
		WriteParamWithSingleMaskEntry( pDestReg, i, params[0], sizeof( params[0] ) );
		WriteParamWithSingleMaskEntry( pSrc0Reg, i, params[1], sizeof( params[1] ) );
		WriteParamWithSingleMaskEntry( pSrc1Reg, i, params[2], sizeof( params[2] ) );
		WriteParamWithSingleMaskEntry( pSrc2Reg, i, params[3], sizeof( params[3] ) );

		PrintToBufWithIndents( *m_pBufALUCode, "%s = ( %s >= 0.0 ) ? %s : %s;\n", params[0], params[1], params[2], params[3] );
	}
}

void D3DToGL::Handle_CMP()
{
	// In Direct3D, result = (src0 >= 0.0) ? src1 : src2
	// In OpenGL,	result = (src0 <  0.0) ? src1 : src2
	//
	// As a result, arguments are effectively in a different order than Direct3D!  !#$&*!%#$&
	char pDestReg[64], pSrc0Reg[64], pSrc1Reg[64], pSrc2Reg[64];
	uint32 nDestToken = GetNextToken();
	PrintParameterToString( nDestToken, DST_REGISTER, pDestReg, sizeof( pDestReg ), false, NULL );
	PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false, NULL );
	PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc1Reg, sizeof( pSrc1Reg ), false, NULL );
	PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc2Reg, sizeof( pSrc2Reg ), false, NULL );
	
	// These are a tricky case.. we have to expand it out into multiple statements.
	char szDestBase[256];
	GetParamNameWithoutSwizzle( pDestReg, szDestBase, sizeof( szDestBase ) );

	V_strncpy( pSrc0Reg, FixGLSLSwizzle( pDestReg, pSrc0Reg ), sizeof( pSrc0Reg ) );
	V_strncpy( pSrc1Reg, FixGLSLSwizzle( pDestReg, pSrc1Reg ), sizeof( pSrc1Reg ) );
	V_strncpy( pSrc2Reg, FixGLSLSwizzle( pDestReg, pSrc2Reg ), sizeof( pSrc2Reg ) );

	// This isn't reliable!
	//if ( DoParamNamesMatch( pDestReg, pSrc0Reg ) && GetNumSwizzleComponents( pDestReg ) > 1  )
	if ( 1 ) 
	{
		// So the dest register is the same as the comparand. We're in danger of screwing up our results.
		//
		// For example, this code:
		//		CMP r0.xy, r0.xx, r1, r2
		// would generate this:
		//		r0.x = (r0.x >= 0) ? r1.x : r2.x;
		//		r0.y = (r0.x >= 0) ? r1.x : r2.x;
		//
		// But the first lines changes r0.x and thus screws the atomicity of the CMP instruction for the second line.
		// So we assign r0 to a temporary first and then write to the temporary.
		PrintToBufWithIndents( *m_pBufALUCode, "%s = %s;\n", g_pAtomicTempVarName, szDestBase );

		char szTempVar[256];
		ReplaceParamName( pDestReg, g_pAtomicTempVarName, szTempVar, sizeof( szTempVar ) );
		WriteGLSLCmp( szTempVar, pSrc0Reg, pSrc1Reg, pSrc2Reg );

		PrintToBufWithIndents( *m_pBufALUCode, "%s = %s;\n", szDestBase, g_pAtomicTempVarName );
		m_bUsedAtomicTempVar = true;
	}
	else
	{
		// Just write out the simple expanded version of the CMP. No need to use atomic_temp_var.
		WriteGLSLCmp( pDestReg, pSrc0Reg, pSrc1Reg, pSrc2Reg );
	}
		
	// If the _SAT instruction modifier is used, then do a saturate here.
	if ( nDestToken & D3DSPDM_SATURATE )
	{
		int nComponents = GetNumSwizzleComponents( pDestReg );
		if ( nComponents == 0 )
			nComponents = 4;
			
		PrintToBufWithIndents( *m_pBufALUCode, "%s = clamp( %s, %s, %s );\n", pDestReg, pDestReg, g_szVecZeros[nComponents], g_szVecOnes[nComponents] );
	}
}

void D3DToGL::Handle_NRM()
{
	char pDestReg[64];
	char pSrc0Reg[64];
	PrintParameterToString( GetNextToken(), DST_REGISTER, pDestReg, sizeof( pDestReg ), false, NULL );
	int nARLSrcComp = ARL_DEST_NONE;
	PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false, &nARLSrcComp );
	
	if ( nARLSrcComp != -1 )
	{
		InsertMoveFromAddressRegister( m_pBufALUCode, nARLSrcComp, -1, -1 );
	}

	CUtlString sSrc = EnsureNumSwizzleComponents( pSrc0Reg, 3 );
	PrintToBufWithIndents( *m_pBufALUCode, "%s = normalize( %s );\n", pDestReg, sSrc.String() );
}

void D3DToGL::Handle_UnaryOp( uint32 nInstruction )
{
	uint32 nDestToken = GetNextToken();
	CUtlString sParam1 = GetParameterString( nDestToken, DST_REGISTER, false, NULL );
	CUtlString sParam2 = GetParameterString( GetNextToken(), SRC_REGISTER, false, NULL );
	sParam2 = FixGLSLSwizzle( sParam1, sParam2 );

	
	if ( nInstruction == D3DSIO_MOV )
	{
		PrintToBufWithIndents( *m_pBufALUCode, "%s = %s;\n", sParam1.String(), sParam2.String() );
	}
	else if ( nInstruction == D3DSIO_RSQ )
	{
		PrintToBufWithIndents( *m_pBufALUCode, "%s = inversesqrt( %s );\n", sParam1.String(), sParam2.String() );
	}
	else if ( nInstruction == D3DSIO_RCP )
	{
		PrintToBufWithIndents( *m_pBufALUCode, "%s = 1.0 / %s;\n", sParam1.String(), sParam2.String() );
	}
	else if ( nInstruction == D3DSIO_EXP )
	{
		PrintToBufWithIndents( *m_pBufALUCode, "%s = exp2( %s );\n", sParam1.String(), sParam2.String() );
	}
	else if ( nInstruction == D3DSIO_FRC )
	{
		PrintToBufWithIndents( *m_pBufALUCode, "%s = fract( %s );\n", sParam1.String(), sParam2.String() );
	}
	else if ( nInstruction == D3DSIO_LOG )	// d3d 'log' is log base 2
	{
		PrintToBufWithIndents( *m_pBufALUCode, "%s = log2( %s );\n", sParam1.String(), sParam2.String() );
	}
	else if ( nInstruction == D3DSIO_ABS )	// rbarris did this one, Jason please check
	{
		PrintToBufWithIndents( *m_pBufALUCode, "%s = abs( %s );\n", sParam1.String(), sParam2.String() );
	}
	else if ( nInstruction == D3DSIO_MOVA )
	{
		m_bDeclareAddressReg = true;
		PrintToBufWithIndents( *m_pBufALUCode, "%s = %s;\n", sParam1.String(), sParam2.String() );
			
		if ( !m_bGenerateBoneUniformBuffer )
		{
			m_nHighestRegister = DXABSTRACT_VS_PARAM_SLOTS - 1;
		}
	}
	else
	{
		Error( "Unsupported instruction" );
	}

	// If the _SAT instruction modifier is used, then do a saturate here.
	if ( nDestToken & D3DSPDM_SATURATE )
	{
		int nComponents = GetNumSwizzleComponents( sParam1.String() );
		if ( nComponents == 0 )
		{
			nComponents = 4;
		}

		PrintToBufWithIndents( *m_pBufALUCode, "%s = clamp( %s, %s, %s );\n", sParam1.String(), sParam1.String(), g_szVecZeros[nComponents], g_szVecOnes[nComponents] );
	}
}

void D3DToGL::WriteGLSLSamplerDefinitions()
{
	int nSamplersWritten = 0;
	for ( int i=0; i < ARRAYSIZE( m_dwSamplerTypes ); i++ )
	{
		if ( m_dwSamplerTypes[i] == SAMPLER_TYPE_2D )
		{
			if ( ( ( 1 << i ) & m_nShadowDepthSamplerMask ) != 0 )
			{
				PrintToBuf( *m_pBufHeaderCode, "uniform sampler2DShadow sampler%d;\n", i );
			}
			else
			{
				PrintToBuf( *m_pBufHeaderCode, "uniform sampler2D sampler%d;\n", i );
			}
			++nSamplersWritten;
		}
		else if ( m_dwSamplerTypes[i] == SAMPLER_TYPE_3D )
		{
			PrintToBuf( *m_pBufHeaderCode, "uniform sampler3D sampler%d;\n", i );
			++nSamplersWritten;
		}
		else if ( m_dwSamplerTypes[i] == SAMPLER_TYPE_CUBE )
		{
			PrintToBuf( *m_pBufHeaderCode, "uniform samplerCube sampler%d;\n", i );
			++nSamplersWritten;
		}
		else if ( m_dwSamplerTypes[i] != SAMPLER_TYPE_UNUSED )
		{
			Error( "Unknown sampler type." );
		}
	}

	if ( nSamplersWritten > 0 )
		PrintToBuf( *m_pBufHeaderCode, "\n\n" );
}

void D3DToGL::WriteGLSLOutputVariableAssignments()
{
	if ( m_bVertexShader )
	{
		// Map output "oN" registers back to GLSL output variables.
		if ( m_bAddHexCodeComments )
		{
			PrintToBuf( *m_pBufAttribCode, "\n// Now we're storing the oN variables from the output dcl_ statements back into their GLSL equivalents.\n" );
		}
				
		for ( int i=0; i < ARRAYSIZE( m_DeclaredOutputs ); i++ )
		{
			if ( m_DeclaredOutputs[i] == UNDECLARED_OUTPUT )
				continue;

			if ( ( m_dwTexCoordOutMask & ( 1 << i ) ) == 0 )
				continue;

			uint32 dwToken = m_DeclaredOutputs[i];

			uint32 dwUsage = ( dwToken & D3DSP_DCL_USAGE_MASK );
			uint32 dwUsageIndex = ( dwToken & D3DSP_DCL_USAGEINDEX_MASK ) >> D3DSP_DCL_USAGEINDEX_SHIFT;

			if ( ( dwUsage == D3DDECLUSAGE_FOG ) || ( dwUsage == D3DDECLUSAGE_PSIZE ) )
			{
				TranslationError(); // Not supported yet, but can be if we need it.
			}

			if ( dwUsage == D3DDECLUSAGE_COLOR )
			{
				PrintToBufWithIndents( *m_pBufALUCode, "%s = oTempT%d;\n", dwUsageIndex ? "gl_FrontSecondaryColor" : "gl_FrontColor", i );
			}
			else if ( dwUsage == D3DDECLUSAGE_TEXCOORD )
			{
				char buf[256];
				if ( m_nCentroidMask & ( 0x00000001 << dwUsageIndex ) )
				{
					V_snprintf( buf, sizeof( buf ), "centroid varying vec4 oT%d;\n", dwUsageIndex ); // centroid varying
				}
				else
				{
					V_snprintf( buf, sizeof( buf ), "varying vec4 oT%d;\n", dwUsageIndex );
				}
				StrcatToHeaderCode( buf );
									
				PrintToBufWithIndents( *m_pBufALUCode, "oT%d = oTempT%d;\n", dwUsageIndex, i );
			}
		}
	}
}

void D3DToGL::WriteGLSLInputVariableAssignments()
{
	if ( m_bVertexShader )
		return;
		
	for ( int i=0; i < ARRAYSIZE( m_DeclaredInputs ); i++ )
	{
		if ( m_DeclaredInputs[i] == UNDECLARED_INPUT )
			continue;
				
		uint32 dwToken = m_DeclaredInputs[i];

		uint32 dwUsage = ( dwToken & D3DSP_DCL_USAGE_MASK );
		uint32 dwUsageIndex = ( dwToken & D3DSP_DCL_USAGEINDEX_MASK ) >> D3DSP_DCL_USAGEINDEX_SHIFT;

		if ( dwUsage == D3DDECLUSAGE_COLOR )
		{
			PrintToBufWithIndents( *m_pBufAttribCode, "vec4 oTempT%d = %s;\n", i, dwUsageIndex ? "gl_SecondaryColor" : "gl_Color" );
		}
		else if ( dwUsage == D3DDECLUSAGE_TEXCOORD )
		{
			PrintToBufWithIndents( *m_pBufAttribCode, "vec4 oTempT%d = oT%d;\n", i, dwUsageIndex );
		}
	}
}

void D3DToGL::Handle_DeclarativeNonDclOp( uint32 nInstruction )
{
	char buff[128];
	uint32 dwToken = GetNextToken();
	PrintParameterToString( dwToken, DST_REGISTER, buff, sizeof( buff ), false, NULL );

	if ( nInstruction == D3DSIO_TEXKILL )
	{
		// TEXKILL is supposed to discard the pixel if any of the src register's X, Y, or Z components are less than zero.
		// We have to translate it to something like:
		//   if ( r0.x < 0.0 || r0.y < 0.0 )
		//        discard;
		char c[3];
		c[0] = GetSwizzleComponent( buff, 0 );
		c[1] = GetSwizzleComponent( buff, 1 );
		c[2] = GetSwizzleComponent( buff, 2 );

		// Get the unique components.
		char cUnique[3];
		cUnique[0] = c[0];

		int nUnique = 1;
		if ( c[1] != c[0] )
			cUnique[nUnique++] = c[1];

		if ( c[2] != c[1] && c[2] != c[0] )
			cUnique[nUnique++] = c[2];

		// Get the src register base name.
		char szBase[256];
		GetParamNameWithoutSwizzle( buff, szBase, sizeof( szBase ) );

		PrintToBufWithIndents( *m_pBufALUCode, "if ( %s.%c < 0.0 ", szBase, cUnique[0] );
		for ( int i=1; i < nUnique; i++ )
		{
			PrintToBuf( *m_pBufALUCode, "|| %s.%c < 0.0 ", szBase, cUnique[i] );
		}
		PrintToBuf( *m_pBufALUCode, ")\n{\n\tdiscard;\n}\n" );
	}
	else
	{
		char szOpcode[128];
		PrintOpcode( nInstruction, szOpcode, sizeof( szOpcode ) );
		StrcatToALUCode( szOpcode );

		StrcatToALUCode( buff );
		StrcatToALUCode( ";\n" );
	}
}


void D3DToGL::NoteTangentInputUsed()
{
	if ( !m_bTangentInputUsed )
	{
		m_bTangentInputUsed = true;
//		PrintToBuf( *m_pBufParamCode, "attribute vec4 %s;\n", g_pTangentAttributeName );
	}
}


// These are the only ARL instructions that should appear in the instruction stream
void D3DToGL::InsertMoveInstruction( CUtlBuffer *pCode, int nARLComponent )
{
	PrintIndentation( ( char * )pCode->Base(), pCode->Size() );

	switch ( nARLComponent )
	{
		case ARL_DEST_X:
			strcat_s( ( char * )pCode->Base(), pCode->Size(), "a0 = int( va_r.x );\n" );
			break;
		case ARL_DEST_Y:
			strcat_s( ( char * )pCode->Base(), pCode->Size(), "a0 = int( va_r.y );\n" );
			break;
		case ARL_DEST_Z:
			strcat_s( ( char * )pCode->Base(), pCode->Size(), "a0 = int( va_r.z );\n" );
			break;
		case ARL_DEST_W:
			strcat_s( ( char * )pCode->Base(), pCode->Size(), "a0 = int( va_r.w );\n" );
			break;
	}
}

// This optionally inserts a move from our dummy address register to the .x component of the real one
void D3DToGL::InsertMoveFromAddressRegister( CUtlBuffer *pCode, int nARLComp0, int nARLComp1, int nARLComp2 /* = ARL_DEST_NONE */ )
{
	// We no longer need to do this in GLSL - we put the cast to int from the dummy address register va_r.x, va_r.y, etc. directly into the instruction
	return;
}


//------------------------------------------------------------------------------
// TranslateShader()
//
// This is the main function that the outside world sees.  A pointer to the
// uint32 stream returned from the D3DX compile routine is parsed and used
// to write human-readable asm code into the character array pointed to by
// pDisassembledCode.  An error code is returned.
//------------------------------------------------------------------------------


int D3DToGL::TranslateShader( uint32* code, CUtlBuffer *pBufDisassembledCode, bool *bVertexShader, uint32 options, int32 nShadowDepthSamplerMask, uint32 nCentroidMask, char *debugLabel )
{
	CUtlString sLine, sParamName;
	uint32 i, dwToken, nInstruction, nNumTokensToSkip;
	char buff[256];

	// obey options
	m_bUseEnvParams = (options & D3DToGL_OptionUseEnvParams) != 0;
	m_bDoFixupZ = (options & D3DToGL_OptionDoFixupZ) != 0;
	m_bDoFixupY = (options & D3DToGL_OptionDoFixupY) != 0;
	m_bDoUserClipPlanes = (options & D3DToGL_OptionDoUserClipPlanes) != 0;
	
	m_bAddHexCodeComments = (options & D3DToGL_AddHexComments) != 0;
	m_bPutHexCodesAfterLines = (options & D3DToGL_PutHexCommentsAfterLines) != 0;
	m_bGeneratingDebugText = (options & D3DToGL_GeneratingDebugText) != 0;
	m_bGenerateSRGBWriteSuffix = (options & D3DToGL_OptionSRGBWriteSuffix) != 0;

	m_NumIndentTabs = 1; // start code indented one tab
	m_nLoopDepth = 0;

	// debugging
	m_bSpew = (options & D3DToGL_OptionSpew) != 0;
	
	// These are not accessed below in a way that will cause them to glow, so
	// we could overflow these and/or the buffer pointed to by pDisassembledCode
	m_pBufAttribCode = new CUtlBuffer( 100, 10000, CUtlBuffer::TEXT_BUFFER );
	m_pBufParamCode = new CUtlBuffer( 100, 10000, CUtlBuffer::TEXT_BUFFER );
	m_pBufALUCode = new CUtlBuffer( 100, 60000, CUtlBuffer::TEXT_BUFFER );

	// Pointers to text buffers for assembling sections of the program
	m_pBufHeaderCode = pBufDisassembledCode;
	char *pAttribMapStart = NULL;
	((char*)m_pBufHeaderCode->Base())[0] = 0;
	((char*)m_pBufAttribCode->Base())[0] = 0;
	((char*)m_pBufParamCode->Base())[0] = 0;
	((char*)m_pBufALUCode->Base())[0] = 0;


	for ( i=0; i<MAX_SHADER_CONSTANTS; i++ )
	{
		m_bConstantRegisterDefined[i] = false;
	}

	// Track shadow sampler usage for proper declaration
	m_nShadowDepthSamplerMask = nShadowDepthSamplerMask;
	m_bDeclareShadowOption = false;

	// Various flags set while parsing code to drive various declaration instructions
	m_bNeedsD2AddTemp = false;
	m_bNeedsLerpTemp = false;
	m_bNeedsNRMTemp = false;
	m_bNeedsSinCosDeclarations = false;
	m_bDeclareAddressReg = false;
	m_bDeclareVSOPos = false;
	m_bDeclareVSOFog = false;
	m_dwTexCoordOutMask = 0x00000000;
	m_nVSPositionOutput = -1;
	m_bOutputColorRegister[0] = false;
	m_bOutputColorRegister[1] = false;
	m_bOutputColorRegister[2] = false;
	m_bOutputColorRegister[3] = false;
	m_bOutputDepthRegister = false;
	m_bTangentInputUsed = false;
	m_bUsesDSTInstruction = false;
	m_dwTempUsageMask = 0x00000000;
	m_dwSamplerUsageMask = 0x00000000;
	m_dwConstIntUsageMask = 0x00000000;
	m_dwDefConstIntUsageMask = 0x00000000;
	memset( m_dwDefConstIntIterCount, 0, sizeof( m_dwDefConstIntIterCount ) );
	m_dwConstBoolUsageMask = 0x00000000;
	m_nCentroidMask = nCentroidMask;
	m_nHighestRegister = 0;
	m_nHighestBoneRegister = -1;
	m_bGenerateBoneUniformBuffer = false;
	m_bUseBindlessTexturing = ((options & D3DToGL_OptionUseBindlessTexturing) != 0);
		
	m_bUsedAtomicTempVar = false;
	for ( int i=0; i < ARRAYSIZE( m_dwSamplerTypes ); i++ )
	{
		m_dwSamplerTypes[i] = SAMPLER_TYPE_UNUSED;
	}

	for ( int i=0; i < ARRAYSIZE( m_DeclaredOutputs ); i++ )
	{
		m_DeclaredOutputs[i] = UNDECLARED_OUTPUT;
	}

	for ( int i=0; i < ARRAYSIZE( m_DeclaredInputs ); i++ )
	{
		m_DeclaredInputs[i] = UNDECLARED_INPUT;
	}

	memset( m_dwAttribMap, 0xFF, sizeof(m_dwAttribMap) );
	
	m_pdwBaseToken = m_pdwNextToken = code;	 // Initialize dwToken pointers

	dwToken = GetNextToken();
	m_dwMajorVersion = D3DSHADER_VERSION_MAJOR( dwToken );
	m_dwMinorVersion = D3DSHADER_VERSION_MINOR( dwToken );

	// If pixel shader
	const char *glslExtText = "#extension GL_ARB_shader_texture_lod : require\n";//m_bUseBindlessTexturing ? "#extension GL_NV_bindless_texture : require\n" : "";
	// 7ls
	const char *glslVersionText = m_bUseBindlessTexturing ? "330 compatibility" : "120";

	if ( ( dwToken & 0xFFFF0000 ) == 0xFFFF0000 )
	{
		// must explicitly enable extensions if emitting GLSL
		V_snprintf( (char *)m_pBufHeaderCode->Base(), m_pBufHeaderCode->Size(), "#version %s\n%s", glslVersionText, glslExtText );
		m_bVertexShader = false;
	}
	else // vertex shader
	{
		m_bGenerateSRGBWriteSuffix = false;

		V_snprintf( (char *)m_pBufHeaderCode->Base(), m_pBufHeaderCode->Size(), "#version %s\n%s//ATTRIBMAP-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx\n", glslVersionText, glslExtText );
		
		// find that first '-xx' which is where the attrib map will be written later.
		pAttribMapStart = strstr( (char *)m_pBufHeaderCode->Base(), "-xx" ) + 1;
		
		m_bVertexShader = true;
	}
	
	*bVertexShader = m_bVertexShader;
	
	m_bGenerateBoneUniformBuffer = m_bVertexShader && ((options & D3DToGL_OptionGenerateBoneUniformBuffer) != 0);
			
	if ( m_bAddHexCodeComments )
	{
		RecordInputAndOutputPositions();
	}
	
	if ( m_bSpew )
	{
		printf("\n************* translating shader " );
	}
	
	int opcounter = 0;
	
	// Loop until we hit the end dwToken...note that D3DPS_END() == D3DVS_END() so this works for either
	while ( dwToken != D3DPS_END() )
	{
		if ( m_bAddHexCodeComments )
		{
			AddTokenHexCode();
			RecordInputAndOutputPositions();
		}
		
#ifdef POSIX
		int tokenIndex = m_pdwNextToken - code;
#endif
		int aluCodeLength0 = V_strlen( (char *) m_pBufALUCode->Base() );
		
		dwToken = GetNextToken();	// Get next dwToken in the stream
		nInstruction = Opcode( dwToken ); // Mask out the instruction opcode

		if ( m_bSpew )
		{
#ifdef POSIX
			printf("\n** token# %04x inst# %04d  opcode %s (%08x)", tokenIndex, opcounter, GLMDecode(eD3D_SIO, nInstruction), dwToken );
#endif
			opcounter++;
		}
		
		switch ( nInstruction )
		{
			// -- No arguments at all -----------------------------------------------
			case D3DSIO_NOP:
				// D3D compiler outputs NOPs when shader debugging/optimizations are disabled.
				break;

			case D3DSIO_PHASE:
			case D3DSIO_RET:
			case D3DSIO_ENDLOOP:
			case D3DSIO_BREAK:
				TranslationError();
				PrintOpcode( nInstruction, buff, sizeof( buff ) );
				StrcatToALUCode( buff );
				StrcatToALUCode( ";\n" );
				break;

			// -- "Declarative" non dcl ops ----------------------------------------
			case D3DSIO_TEXDEPTH:
			case D3DSIO_TEXKILL:
				Handle_DeclarativeNonDclOp( nInstruction );
				break;

			// -- Unary ops -------------------------------------------------
			case D3DSIO_BEM:
			case D3DSIO_TEXBEM:
			case D3DSIO_TEXBEML:
			case D3DSIO_TEXDP3:
			case D3DSIO_TEXDP3TEX:
			case D3DSIO_TEXM3x2DEPTH:
			case D3DSIO_TEXM3x2TEX:
			case D3DSIO_TEXM3x3:
			case D3DSIO_TEXM3x3PAD:
			case D3DSIO_TEXM3x3TEX:
			case D3DSIO_TEXM3x3VSPEC:
			case D3DSIO_TEXREG2AR:
			case D3DSIO_TEXREG2GB:
			case D3DSIO_TEXREG2RGB:
			case D3DSIO_LABEL:
			case D3DSIO_CALL:
			case D3DSIO_LOOP:
			case D3DSIO_BREAKP:
			case D3DSIO_DSX:
			case D3DSIO_DSY:
				TranslationError();
				break;

			case D3DSIO_IFC:
			{
				static const char *s_szCompareStrings[ 7 ] =
				{
					"__INVALID__",
					">",
					"==",
					">=",
					"<",
					"!=",
					"<="
				};

				// Compare mode is encoded in instruction token
				uint32 dwCompareMode = OpcodeSpecificData( dwToken );

				Assert( ( dwCompareMode >= 1 ) && ( dwCompareMode <= 6 ) );

				// Get left side of compare
				dwToken = GetNextToken();
				char szLeftSide[32];
				PrintParameterToString( dwToken, SRC_REGISTER, szLeftSide, sizeof( szLeftSide ), false, NULL );

				// Get right side of compare
				dwToken = GetNextToken();
				char szRightSide[32];
				PrintParameterToString( dwToken, SRC_REGISTER, szRightSide, sizeof( szRightSide ), false, NULL );

				PrintToBufWithIndents( *m_pBufALUCode, "if ( %s %s %s )\n", szLeftSide, s_szCompareStrings[dwCompareMode], szRightSide );
				StrcatToALUCode( "{\n" );
				m_NumIndentTabs++;
				
				break;
			}
			case D3DSIO_IF:
				dwToken = GetNextToken();
				PrintParameterToString( dwToken, SRC_REGISTER, buff, sizeof( buff ), false, NULL );

				PrintToBufWithIndents( *m_pBufALUCode, "if ( %s )\n", buff );
				StrcatToALUCode( "{\n" );
				m_NumIndentTabs++;
				
				break;

			case D3DSIO_ELSE:
				m_NumIndentTabs--;
				StrcatToALUCode( "}\n" );
				StrcatToALUCode( "else\n" );
				StrcatToALUCode( "{\n" );
				m_NumIndentTabs++;
				
				break;

			case D3DSIO_ENDIF:
				m_NumIndentTabs--;
				StrcatToALUCode( "}\n" );
				
				break;

			case D3DSIO_REP:
				dwToken = GetNextToken();
				PrintParameterToString( dwToken, SRC_REGISTER, buff, sizeof( buff ), false, NULL );

				// In practice, this is the only form of for loop that will appear in DX asm
				PrintToBufWithIndents( *m_pBufALUCode, "for( int i=0; i < %s; i++ )\n", buff );
				StrcatToALUCode( "{\n" );

				m_nLoopDepth++;

				// For now, we don't deal with loop nesting
				// Easy enough to fix later with an array of loop names i, j, k etc
				Assert( m_nLoopDepth <= 1 );

				m_NumIndentTabs++;
				
				break;

			case D3DSIO_ENDREP:
				m_nLoopDepth--;
				m_NumIndentTabs--;
				StrcatToALUCode( "}\n" );
				
				break;

			case D3DSIO_NRM:
				Handle_NRM();
				break;

			case D3DSIO_MOVA:

				Handle_UnaryOp( nInstruction );
				
				break;
				
			// Unary operations
			case D3DSIO_MOV:
			case D3DSIO_RCP:
			case D3DSIO_RSQ:
			case D3DSIO_EXP:
			case D3DSIO_EXPP:
			case D3DSIO_LOG:
			case D3DSIO_LOGP:
			case D3DSIO_FRC:
			case D3DSIO_LIT:
			case D3DSIO_ABS:
				Handle_UnaryOp( nInstruction );
				break;

			// -- Binary ops -------------------------------------------------
			case D3DSIO_TEXM3x3SPEC:
			case D3DSIO_M4x4:
			case D3DSIO_M4x3:
			case D3DSIO_M3x4:
			case D3DSIO_M3x3:
			case D3DSIO_M3x2:
			case D3DSIO_CALLNZ:
			case D3DSIO_SETP:
				TranslationError();
				break;

			case D3DSIO_BREAKC:
				Handle_BREAKC( dwToken );
				break;

			// Binary Operations
			case D3DSIO_ADD:
			case D3DSIO_SUB:
			case D3DSIO_MUL:
			case D3DSIO_DP3:
			case D3DSIO_DP4:
			case D3DSIO_MIN:
			case D3DSIO_MAX:
			case D3DSIO_DST:
			case D3DSIO_SLT:
			case D3DSIO_SGE:
			case D3DSIO_CRS:
			case D3DSIO_POW:
				HandleBinaryOp_GLSL( nInstruction );
				
				break;

			// -- Ternary ops -------------------------------------------------
			case D3DSIO_DP2ADD:
				Handle_DP2ADD();
				break;
			case D3DSIO_LRP:
				Handle_LRP( nInstruction );
				break;
			case D3DSIO_SGN:
				Assert( m_bVertexShader );
				TranslationError();		// TODO emulate with SLT etc
				break;
			case D3DSIO_CND:
				TranslationError();
				break;
			case D3DSIO_CMP:
				Handle_CMP();
				break;
			case D3DSIO_SINCOS:
				Handle_SINCOS();
				break;
			case D3DSIO_MAD:
				Handle_MAD( nInstruction );
				break;

			// -- Quaternary op ------------------------------------------------
			case D3DSIO_TEXLDD:
				Handle_TexLDD( nInstruction );
				break;
				
			// -- Special cases: texcoord vs texcrd	and	tex vs texld -----------
			case D3DSIO_TEXCOORD:
				Handle_TexCoord();
				break;

			case D3DSIO_TEX:
				Handle_TEX( dwToken, false );
				break;

			case D3DSIO_TEXLDL:
				Handle_TEX( nInstruction, true );
				break;
				
			case D3DSIO_DCL:
				Handle_DCL();
				break;

			case D3DSIO_DEFB:
			case D3DSIO_DEFI:
				Handle_DEFIB( nInstruction );
				break;

			case D3DSIO_DEF:
				Handle_DEF();
				break;

			case D3DSIO_COMMENT:
				// Using OpcodeSpecificData() can fail here since the comments can be longer than 0xff dwords
				nNumTokensToSkip = ( dwToken & 0x0fff0000 ) >> 16;
				SkipTokens( nNumTokensToSkip );
				break;

			case D3DSIO_END:
				break;
		}
		
		if ( m_bSpew )
		{
			int aluCodeLength1 = V_strlen( (char *) m_pBufALUCode->Base() );
			if ( aluCodeLength1 != aluCodeLength0 )
			{
				// code was emitted
				printf( "\n    > %s", ((char *)m_pBufALUCode->Base()) + aluCodeLength0 );
				
				aluCodeLength0 = aluCodeLength1;
			}
		}
	}

	// Note that this constant packing expects .wzyx swizzles in case we ever use the SINCOS code in a ps_2_x shader
	//
	// The Microsoft documentation on this is all kinds of broken and, strangely, these numbers don't even
	// match the D3DSINCOSCONST1 and D3DSINCOSCONST2 constants used by the D3D assembly sincos instruction...
	if ( m_bNeedsSinCosDeclarations )
	{
		PrintIndentation( (char*)m_pBufParamCode->Base(), m_pBufParamCode->Size() );
		StrcatToParamCode( "vec4 scA = vec4( -1.55009923e-6, -2.17013894e-5, 0.00260416674, 0.00026041668 );\n" );
		PrintIndentation( (char*)m_pBufParamCode->Base(), m_pBufParamCode->Size() );
		StrcatToParamCode( "vec4 scB = vec4( -0.020833334, -0.125, 1.0, 0.5 );\n" );			
	}

	// Stick in the sampler mask in hex
	PrintToBuf( *m_pBufHeaderCode, "%sSAMPLERMASK-%x\n", "//", m_dwSamplerUsageMask );

	uint nSamplerTypes = 0;
	for ( int i = 0; i < 16; i++ )
	{
		Assert( m_dwSamplerTypes[i] < 4);
		nSamplerTypes |= ( m_dwSamplerTypes[i] << ( i * 2 ) );
	}
	
	PrintToBuf( *m_pBufHeaderCode, "%sSAMPLERTYPES-%x\n", "//", nSamplerTypes );

	// fragData outputs referenced
	uint nFragDataMask = 0;
	for ( int i = 0; i < 4; i++ )
	{
		nFragDataMask |= m_bOutputColorRegister[ i ] ? ( 1 << i ) : 0;
	}

	PrintToBuf( *m_pBufHeaderCode, "%sFRAGDATAMASK-%x\n", "//", nFragDataMask );

	// Uniforms

	PrintToBuf( *m_pBufHeaderCode, "//HIGHWATER-%d\n", m_nHighestRegister + 1 );
	if ( ( m_bVertexShader ) && ( m_bGenerateBoneUniformBuffer ) )
	{
		PrintToBuf( *m_pBufHeaderCode, "//HIGHWATERBONE-%i\n", m_nHighestBoneRegister + 1 );
	}

	PrintToBuf( *m_pBufHeaderCode, "\nuniform vec4 %s[%d];\n", m_bVertexShader ? "vc" : "pc", m_nHighestRegister + 1 );

	if ( ( m_nHighestBoneRegister >= 0 ) && ( m_bVertexShader ) && ( m_bGenerateBoneUniformBuffer ) )
	{
		PrintToBuf( *m_pBufHeaderCode, "\nuniform vec4 %s[%d];\n", "vcbones", m_nHighestBoneRegister + 1 );
	}

	if ( m_bVertexShader )
	{
		PrintToBuf( *m_pBufHeaderCode, "\nuniform vec4 vcscreen;\n" );
	}
				
	for( int i=0; i<32; i++ )
	{
		if ( ( m_dwConstIntUsageMask & ( 0x00000001 << i ) ) &&
			( !( m_dwDefConstIntUsageMask & ( 0x00000001 << i ) ) )
			)
		{
			PrintToBuf( *m_pBufHeaderCode, "uniform int i%d ;\n", i );
		}
	}

	for( int i=0; i<32; i++ )
	{
		if ( m_dwDefConstIntUsageMask & ( 0x00000001 << i ) )
		{
			PrintToBuf( *m_pBufHeaderCode, "const int i%d = %i;\n", i, m_dwDefConstIntIterCount[i] );
		}
	}

	for( int i=0; i<32; i++ )
	{
		if ( m_dwConstBoolUsageMask & ( 0x00000001 << i ) )
		{
			PrintToBuf( *m_pBufHeaderCode, m_bVertexShader ? "uniform bool b%d;\n" : "uniform bool fb%d;\n", i );
		}
	}

	// Control bit for sRGB Write suffix
	if ( m_bGenerateSRGBWriteSuffix )
	{
		// R500 Hookup
		// Set this guy to 1 when the sRGBWrite state is true, otherwise 0
		StrcatToHeaderCode( "uniform float flSRGBWrite;\n" );
	}

	PrintToBuf( *m_pBufHeaderCode, "\n" );

	// Write samplers
	WriteGLSLSamplerDefinitions();

	if ( m_bUsesDSTInstruction )
	{
		PrintToBuf( *m_pBufHeaderCode, "vec4 dst(vec4 src0,vec4 src1) { return vec4(1.0f,src0.y*src1.y,src0.z,src1.w); }\n" );
	}
	
	if ( m_bDeclareAddressReg )
	{
		if ( !m_bGenerateBoneUniformBuffer )
		{
			m_nHighestRegister = DXABSTRACT_VS_PARAM_SLOTS - 1;
		}

		PrintIndentation( (char*)m_pBufParamCode->Base(), m_pBufParamCode->Size() );
		StrcatToParamCode( "vec4 va_r;\n" );
	}

	char *pTempVarStr = "TEMP";
	pTempVarStr = "vec4";
	
	// Declare temps in Param code buffer
	for( int i=0; i<32; i++ )
	{
		if ( m_dwTempUsageMask & ( 0x00000001 << i ) )
		{
			PrintIndentation( (char*)m_pBufParamCode->Base(), m_pBufParamCode->Size() );
			PrintToBuf( *m_pBufParamCode, "%s r%d;\n", pTempVarStr, i );
		}
	}

	if ( m_bVertexShader && (m_bDoUserClipPlanes || m_bDoFixupZ  || m_bDoFixupY ) )
	{
		PrintIndentation( (char*)m_pBufParamCode->Base(), m_pBufParamCode->Size() );
		StrcatToParamCode( "vec4 vTempPos;\n" );
	}

	if ( ( m_bVertexShader ) && ( m_dwMajorVersion == 3 ) )
	{
		for ( int i = 0; i < 32; i++ )
		{
			if ( m_dwTexCoordOutMask & ( 1 << i ) )
			{
				PrintIndentation( (char*)m_pBufParamCode->Base(), m_pBufParamCode->Size() );

				char buf[256];
				V_snprintf( buf, sizeof( buf ), "vec4 oTempT%i = vec4( 0, 0, 0, 0 );\n", i );
				StrcatToParamCode( buf );
			}
		}
	}

	if ( m_bNeedsSinCosDeclarations )
	{
		StrcatToParamCode( "vec3 vSinCosTmp;\n" ); // declare temp used by GLSL sin and cos intrinsics
	}

	// Optional temps needed to emulate d2add instruction in DX pixel shaders
	if ( m_bNeedsD2AddTemp )
	{
		PrintToBuf( *m_pBufParamCode, "%s DP2A0;\n%s DP2A1;\n", pTempVarStr, pTempVarStr );
	}

	// Optional temp needed to emulate lerp instruction in DX vertex shaders
	if ( m_bNeedsLerpTemp )
	{
		PrintToBuf( *m_pBufParamCode, "%s LRP_TEMP;\n", pTempVarStr );
	}

	// Optional temp needed to emulate NRM instruction in DX shaders
	if ( m_bNeedsNRMTemp )
	{
		PrintToBuf( *m_pBufParamCode, "%s NRM_TEMP;\n", pTempVarStr );
	}
		
	if ( m_bDeclareVSOPos && m_bVertexShader )
	{
		if ( m_bDoUserClipPlanes )
		{
			StrcatToALUCode( "gl_ClipVertex = vTempPos;\n" ); // if user clip is enabled, jam clip space position into gl_ClipVertex
		}
		
		if ( m_bDoFixupZ  || m_bDoFixupY )
		{
			// TODO: insert clip distance computation something like this:
			//
			// StrcatToALUCode( "DP4 oCLP[0].x, oPos, vc[215]; \n" );
			//

			if ( m_bDoFixupZ )
			{
				StrcatToALUCode( "vTempPos.z = vTempPos.z * vc[0].z - vTempPos.w; // z' = (2*z)-w\n" );
			}

			if ( m_bDoFixupY )
			{
				// append instructions to flip Y over
				// new Y = -(old Y)
				StrcatToALUCode( "vTempPos.y = -vTempPos.y; // y' = -y \n" );
			}

			// Apply half pixel offset (0.5f pixel offset D3D) to output vertices to account for the pixel center difference between D3D9 and OpenGL.
			// This is the actual work in the shader. This works out to be 0.5 pixels wide because clip space is 2 units wide (-1, 1).
			StrcatToALUCode( "vTempPos.xy += vcscreen.xy * vTempPos.w;\n" );

			StrcatToALUCode( "gl_Position = vTempPos;\n" );
		}
		else
		{
			StrcatToParamCode( "OUTPUT oPos = result.position;\n" );

			// TODO: insert clip distance computation something like this:
			//
			// StrcatToALUCode( "DP4 oCLP[0].x, oPos, c[215]; \n" );
			//
		}
	}
		
	if ( m_bVertexShader )
	{
		if ( m_dwMajorVersion == 3 )
		{
			WriteGLSLOutputVariableAssignments();
		}
		else
		{
			for ( int i=0; i<32; i++ )
			{
				char outTexCoordBuff[64];

				// Don't declare a varying for the output that is mapped to the position output
				if ( i != m_nVSPositionOutput )
				{
					if ( m_dwTexCoordOutMask & ( 0x00000001 << i ) )
					{
						if ( m_nCentroidMask & ( 0x00000001 << i ) )
						{
							V_snprintf( outTexCoordBuff, sizeof( outTexCoordBuff ), "centroid varying vec4 oT%d;\n", i ); // centroid varying
							StrcatToHeaderCode( outTexCoordBuff );
						}
						else
						{
							V_snprintf( outTexCoordBuff, sizeof( outTexCoordBuff ), "varying vec4 oT%d;\n", i );
							StrcatToHeaderCode( outTexCoordBuff );
						}
					}
				}
			}			
		}
	}
	else 
	{
		if ( m_dwMajorVersion == 3 )
		{
			WriteGLSLInputVariableAssignments();
		}
	}
		
	// do some annotation at the end of the attrib block
	{
		char temp[1000];

		if ( m_bVertexShader )
		{
			// write attrib map into the text starting at pAttribMapStart - two hex digits per attrib
			for( int i=0; i<16; i++ )
			{
				if ( m_dwAttribMap[i] != 0xFFFFFFFF )
				{
					V_snprintf( temp, sizeof(temp), "%02X", m_dwAttribMap[i] );
					memcpy( pAttribMapStart + (i*3), temp, 2 );
				}
			}
		}

		PrintIndentation( (char*)m_pBufAttribCode->Base(), m_pBufAttribCode->Size() );
				
		// This used to write out a translation counter into the shader as a comment. However, the order that shaders get in here 
		// is non-deterministic between runs, and the change in this comment would cause shaders to appear different to the GL disk cache,
		// significantly increasing app load time.
		// Other code looks for trans#%d, so we can't just remove it. Instead, output it as 0.
		V_snprintf( temp, sizeof(temp), "%s trans#%d label:%s\n", "//", 0, debugLabel ? debugLabel : "none" );
		StrcatToAttribCode( temp );
	}

	// If we actually sample from a shadow depth sampler, we need to declare the shadow option at the top
	if ( m_bDeclareShadowOption )
	{
		StrcatToHeaderCode( "OPTION ARB_fragment_program_shadow;\n" );
	}

	StrcatToHeaderCode( "\nvoid main()\n{\n" );
	if ( m_bUsedAtomicTempVar )
	{
		PrintToBufWithIndents( *m_pBufHeaderCode, "vec4 %s;\n\n", g_pAtomicTempVarName );
	}
	
	// sRGB Write suffix
	if ( m_bGenerateSRGBWriteSuffix )
	{
		StrcatToALUCode( "vec3 sRGBFragData;\n" );
		StrcatToALUCode( "sRGBFragData.xyz = log( gl_FragData[0].xyz );\n" );
		StrcatToALUCode( "sRGBFragData.xyz = sRGBFragData.xyz * vec3( 0.454545f, 0.454545f, 0.454545f );\n" );
		StrcatToALUCode( "sRGBFragData.xyz = exp( sRGBFragData.xyz );\n" );
		StrcatToALUCode( "gl_FragData[0].xyz = mix( gl_FragData[0].xyz, sRGBFragData, flSRGBWrite );\n" );
	}

	strcat_s( (char*)m_pBufALUCode->Base(), m_pBufALUCode->Size(), "}\n" );
	
	// Put all of the strings together for final program ( pHeaderCode + pAttribCode + pParamCode + pALUCode )
	StrcatToHeaderCode( (char*)m_pBufAttribCode->Base() );
	StrcatToHeaderCode( (char*)m_pBufParamCode->Base() );
	StrcatToHeaderCode( (char*)m_pBufALUCode->Base() );

	// Cleanup - don't touch m_pBufHeaderCode, as it is managed by the caller
	delete m_pBufAttribCode;
	delete m_pBufParamCode;
	delete m_pBufALUCode;
	m_pBufAttribCode = m_pBufParamCode = m_pBufALUCode = NULL;

	if ( m_bSpew )
	{
		printf("\n************* translation complete\n\n " );
	}

	return DISASM_OK;
}
