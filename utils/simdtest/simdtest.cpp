//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include "tier2/tier2.h"
#include "tier0/memdbgon.h"
#include "mathlib/ssemath.h"

#ifdef _X360
#include "xbox/xbox_console.h"
#endif



#define PROBLEM_SIZE 1000
#define N_ITERS 100000
//#define RECORD_OUTPUT


static FourVectors g_XYZ[PROBLEM_SIZE];
static fltx4 g_CreationTime[PROBLEM_SIZE];



bool SIMDTest()
{
	const Vector StartPnt(0,0,0);
	const Vector MidP(0,0,100);
	const Vector EndPnt(100,0,50);

	// This app doesn't go through regular engine init, so init FPU/VPU math behaviour here:
	SetupFPUControlWord();
	TestVPUFlags();

	// Initialize g_XYZ[] and g_CreationTime[]
	SeedRandSIMD(1987301);
	for (int i = 0;i < PROBLEM_SIZE;i++)
	{
		float  fourStartTimes[4];
		Vector fourPoints[4];
		Vector offset;
		for (int j = 0;j < 4;j++)
		{
			float           t = (j + 4 * i) / (4.0f * (PROBLEM_SIZE - 1));
			fourStartTimes[j] = t;
			fourPoints[j]     = StartPnt + t*( EndPnt - StartPnt );
			offset.Random( -10.0f, +10.0f );
			fourPoints[j]    += offset;
		}
		g_XYZ[i].LoadAndSwizzle( fourPoints[0], fourPoints[1], fourPoints[2], fourPoints[3] );
		g_CreationTime[i] = LoadUnalignedSIMD( fourStartTimes );
	}

#ifdef RECORD_OUTPUT
	char outputBuffer[1024];
	Q_snprintf( outputBuffer, sizeof( outputBuffer ), "float testOutput[%d][4][3] = {\n", N_ITERS );
	Warning(outputBuffer);
#endif // RECORD_OUTPUT

	double STime=Plat_FloatTime();
	bool bChangedSomething = false;
	for(int i=0;i<N_ITERS;i++)
	{
		float t=i*(1.0/N_ITERS);
		FourVectors * __restrict pXYZ = g_XYZ;

		fltx4 * __restrict pCreationTime = g_CreationTime;

		fltx4 CurTime   = ReplicateX4( t );
		fltx4 TimeScale = ReplicateX4( 1.0/(max(0.001,  1.0 ) ) );

		// calculate radius spline
		bool bConstantRadius = true;
		fltx4 Rad0=ReplicateX4(2.0);
		fltx4 Radm=Rad0;
		fltx4 Rad1=Rad0;
	
		fltx4 RadmMinusRad0=SubSIMD( Radm, Rad0);
		fltx4 Rad1MinusRadm=SubSIMD( Rad1, Radm);
		
		fltx4 SIMDMinDist=ReplicateX4( 2.0 );
		fltx4 SIMDMinDist2=ReplicateX4( 2.0*2.0 );
		
		fltx4 SIMDMaxDist=MaxSIMD( Rad0, MaxSIMD( Radm, Rad1 ) );
		fltx4 SIMDMaxDist2=MulSIMD( SIMDMaxDist, SIMDMaxDist);
		

		FourVectors StartP;
		StartP.DuplicateVector( StartPnt );
		
		FourVectors MiddleP;
		MiddleP.DuplicateVector( MidP );
		
		// form delta terms needed for quadratic bezier
		FourVectors Delta0;
		Delta0.DuplicateVector( MidP-StartPnt );
		
		FourVectors Delta1;
		Delta1.DuplicateVector( EndPnt-MidP );
		int nLoopCtr = PROBLEM_SIZE;
		do
		{
			fltx4 TScale=MinSIMD(
				Four_Ones,
				MulSIMD( TimeScale, SubSIMD( CurTime, *pCreationTime ) ) );

			// bezier(a,b,c,t)=lerp( lerp(a,b,t),lerp(b,c,t),t)
			FourVectors L0 = Delta0;
			L0 *= TScale;
			L0 += StartP;
			
			FourVectors L1= Delta1;
			L1 *= TScale;
			L1 += MiddleP;
			
			FourVectors Center = L1;
			Center -= L0;
			Center *= TScale;
			Center += L0;

			FourVectors pts_original = *(pXYZ);
			FourVectors pts	= pts_original;
			pts -= Center;

			// calculate radius at the point. !!speed!! - use special case for constant radius
			
			fltx4 dist_squared= pts * pts;
			fltx4 TooFarMask = CmpGtSIMD( dist_squared, SIMDMaxDist2 );
			if ( ( !bConstantRadius) && ( ! IsAnyNegative( TooFarMask ) ) )
			{
				// need to calculate and adjust for true radius =- we've only trivially rejected note
				// voodoo here - we update simdmaxdist for true radius, but not max dist^2, since
				// that's used only for the trivial reject case, which we've already done
				fltx4 R0=AddSIMD( Rad0, MulSIMD( RadmMinusRad0, TScale ) );
				fltx4 R1=AddSIMD( Radm, MulSIMD( Rad1MinusRadm, TScale ) );
				SIMDMaxDist = AddSIMD( R0, MulSIMD( SubSIMD( R1, R0 ), TScale) );
				
				// now that we know the true radius, update our mask
				TooFarMask = CmpGtSIMD( dist_squared, MulSIMD( SIMDMaxDist, SIMDMaxDist ) );
			}

			fltx4 TooCloseMask = CmpLtSIMD( dist_squared, SIMDMinDist2 );
			fltx4 NeedAdjust = OrSIMD( TooFarMask, TooCloseMask );
			if ( IsAnyNegative( NeedAdjust ) )				// any out of bounds?
			{
				// change squared distance into approximate rsqr root
				fltx4 guess=ReciprocalSqrtEstSIMD(dist_squared);
				// newton iteration for 1/sqrt(x) : y(n+1)=1/2 (y(n)*(3-x*y(n)^2));
				guess=MulSIMD(guess,SubSIMD(Four_Threes,MulSIMD(dist_squared,MulSIMD(guess,guess))));
				guess=MulSIMD(Four_PointFives,guess);
				pts *= guess;
				
				FourVectors clamp_far=pts;
				clamp_far *= SIMDMaxDist;
				clamp_far += Center;
				FourVectors clamp_near=pts;
				clamp_near *= SIMDMinDist;
				clamp_near += Center;
				pts.x = MaskedAssign( TooCloseMask, clamp_near.x, MaskedAssign( TooFarMask, clamp_far.x, pts_original.x ));
				pts.y = MaskedAssign( TooCloseMask, clamp_near.y, MaskedAssign( TooFarMask, clamp_far.y, pts_original.y ));
				pts.z = MaskedAssign( TooCloseMask, clamp_near.z, MaskedAssign( TooFarMask, clamp_far.z, pts_original.z ));
				*(pXYZ) = pts;
				bChangedSomething = true;
			}

#ifdef RECORD_OUTPUT
			if (nLoopCtr == 257)
			{
				Q_snprintf(	outputBuffer, sizeof( outputBuffer ), "/*%04d:*/ { {%+14e,%+14e,%+14e}, {%+14e,%+14e,%+14e}, {%+14e,%+14e,%+14e}, {%+14e,%+14e,%+14e} },\n", i,
							pXYZ->X(0), pXYZ->Y(0), pXYZ->Z(0),
							pXYZ->X(1), pXYZ->Y(1), pXYZ->Z(1),
							pXYZ->X(2), pXYZ->Y(2), pXYZ->Z(2),
							pXYZ->X(3), pXYZ->Y(3), pXYZ->Z(3));
				Warning(outputBuffer);
			}
#endif // RECORD_OUTPUT

			++pXYZ;
			++pCreationTime;
		} while ( --nLoopCtr );
	}
	double ETime=Plat_FloatTime()-STime;

#ifdef RECORD_OUTPUT
	Q_snprintf(	outputBuffer, sizeof( outputBuffer ), "         };\n" );
	Warning(outputBuffer);
#endif // RECORD_OUTPUT

	printf("elapsed time=%f p/s=%f\n",ETime, (4.0*PROBLEM_SIZE*N_ITERS)/ETime );
	return bChangedSomething;
}


#ifdef _X360

__declspec(passinreg) struct float4
{
	operator __vector4 () const { return vmx; }
	__vector4 vmx;
};

void OctoberXDKCompilerIssueTestCode( const fltx4 & val, fltx4 * out )
{
	// UNDONE: This code demonstrates serious 360 compiler issues. XBox Developer Support has been contacted.
	//         The assembly contains tons of useless instructions (vector stores and supporting integer math), even in the
	//         below code - no use of pointers or static constants, no wrapper layers on top of the vector intrinsics.
	//         If/when the compiler issue is resolved, other known issues are:
	//          - pass vector params by const reference
	//          - avoid putting __vector4 in a union or an array
	//          - avoid default constructors, return constructed objects directly ("return VecClass(__vector4Val);")

#define DECL_ASS( _var_, _val_ )	fltx4  _var_ = _val_
//#define DECL_ASS( _var_, _val_ )	float4 _var_; _var_.vmx = _val_
//#define DECL_ASS( _var_, _val_ )	float4 _var_( _val_ )

	DECL_ASS( resultx, Four_Zeros ); DECL_ASS( resulty, Four_Zeros ); DECL_ASS( resultz, Four_Zeros );

	DECL_ASS( CurTime, __vmulfp( val, Four_PointFives ) );
	DECL_ASS( TimeScale, val );
	//fltx4 *pCreationTime = g_CreationTime;
	DECL_ASS( Delta0x, val ); DECL_ASS( Delta0y, val ); DECL_ASS( Delta0z, val );
	DECL_ASS( Delta1x,  __vaddfp(Delta0x, Delta0x) ); DECL_ASS( Delta1y,  __vaddfp(Delta0y, Delta0y) ); DECL_ASS( Delta1z,  __vaddfp(Delta0z, Delta0z) );
	DECL_ASS( StartPx,  __vaddfp(Delta0x, Delta0x) ); DECL_ASS( StartPy,  __vaddfp(Delta0y, Delta0y) ); DECL_ASS( StartPz,  __vaddfp(Delta0z, Delta0z) );
	DECL_ASS( MiddlePx, __vaddfp(StartPx, StartPx) ); DECL_ASS( MiddlePy, __vaddfp(StartPy, StartPy) ); DECL_ASS( MiddlePz, __vaddfp(StartPz, StartPz) );
	for (int i = 0;i < 1000;i++)
	{
		DECL_ASS( TScale, __vsubfp( CurTime, resultx ) );//*pCreationTime );
		TScale			= __vmulfp( TScale,  TimeScale );
		TScale			= __vminfp( TScale,  resulty );//Four_Ones );

		//resultx = __vaddfp( resultx, TScale );
		//resulty = __vaddfp( resulty, TScale );
		//resultz = __vaddfp( resultz, TScale );

		DECL_ASS( L0x, Delta0x ); DECL_ASS( L0y, Delta0y ); DECL_ASS( L0z, Delta0z );
		L0x = __vmulfp(L0x,TScale);   L0y = __vmulfp(L0y,TScale);   L0z = __vmulfp(L0z,TScale);
		L0x = __vaddfp(StartPx,L0x);  L0y = __vaddfp(StartPy,L0y);  L0z = __vaddfp(StartPz,L0z);

		DECL_ASS( L1x, Delta1x ); DECL_ASS( L1y, Delta1y ); DECL_ASS( L1z, Delta1z );
		L1x = __vmulfp(L1x,TScale);   L1y = __vmulfp(L1y,TScale);   L1z = __vmulfp(L1z,TScale);
		L1x = __vaddfp(MiddlePx,L1x); L1y = __vaddfp(MiddlePy,L1y); L1z = __vaddfp(MiddlePz,L1z);

		L0x = __vaddfp(L0x,L1x);      L0y = __vaddfp(L0y,L1y);      L0z = __vaddfp(L0z,L1z);

		resultx = __vaddfp( resultx, L0x );
		resulty = __vaddfp( resulty, L0y );
		resultz = __vaddfp( resultz, L0z );

		//pCreationTime++;
	}

	out[0] = resultx;
	out[1] = resulty;
	out[2] = resultz;
}

#else // _X360

void
SSEClassTest( const fltx4 & val, fltx4 & out )
{
	fltx4 result = Four_Zeros;
	for (int i = 0;i < N_ITERS;i++)
	{
		result = SubSIMD( val, result );
		result = MulSIMD( val, result );
		result = AddSIMD( val, result );
		result = MinSIMD( val, result );
	}
	FourVectors result4; result4.x = result; result4.y = result; result4.z = result;
	for (int i = 0;i < N_ITERS;i++)
	{
		result4 *= result4;
		result4 += result4;
		result4 *= result4;
		result4 += result4;
	}
	result = result4*result4;
	out = result;
}

#endif // !_X360


int main(int argc,char **argv)
{
#ifndef _X360

	// UNDONE: InitCommandLineProgram needs fixing for 360 (if we want to make lots of new 360 executables)
	InitCommandLineProgram( argc, argv );

	// This function is useful for inspecting compiler output
	fltx4 result;
	SSEClassTest( Four_PointFives, result );
	printf("(%f,%f,%f,%f)\n", SubFloat( result, 0 ), SubFloat( result, 1 ), SubFloat( result, 2 ), SubFloat( result, 3 ) );

#else // _X360

	// Wait for VXConsole, so that all debug output goes there
	XBX_InitConsoleMonitor(true);

	// This function is useful for inspecting compiler output
	FourVectors result;
	OctoberXDKCompilerIssueTestCode( Four_PointFives, (fltx4 *)&result );
	printf("(%f,%f,%f,%f)\n", result.X(0), result.X(1), result.X(2), result.X(3));
	printf("(%f,%f,%f,%f)\n", result.Y(0), result.Y(1), result.Y(2), result.Y(3));
	printf("(%f,%f,%f,%f)\n", result.Z(0), result.Z(1), result.Z(2), result.Z(3));

#endif // _X360

	// Run the perf. test
	SIMDTest();
	
	return 0;
}
