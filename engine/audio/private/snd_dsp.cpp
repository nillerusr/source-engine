//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// snd_dsp.c -- audio processing routines


#include "audio_pch.h"
#include "snd_mix_buf.h"

#include "iprediction.h"
#include "../../common.h"		// for parsing routines
#include "vstdlib/random.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SIGN(d)				((d)<0?-1:1)

#define ABS(a)	abs(a)

#define MSEC_TO_SAMPS(a)	(((a)*SOUND_DMA_SPEED) / 1000)		// convert milliseconds to # samples in equivalent time
#define SEC_TO_SAMPS(a)		((a)*SOUND_DMA_SPEED)				// convert seconds to # samples in equivalent time

// Suppress the noisy warnings caused by CLIP_DSP
#if defined(__clang__)
	#pragma GCC diagnostic ignored "-Wself-assign"
#endif
#define CLIP_DSP(x) (x)

extern ConVar das_debug;

#define SOUND_MS_PER_FT	1			// sound travels approx 1 foot per millisecond
#define ROOM_MAX_SIZE	1000		// max size in feet of room simulation for dsp

void DSP_ReleaseMemory( void );
bool DSP_LoadPresetFile( void );

extern float Gain_To_dB ( float gain );
extern float dB_To_Gain ( float dB );
extern float Gain_To_Amplitude ( float gain );
extern float Amplitude_To_Gain ( float amplitude );

extern bool g_bdas_room_init;
extern bool g_bdas_init_nodes;

//===============================================================================
//
// Digital Signal Processing algorithms for audio FX.
//
// KellyB 2/18/03
//===============================================================================

// Performance notes:

// DSP processing should take no more than 3ms total time per frame to remain on par with hl1
// Assume a min frame rate of 24fps = 42ms per frame
// at 24fps, to maintain 44.1khz output rate, we must process about 1840 mono samples per frame.
// So we must process 1840 samples in 3ms.

// on a 1Ghz CPU (mid-low end CPU) 3ms provides roughly 3,000,000 cycles.
// Thus we have 3e6 / 1840 = 1630 cycles per sample.  

#define PBITS			12					// parameter bits
#define PMAX			((1 << PBITS))		// parameter max

// crossfade from y2 to y1 at point r (0 < r < PMAX)

#define XFADE(y1,y2,r)  ((y2) + ( ( ((y1) - (y2)) * (r) ) >> PBITS) )

// exponential crossfade from y2 to y1 at point r (0 < r < PMAX)

#define XFADE_EXP(y1, y2, r)	((y2) + ((((((y1) - (y2)) * (r) ) >> PBITS)	* (r)) >> PBITS) )

/////////////////////
// dsp helpers
/////////////////////

// reverse delay pointer

inline void DlyPtrReverse (int dlysize, int *psamps, int **ppsamp)
{
	// when *ppsamp = psamps - 1, it wraps around to *ppsamp = psamps + dlysize

	if ( *ppsamp < psamps )
		*ppsamp += dlysize + 1;		
}

// advance delay pointer

inline void DlyPtrForward (int dlysize, int *psamps, int **ppsamp)
{
	// when *ppsamp = psamps + dlysize + 1, it wraps around to *ppsamp = psamps

	if ( *ppsamp > psamps + dlysize )
		*ppsamp -= dlysize + 1;		
}

// Infinite Impulse Response (feedback) filter, cannonical form

//  returns single sample 'out' for current input value 'in'
//  in:				input sample
//	psamp:			internal state array, dimension max(cdenom,cnumer) + 1
//  cnumer,cdenom:	numerator and denominator filter orders
//  denom,numer:	cdenom+1 dimensional arrays of filter params
// 
//  for cdenom = 4:
//
//                1   psamp0(n)     numer0	 
// in(n)--->(+)--(*)---.------(*)---->(+)---> out(n)
//           ^         |               ^
//           |     [Delay d]           |
//           |         |               |
//           | -denom1 |psamp1 numer1  |
//			 ----(*)---.------(*)-------
//           ^         |               ^
//           |     [Delay d]           |
//           |         |               |
//           | -denom2 |psamp2 numer2  |
//			 ----(*)---.------(*)-------
//           ^         |               ^
//           |     [Delay d]           |
//           |         |               |
//           | -denom3 |psamp3 numer3  |
//			 ----(*)---.------(*)-------
//           ^         |               ^
//           |     [Delay d]           |
//           |         |               |
//           | -denom4 |psamp4 numer4  |
//			 ----(*)---.------(*)-------
//
//	for each input sample in:
//			psamp0 = in - denom1*psamp1 - denom2*psamp2 - ... 
//			out = numer0*psamp0 + numer1*psamp1 + ...
//			psampi = psampi-1, i = cmax, cmax-1, ..., 1

inline int IIRFilter_Update_OrderN ( int cdenom, int *denom, int cnumer, int *numer, int *psamp, int in )
{
	int cmax, i;
	int out;
	int in0;					
	
	out = 0;
	in0 = in;

	cmax = max ( cdenom, cnumer );				
	
	// add input values

	// for (i = 1; i <= cdenom; i++)		
	//	psamp[0] -= ( denom[i] * psamp[i] ) >> PBITS;

		switch (cdenom)
	{
	case 12: in0 -= ( denom[12] * psamp[12] ) >> PBITS;
	case 11: in0 -= ( denom[11] * psamp[11] ) >> PBITS;
	case 10: in0 -= ( denom[10] * psamp[10] ) >> PBITS;
	case 9:  in0 -= ( denom[9] * psamp[9] ) >> PBITS;
	case 8:  in0 -= ( denom[8] * psamp[8] ) >> PBITS;
	case 7:  in0 -= ( denom[7] * psamp[7] ) >> PBITS;
	case 6:  in0 -= ( denom[6] * psamp[6] ) >> PBITS;
	case 5:  in0 -= ( denom[5] * psamp[5] ) >> PBITS;
	case 4:  in0 -= ( denom[4] * psamp[4] ) >> PBITS;
	case 3:  in0 -= ( denom[3] * psamp[3] ) >> PBITS;
	case 2:  in0 -= ( denom[2] * psamp[2] ) >> PBITS;
	default:
	case 1:  in0 -= ( denom[1] * psamp[1] ) >> PBITS;
	}

	psamp[0] = in0;
	
	// add output values

	//for (i = 0; i <= cnumer; i++)		
	//	out += ( numer[i] * psamp[i] ) >> PBITS;

	switch (cnumer)
	{
	case 12: out += ( numer[12] * psamp[12] ) >> PBITS;
	case 11: out += ( numer[11] * psamp[11] ) >> PBITS;
	case 10: out += ( numer[10] * psamp[10] ) >> PBITS;
	case 9:  out += ( numer[9] * psamp[9] ) >> PBITS;
	case 8:  out += ( numer[8] * psamp[8] ) >> PBITS;
	case 7:  out += ( numer[7] * psamp[7] ) >> PBITS;
	case 6:  out += ( numer[6] * psamp[6] ) >> PBITS;
	case 5:  out += ( numer[5] * psamp[5] ) >> PBITS;
	case 4:  out += ( numer[4] * psamp[4] ) >> PBITS;
	case 3:  out += ( numer[3] * psamp[3] ) >> PBITS;
	case 2:  out += ( numer[2] * psamp[2] ) >> PBITS;
	default:
	case 1:  out += ( numer[1] * psamp[1] ) >> PBITS;
	case 0:  out += ( numer[0] * psamp[0] ) >> PBITS;
	}
	
	// update internal state (reverse order)

	for (i = cmax; i >= 1; i--)		
		psamp[i] = psamp[i-1];

	// return current output sample

	return out;						
}

// 1st order filter - faster version

inline int IIRFilter_Update_Order1 ( int *denom, int cnumer, int *numer, int *psamp, int in )
{
	int out;					
	
	if (!psamp[0] && !psamp[1] && !in)
		return 0;

	psamp[0] = in - (( denom[1] * psamp[1] ) >> PBITS);

	out = ( ( numer[1] * psamp[1] ) + ( numer[0] * psamp[0] ) ) >> PBITS;

	psamp[1] = psamp[0];

	return out;	
}

// return 'tdelay' delayed sample from delay buffer
// dlysize:		delay samples
// psamps:		head of delay buffer psamps[0...dlysize]
// psamp:		current data pointer
// sdly:		0...dlysize

inline int GetDly ( int dlysize, int *psamps, int *psamp, int tdelay )
{		
	int *pout;

	pout = psamp + tdelay;
	
	if ( pout <= (psamps + dlysize))
		return *pout;
	else
		return *(pout - dlysize - 1);
}

// update the delay buffer pointer
// dlysize:		delay samples
// psamps:		head of delay buffer psamps[0...dlysize]
// ppsamp:		data pointer

inline void DlyUpdate ( int dlysize, int *psamps, int **ppsamp )
{
	// decrement pointer and fix up on buffer boundary

	// when *ppsamp = psamps-1, it wraps around to *ppsamp = psamps+dlysize

	(*ppsamp)--;										
	DlyPtrReverse ( dlysize, psamps, ppsamp );		
}

// simple delay with feedback, no filter in feedback line.
// delaysize:	delay line size in samples
// tdelay:		tap from this location - <= delaysize
// psamps:		delay line buffer pointer of dimension delaysize+1
// ppsamp:		circular pointer, must be init to &psamps[0] before first call
// fbgain:		feedback value, 0-PMAX (normalized to 0.0-1.0)
// outgain:		gain
// in:	input sample

//                    psamps0(n)  outgain	 
// in(n)--->(+)--------.-----(*)-> out(n)
//           ^         |            
//           |     [Delay d]        
//           |         |            
//           | fbgain  |Wd(n)       
//			 ----(*)---.

inline int ReverbSimple ( int delaysize, int tdelay, int *psamps, int **ppsamp, int fbgain, int outgain, int in )
{
	int out, sD;

	// get current delay output

	sD = GetDly ( delaysize, psamps, *ppsamp, tdelay );	

	// calculate output + delay * gain	

	out = in + (( fbgain * sD ) >> PBITS);					
	
	// write to delay

	**ppsamp = out;										

	// advance internal delay pointers

	DlyUpdate ( delaysize, psamps, ppsamp );			

	return ( (out * outgain) >> PBITS );
}

inline int ReverbSimple_xfade ( int delaysize, int tdelay, int tdelaynew, int xf, int *psamps, int **ppsamp, int fbgain, int outgain, int in )
{
	int out, sD;
	int sDnew;

	// crossfade from tdelay to tdelaynew samples. xfade is 0..PMAX

	sD = GetDly ( delaysize, psamps, *ppsamp, tdelay );		
	sDnew = GetDly ( delaysize, psamps, *ppsamp, tdelaynew );
	sD = sD + (((sDnew - sD) * xf) >> PBITS); 

	out = in + (( fbgain * sD ) >> PBITS);			
	**ppsamp = out;									
	DlyUpdate ( delaysize, psamps, ppsamp );					

	return ( (out * outgain) >> PBITS );
}

// multitap simple reverb

// NOTE: tdelay3 > tdelay2 > tdelay1 > t0
// NOTE: fbgain * 4 < 1!

inline int ReverbSimple_multitap ( int delaysize, int tdelay0, int tdelay1, int tdelay2, int tdelay3, int *psamps, int **ppsamp, int fbgain, int outgain, int in )
{
	int s1, s2, s3, s4, sum;
	
	s1 = GetDly ( delaysize, psamps, *ppsamp, tdelay0 );
	s2 = GetDly ( delaysize, psamps, *ppsamp, tdelay1 );
	s3 = GetDly ( delaysize, psamps, *ppsamp, tdelay2 );
	s4 = GetDly ( delaysize, psamps, *ppsamp, tdelay3 );

	sum = s1 + s2 + s3 + s4;

	// write to delay

	**ppsamp = in + ((s4 * fbgain) >> PBITS);
	
	// update delay pointers

	DlyUpdate ( delaysize, psamps, ppsamp );		

	return ( ((sum + in) * outgain ) >> PBITS );
}

// modulate smallest tap delay only

inline int ReverbSimple_multitap_xfade ( int delaysize, int tdelay0, int tdelaynew, int xf, int tdelay1, int tdelay2, int tdelay3, int *psamps, int **ppsamp, int fbgain, int outgain, int in )
{
	int s1, s2, s3, s4, sum;
	int sD, sDnew;
	
	// crossfade from tdelay to tdelaynew tap. xfade is 0..PMAX

	sD	  = GetDly ( delaysize, psamps, *ppsamp, tdelay3 );	
	sDnew = GetDly ( delaysize, psamps, *ppsamp, tdelaynew );

	s4 = sD + (((sDnew - sD) * xf) >> PBITS);

	s1 = GetDly ( delaysize, psamps, *ppsamp, tdelay0 );
	s2 = GetDly ( delaysize, psamps, *ppsamp, tdelay1 );
	s3 = GetDly ( delaysize, psamps, *ppsamp, tdelay2 );

	sum = s1 + s2 + s3 + s4;

	// write to delay

	**ppsamp = in + ((s4 * fbgain) >> PBITS);		

	// update delay pointers

	DlyUpdate ( delaysize, psamps, ppsamp );		

	return ( ((sum + in) * outgain ) >> PBITS );
}

// straight delay, no feedback
//
// delaysize:	 delay line size in samples
// tdelay:		 tap from this location - <= delaysize
// psamps:		 delay line buffer pointer of dimension delaysize+1
// ppsamp:		 circular pointer, must be init to &psamps[0] before first call
// in:			 input sample
//                    
//  in(n)--->[Delay d]---> out(n)
//
 
inline int DelayLinear ( int delaysize, int tdelay, int *psamps, int **ppsamp, int in )
{
	int out;

	out = GetDly ( delaysize, psamps, *ppsamp, tdelay );		

	**ppsamp = in;							

	DlyUpdate ( delaysize, psamps, ppsamp );				

	return ( out );
}

// crossfade delay values from tdelay to tdelaynew, with xfade1 for tdelay and xfade2 for tdelaynew. xfade = 0...PMAX

inline int DelayLinear_xfade ( int delaysize, int tdelay, int tdelaynew, int xf, int *psamps, int **ppsamp, int in )
{
	int out;
	int outnew;

	out = GetDly ( delaysize, psamps, *ppsamp, tdelay );
	
	outnew = GetDly ( delaysize, psamps, *ppsamp, tdelaynew );

	out = out + (((outnew - out) * xf) >> PBITS); 	

	**ppsamp = in;
	
	DlyUpdate ( delaysize, psamps, ppsamp );					

	return ( out );
}

// lowpass reverberator, replace feedback multiplier 'fbgain' in 
// reverberator with a low pass filter

// delaysize:	delay line size in samples
// tdelay:		tap from this location - <= delaysize
// psamps:		delay line buffer pointer of dimension delaysize+1
// ppsamp:		circular pointer, must be init to &w[0] before first call
// fbgain:		feedback gain (built into filter gain)
// outgain:		output gain
// cnumer:		filter order
// numer:		filter numerator, 0-PMAX (normalized to 0.0-1.0), cnumer+1 dimensional
// denom:		filter denominator, 0-PMAX (normalized to 0.0-1.0), cnumer+1 dimensional
// pfsamps:		filter state, cnumer+1 dimensional
// in:			input sample

//            psamps0(n)   	   outgain
// in(n)--->(+)--------------.----(*)--> out(n)
//           ^               |            
//           |           [Delay d]        
//           |               |            
//           |  fbgain       |Wd(n)       
//			 --(*)--[Filter])-

inline int DelayLowpass ( int delaysize, int tdelay, int *psamps, int **ppsamp, int fbgain, int outgain, int *denom, int Ll, int *numer, int *pfsamps, int in )
{
	int out, sD;

	// delay output is filter input

	sD = GetDly ( delaysize, psamps, *ppsamp, tdelay );						

	// filter output, with feedback 'fbgain' baked into filter params

	out = in + IIRFilter_Update_Order1 ( denom, Ll, numer, pfsamps, sD );
	
	// write to delay

	**ppsamp = out;	
	
	// update delay pointers

	DlyUpdate ( delaysize, psamps, ppsamp );									

	// output with gain

	return ( (out * outgain) >> PBITS );									
}

inline int DelayLowpass_xfade ( int delaysize, int tdelay, int tdelaynew, int xf, int *psamps, int **ppsamp, int fbgain, int outgain, int *denom, int Ll, int *numer, int *pfsamps, int in )
{
	int out, sD;
	int sDnew;

	// crossfade from tdelay to tdelaynew tap. xfade is 0..PMAX

	sD = GetDly ( delaysize, psamps, *ppsamp, tdelay );						
	sDnew = GetDly ( delaysize, psamps, *ppsamp, tdelaynew );
	sD = sD + (((sDnew - sD) * xf) >> PBITS);

	// filter output with feedback 'fbgain' baked into filter params

	out = in + IIRFilter_Update_Order1 ( denom, Ll, numer, pfsamps, sD );		

	// write to delay

	**ppsamp = out;															

	// update delay ptrs

	DlyUpdate ( delaysize, psamps, ppsamp );								

	// output with gain

	return ( (out * outgain) >> PBITS ); 
}

// delay is multitap tdelay0,tdelay1,tdelay2,tdelay3

// NOTE: tdelay3 > tdelay2 > tdelay1 > tdelay0
// NOTE: fbgain * 4 < 1!

inline int DelayLowpass_multitap ( int delaysize, int tdelay0, int tdelay1, int tdelay2, int tdelay3, int *psamps, int **ppsamp, int fbgain, int outgain, int *denom, int Ll, int *numer, int *pfsamps, int in )
{
	int s0, s1, s2, s3, s4, sum;
	
	s1 = GetDly ( delaysize, psamps, *ppsamp, tdelay0 );
	s2 = GetDly ( delaysize, psamps, *ppsamp, tdelay1 );
	s3 = GetDly ( delaysize, psamps, *ppsamp, tdelay2 );
	s4 = GetDly ( delaysize, psamps, *ppsamp, tdelay3 );

	sum = s1 + s2 + s3 + s4;

	s0 = in + IIRFilter_Update_Order1 ( denom, Ll, numer, pfsamps, s4 );
	
	// write to delay

	**ppsamp = s0;									

	// update delay ptrs

	DlyUpdate ( delaysize, psamps, ppsamp );		

	return ( ((sum + in) * outgain ) >> PBITS );
}

inline int DelayLowpass_multitap_xfade ( int delaysize, int tdelay0, int tdelaynew, int xf, int tdelay1, int tdelay2, int tdelay3, int *psamps, int **ppsamp, int fbgain, int outgain, int *denom, int Ll, int *numer, int *pfsamps, int in )
{
	int s0, s1, s2, s3, s4, sum;

	int sD, sDnew;

	// crossfade from tdelay to tdelaynew tap. xfade is 0..PMAX

	sD = GetDly ( delaysize, psamps, *ppsamp, tdelay3 );	
	sDnew = GetDly ( delaysize, psamps, *ppsamp, tdelaynew );
	
	s4 = sD + (((sDnew - sD) * xf) >> PBITS);

	s1 = GetDly ( delaysize, psamps, *ppsamp, tdelay0 );
	s2 = GetDly ( delaysize, psamps, *ppsamp, tdelay1 );
	s3 = GetDly ( delaysize, psamps, *ppsamp, tdelay2 );
	
	sum = s1 + s2 + s3 + s4;

	s0 = in + IIRFilter_Update_Order1 ( denom, Ll, numer, pfsamps, s4 );
	
	**ppsamp = s0;							
	DlyUpdate ( delaysize, psamps, ppsamp );			

	return ( ((sum + in) * outgain ) >> PBITS );
}

// linear delay with lowpass filter on delay output and gain stage
// delaysize:	delay line size in samples
// tdelay:		delay tap from this location - <= delaysize
// psamps:		delay line buffer pointer of dimension delaysize+1
// ppsamp:		circular pointer, must init &psamps[0] before first call
// fbgain:		feedback gain (ignored)
// outgain:		output gain
// cnumer:		filter order
// numer:		filter numerator, 0-PMAX (normalized to 0.0-1.0), cnumer+1 dimensional
// denom:		filter denominator, 0-PMAX (normalized to 0.0-1.0), cnumer+1 dimensional
// pfsamps:		filter state, cnumer+1 dimensional
// in:			input sample

//  in(n)--->[Delay d]--->[Filter]-->(*outgain)---> out(n)

inline int DelayLinear_lowpass ( int delaysize, int tdelay, int *psamps, int **ppsamp, int fbgain, int outgain, int *denom, int cnumer, int *numer, int *pfsamps, int in )
{
	int out, sD;
	
	// delay output is filter input

	sD = GetDly ( delaysize, psamps, *ppsamp, tdelay );					

	// calc filter output

	out = IIRFilter_Update_Order1 ( denom, cnumer, numer, pfsamps, sD );	

	// input sample to delay input

	**ppsamp = in;															

	// update delay pointers

	DlyUpdate ( delaysize, psamps, ppsamp );								

	// output with gain

	return ( (out * outgain) >> PBITS );								
}

inline int DelayLinear_lowpass_xfade ( int delaysize, int tdelay, int tdelaynew, int xf, int *psamps, int **ppsamp, int fbgain, int outgain, int *denom, int cnumer, int *numer, int *pfsamps, int in )
{
	int out, sD;
	int sDnew;

	// crossfade from tdelay to tdelaynew tap. xfade is 0..PMAX

	sD = GetDly ( delaysize, psamps, *ppsamp, tdelay );				
	sDnew = GetDly ( delaysize, psamps, *ppsamp, tdelaynew );
	sD = sD + (((sDnew - sD) * xf) >> PBITS);

	out = IIRFilter_Update_Order1 ( denom, cnumer, numer, pfsamps, sD );

	**ppsamp = in;														

	DlyUpdate ( delaysize, psamps, ppsamp );							

	return ( (out * outgain) >> PBITS );								
}


// classic allpass reverb
// delaysize:	delay line size in samples
// tdelay:		tap from this location - <= D
// psamps:		delay line buffer pointer of dimension delaysize+1
// ppsamp:		circular pointer, must be init to &psamps[0] before first call
// fbgain:		feedback value, 0-PMAX (normalized to 0.0-1.0)
// outgain:		gain

//                    psamps0(n)  -fbgain outgain
//  in(n)--->(+)--------.-----(*)-->(+)--(*)-> out(n)
//           ^         |            ^
//           |     [Delay d]        |
//           |         |            |
//           | fbgain  |psampsd(n)  |
//			 ----(*)---.-------------
//
//	for each input sample 'in':
//		psamps0 = in + fbgain * psampsd
//		y = -fbgain * psamps0 + psampsd
//		delay (d, psamps) - psamps is the delay buffer array
//
// or, using circular delay, for each input sample 'in':
//
//		Sd = GetDly (delaysize,psamps,ppsamp,delaysize)
//		S0 = in + fbgain*Sd
//		y = -fbgain*S0 + Sd
//		*ppsamp = S0
//		DlyUpdate(delaysize, psamps, &ppsamp)		

inline int DelayAllpass ( int delaysize, int tdelay, int *psamps, int **ppsamp, int fbgain, int outgain, int in ) 
{
	int out, s0, sD;
	
	sD = GetDly ( delaysize, psamps, *ppsamp, tdelay );			
	s0 = in + (( fbgain * sD ) >> PBITS);

	out = ( ( -fbgain * s0 ) >> PBITS ) + sD;		
	**ppsamp = s0;									
	DlyUpdate ( delaysize, psamps, ppsamp );		

	return ( (out * outgain) >> PBITS );
}


inline int DelayAllpass_xfade ( int delaysize, int tdelay, int tdelaynew, int xf, int *psamps, int **ppsamp, int fbgain, int outgain, int in ) 
{
	int out, s0, sD;
	int sDnew;

	// crossfade from t to tnew tap. xfade is 0..PMAX

	sD = GetDly ( delaysize, psamps, *ppsamp, tdelay );	
	sDnew = GetDly ( delaysize, psamps, *ppsamp, tdelaynew );
	sD = sD + (((sDnew - sD) * xf) >> PBITS);

	s0 = in + (( fbgain * sD ) >> PBITS);

	out = ( ( -fbgain * s0 ) >> PBITS ) + sD;	
	**ppsamp = s0;								
	DlyUpdate ( delaysize, psamps, ppsamp );			

	return ( (out * outgain) >> PBITS );
}

///////////////////////////////////////////////////////////////////////////////////
// fixed point math for real-time wave table traversing, pitch shifting, resampling
///////////////////////////////////////////////////////////////////////////////////

#define FIX20_BITS			20									// 20 bits of fractional part
#define FIX20_SCALE			(1 << FIX20_BITS)

#define FIX20_INTMAX		((1 << (32 - FIX20_BITS))-1)		// maximum step integer

#define FLOAT_TO_FIX20(a)	((int)((a) * (float)FIX20_SCALE))		// convert float to fixed point
#define INT_TO_FIX20(a)		(((int)(a)) << FIX20_BITS)			// convert int to fixed point
#define FIX20_TO_FLOAT(a)	((float)(a) / (float)FIX20_SCALE)	// convert fix20 to float
#define FIX20_INTPART(a)	(((int)(a)) >> FIX20_BITS)			// get integer part of fixed point
#define FIX20_FRACPART(a)	((a) - (((a) >> FIX20_BITS) << FIX20_BITS))	// get fractional part of fixed point

#define FIX20_FRACTION(a,b)	(FIX(a)/(b))						// convert int a to fixed point, divide by b

typedef int fix20int;

/////////////////////////////////
// DSP processor parameter block
/////////////////////////////////

// NOTE: these prototypes must match the XXX_Params ( prc_t *pprc ) and XXX_GetNext ( XXX_t *p, int x ) functions

typedef void * (*prc_Param_t)( void *pprc );					// individual processor allocation functions
typedef int (*prc_GetNext_t) ( void *pdata, int x );			// get next function for processor
typedef int (*prc_GetNextN_t) ( void *pdata,  portable_samplepair_t *pbuffer, int SampleCount, int op);	// batch version of getnext
typedef void (*prc_Free_t) ( void *pdata );						// free function for processor
typedef void (*prc_Mod_t) (void *pdata, float v);				// modulation function for processor	

#define	OP_LEFT				0		// batch process left channel in place
#define OP_RIGHT			1		// batch process right channel in place
#define OP_LEFT_DUPLICATE	2		// batch process left channel in place, duplicate to right channel

#define PRC_NULL			0		// pass through - must be 0
#define PRC_DLY				1		// simple feedback reverb
#define PRC_RVA				2		// parallel reverbs
#define PRC_FLT				3		// lowpass or highpass filter
#define PRC_CRS				4		// chorus
#define	PRC_PTC				5		// pitch shifter
#define PRC_ENV				6		// adsr envelope
#define PRC_LFO				7		// lfo
#define PRC_EFO				8		// envelope follower
#define PRC_MDY				9		// mod delay
#define PRC_DFR				10		// diffusor - n series allpass delays
#define PRC_AMP				11		// amplifier with distortion

#define QUA_LO				0		// quality of filter or reverb.  Must be 0,1,2,3.
#define QUA_MED				1
#define QUA_HI				2
#define QUA_VHI				3
#define QUA_MAX				QUA_VHI

#define CPRCPARAMS			16		// up to 16 floating point params for each processor type

// processor definition - one for each running instance of a dsp processor

struct prc_t
{
	int type;					// PRC type

	float prm[CPRCPARAMS];		// dsp processor parameters - array of floats

	prc_Param_t pfnParam;		// allocation function - takes ptr to prc, returns ptr to specialized data struct for proc type
	prc_GetNext_t pfnGetNext;	// get next function
	prc_GetNextN_t pfnGetNextN;	// batch version of get next
	prc_Free_t pfnFree;			// free function
	prc_Mod_t pfnMod;			// modulation function

	void *pdata;				// processor state data - ie: pdly, pflt etc.
};

// processor parameter ranges - for validating parameters during allocation of new processor

typedef struct prm_rng_t
{
	int iprm;		// parameter index
	float lo;		// min value of parameter
	float hi;		// max value of parameter
} prm_rng_s;

void PRC_CheckParams ( prc_t *pprc, prm_rng_t *prng );

///////////
// Filters
///////////

#define CFLTS	64			// max number of filters simultaneously active
#define FLT_M	12			// max order of any filter

#define FLT_LP	0			// lowpass filter
#define FLT_HP	1			// highpass filter
#define FLT_BP	2			// bandpass filter
#define FTR_MAX	FLT_BP

// flt parameters

struct flt_t
{
	bool fused;				// true if slot in use

	int b[FLT_M+1];			// filter numerator parameters  (convert 0.0-1.0 to 0-PMAX representation)
	int a[FLT_M+1];			// filter denominator parameters (convert 0.0-1.0 to 0-PMAX representation)
	int w[FLT_M+1];			// filter state - samples (dimension of max (M, L))
	int L;					// filter order numerator (dimension of a[M+1])
	int M;					// filter order denominator (dimension of b[L+1])
	int N;					// # of series sections - 1 (0 = 1 section, 1 = 2 sections etc)

	flt_t *pf1;				// series cascaded versions of filter
	flt_t *pf2;				
	flt_t *pf3;
};

// flt flts

flt_t flts[CFLTS];

void FLT_Init ( flt_t *pf ) { if ( pf ) Q_memset ( pf, 0, sizeof (flt_t) ); }
void FLT_InitAll ( void ) {	for ( int i = 0 ; i < CFLTS; i++ ) FLT_Init ( &flts[i] ); }

void FLT_Free ( flt_t *pf ) 
{
	if ( pf )	
	{
		if (pf->pf1)
			Q_memset ( pf->pf1, 0, sizeof (flt_t) );	
		
		if (pf->pf2)
			Q_memset ( pf->pf2, 0, sizeof (flt_t) );	
		
		if (pf->pf3)
			Q_memset ( pf->pf3, 0, sizeof (flt_t) );	
		
		Q_memset ( pf, 0, sizeof (flt_t) );	
	}
}

void FLT_FreeAll ( void ) {	for (int i = 0 ; i < CFLTS; i++) FLT_Free ( &flts[i] ); }


// find a free filter from the filter pool
// initialize filter numerator, denominator b[0..M], a[0..L]
// gain scales filter numerator
// N is # of series sections - 1

flt_t * FLT_Alloc ( int N, int M, int L, int *a, int *b, float gain )
{
	int i, j;
	flt_t *pf = NULL;
	
	for (i = 0; i < CFLTS; i++)
	{
		if ( !flts[i].fused )
			{
			pf = &flts[i];

			// transfer filter params into filter struct
			pf->M = M;
			pf->L = L;
			pf->N = N;

			for (j = 0; j <= M; j++)
				pf->a[j] = a[j];

			for (j = 0; j <= L; j++)
				pf->b[j] = (int)((float)(b[j]) * gain);

			pf->pf1 = NULL;
			pf->pf2 = NULL;
			pf->pf3 = NULL;

			pf->fused = true;
			break;
			}
	}

	Assert(pf);	// make sure we're not trying to alloc more than CFLTS flts

	return pf;
}

// convert filter params cutoff and type into
// iir transfer function params M, L, a[], b[]

// iir filter, 1st order, transfer function is H(z) = b0 + b1 Z^-1  /  a0 + a1 Z^-1
// or H(z) = b0 - b1 Z^-1 / a0 + a1 Z^-1 for lowpass

// design cutoff filter at 3db (.5 gain) p579

void FLT_Design_3db_IIR ( float cutoff, float ftype, int *pM, int *pL, int *a, int *b )
{
	// ftype: FLT_LP, FLT_HP, FLT_BP
	
	double Wc = 2.0 * M_PI * cutoff / SOUND_DMA_SPEED;			// radians per sample
	double Oc;
	double fa;
	double fb; 

	// calculations:
	// Wc = 2pi * fc/44100								convert to radians
	// Oc = tan (Wc/2) * Gc / sqt ( 1 - Gc^2)			get analog version, low pass
	// Oc = tan (Wc/2) * (sqt (1 - Gc^2)) / Gc			analog version, high pass
	// Gc = 10 ^ (-Ac/20)								gain at cutoff.  Ac = 3db, so Gc^2 = 0.5
	// a = ( 1 - Oc ) / ( 1 + Oc )
	// b = ( 1 - a ) / 2

	Oc = tan ( Wc / 2.0 );

	fa = ( 1.0 - Oc ) / ( 1.0 + Oc );

	fb = ( 1.0 - fa ) / 2.0;

	if ( ftype == FLT_HP )
		fb = ( 1.0 + fa ) / 2.0;

	a[0] = 0;						// a0 always ignored
	a[1] = (int)( -fa * PMAX );		// quantize params down to 0-PMAX >> PBITS
	b[0] = (int)( fb * PMAX );
	b[1] = b[0];
	
	if ( ftype == FLT_HP )
		b[1] = -b[1];

	*pM = *pL = 1;

	return;
}

// filter parameter order
	
typedef enum
{
	flt_iftype,				
	flt_icutoff,		
	flt_iqwidth,		
	flt_iquality,			
	flt_igain,
	
	flt_cparam				// # of params
} flt_e;

// filter parameter ranges

prm_rng_t flt_rng[] = {

	{flt_cparam,	0, 0},			// first entry is # of parameters

	{flt_iftype,	0, FTR_MAX},	// filter type FLT_LP, FLT_HP, FLT_BP
	{flt_icutoff,	10, 22050},		// cutoff frequency in hz at -3db gain
	{flt_iqwidth,	0, 11025},		// width of BP (cut in starts at cutoff)
	{flt_iquality,	0, QUA_MAX},	// QUA_LO, _MED, _HI, _VHI = # of series sections
	{flt_igain,		0.0, 10.0},		// output gain 0-10.0
};


// convert prc float params to iir filter params, alloc filter and return ptr to it
// filter quality set by prc quality - 0,1,2

flt_t * FLT_Params ( prc_t *pprc )
{
	float qual		= pprc->prm[flt_iquality];
	float cutoff	= pprc->prm[flt_icutoff];
	float ftype		= pprc->prm[flt_iftype];
	float qwidth	= pprc->prm[flt_iqwidth];
	float gain		= pprc->prm[flt_igain];

	int L = 0;					// numerator order
	int M = 0;					// denominator order
	int b[FLT_M+1];				// numerator params	 0..PMAX
	int b_scaled[FLT_M+1];		// gain scaled numerator
	int a[FLT_M+1];				// denominator params 0..PMAX

	int L_bp = 0;				// bandpass numerator order
	int M_bp = 0;				// bandpass denominator order
	int b_bp[FLT_M+1];			// bandpass numerator params	 0..PMAX
	int b_bp_scaled[FLT_M+1];	// gain scaled numerator
	int a_bp[FLT_M+1];			// bandpass denominator params 0..PMAX

	int N;						// # of series sections
	bool bpass = false;

	// if qwidth > 0 then alloc bandpass filter (pf is lowpass)

	if ( qwidth > 0.0 )
		bpass = true;
	
	if (bpass)
	{
		ftype = FLT_LP;
	}

	// low pass and highpass filter design 
	
	//	1st order IIR filter, 3db cutoff at fc

	if ( bpass )
	{
		// highpass section

		FLT_Design_3db_IIR ( cutoff, FLT_HP, &M_bp, &L_bp, a_bp, b_bp );
		M_bp = clamp (M_bp, 1, FLT_M);
		L_bp = clamp (L_bp, 1, FLT_M);
		cutoff += qwidth;
	}
		
	// lowpass section

	FLT_Design_3db_IIR ( cutoff, (int)ftype, &M, &L, a, b );
			
	M = clamp (M, 1, FLT_M);
	L = clamp (L, 1, FLT_M);

	// quality = # of series sections - 1

	N = clamp ((int)qual, 0, 3);	

	// make sure we alloc at least 2 filters

	if (bpass)
		N = max(N, 1);

	flt_t *pf0 = NULL;
	flt_t *pf1 = NULL;
	flt_t *pf2 = NULL;
	flt_t *pf3 = NULL;
	
	// scale b numerators with gain - only scale for first filter if series filters

	for (int i = 0; i < FLT_M; i++)
	{
		b_bp_scaled[i] = (int)((float)(b_bp[i]) * gain );
		b_scaled[i] = (int)((float)(b[i]) * gain );
	}

	if (bpass)
	{
		// 1st filter is lowpass

		pf0 = FLT_Alloc ( N, M_bp, L_bp, a_bp, b_bp_scaled, 1.0 );
	}
	else
	{
		pf0 = FLT_Alloc ( N, M, L, a, b_scaled, 1.0 );
	}

	// allocate series filters

	if (pf0)
	{
		switch (N)
		{
		case 3:
			// alloc last filter as lowpass also if FLT_BP
			if (bpass)
				pf3 = FLT_Alloc ( 0, M_bp, L_bp, a_bp, b_bp, 1.0 );
			else
				pf3 = FLT_Alloc ( 0, M, L, a, b, 1.0 );
		case 2:
			pf2 = FLT_Alloc ( 0, M, L, a, b, 1.0 );
		case 1:
			pf1 = FLT_Alloc ( 0, M, L, a, b, 1.0 );
		case 0:
			break;
		}
		
		pf0->pf1 = pf1;
		pf0->pf2 = pf2;
		pf0->pf3 = pf3;
	}

	return pf0;
}

inline void * FLT_VParams ( void *p ) 
{
	PRC_CheckParams( (prc_t *)p, flt_rng);
	return (void *) FLT_Params ((prc_t *)p); 
}

inline void FLT_Mod ( void *p, float v ) { return; }

// get next filter value for filter pf and input x

inline int FLT_GetNext ( flt_t *pf, int  x )
{
	flt_t *pf1;
	flt_t *pf2;
	flt_t *pf3;
	int y;

	switch( pf->N )
	{
	default:
	case 0:
		return IIRFilter_Update_Order1(pf->a, pf->L, pf->b, pf->w, x);
	case 1:
		pf1 = pf->pf1;

		y =		IIRFilter_Update_Order1(pf->a, pf->L, pf->b, pf->w, x);	
		return	IIRFilter_Update_Order1(pf1->a, pf1->L, pf1->b, pf1->w, y);
	case 2:
		pf1 = pf->pf1;
		pf2 = pf->pf2;
		
		y =		IIRFilter_Update_Order1(pf->a, pf->L, pf->b, pf->w, x);
		y =		IIRFilter_Update_Order1(pf1->a, pf1->L, pf1->b, pf1->w, y);
		return	IIRFilter_Update_Order1(pf2->a, pf2->L, pf2->b, pf2->w, y);
	case 3:
		pf1 = pf->pf1;
		pf2 = pf->pf2;
		pf3 = pf->pf3;

		y =		IIRFilter_Update_Order1(pf->a, pf->L, pf->b, pf->w, x);
		y =		IIRFilter_Update_Order1(pf1->a, pf1->L, pf1->b, pf1->w, y);
		y =		IIRFilter_Update_Order1(pf2->a, pf2->L, pf2->b, pf2->w, y);
		return	IIRFilter_Update_Order1(pf3->a, pf3->L, pf3->b, pf3->w, y);
	}
}

// batch version for performance

inline void FLT_GetNextN( flt_t *pflt, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = FLT_GetNext( pflt, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = FLT_GetNext( pflt, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = FLT_GetNext( pflt, pb->left );
			pb++;
		}
		return;
	}
}

///////////////////////////////////////////////////////////////////////////
// Positional updaters for pitch shift etc
///////////////////////////////////////////////////////////////////////////

// looping position within a wav, with integer and fractional parts
// used for pitch shifting, upsampling/downsampling
// 20 bits of fraction, 8+ bits of integer

struct pos_t
{

	fix20int step;	// wave table whole and fractional step value
	fix20int cstep;	// current cummulative step value
	int pos;		// current position within wav table
	
	int D;			// max dimension of array w[0...D] ie: # of samples = D+1
};

// circular wrap of pointer p, relative to array w
// D max buffer index w[0...D] (count of samples in buffer is D+1)
// i circular index

inline void POS_Wrap ( int D, int *i )
{
	if ( *i > D )
		*i -= D + 1;		// when *pi = D + 1, it wraps around to *pi = 0
	
	if ( *i < 0 )
		*i += D + 1;		// when *pi = - 1, it wraps around to *pi = D
}

// set initial update value - fstep can have no more than 8 bits of integer and 20 bits of fract
// D is array max dimension w[0...D] (ie: size D+1)
// w is ptr to array
// p is ptr to pos_t to initialize

inline void POS_Init( pos_t *p, int D, float fstep )
{
	float step = fstep;

	// make sure int part of step is capped at fix20_intmax

	if ((int)step > FIX20_INTMAX)
		step = (step - (int)step) + FIX20_INTMAX;

	p->step = FLOAT_TO_FIX20(step);	// convert fstep to fixed point
	p->cstep = 0;			
	p->pos = 0;							// current update value

	p->D = D;							// always init to end value, in case we're stepping backwards
}

// change step value - this is an instantaneous change, not smoothed.

inline void POS_ChangeVal( pos_t *p, float fstepnew )
{
	p->step = FLOAT_TO_FIX20( fstepnew );	// convert fstep to fixed point
}

// return current integer position, then update internal position value

inline int POS_GetNext ( pos_t *p )
{
	
	//float f = FIX20_TO_FLOAT(p->cstep);
	//int i1 = FIX20_INTPART(p->cstep);
	//float f1 = FIX20_TO_FLOAT(FIX20_FRACPART(p->cstep));
	//float f2 = FIX20_TO_FLOAT(p->step);

	p->cstep += p->step;						// update accumulated fraction step value (fixed point)
	p->pos += FIX20_INTPART( p->cstep );		// update pos with integer part of accumulated step
	p->cstep = FIX20_FRACPART( p->cstep );		// throw away the integer part of accumulated step

	// wrap pos around either end of buffer if needed

	POS_Wrap(p->D, &(p->pos));

	// make sure returned position is within array bounds

	Assert (p->pos <= p->D);

	return p->pos;
}

// oneshot position within wav 
struct pos_one_t
{
	pos_t p;				// pos_t

	bool fhitend;			// flag indicating we hit end of oneshot wav
};

// set initial update value - fstep can have no more than 8 bits of integer and 20 bits of fract
// one shot position - play only once, don't wrap, when hit end of buffer, return last position

inline void POS_ONE_Init( pos_one_t *p1, int D, float fstep )
{
	POS_Init( &p1->p, D, fstep ) ;
	
	p1->fhitend = false;
}

// return current integer position, then update internal position value

inline int POS_ONE_GetNext ( pos_one_t *p1 )
{
	int pos;
	pos_t *p0;

	pos = p1->p.pos;							// return current position
	
	if (p1->fhitend)
		return pos;

	p0 = &(p1->p);
	p0->cstep += p0->step;						// update accumulated fraction step value (fixed point)
	p0->pos += FIX20_INTPART( p0->cstep );		// update pos with integer part of accumulated step
	//p0->cstep = SIGN(p0->cstep) * FIX20_FRACPART( p0->cstep );
	p0->cstep = FIX20_FRACPART( p0->cstep );		// throw away the integer part of accumulated step

	// if we wrapped, stop updating, always return last position
	// if step value is 0, return hit end

	if (!p0->step || p0->pos < 0 || p0->pos >= p0->D )
		p1->fhitend = true;
	else
		pos = p0->pos;

	// make sure returned value is within array bounds

	Assert ( pos <= p0->D );

	return pos;
}


/////////////////////
// Reverbs and delays
/////////////////////

#define CDLYS			128				// max delay lines active. Also used for lfos.

#define DLY_PLAIN			0				// single feedback loop
#define DLY_ALLPASS			1				// feedback and feedforward loop - flat frequency response (diffusor)
#define DLY_LOWPASS			2				// lowpass filter in feedback loop
#define DLY_LINEAR			3				// linear delay, no feedback, unity gain
#define DLY_FLINEAR			4				// linear delay with lowpass filter and output gain
#define DLY_LOWPASS_4TAP	5				// lowpass filter in feedback loop, 4 delay taps
#define DLY_PLAIN_4TAP		6				// single feedback loop, 4 delay taps

#define DLY_MAX			DLY_PLAIN_4TAP

#define DLY_HAS_MULTITAP(a)	((a) == DLY_LOWPASS_4TAP || (a) == DLY_PLAIN_4TAP)
#define DLY_HAS_FILTER(a)	((a) == DLY_FLINEAR || (a) == DLY_LOWPASS || (a) == DLY_LOWPASS_4TAP)

#define DLY_TAP_FEEDBACK_GAIN 0.25		// drop multitap feedback to compensate for sum of taps in dly_*multitap()

#define DLY_NORMALIZING_REDUCTION_MAX	0.25	// don't reduce gain (due to feedback) below N% of original gain

// delay line 

struct dly_t
{
	
	bool fused;						// true if dly is in use
	int type;						// delay type

	int D;							// delay size, in samples
	int t;							// current tap, <= D
	int tnew;						// crossfading to tnew
	int xf;							// crossfade value of t		(0..PMAX)
	int t1,t2,t3;					// additional taps for multi-tap delays
	int a1,a2,a3;					// feedback values for taps
	int D0;							// original delay size (only relevant if calling DLY_ChangeVal)
	int *p;							// circular buffer pointer
	int *w;							// array of samples

	int a;							// feedback value 0..PMAX,normalized to 0-1.0
	int b;							// gain value 0..PMAX, normalized to 0-1.0

	flt_t *pflt;					// pointer to filter, if type DLY_LOWPASS
};

dly_t dlys[CDLYS];					// delay lines

void DLY_Init ( dly_t *pdly ) {	if ( pdly )	Q_memset( pdly, 0, sizeof (dly_t)); }
void DLY_InitAll ( void ) {	for (int i = 0 ; i < CDLYS; i++) DLY_Init ( &dlys[i] ); }
void DLY_Free ( dly_t *pdly )
{
	// free memory buffer

	if ( pdly )
	{
		FLT_Free ( pdly->pflt );

		if ( pdly->w )
		{
			delete[] pdly->w;
		}
		
		// free dly slot

		Q_memset ( pdly, 0, sizeof (dly_t) );
	}
}


void DLY_FreeAll ( void ) {	for (int i = 0; i < CDLYS; i++ ) DLY_Free ( &dlys[i] ); }

// return adjusted feedback value for given dly
// such that decay time is same as that for dmin and fbmin

// dmin - minimum delay
// fbmin - minimum feedback
// dly - delay to match decay to dmin, fbmin

float DLY_NormalizeFeedback ( int dmin, float fbmin, int dly )
{
	// minimum decay time T to -60db for a simple reverb is:

	//		Tmin = (ln 10^-3 / Ln fbmin) * (Dmin / fs)

	// where fs = sample frequency

	// similarly,
	
	//		Tdly = (ln 10^-3 / Ln fb) * (D / fs)

	// setting Tdly = Tmin and solving for fb gives:

	//		D / Dmin = ln fb / ln fbmin

	// since y^x = z gives x = ln z / ln y

	//		fb = fbmin ^ (D/Dmin)

	float fb = powf (fbmin, (float)dly / (float) dmin);

	return fb;
}

// set up 'b' gain parameter of feedback delay to
// compensate for gain caused by feedback 'fb'.  

void DLY_SetNormalizingGain ( dly_t *pdly, int feedback )
{
	// compute normalized gain, set as output gain

	// calculate gain of delay line with feedback, and use it to
	// reduce output.  ie: force delay line with feedback to unity gain

	// for constant input x with feedback fb:

	// out = x + x*fb + x * fb^2 + x * fb^3...
	// gain = out/x
	// so gain = 1 + fb + fb^2 + fb^3...
	// which, by the miracle of geometric series, equates to 1/1-fb
	// thus, gain = 1/(1-fb)
	
	float fgain = 0;
	float gain;
	int b;
	float fb = (float)feedback;

	fb = fb / (float)PMAX;
	fb = fpmin(fb, 0.999f);
	
	// if b is 0, set b to PMAX (1)

	b = pdly->b ? pdly->b : PMAX;

	fgain = 1.0 / (1.0 - fb);

	// compensating gain -  multiply rva output by gain then >> PBITS

	gain = (int)((1.0 / fgain) * PMAX);	

	gain = gain * 4;	// compensate for fact that gain calculation is for +/- 32767 amplitude wavs
						// ie: ok to allow a bit more gain because most wavs are not at theoretical peak amplitude at all times

	// limit gain reduction to N% PMAX

	gain = clamp (gain, (float)(PMAX * DLY_NORMALIZING_REDUCTION_MAX), (float)PMAX);		
	
	gain = ((float)b/(float)PMAX) * gain;	// scale final gain by pdly->b.

	pdly->b = (int)gain;
}

void DLY_ChangeTaps ( dly_t *pdly, int t0, int t1, int t2, int t3 );

// allocate a new delay line
// D number of samples to delay
// a feedback value (0-PMAX normalized to 0.0-1.0)
// b gain value (0-PMAX normalized to 0.0-1.0) - this is folded into the filter fb params
// if DLY_LOWPASS or DLY_FLINEAR:
//		L - numerator order of filter
//		M - denominator order of filter
//		fb - numerator params, M+1 
//		fa - denominator params, L+1

dly_t * DLY_AllocLP ( int D, int a, int b, int type, int M, int L, int *fa, int *fb )
{
	int *w;
	int i;
	dly_t *pdly = NULL;
	int feedback;

	// find open slot

	for (i = 0; i < CDLYS; i++)
	{
		if (!dlys[i].fused)
		{
			pdly = &dlys[i];
			DLY_Init( pdly );
			break;
		}
	}
	
	if ( i == CDLYS )
	{
		DevMsg ("DSP: Warning, failed to allocate delay line.\n" );
		return NULL;					// all delay lines in use
	}

	// save original feedback value

	feedback = a;

	// adjust feedback a, gain b if delay is multitap unit

	if ( DLY_HAS_MULTITAP(type) )
	{
		// split output gain over 4 taps
	
		b = (int)((float)(b) * DLY_TAP_FEEDBACK_GAIN);
	}

	if ( DLY_HAS_FILTER(type) )
	{
		// alloc lowpass iir_filter
		// delay feedback gain is built into filter gain
	
		float gain = (float)a / (float)(PMAX);
		
		pdly->pflt = FLT_Alloc( 0, M, L, fa, fb, gain );
		if ( !pdly->pflt )
		{
			DevMsg ("DSP: Warning, failed to allocate filter for delay line.\n" );
			return NULL;
		}	
	}

	// alloc delay memory
	w = new int[D+1];
	if ( !w )
	{ 
		Warning( "Sound DSP: Failed to lock.\n");
		FLT_Free ( pdly->pflt );
		return NULL; 
	}
	
	// clear delay array

	Q_memset (w, 0, sizeof(int) * (D+1));
	
	// init values

	pdly->type = type;
	pdly->D = D;
	pdly->t = D;		// set delay tap to full delay
	pdly->tnew = D;
	pdly->xf = 0;
	pdly->D0 = D;
	pdly->p = w;		// init circular pointer to head of buffer
	pdly->w = w;
	pdly->a = min( a, PMAX - 1 );		// do not allow 100% feedback
	pdly->b = b;
	pdly->fused = true;

	if ( type == DLY_LINEAR || type == DLY_FLINEAR ) 
	{
		// linear delay has no feedback and unity gain

		pdly->a = 0;
		pdly->b = PMAX;
	}
	else
	{
		// adjust b to compensate for feedback gain of steady state max input

		DLY_SetNormalizingGain( pdly, feedback );	
	}

	if ( DLY_HAS_MULTITAP(type) )
	{
		// initially set up all taps to same value - caller uses DLY_ChangeTaps to change values

		DLY_ChangeTaps( pdly, D, D, D, D );
	}

	return (pdly);
}

// allocate lowpass or allpass delay

dly_t * DLY_Alloc( int D, int a, int b, int type )
{
	return DLY_AllocLP( D, a, b, type, 0, 0, 0, 0 );
}


// Allocate new delay, convert from float params in prc preset to internal parameters
// Uses filter params in prc if delay is type lowpass

// delay parameter order

typedef enum {

 dly_idtype,		// NOTE: first 8 params must match those in mdy_e
 dly_idelay,		
 dly_ifeedback,		
 dly_igain,		

 dly_iftype,		
 dly_icutoff,	
 dly_iqwidth,		
 dly_iquality, 

 dly_itap1,
 dly_itap2,
 dly_itap3,

 dly_cparam

} dly_e;


// delay parameter ranges

prm_rng_t dly_rng[] = {

	{dly_cparam,	0, 0},			// first entry is # of parameters
		
	// delay params

	{dly_idtype,	0, DLY_MAX},		// delay type DLY_PLAIN, DLY_LOWPASS, DLY_ALLPASS etc	
	{dly_idelay,	-1.0, 1000.0},		// delay in milliseconds (-1 forces auto dsp to set delay value from room size)
	{dly_ifeedback,	0.0, 0.99},			// feedback 0-1.0
	{dly_igain,	    0.0, 10.0},			// final gain of output stage, 0-10.0 

	// filter params if dly type DLY_LOWPASS or DLY_FLINEAR

	{dly_iftype,	0, FTR_MAX},			
	{dly_icutoff,	10.0, 22050.0},
	{dly_iqwidth,	100.0, 11025.0},
	{dly_iquality,	0, QUA_MAX},
										// note: -1 flag tells auto dsp to get value directly from room size
	{dly_itap1,	-1.0, 1000.0},			// delay in milliseconds NOTE: delay > tap3 > tap2 > tap1
	{dly_itap2,	-1.0, 1000.0},			// delay in milliseconds
	{dly_itap3,	-1.0, 1000.0},			// delay in milliseconds
};

dly_t * DLY_Params ( prc_t *pprc )
{
	dly_t *pdly = NULL;
	int D, a, b;
	
	float delay		= fabs(pprc->prm[dly_idelay]);
	float feedback	= pprc->prm[dly_ifeedback];
	float gain		= pprc->prm[dly_igain];
	int type		= pprc->prm[dly_idtype];

	float ftype 	= pprc->prm[dly_iftype];
	float cutoff	= pprc->prm[dly_icutoff];
	float qwidth	= pprc->prm[dly_iqwidth];
	float qual		= pprc->prm[dly_iquality];

	float t1		= fabs(pprc->prm[dly_itap1]);
	float t2		= fabs(pprc->prm[dly_itap2]);
	float t3		= fabs(pprc->prm[dly_itap3]);

	D = MSEC_TO_SAMPS(delay);					// delay samples
	a = feedback * PMAX;						// feedback
	b = gain * PMAX;							// gain
	
	switch ( (int) type )
	{
	case DLY_PLAIN:
	case DLY_PLAIN_4TAP:
	case DLY_ALLPASS:
	case DLY_LINEAR:
		pdly = DLY_Alloc( D, a, b, type );
		break;

	case DLY_FLINEAR:
	case DLY_LOWPASS: 
	case DLY_LOWPASS_4TAP:
		{
		// set up dummy lowpass filter to convert params

		prc_t prcf;

		prcf.prm[flt_iquality]	= qual;	// 0,1,2 -  (0 or 1 low quality implies faster execution time)
		prcf.prm[flt_icutoff]	= cutoff;
		prcf.prm[flt_iftype]	= ftype;
		prcf.prm[flt_iqwidth]	= qwidth;
		prcf.prm[flt_igain]		= 1.0;

		flt_t *pflt = (flt_t *)FLT_Params ( &prcf );
		
		if ( !pflt )
		{
			DevMsg ("DSP: Warning, failed to allocate filter.\n" );
			return NULL;
		}

		pdly = DLY_AllocLP ( D, a, b, type, pflt->M, pflt->L, pflt->a, pflt->b );
		
		FLT_Free ( pflt );
		break;
		}
	}

	// set up multi-tap delays

	if ( pdly && DLY_HAS_MULTITAP((int)type) )
		DLY_ChangeTaps( pdly, D, MSEC_TO_SAMPS(t1), MSEC_TO_SAMPS(t2), MSEC_TO_SAMPS(t3) );

	return pdly;
}

inline void * DLY_VParams ( void *p ) 
{
	PRC_CheckParams( (prc_t *)p, dly_rng );
	return (void *) DLY_Params ((prc_t *)p); 
}

// get next value from delay line, move x into delay line

inline int DLY_GetNext ( dly_t *pdly, int x )
{
	switch (pdly->type)
	{
	default:
	case DLY_PLAIN:
		return ReverbSimple( pdly->D, pdly->t, pdly->w, &pdly->p, pdly->a, pdly->b, x );
	case DLY_ALLPASS:
		return DelayAllpass( pdly->D, pdly->t, pdly->w, &pdly->p, pdly->a, pdly->b, x );
	case DLY_LOWPASS:
		return DelayLowpass( pdly->D, pdly->t, pdly->w, &(pdly->p), pdly->a, pdly->b, pdly->pflt->a, pdly->pflt->L, pdly->pflt->b, pdly->pflt->w, x );
	case DLY_LINEAR:
		return DelayLinear( pdly->D, pdly->t, pdly->w, &pdly->p, x );
	case DLY_FLINEAR:
		return DelayLinear_lowpass( pdly->D, pdly->t, pdly->w, &(pdly->p), pdly->a, pdly->b, pdly->pflt->a, pdly->pflt->L, pdly->pflt->b, pdly->pflt->w, x );
	case DLY_PLAIN_4TAP:
		return ReverbSimple_multitap( pdly->D, pdly->t, pdly->t1, pdly->t2, pdly->t3, pdly->w, &pdly->p, pdly->a, pdly->b, x );
	case DLY_LOWPASS_4TAP:
		return DelayLowpass_multitap( pdly->D, pdly->t, pdly->t1, pdly->t2,pdly->t3, pdly->w, &(pdly->p), pdly->a, pdly->b, pdly->pflt->a, pdly->pflt->L, pdly->pflt->b, pdly->pflt->w, x );
	}		
}

inline int DLY_GetNextXfade ( dly_t *pdly, int x )
{

	switch (pdly->type)
	{
	default:
	case DLY_PLAIN:
		return ReverbSimple_xfade( pdly->D, pdly->t, pdly->tnew, pdly->xf, pdly->w, &pdly->p, pdly->a, pdly->b, x );
	case DLY_ALLPASS:
		return DelayAllpass_xfade( pdly->D, pdly->t, pdly->tnew, pdly->xf, pdly->w, &pdly->p, pdly->a, pdly->b, x );
	case DLY_LOWPASS:
		return DelayLowpass_xfade( pdly->D, pdly->t, pdly->tnew, pdly->xf, pdly->w, &(pdly->p), pdly->a, pdly->b, pdly->pflt->a, pdly->pflt->L, pdly->pflt->b, pdly->pflt->w, x );
	case DLY_LINEAR:
		return DelayLinear_xfade( pdly->D, pdly->t, pdly->tnew, pdly->xf, pdly->w, &pdly->p, x );
	case DLY_FLINEAR:
		return DelayLinear_lowpass_xfade( pdly->D, pdly->t, pdly->tnew, pdly->xf, pdly->w, &(pdly->p), pdly->a, pdly->b, pdly->pflt->a, pdly->pflt->L, pdly->pflt->b, pdly->pflt->w, x );
	case DLY_PLAIN_4TAP:
		return ReverbSimple_multitap_xfade( pdly->D, pdly->t, pdly->tnew, pdly->xf, pdly->t1, pdly->t2, pdly->t3, pdly->w, &pdly->p, pdly->a, pdly->b, x );
	case DLY_LOWPASS_4TAP:
		return DelayLowpass_multitap_xfade( pdly->D, pdly->t, pdly->tnew, pdly->xf, pdly->t1, pdly->t2, pdly->t3, pdly->w, &(pdly->p), pdly->a, pdly->b, pdly->pflt->a, pdly->pflt->L, pdly->pflt->b, pdly->pflt->w, x );
	}		
}

// batch version for performance
// UNDONE: a) unwind this more - pb increments by 2 to avoid pb->left or pb->right deref.
// UNDONE: b) all filter and delay params are dereferenced outside of DLY_GetNext and passed as register values
// UNDONE: c) pull case statement in dly_getnext out, so loop directly calls the inline dly_*() routine.

inline void DLY_GetNextN( dly_t *pdly, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = DLY_GetNext( pdly, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = DLY_GetNext( pdly, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = DLY_GetNext( pdly, pb->left );
			pb++;
		}
		return;
	}
}

// get tap on t'th sample in delay - don't update buffer pointers, this is done via DLY_GetNext
// Only valid for DLY_LINEAR.

inline int DLY_GetTap ( dly_t *pdly, int t )
{
	return GetDly (pdly->D, pdly->w, pdly->p, t ); 
}

#define SWAP(a,b,t)				{(t) = (a); (a) = (b); (b) = (t);}

// make instantaneous change to tap values t0..t3
// all values of t must be less than original delay D
// only processed for DLY_LOWPASS_4TAP & DLY_PLAIN_4TAP
// NOTE: pdly->a feedback must have been set before this call!
void DLY_ChangeTaps ( dly_t *pdly, int t0, int t1, int t2, int t3 )
{
	if (!pdly)
		return;
	
	int temp;

	// sort taps to make sure t3 > t2 > t1 > t0 !

	for (int i = 0; i < 4; i++)
	{
		if (t0 > t1) SWAP(t0, t1, temp);
		if (t1 > t2) SWAP(t1, t2, temp);
		if (t2 > t3) SWAP(t2, t3, temp);
	}

	pdly->t		= min ( t0, pdly->D0 );
	pdly->t1	= min ( t1, pdly->D0 );
	pdly->t2	= min ( t2, pdly->D0 );
	pdly->t3	= min ( t3, pdly->D0 );

}

// make instantaneous change for first delay tap 't' to new delay value.
// t tap value must be <= original D (ie: we don't do any reallocation here)

void DLY_ChangeVal ( dly_t *pdly, int t )
{
	// never set delay > original delay

	pdly->t = min ( t, pdly->D0 );
}

// ignored - use MDY_ for modulatable delay

inline void DLY_Mod ( void *p, float v ) { return; }


/////////////////////////////////////////////////////////////////////////////
// Ramp - used for varying smoothly between int parameters ie: modulation delays
/////////////////////////////////////////////////////////////////////////////


struct rmp_t
{
	int initval;					// initial ramp value
	int target;						// final ramp value
	int sign;						// increasing (1) or decreasing (-1) ramp
	
	int yprev;						// previous output value
	bool fhitend;					// true if hit end of ramp
	bool bEndAtTime;				// if true, fhitend is true when ramp time is hit (even if target not hit)
									// if false, then fhitend is true only when target is hit
	pos_one_t ps;					// current ramp output
};

// ramp smoothly between initial value and target value in approx 'ramptime' seconds.
// (initial value may be greater or less than target value)
// never changes output by more than +1 or -1 (which can cause the ramp to take longer to complete than ramptime - see bEndAtTime)
// called once per sample while ramping
// ramptime - duration of ramp in seconds
// initval - initial ramp value
// targetval - target ramp value
// if bEndAtTime is true, then RMP_HitEnd returns true when ramp time is reached, EVEN IF TARGETVAL IS NOT REACHED
// if bEndAtTime is false, then RMP_HitEnd returns true when targetval is reached, EVEN IF DELTA IN RAMP VALUES IS > +/- 1

void RMP_Init( rmp_t *prmp, float ramptime, int initval, int targetval, bool bEndAtTime ) 
{
	int rise;
	int run;

	if (prmp)
		Q_memset( prmp, 0, sizeof (rmp_t) ); 
	else
		return;
			
	run = (int) (ramptime * SOUND_DMA_SPEED);		// 'samples' in ramp
	rise = (targetval - initval);					// height of ramp

	// init fixed point iterator to iterate along the height of the ramp 'rise'
	// always iterates from 0..'rise', increasing in value

	POS_ONE_Init( &prmp->ps, ABS( rise ), ABS((float) rise) / ((float) run) );
	
	prmp->yprev = initval;
	prmp->initval = initval;
	prmp->target = targetval;
	prmp->sign = SIGN( rise );
	prmp->bEndAtTime = bEndAtTime;

}

// continues from current position to new target position

void RMP_SetNext( rmp_t *prmp, float ramptime, int targetval )
{
	RMP_Init ( prmp, ramptime, prmp->yprev, targetval, prmp->bEndAtTime );
}

inline bool RMP_HitEnd ( rmp_t *prmp )
{
	return prmp->fhitend;
}

inline void RMP_SetEnd ( rmp_t *prmp )
{
	prmp->fhitend = true;
}

// get next ramp value & update ramp, if bEndAtTime is true, never varies by more than +1 or -1 between calls
// when ramp hits target value, it thereafter always returns last value

inline int RMP_GetNext( rmp_t *prmp ) 
{
	int y;
	int d;
	
	// if we hit ramp end, return last value

	if (prmp->fhitend)
		return prmp->yprev;
	
	// get next integer position in ramp height. 

	d = POS_ONE_GetNext( &prmp->ps );
	
	if ( prmp->ps.fhitend )
		prmp->fhitend = true;

	// increase or decrease from initval, depending on ramp sign

	if ( prmp->sign > 0 )
		y = prmp->initval + d;
	else
		y = prmp->initval - d;

	// if bEndAtTime is true, only update current height by a max of +1 or -1
	// this also means that for short ramp times, we may not hit target
	
	if (prmp->bEndAtTime)
	{
		if ( ABS( y - prmp->yprev ) >= 1 )
			prmp->yprev += prmp->sign;
	}
	else
	{
		// always hits target - but varies by more than +/- 1

		prmp->yprev = y;
	}

	return prmp->yprev;
}

// get current ramp value, don't update ramp

inline int RMP_GetCurrent( rmp_t *prmp )
{
	return prmp->yprev;
}


//////////////
// mod delay
//////////////

// modulate delay time anywhere from 0..D using MDY_ChangeVal. no output glitches (uses RMP)

#define CMDYS				64				// max # of mod delays active (steals from delays)

struct mdy_t
{
	bool fused;

	bool fchanging;			// true if modulating to new delay value

	dly_t *pdly;			// delay
	
	float ramptime;			// ramp 'glide' time - time in seconds to change between values

	int mtime;				// time in samples between delay changes. 0 implies no self-modulating
	int mtimecur;			// current time in samples until next delay change
	float depth;			// modulate delay from D to D - (D*depth)  depth 0-1.0

	int mix;				// PMAX as % processed fx signal mix
	
	rmp_t rmp_interp;		// interpolation ramp 0...PMAX

	bool bPhaseInvert;		// if true, invert phase of output

};

mdy_t mdys[CMDYS];

void MDY_Init( mdy_t *pmdy ) { if (pmdy) Q_memset( pmdy, 0, sizeof (mdy_t) ); };
void MDY_Free( mdy_t *pmdy ) { if (pmdy) { DLY_Free (pmdy->pdly); Q_memset( pmdy, 0, sizeof (mdy_t) ); } };
void MDY_InitAll() { for (int i = 0; i < CMDYS; i++) MDY_Init( &mdys[i] ); };
void MDY_FreeAll() { for (int i = 0; i < CMDYS; i++) MDY_Free( &mdys[i] ); };


// allocate mod delay, given previously allocated dly (NOTE: mod delay only sweeps tap 0, not t1,t2 or t3)
// ramptime is time in seconds for delay to change from dcur to dnew
// modtime is time in seconds between modulations. 0 if no self-modulation
// depth is 0-1.0 multiplier, new delay values when modulating are Dnew = randomlong (D - D*depth, D)
// mix - 0-1.0, default 1.0 for 100% fx mix - pans between input signal and fx signal

mdy_t *MDY_Alloc ( dly_t *pdly, float ramptime, float modtime, float depth, float mix )
{
	int i;
	mdy_t *pmdy;

	if ( !pdly )
		return NULL;

	for (i = 0; i < CMDYS; i++)
	{
		if ( !mdys[i].fused )
		{
			pmdy = &mdys[i];

			MDY_Init ( pmdy );

			pmdy->pdly = pdly;

			if ( !pmdy->pdly )
			{
				DevMsg ("DSP: Warning, failed to allocate delay for mod delay.\n" );
				return NULL;
			}

			pmdy->fused = true;
			pmdy->ramptime = ramptime;
			pmdy->mtime = SEC_TO_SAMPS(modtime);
			pmdy->mtimecur = pmdy->mtime;
			pmdy->depth = depth;
			pmdy->mix = int ( PMAX * mix );
			pmdy->bPhaseInvert = false;

			return pmdy;
		}
	}

	DevMsg ("DSP: Warning, failed to allocate mod delay.\n" );
	return NULL;
}

// change to new delay tap value t samples, ramp linearly over ramptime seconds

void MDY_ChangeVal ( mdy_t *pmdy, int t )
{
	// if D > original delay value, cap at original value

	t = min (pmdy->pdly->D0, t);
	
	pmdy->fchanging = true;

	// init interpolation ramp - always hit target

	RMP_Init ( &pmdy->rmp_interp, pmdy->ramptime, 0, PMAX, false );

	// init delay xfade values

	pmdy->pdly->tnew = t;
	pmdy->pdly->xf = 0;
}

// interpolate between current and target delay values

inline int MDY_GetNext( mdy_t *pmdy, int x )
{
	int xout;
	
	if ( !pmdy->fchanging )
	{
		// not modulating...

		xout = DLY_GetNext( pmdy->pdly, x );

		if ( !pmdy->mtime )
		{
			// return right away if not modulating (not changing and not self modulating)

			goto mdy_return;
		}
	}
	else
	{
		// modulating...

		xout = DLY_GetNextXfade( pmdy->pdly, x );
	
		// get xfade ramp & set up delay xfade value for next call to DLY_GetNextXfade()

		pmdy->pdly->xf = RMP_GetNext( &pmdy->rmp_interp ); // 0...PMAX

		if ( RMP_HitEnd( &pmdy->rmp_interp ) )
		{
			// done. set delay tap & value = target

			DLY_ChangeVal( pmdy->pdly, pmdy->pdly->tnew );

			pmdy->pdly->t = pmdy->pdly->tnew;

			pmdy->fchanging = false;
		}
	}
	
	// if self-modulating and timer has expired, get next change

	if ( pmdy->mtime && !pmdy->mtimecur-- )
	{
		pmdy->mtimecur = pmdy->mtime;

		int D0 = pmdy->pdly->D0;
		int Dnew;
		float D1;

		// modulate between 0 and 100% of d0

		D1 = (float)D0 * (1.0 - pmdy->depth);

		Dnew = RandomInt( (int)D1, D0 );

		// set up modulation to new value

		MDY_ChangeVal ( pmdy, Dnew );
	}

mdy_return:

	// reverse phase of output

	if ( pmdy->bPhaseInvert )
		xout = -xout;

	// 100% fx mix

	if ( pmdy->mix == PMAX)
		return xout;
	
	// special case 50/50 mix

	if ( pmdy->mix == PMAX / 2)
		return ( (xout + x) >> 1 );

	// return mix of input and processed signal

	return ( x + (((xout - x) * pmdy->mix) >> PBITS) );
}


// batch version for performance
// UNDONE: unwind MDY_GetNext so that it directly calls DLY_GetNextN: 
// UNDONE: a) if not currently modulating and never self-modulating, then just unwind like DLY_GetNext
// UNDONE: b) if not currently modulating, figure out how many samples N until self-modulation timer kicks in again
//			  and stream out N samples just like DLY_GetNext

inline void MDY_GetNextN( mdy_t *pmdy, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = MDY_GetNext( pmdy, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = MDY_GetNext( pmdy, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = MDY_GetNext( pmdy, pb->left );
			pb++;
		}
		return;
	}
}

// parameter order

typedef enum {

	 mdy_idtype,			// NOTE: first 8 params must match params in dly_e
	 mdy_idelay,		
	 mdy_ifeedback,		
	 mdy_igain,		

	 mdy_iftype,
	 mdy_icutoff,	
	 mdy_iqwidth,		
	 mdy_iquality, 
		
	 mdy_imodrate,
	 mdy_imoddepth,
	 mdy_imodglide,

	 mdy_imix,
	 mdy_ibxfade,

	 mdy_cparam

} mdy_e;


// parameter ranges

prm_rng_t mdy_rng[] = {

	{mdy_cparam,	0, 0},				// first entry is # of parameters
		
	// delay params

	{mdy_idtype,	0, DLY_MAX},		// delay type DLY_PLAIN, DLY_LOWPASS, DLY_ALLPASS	
	{mdy_idelay,	0.0, 1000.0},		// delay in milliseconds
	{mdy_ifeedback,	0.0, 0.99},			// feedback 0-1.0
	{mdy_igain,	    0.0, 1.0},			// final gain of output stage, 0-1.0

	// filter params if mdy type DLY_LOWPASS

	{mdy_iftype,	0, FTR_MAX},		
	{mdy_icutoff,	10.0, 22050.0},
	{mdy_iqwidth,	100.0, 11025.0},
	{mdy_iquality,	0, QUA_MAX},
	
	{mdy_imodrate,	0.01, 200.0},		// frequency at which delay values change to new random value. 0 is no self-modulation
	{mdy_imoddepth,	0.0, 1.0},			// how much delay changes (decreases) from current value (0-1.0) 
	{mdy_imodglide,	0.01, 100.0},		// glide time between dcur and dnew in milliseconds
	{mdy_imix,		0.0, 1.0}			// 1.0 = full fx mix, 0.5 = 50% fx, 50% dry
};


// convert user parameters to internal parameters, allocate and return

mdy_t * MDY_Params ( prc_t *pprc )
{
	mdy_t *pmdy;
	dly_t *pdly;	

	float ramptime = pprc->prm[mdy_imodglide] / 1000.0;			// get ramp time in seconds
	float modtime = 0.0f;
	if ( pprc->prm[mdy_imodrate] != 0.0f )
	{
		modtime = 1.0 / pprc->prm[mdy_imodrate];				// time between modulations in seconds
	}
	float depth = pprc->prm[mdy_imoddepth];						// depth of modulations 0-1.0
	float mix	= pprc->prm[mdy_imix];

	// alloc plain, allpass or lowpass delay

	pdly = DLY_Params( pprc );
	
	if ( !pdly )
		return NULL;

	pmdy = MDY_Alloc ( pdly, ramptime, modtime, depth, mix );
	
	return pmdy;
}

inline void * MDY_VParams ( void *p ) 
{
	PRC_CheckParams ( (prc_t *)p, mdy_rng ); 
	return (void *) MDY_Params ((prc_t *)p); 
}

// v is +/- 0-1.0
// change current delay value 0..D

void MDY_Mod ( mdy_t *pmdy, float v ) 
{

	int D0 = pmdy->pdly->D0;				// base delay value
	float v2;

	// if v is < -2.0 then delay is v + 10.0
	// invert phase of output. hack.

	if ( v < -2.0 )
	{
		v = v + 10.0;
		pmdy->bPhaseInvert = true;
	}
	else
	{
		pmdy->bPhaseInvert = false;
	}

	v2 = -(v + 1.0)/2.0;				// v2 varies -1.0-0.0

	// D0 varies 0..D0

	D0 = D0 + (int)((float)D0 * v2);

	// change delay

	MDY_ChangeVal( pmdy, D0 );

	return; 
}


///////////////////
// Parallel reverbs
///////////////////

// Reverb A
// M parallel reverbs, mixed to mono output

#define CRVAS				64				// max number of parallel series reverbs active

#define CRVA_DLYS			12				// max number of delays making up reverb_a

struct rva_t
{
	bool fused;
	int m;						// number of parallel plain or lowpass delays
	int fparallel;				// true if filters in parallel with delays, otherwise single output filter	
	flt_t *pflt;				// series filters
	
	dly_t *pdlys[CRVA_DLYS];	// array of pointers to delays
	mdy_t *pmdlys[CRVA_DLYS];	// array of pointers to mod delays

	bool fmoddly;				// true if using mod delays
};

rva_t rvas[CRVAS];

void RVA_Init ( rva_t *prva ) {	if ( prva )	Q_memset (prva, 0, sizeof (rva_t)); }
void RVA_InitAll( void ) { for (int i = 0; i < CRVAS; i++) RVA_Init ( &rvas[i] ); }

// free parallel series reverb

void RVA_Free( rva_t *prva )
{
	int i;

	if ( prva )
	{
		// free all delays
		for (i = 0; i < CRVA_DLYS; i++)
			DLY_Free ( prva->pdlys[i] );
		
		// zero all ptrs to delays in mdy array
		for (i = 0; i < CRVA_DLYS; i++)
		{
			if ( prva->pmdlys[i] )
				prva->pmdlys[i]->pdly = NULL;
		}

		// free all mod delays
		for (i = 0; i < CRVA_DLYS; i++)
			MDY_Free ( prva->pmdlys[i] );
		
		FLT_Free( prva->pflt );
		
		Q_memset( prva, 0, sizeof (rva_t) );
	}
}


void RVA_FreeAll( void ) { for (int i = 0; i < CRVAS; i++) RVA_Free( &rvas[i] ); }

// create parallel reverb - m parallel reverbs summed 

// D array of CRVB_DLYS reverb delay sizes max sample index w[0...D] (ie: D+1 samples)
// a array of reverb feedback parms for parallel reverbs (CRVB_P_DLYS)
//		if a[i] < 0 then this is a predelay - use DLY_FLINEAR instead of DLY_LOWPASS
// b array of CRVB_P_DLYS - mix params for parallel reverbs
// m - number of parallel delays
// pflt - filter template, to be used by all parallel delays
// fparallel - true if filter operates in parallel with delays, otherwise filter output only
// fmoddly -  > 0 if delays are all mod delays (milliseconds of delay modulation)
// fmodrate - # of delay repetitions between changes to mod delay
// ftaps - if > 0, use 4 taps per reverb delay unit (increases density) tap = D - n*ftaps  n = 0,1,2,3

rva_t * RVA_Alloc ( int *D, int *a, int *b, int m, flt_t *pflt, int fparallel, float fmoddly, float fmodrate, float ftaps )
{
	
	int i;
	int dtype;
	rva_t *prva;
	flt_t *pflt2 = NULL;
	
	bool btaps = ftaps > 0.0;

	// find open slot

	for ( i = 0; i < CRVAS; i++ )
	{
		if ( !rvas[i].fused )
			break;
	}

	// return null if no free slots

	if (i == CRVAS)
	{
		DevMsg ("DSP: Warning, failed to allocate reverb.\n" );
		return NULL;
	}
	
	prva = &rvas[i];

	// if series filter specified, alloc two series filters

	if ( pflt && !fparallel)
	{
		// use filter data as template for a filter on output (2 cascaded filters)

		pflt2 = FLT_Alloc (0, pflt->M, pflt->L, pflt->a, pflt->b, 1.0);

		if (!pflt2)
		{
			DevMsg ("DSP: Warning, failed to allocate flt for reverb.\n" );
			return NULL;
		}

		pflt2->pf1 = FLT_Alloc (0, pflt->M, pflt->L, pflt->a, pflt->b, 1.0);
		pflt2->N = 1;
	}

	// allocate parallel delays
	
	for (i = 0; i < m; i++)
	{
		// set delay type

		if ( pflt && fparallel )
			// if a[i] param is < 0, allocate delay as predelay instead of feedback delay
			dtype = a[i] < 0 ? DLY_FLINEAR : DLY_LOWPASS;
		else
			// if no filter specified, alloc as plain or multitap plain delay
			dtype = btaps ? DLY_PLAIN_4TAP : DLY_PLAIN;

		if ( dtype == DLY_LOWPASS && btaps )
			dtype = DLY_LOWPASS_4TAP;

		// if filter specified and parallel specified, alloc 1 filter per delay

		if ( DLY_HAS_FILTER(dtype) )
			prva->pdlys[i] = DLY_AllocLP( D[i], abs(a[i]), b[i], dtype, pflt->M, pflt->L, pflt->a, pflt->b );
		else
			prva->pdlys[i] = DLY_Alloc( D[i], abs(a[i]), b[i], dtype );

		if ( DLY_HAS_MULTITAP(dtype) )
		{
			// set up delay taps to increase density around delay value. 
			
			// value of ftaps is the seed for all tap values 

			float t1 = max((double)MSEC_TO_SAMPS(5), D[i] * (1.0 - ftaps * 3.141592) );	
			float t2 = max((double)MSEC_TO_SAMPS(7), D[i] * (1.0 - ftaps * 1.697043) );	
			float t3 = max((double)MSEC_TO_SAMPS(10), D[i] * (1.0 - ftaps * 0.96325) ); 

			DLY_ChangeTaps( prva->pdlys[i], (int)t1, (int)t2, (int)t3, D[i] );
		}
	}	

	
	if ( fmoddly > 0.0 )
	{
		// alloc mod delays, using previously alloc'd delays
		
		// ramptime is time in seconds for delay to change from dcur to dnew
		// modtime is time in seconds between modulations. 0 if no self-modulation
		// depth is 0-1.0 multiplier, new delay values when modulating are Dnew = randomlong (D - D*depth, D)
		
		float ramptime;
		float modtime;
		float depth;

		for (i = 0; i < m; i++)
		{
			int Do = prva->pdlys[i]->D;

			modtime = (float)Do / (float)(SOUND_DMA_SPEED);	// seconds per delay
			depth = (fmoddly * 0.001f) / modtime;								// convert milliseconds to 'depth' %
			depth = clamp (depth, 0.01f, 0.99f);
			modtime = modtime * fmodrate;										// modulate every N delay passes

			ramptime = fpmin(20.0f/1000.0f, modtime / 2);							// ramp between delay values in N ms

			prva->pmdlys[i] = MDY_Alloc( prva->pdlys[i], ramptime, modtime, depth, 1.0 );
		}

		prva->fmoddly = true;
	}
	
	// if we failed to alloc any reverb, free all, return NULL

	for (i = 0; i < m; i++)
	{
		if ( !prva->pdlys[i] )
		{
			FLT_Free( pflt2 );
			RVA_Free( prva );
			DevMsg ("DSP: Warning, failed to allocate delay for reverb.\n" );
			return NULL;
		}
	}

	prva->fused = true;
	prva->m = m;
	prva->fparallel = fparallel;
	prva->pflt = pflt2;
	return prva;
}


// parallel reverberator
//
// for each input sample x do:
//		x0 = plain(D0,w0,&p0,a0,x)
//		x1 = plain(D1,w1,&p1,a1,x)
//		x2 = plain(D2,w2,&p2,a2,x)
//		x3 = plain(D3,w3,&p3,a3,x)
//		y = b0*x0 + b1*x1 + b2*x2 + b3*x3
//
//		rgdly - array of M delays:
//		D - Delay values (typical - 29, 37, 44, 50, 27, 31)
//		w - array of delayed values
//		p - array of pointers to circular delay line pointers
//		a - array of M feedback values (typical - all equal, like 0.75 * PMAX)
//		b - array of M gain values for plain reverb outputs (1, .9, .8, .7)
//		xin - input value
//		if fparallel, filters are built into delays,
//		otherwise, filter is in feedback loop


int g_MapIntoPBITSDivInt[] = 
{
	0, PMAX/1, PMAX/2,	PMAX/3,	PMAX/4,	PMAX/5,	PMAX/6,	PMAX/7,	PMAX/8, 
	   PMAX/9, PMAX/10, PMAX/11,PMAX/12,PMAX/13,PMAX/14,PMAX/15,PMAX/16, 
};

inline int RVA_GetNext( rva_t *prva, int x )
{
	int m = prva->m;			
	int y = 0;

	if ( prva->fmoddly )
	{
		// get output of parallel mod delays

		for (int i = 0; i < m; i++ )
			y += MDY_GetNext( prva->pmdlys[i], x );
	}
	else
	{
		// get output of parallel delays

		for (int i = 0; i < m; i++ )
			y += DLY_GetNext( prva->pdlys[i], x );
	}

  	// PERFORMANCE: y/m is now baked into the 'b' gain params for each delay ( b = b/m )
	// y = (y * g_MapIntoPBITSDivInt[m]) >> PBITS;

	if ( prva->fparallel )
		return y;

	// run series filters if present

	if ( prva->pflt )
	{
		y = FLT_GetNext( prva->pflt, y);
	}
	
	return y;
}


// batch version for performance
// UNDONE: unwind RVA_GetNextN so that it directly calls DLY_GetNextN or MDY_GetNextN

inline void RVA_GetNextN( rva_t *prva, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = RVA_GetNext( prva, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = RVA_GetNext( prva, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = RVA_GetNext( prva, pb->left );
			pb++;
		}
		return;
	}
}

// reverb parameter order
	
typedef enum
{

// parameter order

	rva_size_max,
	rva_size_min,

	rva_inumdelays,	
	rva_ifeedback,	
	rva_igain, 
	
	rva_icutoff,		
						
	rva_ifparallel,
	rva_imoddly,
	rva_imodrate,

	rva_width,
	rva_depth,
	rva_height,

	rva_fbwidth,
	rva_fbdepth,
	rva_fbheight,

	rva_iftaps,

	rva_cparam		// # of params
} rva_e;

// filter parameter ranges

prm_rng_t rva_rng[] = {

	{rva_cparam,	0, 0},			// first entry is # of parameters
	
	// reverb params
	{rva_size_max,	0.0, 1000.0},	// max room delay in milliseconds
	{rva_size_min,	0.0, 1000.0},	// min room delay in milliseconds
	{rva_inumdelays,1.0, 12.0},		// controls # of parallel or series delays
	{rva_ifeedback,	0.0, 1.0},		// feedback of delays 
	{rva_igain,		0.0, 10.0},		// output gain

	// filter params for each parallel reverb (quality set to 0 for max execution speed)
			
	{rva_icutoff,	10, 22050},
	
	{rva_ifparallel, 0,	1},			// if 1, then all filters operate in parallel with delays. otherwise filter output only
	{rva_imoddly, 0.0, 50.0},		// if > 0 then all delays are modulating delays, mod param controls milliseconds of mod depth
	{rva_imodrate, 0.0, 10.0},		// how many delay repetitions pass between mod changes to delayl

	// override params - for more detailed description of room
										// note: width/depth/height < 0 only for some automatic dsp presets
	{rva_width,		-1000.0, 1000.0},	// 0-1000.0 millisec (room width in feet) - used instead of size if non-zero
	{rva_depth,		-1000.0, 1000.0},	// 0-1000.0 room depth in feet - used instead of size if non-zero
	{rva_height,	-1000.0, 1000.0},	// 0-1000.0 room height in feet - used instead of size if non-zero

	{rva_fbwidth,	-1.0, 1.0},		// 0-1.0 material reflectivity - used as feedback param instead of decay if non-zero	
	{rva_fbdepth,	-1.0, 1.0},		// 0-1.0 material reflectivity - used as feedback param instead of decay if non-zero	
	{rva_fbheight,	-1.0, 1.0},		// 0-1.0 material reflectivity - used as feedback param instead of decay if non-zero	
									// if < 0, a predelay is allocated, then feedback is -1*param given

	{rva_iftaps,	0.0, 0.333}		// if > 0, use 3 extra taps with delay values = d * (1 - faps*n) n = 0,1,2,3
};

#define RVA_BASEM		1				// base number of parallel delays

// nominal delay and feedback values. More delays = more density.

#define RVADLYSMAX	49
float rvadlys[] =   {18,  23,  28,  33,   42,  21,  26,  36,   39,  45,  47,  30};
float rvafbs[] =	{0.9, 0.9, 0.9, 0.85, 0.8, 0.9, 0.9, 0.85, 0.8, 0.8, 0.8, 0.85};

#define SWAP(a,b,t)				{(t) = (a); (a) = (b); (b) = (t);}

#define RVA_MIN_SEPARATION		7					// minimum separation between reverbs, in ms.

// Construct D,a,b delay arrays given array of length,width,height sizes and feedback values
// rgd[] array of delay values in milliseconds (feet)
// rgf[] array of feedback values 0..1
// m # of parallel reverbs to construct
// D[] array of output delay values for parallel reverbs
// a[] array of output feedback values
// b[] array of output gain values = 1/m
// gain - output gain
// feedback - default feedback if rgf members are 0

void RVA_ConstructDelays( float *rgd, float *rgf, int m, int *D, int *a, int *b, float gain, float feedback )
{

	int i;
	float r;
	int d;
	float t, d1, d2, dm;
	bool bpredelay;
	
	// sort descending, so rgd[0] is largest delay & rgd[2] is smallest

	if (rgd[2] > rgd[1]) { SWAP(rgd[2], rgd[1], t); SWAP(rgf[2], rgf[1], t); }
	if (rgd[1] > rgd[0]) { SWAP(rgd[0], rgd[1], t); SWAP(rgf[0], rgf[1], t); }
	if (rgd[2] > rgd[1]) { SWAP(rgd[2], rgd[1], t); SWAP(rgf[2], rgf[1], t); }

	// if all feedback values 0, use default feedback

	if (rgf[0] == 0.0 && rgf[1] == 0.0 && rgf[2] == 0.0 )
	{
		// use feedback param for all 
		
		rgf[0] = rgf[1] = rgf[2] = feedback;

		// adjust feedback down for larger delays so that decay is constant for all delays

		rgf[0] = DLY_NormalizeFeedback( rgd[2], rgf[2], rgd[0] );
		rgf[1] = DLY_NormalizeFeedback( rgd[2], rgf[2], rgd[1] );

	}
		
	// make sure all reverbs are different by at least RVA_MIN_SEPARATION * m/3	m is 3,6,9 or 12

	int dmin = (m/3) * RVA_MIN_SEPARATION;

	d1 = rgd[1] - rgd[2];
	
	if (d1 <= dmin)
		rgd[1] += (dmin-d1);	// make difference = dmin

	d2 = rgd[0] - rgd[1];
	
	if (d2 <= dmin)
		rgd[0] += (dmin-d1);	// make difference = dmin

	for ( i = 0; i < m; i++ )
	{
		// reverberations due to room width, depth, height
		// assume sound moves at approx 1ft/ms

		int j = (int)(fmod ((float)i, 3.0f));	// j counts   0,1,2  0,1,2 0,1..
		
		d = (int)rgd[j];
		r = fabs(rgf[j]);

		bpredelay = ((rgf[j] < 0) && i < 3);

		// re-use predelay values as reverb values:

		if (rgf[j] < 0 && !bpredelay)
			d = max((int)(rgd[j] / 4.0), RVA_MIN_SEPARATION);

		if (i < 3)
			dm = 0.0;
		else
			dm = max( (double)(RVA_MIN_SEPARATION * (i/3)), ((i/3) * ((float)d * 0.18)) );

		d += (int)dm;
		D[i] = MSEC_TO_SAMPS(d);		

		// D[i] = MSEC_TO_SAMPS(d + ((i/3) * RVA_MIN_SEPARATION));		// (i/3) counts 0,0,0 1,1,1 2,2,2 ... separate all reverbs by 5ms

		// feedback - due to wall/floor/ceiling reflectivity
		a[i] = (int) min (0.999 * PMAX, (double)PMAX * r);

		if (bpredelay)
			a[i] = -a[i];		// flag delay as predelay

		b[i] = (int)((float)(gain * PMAX) / (float)m);
	}
}

void RVA_PerfTest()
{
	double time1, time2;

	int i;
	int k;
	int j;
	int m;
	int a[100];

	time1 = Plat_FloatTime();

	for (m = 0; m < 1000; m++)
	{
		for (i = 0, j = 10000; i < 10000; i++, j--)
		{
			// j = j % 6;
			// k = (i * j) >> PBITS;
	
			k = i / ((j % 6) + 1);
		}
	}

	time2 = Plat_FloatTime();
	
	DevMsg("divide = %2.5f \n", (time2-time1));

	
	for (i=1;i<10;i++)
		a[i] = PMAX / i;

	time1 = Plat_FloatTime();

	for (m = 0; m < 1000; m++)
	{
		for (i = 0, j = 10000; i < 10000; i++, j--)
		{
			k = (i * a[(j % 6) + 1] ) >> PBITS;
		}
	}

	time2 = Plat_FloatTime();

	DevMsg("shift & multiply = %2.5f \n", (time2-time1));
}

rva_t * RVA_Params ( prc_t *pprc )
{
	rva_t *prva;

	float size_max		= pprc->prm[rva_size_max];	// max delay size
	float size_min		= pprc->prm[rva_size_min];	// min delay size

	float numdelays	= pprc->prm[rva_inumdelays];	// controls # of parallel delays
	float feedback	= pprc->prm[rva_ifeedback];		// 0-1.0 controls feedback parameters
	float gain		= pprc->prm[rva_igain];			// 0-10.0 controls output gain

	float cutoff	= pprc->prm[rva_icutoff];		// filter cutoff
	
	float fparallel = pprc->prm[rva_ifparallel];	// if true, all filters are in delay feedback paths - otherwise single flt on output

	float fmoddly	= pprc->prm[rva_imoddly];		// if > 0, milliseconds of delay mod depth
	float fmodrate	= pprc->prm[rva_imodrate];		// if fmoddly > 0, # of delay repetitions between modulations

	float width		= fabs(pprc->prm[rva_width]);			// 0-1000 controls size of 1/3 of delays - used instead of size if non-zero
	float depth		= fabs(pprc->prm[rva_depth]);			// 0-1000 controls size of 1/3 of delays - used instead of size if non-zero
	float height	= fabs(pprc->prm[rva_height]);		// 0-1000 controls size of 1/3 of delays - used instead of size if non-zero

	float fbwidth	= pprc->prm[rva_fbwidth];		// feedback parameter for walls	0..2		
	float fbdepth	= pprc->prm[rva_fbdepth];		// feedback parameter for floor
	float fbheight	= pprc->prm[rva_fbheight];		// feedback parameter for ceiling	
	
	float ftaps		= pprc->prm[rva_iftaps];		// if > 0 increase reverb density using 3 extra taps d = (1.0 - ftaps * n) n = 0,1,2,3

	

//	RVA_PerfTest();

	// D array of CRVB_DLYS reverb delay sizes max sample index w[0...D] (ie: D+1 samples)
	// a array of reverb feedback parms for parallel delays 
	// b array of CRVB_P_DLYS - mix params for parallel reverbs
	// m - number of parallel delays
	
	int D[CRVA_DLYS];
	int a[CRVA_DLYS];
	int b[CRVA_DLYS];
	int m;

	// limit # delays 1-12

	m = clamp (numdelays, (float)RVA_BASEM, (float)CRVA_DLYS);

	// set up D (delay) a (feedback) b (gain) arrays

	if ( int(width) || int(height) || int(depth) )
	{
		// if width, height, depth given, use values as simple delays

		float rgd[3];
		float rgfb[3];

		// force m to 3, 6, 9 or 12
		
		if (m < 3)			m = 3;
		if (m > 3 && m < 6)	m = 6;
		if (m > 6 && m < 9)	m = 9;
		if (m > 9)			m = 12;

		rgd[0] = width;		rgfb[0] = fbwidth;
		rgd[1] = depth;		rgfb[1] = fbdepth;
		rgd[2] = height;	rgfb[2] = fbheight;

		RVA_ConstructDelays( rgd, rgfb, m, D, a, b, gain, feedback );
	}
	else
	{
		// use size parameter instead of width/depth/height

		for ( int i = 0; i < m; i++ )
		{
			// delays of parallel reverb.  D[0] = size_min.
			
			D[i] = MSEC_TO_SAMPS( size_min + (int)( ((float)(size_max - size_min) / (float)m) * (float)i) );
			
			// feedback and gain of parallel reverb

			if (i == 0)
			{
				// set feedback for smallest delay

				a[i] = (int) min (0.999 * PMAX, (double)PMAX * feedback );
			}
			else
			{
				// adjust feedback down for larger delays so that decay time is constant

				a[i] = (int) min (0.999 * PMAX, (double)PMAX * DLY_NormalizeFeedback( D[0], feedback, D[i] ) );
			}
			
			b[i] = (int) ((float)(gain * PMAX) / (float)m);
		}
	}

	// add filter

	flt_t *pflt = NULL;

	if ( cutoff )
	{

		// set up dummy lowpass filter to convert params

		prc_t prcf;

		prcf.prm[flt_iquality]	= QUA_LO;	// force filter to low quality for faster execution time
		prcf.prm[flt_icutoff]	= cutoff;
		prcf.prm[flt_iftype]	= FLT_LP;
		prcf.prm[flt_iqwidth]	= 0;
		prcf.prm[flt_igain]		= 1.0;

		pflt = (flt_t *)FLT_Params ( &prcf );	
	}
	
	prva = RVA_Alloc ( D, a, b, m, pflt, fparallel, fmoddly, fmodrate, ftaps );

	FLT_Free( pflt );

	return prva;
}


inline void * RVA_VParams ( void *p ) 
{
	PRC_CheckParams ( (prc_t *)p, rva_rng ); 
	return (void *) RVA_Params ((prc_t *)p); 
}

inline void RVA_Mod ( void *p, float v ) { return; }



////////////
// Diffusor
///////////

// (N series allpass reverbs)

#define CDFRS				64				// max number of series reverbs active

#define CDFR_DLYS			16				// max number of delays making up diffusor

struct dfr_t
{
	bool fused;
	int n;						// series allpass delays
	int w[CDFR_DLYS];			// internal state array for series allpass filters

	dly_t *pdlys[CDFR_DLYS];	// array of pointers to delays
};

dfr_t dfrs[CDFRS];

void DFR_Init ( dfr_t *pdfr ) {	if ( pdfr )	Q_memset (pdfr, 0, sizeof (dfr_t)); }
void DFR_InitAll( void ) { for (int i = 0; i < CDFRS; i++) DFR_Init ( &dfrs[i] ); }

// free parallel series reverb

void DFR_Free( dfr_t *pdfr )
{
	if ( pdfr )
	{
	// free all delays

	for (int i = 0; i < CDFR_DLYS; i++)
		DLY_Free ( pdfr->pdlys[i] );
	
	Q_memset( pdfr, 0, sizeof (dfr_t) );
	}
}


void DFR_FreeAll( void ) { for (int i = 0; i < CDFRS; i++) DFR_Free( &dfrs[i] ); }

// create n series allpass reverbs

// D array of CRVB_DLYS reverb delay sizes max sample index w[0...D] (ie: D+1 samples)
// a array of reverb feedback parms for series delays
// b array of gain params for parallel reverbs
// n - number of series delays

dfr_t * DFR_Alloc ( int *D, int *a, int *b, int n )
{
	
	int i;
	dfr_t *pdfr;

	// find open slot

	for (i = 0; i < CDFRS; i++)
	{
		if (!dfrs[i].fused)
			break;
	}
	
	// return null if no free slots

	if (i == CDFRS)
	{
		DevMsg ("DSP: Warning, failed to allocate diffusor.\n" );
		return NULL;
	}
	
	pdfr = &dfrs[i];

	DFR_Init( pdfr );

	// alloc reverbs

	for (i = 0; i < n; i++)
		pdfr->pdlys[i] = DLY_Alloc( D[i], a[i], b[i], DLY_ALLPASS );
		
	// if we failed to alloc any reverb, free all, return NULL

	for (i = 0; i < n; i++)
	{
		if ( !pdfr->pdlys[i])
		{
			DFR_Free( pdfr );
			DevMsg ("DSP: Warning, failed to allocate delay for diffusor.\n" );
			return NULL;
		}
	}
	
	pdfr->fused = true;
	pdfr->n = n;

	return pdfr;
}


// series reverberator

inline int DFR_GetNext( dfr_t *pdfr, int x )
{
	int i;			
	int y;
	dly_t *pdly;

	y = x;
	
	for (i = 0; i < pdfr->n; i++)
	{	
		pdly = pdfr->pdlys[i];	
		y = DelayAllpass( pdly->D, pdly->t, pdly->w, &pdly->p, pdly->a, pdly->b, y );
	}

	return y;
}

// batch version for performance

inline void DFR_GetNextN( dfr_t *pdfr, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = DFR_GetNext( pdfr, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = DFR_GetNext( pdfr, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = DFR_GetNext( pdfr, pb->left );
			pb++;
		}
		return;
	}
}

#define DFR_BASEN		1				// base number of series allpass delays

// nominal diffusor delay and feedback values

float dfrdlys[] =   {13,   19,   26,   21,   32,   36,   38,   16,   24,   28,   41,   35,   10,   46,   50,   27};
float dfrfbs[] =    {1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0}; 


// diffusor parameter order
	
typedef enum
{

// parameter order

	dfr_isize,		
	dfr_inumdelays,	
	dfr_ifeedback,		
	dfr_igain,

	dfr_cparam				// # of params

} dfr_e;

// diffusor parameter ranges

prm_rng_t dfr_rng[] = {

	{dfr_cparam,	0, 0},			// first entry is # of parameters

	{dfr_isize,		0.0, 1.0},	// 0-1.0 scales all delays
	{dfr_inumdelays,0.0, 4.0},	// 0-4.0 controls # of series delays 
	{dfr_ifeedback,	0.0, 1.0},	// 0-1.0 scales all feedback parameters 
	{dfr_igain,	0.0, 10.0},		// 0-1.0 scales all feedback parameters 
};


dfr_t * DFR_Params ( prc_t *pprc )
{
	dfr_t *pdfr;
	int i;
	int s;
	float size =		pprc->prm[dfr_isize];			// 0-1.0 scales all delays
	float numdelays =	pprc->prm[dfr_inumdelays];		// 0-4.0 controls # of series delays
	float feedback =	pprc->prm[dfr_ifeedback];		// 0-1.0 scales all feedback parameters 
	float gain =		pprc->prm[dfr_igain];			// 0-10.0 controls output gain

	// D array of CRVB_DLYS reverb delay sizes max sample index w[0...D] (ie: D+1 samples)
	// a array of reverb feedback parms for series delays (CRVB_S_DLYS)
	// b gain of each reverb section
	// n - number of series delays

	int D[CDFR_DLYS];
	int a[CDFR_DLYS];
	int b[CDFR_DLYS];
	int n;

	if (gain == 0.0)
		gain = 1.0;

	// get # series diffusors
	
	// limit m, n to half max number of delays

	n = clamp (Float2Int(numdelays), DFR_BASEN, CDFR_DLYS/2);

	// compute delays for diffusors

	for (i = 0; i < n; i++)
	{
		s = (int)( dfrdlys[i] * size );

		// delay of diffusor

		D[i] = MSEC_TO_SAMPS(s);

		// feedback and gain of diffusor

		a[i] = min (0.999 * PMAX, (double)(dfrfbs[i] * PMAX * feedback));
		b[i] = (int) ( (float)(gain * (float)PMAX) );
	}

	
	pdfr = DFR_Alloc ( D, a, b, n );

	return pdfr;
}

inline void * DFR_VParams ( void *p ) 
{
	PRC_CheckParams ((prc_t *)p, dfr_rng);
	return (void *) DFR_Params ((prc_t *)p); 
}

inline void DFR_Mod ( void *p, float v ) { return; }


//////////////////////
// LFO wav definitions
//////////////////////

#define CLFOSAMPS		512					// samples per wav table - single cycle only
#define LFOBITS			14					// bits of peak amplitude of lfo wav
#define LFOAMP			((1<<LFOBITS)-1)	// peak amplitude of lfo wav

//types of lfo wavs

#define LFO_SIN			0	// sine wav
#define LFO_TRI			1	// triangle wav
#define LFO_SQR			2	// square wave, 50% duty cycle
#define LFO_SAW			3	// forward saw wav
#define LFO_RND			4	// random wav
#define LFO_LOG_IN		5	// logarithmic fade in
#define LFO_LOG_OUT		6	// logarithmic fade out
#define LFO_LIN_IN		7	// linear fade in 
#define LFO_LIN_OUT		8	// linear fade out
#define LFO_MAX			LFO_LIN_OUT

#define CLFOWAV	9			// number of LFO wav tables

struct lfowav_t		// lfo or envelope wave table
{
	int	type;				// lfo type
	dly_t *pdly;			// delay holds wav values and step pointers
};

lfowav_t lfowavs[CLFOWAV];

// deallocate lfo wave table. Called only when sound engine exits.

void LFOWAV_Free( lfowav_t *plw )
{
	// free delay

	if ( plw )
		DLY_Free( plw->pdly );

	Q_memset( plw, 0, sizeof (lfowav_t) );
}

// deallocate all lfo wave tables. Called only when sound engine exits.

void LFOWAV_FreeAll( void )
{
	for ( int i = 0; i < CLFOWAV; i++ )
		LFOWAV_Free( &lfowavs[i] );
}

// fill lfo array w with count samples of lfo type 'type'
// all lfo wavs except fade out, rnd, and log_out should start with 0 output

void LFOWAV_Fill( int *w, int count, int type )
{
	int i,x;
	switch (type)
	{
	default:
	case LFO_SIN:			// sine wav, all values 0 <= x <= LFOAMP, initial value = 0
			for (i = 0; i < count; i++ )
			{
				x = ( int )( (float)(LFOAMP) * sinf( (2.0 * M_PI_F * (float)i / (float)count ) + (M_PI_F * 1.5) ) );
				w[i] = (x + LFOAMP)/2;
			}
			break;
	case LFO_TRI:			// triangle wav, all values 0 <= x <= LFOAMP, initial value = 0
			for (i = 0; i < count; i++)
				{
				w[i] = ( int ) ( (float)(2 * LFOAMP * i ) / (float)(count) );
				
				if ( i > count / 2 )
					w[i] = ( int ) ( (float) (2 * LFOAMP) - (float)( 2 * LFOAMP * i ) / (float)(count) );
				}
			break;
	case LFO_SQR:			// square wave, 50% duty cycle, all values 0 <= x <= LFOAMP, initial value = 0
			for (i = 0; i < count; i++)
				w[i] = i > count / 2 ? 0 : LFOAMP;
			break;
	case LFO_SAW:			// forward saw wav, aall values 0 <= x <= LFOAMP, initial value = 0
			for (i = 0; i < count; i++)
				w[i] = ( int ) ( (float)(LFOAMP) * (float)i / (float)(count) );
			break;
	case LFO_RND:			// random wav, all values 0 <= x <= LFOAMP
			for (i = 0; i < count; i++)
				w[i] = ( int ) ( RandomInt(0, LFOAMP) );
			break;
	case LFO_LOG_IN:		// logarithmic fade in, all values 0 <= x <= LFOAMP, initial value = 0
			for (i = 0; i < count; i++)
				w[i] = ( int ) ( (float)(LFOAMP) * powf( (float)i / (float)count, 2));
			break;
	case LFO_LOG_OUT:		// logarithmic fade out, all values 0 <= x <= LFOAMP, initial value = LFOAMP
			for (i = 0; i < count; i++)
				w[i] = ( int ) ( (float)(LFOAMP) * powf( 1.0 - ((float)i / (float)count), 2 ));
			break;
	case LFO_LIN_IN:		// linear fade in, all values 0 <= x <= LFOAMP, initial value = 0
			for (i = 0; i < count; i++)
				w[i] = ( int ) ( (float)(LFOAMP) * (float)i / (float)(count) );
			break;
	case LFO_LIN_OUT:		// linear fade out, all values 0 <= x <= LFOAMP, initial value = LFOAMP
			for (i = 0; i < count; i++)
				w[i] = LFOAMP - ( int ) ( (float)(LFOAMP) * (float)i / (float)(count) );
			break;
	}
}

// allocate all lfo wave tables.  Called only when sound engine loads.

void LFOWAV_InitAll()
{
	int i;
	dly_t *pdly;

	Q_memset( lfowavs, 0, sizeof( lfowavs ) );

	// alloc space for each lfo wav type
	
	for (i = 0; i < CLFOWAV; i++)
	{
		pdly = DLY_Alloc( CLFOSAMPS, 0, 0 , DLY_PLAIN);
		
		lfowavs[i].pdly = pdly;
		lfowavs[i].type = i;

		LFOWAV_Fill( pdly->w, CLFOSAMPS, i );
	}
	
	// if any dlys fail to alloc, free all

	for (i = 0; i < CLFOWAV; i++)
	{
		if ( !lfowavs[i].pdly )
			LFOWAV_FreeAll();
	}
}


////////////////////////////////////////
// LFO iterators - one shot and looping
////////////////////////////////////////

#define CLFO	16	// max active lfos (this steals from active delays)

struct lfo_t
{
	bool fused;		// true if slot take

	dly_t *pdly;	// delay points to lfo wav within lfowav_t (don't free this)

	int gain;

	float f;		// playback frequency in hz

	pos_t pos;		// current position within wav table, looping
	pos_one_t pos1;	// current position within wav table, one shot

	int foneshot;	// true - one shot only, don't repeat
};

lfo_t lfos[CLFO];

void LFO_Init( lfo_t *plfo ) { if ( plfo ) Q_memset( plfo, 0, sizeof (lfo_t) ); }
void LFO_InitAll( void ) { for (int i = 0; i < CLFO; i++) LFO_Init(&lfos[i]); }
void LFO_Free( lfo_t *plfo ) { if ( plfo ) Q_memset( plfo, 0, sizeof (lfo_t) ); }
void LFO_FreeAll( void ) { for (int i = 0; i < CLFO; i++) LFO_Free(&lfos[i]); }


// get step value given desired playback frequency

inline float LFO_HzToStep ( float freqHz )
{
	float lfoHz;

	// calculate integer and fractional step values,
	// assume an update rate of SOUND_DMA_SPEED samples/sec

	// 1 cycle/CLFOSAMPS * SOUND_DMA_SPEED samps/sec = cycles/sec = current lfo rate
	//
	// lforate * X = freqHz  so X = freqHz/lforate = update rate

	lfoHz = (float)(SOUND_DMA_SPEED) / (float)(CLFOSAMPS);

	return freqHz / lfoHz;
}

// return pointer to new lfo

lfo_t * LFO_Alloc( int wtype, float freqHz, bool foneshot, float gain )
{
	int i;
	int type = min ( CLFOWAV - 1, wtype );
	float lfostep;
	
	for (i = 0; i < CLFO; i++)
		if (!lfos[i].fused)
		{
			lfo_t *plfo = &lfos[i];

			LFO_Init( plfo );

			plfo->fused = true;
			plfo->pdly = lfowavs[type].pdly;		// pdly in lfo points to wav table data in lfowavs
			plfo->f = freqHz;
			plfo->foneshot = foneshot;
			plfo->gain = gain * PMAX;

			lfostep = LFO_HzToStep( freqHz );

			// init positional pointer (ie: fixed point updater for controlling pitch of lfo)

			if ( !foneshot )
				POS_Init(&(plfo->pos), plfo->pdly->D, lfostep );
			else
				POS_ONE_Init(&(plfo->pos1), plfo->pdly->D,lfostep );

			return plfo;
		}
		DevMsg ("DSP: Warning, failed to allocate LFO.\n" );
		return NULL;
}

// get next lfo value
// Value returned is 0..LFOAMP.  can be normalized by shifting right by LFOBITS
// To play back at correct passed in frequency, routien should be
// called once for every output sample (ie: at SOUND_DMA_SPEED)
// x is dummy param

inline int LFO_GetNext( lfo_t *plfo, int x )
{
	int i;

	// get current position

	if ( !plfo->foneshot )
		i = POS_GetNext( &plfo->pos );
	else
		i = POS_ONE_GetNext( &plfo->pos1 );

	// return current sample

	if (plfo->gain == PMAX)
		return plfo->pdly->w[i];
	else
		return (plfo->pdly->w[i] * plfo->gain ) >> PBITS;
}

// batch version for performance

inline void LFO_GetNextN( lfo_t *plfo, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = LFO_GetNext( plfo, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = LFO_GetNext( plfo, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = LFO_GetNext( plfo, pb->left );
			pb++;
		}
		return;
	}
}

// uses lfowav, rate, foneshot
	
typedef enum
{

// parameter order

	lfo_iwav,		
	lfo_irate,		
	lfo_ifoneshot,
	lfo_igain,
		
	lfo_cparam			// # of params

} lfo_e;

// parameter ranges

prm_rng_t lfo_rng[] = {

	{lfo_cparam,	0, 0},			// first entry is # of parameters

	{lfo_iwav,		0.0, LFO_MAX},	// lfo type to use (LFO_SIN, LFO_RND...)
	{lfo_irate,		0.0, 16000.0},	// modulation rate in hz. for MDY, 1/rate = 'glide' time in seconds
	{lfo_ifoneshot,	0.0, 1.0},		// 1.0 if lfo is oneshot
	{lfo_igain,		0.0, 10.0},		// output gain
};


lfo_t * LFO_Params ( prc_t *pprc )
{
	lfo_t *plfo;
	bool foneshot = pprc->prm[lfo_ifoneshot] > 0 ? true : false;
	float gain = pprc->prm[lfo_igain];

	plfo = LFO_Alloc ( pprc->prm[lfo_iwav], pprc->prm[lfo_irate], foneshot, gain );

	return plfo;
}

void LFO_ChangeVal ( lfo_t *plfo, float fhz )
{
	float fstep = LFO_HzToStep( fhz );

	// change lfo playback rate to new frequency fhz

	if ( plfo->foneshot )
		POS_ChangeVal( &plfo->pos, fstep );
	else
		POS_ChangeVal( &plfo->pos1.p, fstep );
}

inline void * LFO_VParams ( void *p ) 
{
	PRC_CheckParams ( (prc_t *)p, lfo_rng ); 
	return (void *) LFO_Params ((prc_t *)p); 
}

// v is +/- 0-1.0
// v changes current lfo frequency up/down by +/- v%

inline void LFO_Mod ( lfo_t *plfo, float v ) 
{ 
	float fhz;
	float fhznew;

	fhz = plfo->f;
	fhznew = fhz * (1.0 + v);

	LFO_ChangeVal ( plfo, fhznew );

	return; 
}


////////////////////////////////////////
// Time Compress/expand with pitch shift
////////////////////////////////////////

// realtime pitch shift - ie: pitch shift without change to playback rate

#define CPTCS		64

struct ptc_t
{
	bool fused;
	
	dly_t *pdly_in;			// input buffer space
	dly_t *pdly_out;		// output buffer space

	int *pin;				// input buffer (pdly_in->w)
	int *pout;				// output buffer (pdly_out->w)

	int cin;				// # samples in input buffer
	int cout;				// # samples in output buffer

	int cxfade;				// # samples in crossfade segment
	int ccut;				// # samples to cut
	int cduplicate;			// # samples to duplicate (redundant - same as ccut)

	int iin;				// current index into input buffer (reading)
	
	pos_one_t psn;			// stepping index through output buffer

	bool fdup;				// true if duplicating, false if cutting

	float fstep;			// pitch shift & time compress/expand
};

ptc_t ptcs[CPTCS];

void PTC_Init( ptc_t *pptc ) { if (pptc) Q_memset( pptc, 0, sizeof (ptc_t) ); };
void PTC_Free( ptc_t *pptc ) 
{
	if (pptc)
	{
		DLY_Free (pptc->pdly_in);
		DLY_Free (pptc->pdly_out);

		Q_memset( pptc, 0, sizeof (ptc_t) ); 
	}
};
void PTC_InitAll() { for (int i = 0; i < CPTCS; i++) PTC_Init( &ptcs[i] ); };
void PTC_FreeAll() { for (int i = 0; i < CPTCS; i++) PTC_Free( &ptcs[i] ); };



// Time compressor/expander with pitch shift (ie: pitch changes, playback rate does not)
//
// Algorithm:

// 1) Duplicate or discard chunks of sound to provide tslice * fstep seconds of sound.
//    (The user-selectable size of the buffer to process is tslice milliseconds in length)
// 2) Resample this compressed/expanded buffer at fstep to produce a pitch shifted
//    output with the same duration as the input (ie: #samples out = # samples in, an
//    obvious requirement for realtime inline processing).

// timeslice is size in milliseconds of full buffer to process.
// timeslice * fstep is the size of the expanded/compressed buffer
// timexfade is length in milliseconds of crossfade region between duplicated or cut sections
// fstep is % expanded/compressed sound normalized to 0.01-2.0 (1% - 200%)

// input buffer: 

// iin-->

// [0...      tslice              ...D]						input samples 0...D (D is NEWEST sample)
// [0...          ...n][m... tseg ...D]						region to be cut or duplicated m...D
					
// [0...   [p..txf1..n][m... tseg ...D]						fade in  region 1 txf1 p...n
// [0...          ...n][m..[q..txf2..D]						fade out region 2 txf2 q...D


// pitch up: duplicate into output buffer:	tdup = tseg

// [0...          ...n][m... tdup ...D][m... tdup ...D]		output buffer size with duplicate region	
// [0...          ...n][m..[p...xf1..n][m... tdup ...D]		fade in p...n while fading out q...D
// [0...          ...n][m..[q...xf2..D][m... tdup ...D]		
// [0...          ...n][m..[.XFADE...n][m... tdup ...D]		final duplicated output buffer - resample at fstep

// pitch down: cut into output buffer: tcut = tseg

// [0...         ...n][m... tcut  ...D]				input samples with cut region delineated m...D
// [0...         ...n]								output buffer size after cut
// [0... [q..txf2...D]								fade in txf1 q...D while fade out txf2 p...n
// [0... [.XFADE ...D]								final cut output buffer - resample at fstep


ptc_t * PTC_Alloc( float timeslice, float timexfade, float fstep ) 
{
	
	int i;
	ptc_t *pptc;
	float tout;
	int cin, cout;
	float tslice = timeslice;
	float txfade = timexfade;
	float tcutdup;

	// find time compressor slot

	for ( i = 0; i < CPTCS; i++ )
	{
		if ( !ptcs[i].fused )
			break;
	}
	
	if ( i == CPTCS ) 
	{
		DevMsg ("DSP: Warning, failed to allocate pitch shifter.\n" );
		return NULL;
	}

	pptc = &ptcs[i];
	
	PTC_Init ( pptc );

	// get size of region to cut or duplicate

	tcutdup = abs((fstep - 1.0) * timeslice);

	// to prevent buffer overruns:

	// make sure timeslice is greater than cut/dup time
	
	tslice = max ( (double)tslice, 1.1 * tcutdup);

	// make sure xfade time smaller than cut/dup time, and smaller than (timeslice-cutdup) time

	txfade = min ( (double)txfade, 0.9 * tcutdup );
	txfade = min ( (double)txfade, 0.9 * (tslice - tcutdup));

	pptc->cxfade =		MSEC_TO_SAMPS( txfade );
	pptc->ccut =		MSEC_TO_SAMPS( tcutdup );
	pptc->cduplicate =  MSEC_TO_SAMPS( tcutdup );
	
	// alloc delay lines (buffers)

	tout = tslice * fstep;

	cin = MSEC_TO_SAMPS( tslice );
	cout = MSEC_TO_SAMPS( tout );
	
	pptc->pdly_in = DLY_Alloc( cin, 0, 1, DLY_LINEAR );			// alloc input buffer
	pptc->pdly_out = DLY_Alloc( cout, 0, 1, DLY_LINEAR);		// alloc output buffer
	
	if ( !pptc->pdly_in || !pptc->pdly_out )
	{
		PTC_Free( pptc );
		DevMsg ("DSP: Warning, failed to allocate delay for pitch shifter.\n" );
		return NULL;
	}

	// buffer pointers

	pptc->pin = pptc->pdly_in->w;
	pptc->pout = pptc->pdly_out->w;

	// input buffer index

	pptc->iin = 0;

	// output buffer index

	POS_ONE_Init ( &pptc->psn, cout, fstep );

	// if fstep > 1.0 we're pitching shifting up, so fdup = true

	pptc->fdup = fstep > 1.0 ? true : false;
	
	pptc->cin = cin;
	pptc->cout = cout;

	pptc->fstep = fstep;
	pptc->fused = true;

	return pptc;
}

// linear crossfader
// yfadein - instantaneous value fading in
// ydafeout -instantaneous value fading out
// nsamples - duration in #samples of fade
// isample - index in to fade 0...nsamples-1

inline int xfade ( int yfadein, int yfadeout, int nsamples, int isample )
{
	int yout;
	int m = (isample << PBITS ) / nsamples;

//	yout = ((yfadein * m) >> PBITS) + ((yfadeout * (PMAX - m)) >> PBITS);
	yout = (yfadeout + (yfadein - yfadeout) * m ) >> PBITS;

	return yout;
}

// w - pointer to start of input buffer samples
// v - pointer to start of output buffer samples
// cin - # of input buffer samples
// cout = # of output buffer samples
// cxfade = # of crossfade samples
// cduplicate = # of samples in duplicate/cut segment

void TimeExpand( int *w, int *v, int cin, int cout, int cxfade, int cduplicate )
{
	int i,j;
	int m;	
	int p;
	int q;
	int D;

	// input buffer
	//               xfade source   duplicate
	// [0...........][p.......n][m...........D]
	
	// output buffer
	//								 xfade region   duplicate
	// [0.....................n][m..[q.......D][m...........D]

	// D - index of last sample in input buffer
	// m - index of 1st sample in duplication region
	// p - index of 1st sample of crossfade source
	// q - index of 1st sample in crossfade region
	
	D = cin - 1;
	m = cin - cduplicate;			
	p = m - cxfade;	
	q = cin - cxfade;

	// copy up to crossfade region

	for (i = 0; i < q; i++)
		v[i] = w[i];
	
	// crossfade region

	j = p;	

	for (i = q; i <= D; i++)
		v[i] = xfade (w[j++], w[i], cxfade, i-q);	// fade out p..n, fade in q..D
	
	// duplicate region

	j = D+1;

	for (i = m; i <= D; i++)
		v[j++] = w[i];

}

// cut ccut samples from end of input buffer, crossfade end of cut section
// with end of remaining section

// w - pointer to start of input buffer samples
// v - pointer to start of output buffer samples
// cin - # of input buffer samples
// cout = # of output buffer samples
// cxfade = # of crossfade samples
// ccut = # of samples in cut segment

void TimeCompress( int *w, int *v, int cin, int cout, int cxfade, int ccut )
{
	int i,j;
	int m;	
	int p;
	int q;
	int D;

	// input buffer
	//								  xfade source 
	// [0.....................n][m..[p.......D]

	//              xfade region     cut
	// [0...........][q.......n][m...........D]
	
	// output buffer
	//               xfade to source 
	// [0...........][p.......D]
	
	// D - index of last sample in input buffer
	// m - index of 1st sample in cut region
	// p - index of 1st sample of crossfade source
	// q - index of 1st sample in crossfade region
	
	D = cin - 1;
	m = cin - ccut;			
	p = cin - cxfade;	
	q = m - cxfade;

	// copy up to crossfade region

	for (i = 0; i < q; i++)
		v[i] = w[i];
	
	// crossfade region

	j = p;	

	for (i = q; i < m; i++)
		v[i] = xfade (w[j++], w[i], cxfade, i-q);	// fade out p..n, fade in q..D
	
	// skip rest of input buffer
}

// get next sample

// put input sample into input (delay) buffer
// get output sample from output buffer, step by fstep %
// output buffer is time expanded or compressed version of previous input buffer

inline int PTC_GetNext( ptc_t *pptc, int x ) 
{
	int iout, xout;
	bool fhitend = false;

	// write x into input buffer
	Assert (pptc->iin < pptc->cin);

	pptc->pin[pptc->iin] = x;

	pptc->iin++;
	
	// check for end of input buffer

	if ( pptc->iin >= pptc->cin )
		fhitend = true;

	// read sample from output buffer, resampling at fstep

	iout = POS_ONE_GetNext( &pptc->psn );
	Assert (iout < pptc->cout);
	xout = pptc->pout[iout];
	
	if ( fhitend )
	{
		// if hit end of input buffer (ie: input buffer is full)
		//		reset input buffer pointer
		//		reset output buffer pointer
		//		rebuild entire output buffer (TimeCompress/TimeExpand)

		pptc->iin = 0;

		POS_ONE_Init( &pptc->psn, pptc->cout, pptc->fstep );

		if ( pptc->fdup )
			TimeExpand ( pptc->pin, pptc->pout, pptc->cin, pptc->cout, pptc->cxfade, pptc->cduplicate );
		else
			TimeCompress ( pptc->pin, pptc->pout, pptc->cin, pptc->cout, pptc->cxfade, pptc->ccut );
	}

	return xout;
}

// batch version for performance

inline void PTC_GetNextN( ptc_t *pptc, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = PTC_GetNext( pptc, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = PTC_GetNext( pptc, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = PTC_GetNext( pptc, pb->left );
			pb++;
		}
		return;
	}
}

// change time compression to new value
// fstep is new value
// ramptime is how long change takes in seconds (ramps smoothly), 0 for no ramp

void PTC_ChangeVal( ptc_t *pptc, float fstep, float ramptime )
{
// UNDONE: ignored
// UNDONE: just realloc time compressor with new fstep
}

// uses pitch: 
// 1.0 = playback normal rate
// 0.5 = cut 50% of sound (2x playback)
// 1.5 = add 50% sound (0.5x playback)

typedef enum
{

// parameter order

	ptc_ipitch,			
	ptc_itimeslice,		
	ptc_ixfade,			
		
	ptc_cparam			// # of params

} ptc_e;

// diffusor parameter ranges

prm_rng_t ptc_rng[] = {

	{ptc_cparam,	0, 0},				// first entry is # of parameters

	{ptc_ipitch,		0.1, 4.0},		// 0-n.0 where 1.0 = 1 octave up and 0.5 is one octave down	
	{ptc_itimeslice,	20.0, 300.0},	// in milliseconds - size of sound chunk to analyze and cut/duplicate - 100ms nominal
	{ptc_ixfade,		1.0, 200.0},	// in milliseconds - size of crossfade region between spliced chunks - 20ms nominal	
};


ptc_t * PTC_Params ( prc_t *pprc )
{
	ptc_t *pptc;

	float pitch = pprc->prm[ptc_ipitch];
	float timeslice = pprc->prm[ptc_itimeslice];
	float txfade = pprc->prm[ptc_ixfade];

	pptc = PTC_Alloc( timeslice, txfade, pitch );
	
	return pptc;
}

inline void * PTC_VParams ( void *p ) 
{
	PRC_CheckParams ( (prc_t *)p, ptc_rng ); 
	return (void *) PTC_Params ((prc_t *)p); 
}

// change to new pitch value
// v is +/- 0-1.0
// v changes current pitch up/down by +/- v%

void PTC_Mod ( ptc_t *pptc, float v ) 
{ 
	float fstep;
	float fstepnew;

	fstep = pptc->fstep;
	fstepnew = fstep * (1.0 + v);

	PTC_ChangeVal( pptc, fstepnew, 0.01 );
}


////////////////////
// ADSR envelope
////////////////////

#define CENVS		64		// max # of envelopes active
#define CENVRMPS	4		// A, D, S, R

#define ENV_LIN		0		// linear a,d,s,r
#define ENV_EXP		1		// exponential a,d,s,r
#define ENV_MAX		ENV_EXP	

#define ENV_BITS	14		// bits of resolution of ramp

struct env_t
{
	bool fused;

	bool fhitend;			// true if done
	bool fexp;				// true if exponential ramps

	int ienv;				// current ramp
	rmp_t rmps[CENVRMPS];	// ramps
};

env_t envs[CENVS];

void ENV_Init( env_t *penv ) { if (penv) Q_memset( penv, 0, sizeof (env_t) ); };
void ENV_Free( env_t *penv ) { if (penv) Q_memset( penv, 0, sizeof (env_t) ); };
void ENV_InitAll() { for (int i = 0; i < CENVS; i++) ENV_Init( &envs[i] ); };
void ENV_FreeAll() { for (int i = 0; i < CENVS; i++) ENV_Free( &envs[i] ); };


// allocate ADSR envelope
// all times are in seconds
// amp1 - attack amplitude multiplier 0-1.0
// amp2 - sustain amplitude multiplier 0-1.0
// amp3 - end of sustain amplitude multiplier 0-1.0

env_t *ENV_Alloc ( int type, float famp1, float famp2, float famp3, float attack, float decay, float sustain, float release, bool fexp)
{
	int i;
	env_t *penv;

	for (i = 0; i < CENVS; i++)
	{
		if ( !envs[i].fused )
		{

			int amp1 = famp1 * (1 << ENV_BITS);	// ramp resolution
			int amp2 = famp2 * (1 << ENV_BITS);	
			int amp3 = famp3 * (1 << ENV_BITS);

			penv = &envs[i];
			
			ENV_Init (penv);

			// UNDONE: ignoring type = ENV_EXP - use oneshot LFOS instead with sawtooth/exponential

			// set up ramps

			RMP_Init( &penv->rmps[0], attack, 0, amp1, true );
			RMP_Init( &penv->rmps[1], decay, amp1, amp2, true );
			RMP_Init( &penv->rmps[2], sustain, amp2, amp3, true );
			RMP_Init( &penv->rmps[3], release, amp3, 0, true );

			penv->ienv = 0;
			penv->fused = true;
			penv->fhitend = false;
			penv->fexp = fexp;
			return penv;
		}
	}
	DevMsg ("DSP: Warning, failed to allocate envelope.\n" );
	return NULL;
}


inline int ENV_GetNext( env_t *penv, int x )
{
	if ( !penv->fhitend )
	{
		int i;
		int y;

		i = penv->ienv;
		y = RMP_GetNext ( &penv->rmps[i] );
		
		// check for next ramp

		if ( penv->rmps[i].fhitend )
			i++;

		penv->ienv = i;

		// check for end of all ramps

		if ( i > 3)
			penv->fhitend = true;

		// multiply input signal by ramp

		if (penv->fexp)
			return (((x * y) >> ENV_BITS) * y) >> ENV_BITS;
		else
			return (x * y) >> ENV_BITS;
	}

	return 0;
}

// batch version for performance

inline void ENV_GetNextN( env_t *penv, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = ENV_GetNext( penv, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = ENV_GetNext( penv, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = ENV_GetNext( penv, pb->left );
			pb++;
		}
		return;
	}
}

// uses lfowav, amp1, amp2, amp3, attack, decay, sustain, release
// lfowav is type, currently ignored - ie: LFO_LIN_IN, LFO_LOG_IN

// parameter order

typedef enum
{
	env_itype,		
	env_iamp1,		
	env_iamp2,		
	env_iamp3,		
	env_iattack,	
	env_idecay,		
	env_isustain,		
	env_irelease,	
	env_ifexp,
	env_cparam			// # of params

} env_e;

// parameter ranges

prm_rng_t env_rng[] = {

	{env_cparam,	0, 0},			// first entry is # of parameters

	{env_itype,		0.0,ENV_MAX},	// ENV_LINEAR, ENV_LOG - currently ignored
	{env_iamp1,		0.0, 1.0},		// attack peak amplitude 0-1.0
	{env_iamp2,		0.0, 1.0},		// decay target amplitued 0-1.0
	{env_iamp3,		0.0, 1.0},		// sustain target amplitude 0-1.0
	{env_iattack,	0.0, 20000.0},	// attack time in milliseconds
	{env_idecay,	0.0, 20000.0},	// envelope decay time in milliseconds
	{env_isustain,	0.0, 20000.0},	// sustain time in milliseconds	
	{env_irelease,	0.0, 20000.0},	// release time in milliseconds	
	{env_ifexp,		0.0, 1.0},		// 1.0 if exponential ramps
};

env_t * ENV_Params ( prc_t *pprc )
{
	env_t *penv;

	float type		= pprc->prm[env_itype];
	float amp1		= pprc->prm[env_iamp1];
	float amp2		= pprc->prm[env_iamp2];
	float amp3		= pprc->prm[env_iamp3];
	float attack	= pprc->prm[env_iattack]/1000.0;
	float decay		= pprc->prm[env_idecay]/1000.0;
	float sustain	= pprc->prm[env_isustain]/1000.0;
	float release	= pprc->prm[env_irelease]/1000.0;
	float fexp		= pprc->prm[env_ifexp];
	bool bexp;

	bexp = fexp > 0.0 ? 1 : 0;
	penv = ENV_Alloc ( type, amp1, amp2, amp3, attack, decay, sustain, release, bexp );
	return penv;
}

inline void * ENV_VParams ( void *p ) 
{
	PRC_CheckParams( (prc_t *)p, env_rng ); 
	return (void *) ENV_Params ((prc_t *)p); 
}

inline void ENV_Mod ( void *p, float v ) { return; }

//////////////////////////
// Gate & envelope follower
//////////////////////////

#define CEFOS		64		// max # of envelope followers active

struct efo_t
{
	bool fused;

	int xout;				// current output value

	// gate params

	bool bgate;				// if true, gate function is on	

	bool bgateon;			// if true, gate is on		
	bool bexp;				// if true, use exponential fade out

	int thresh;				// amplitude threshold for gate on
	int thresh_off;			// amplitidue threshold for gate off

	float attack_time;		// gate attack time in seconds
	float decay_time;		// gate decay time in seconds

	rmp_t rmp_attack;		// gate on ramp - attack
	rmp_t rmp_decay;		// gate off ramp - decay
};

efo_t efos[CEFOS];

void EFO_Init( efo_t *pefo ) { if (pefo) Q_memset( pefo, 0, sizeof (efo_t) ); };
void EFO_Free( efo_t *pefo ) { if (pefo) Q_memset( pefo, 0, sizeof (efo_t) ); };
void EFO_InitAll() { for (int i = 0; i < CEFOS; i++) EFO_Init( &efos[i] ); };
void EFO_FreeAll() { for (int i = 0; i < CEFOS; i++) EFO_Free( &efos[i] ); };

// return true when gate is off AND decay ramp has hit end

inline bool EFO_GateOff( efo_t *pefo )
{
	return ( !pefo->bgateon && RMP_HitEnd( &pefo->rmp_decay ) );
}


// allocate enveloper follower

#define EFO_HYST_AMP	1000		// hysteresis amplitude

efo_t *EFO_Alloc ( float threshold, float attack_sec, float decay_sec, bool bexp )
{
	int i;
	efo_t *pefo;

	for (i = 0; i < CEFOS; i++)
	{
		if ( !efos[i].fused )
		{
			pefo = &efos[i];

			EFO_Init ( pefo );

			pefo->xout = 0;				
			pefo->fused = true;
			
			// init gate params

			pefo->bgate =  threshold > 0.0;

			if (pefo->bgate)
			{
				pefo->attack_time = attack_sec;
				pefo->decay_time = decay_sec;

				RMP_Init( &pefo->rmp_attack, attack_sec, 0, PMAX, false);
				RMP_Init( &pefo->rmp_decay, decay_sec, PMAX, 0, false);
				RMP_SetEnd( &pefo->rmp_attack );
				RMP_SetEnd( &pefo->rmp_decay );

				pefo->thresh = threshold;
				pefo->thresh_off = max(1.f, threshold - EFO_HYST_AMP);
				pefo->bgateon = false;
				pefo->bexp = bexp;
			}
				
			return pefo;
		}
	}

	DevMsg ("DSP: Warning, failed to allocate envelope follower.\n" );
	return NULL;
}

// values of L for CEFO_BITS_DIVIDE: L = (1 - 1/(1 << CEFO_BITS_DIVIDE)) 
// 1	L = 0.5
// 2	L = 0.75
// 3	L = 0.875
// 4	L = 0.9375
// 5	L = 0.96875
// 6	L = 0.984375
// 7	L = 0.9921875
// 8	L = 0.99609375
// 9	L = 0.998046875
// 10	L = 0.9990234375
// 11	L = 0.99951171875
// 12	L = 0.999755859375


// decay time constant for values of L, for E = 10^-3 = 60dB of attenuation
//
//	Neff = Ln E / Ln L  = -6.9077552 / Ln L
//
//  1	L = 0.5				Neff = 10 samples
//  2	L = 0.75			Neff = 24
//  3	L = 0.875			Neff = 51
//  4	L = 0.9375			Neff = 107
//  5	L = 0.96875			Neff = 217
//  6	L = 0.984375		Neff = 438
// 	7	L = 0.9921875		Neff = 880
// 	8	L = 0.99609375		Neff = 1764
//  9	L = 0.998046875		Neff = 3533
// 10	L = 0.9990234375	Neff = 7070
// 11	L = 0.99951171875	Neff = 14143
// 12	L = 0.999755859375	Neff = 28290

#define CEFO_BITS	11			// 14143 samples in gate window (3hz)

inline int EFO_GetNext( efo_t *pefo, int x )
{
	int r;
	int xa = abs(x);
	int xdif; 


	// get envelope:
	//		Cn = L * Cn-1 + ( 1 - L ) * |x|

	// which simplifies to:
	//		Cn = |x| + (Cn-1 - |x|) * L

	// for  0 < L < 1

	// increasing L increases time to rise or fall to a new input level

	// so: increasing CEFO_BITS_DIVIDE increases rise/fall time

	// where: L = (1 - 1/(1 << CEFO_BITS))
	// xdif = Cn-1 - |x|
	// so:    xdif * L = xdif - xdif / (1 << CEFO_BITS) = ((xdif << CEFO_BITS) - xdif ) >> CEFO_BITS

	xdif = pefo->xout - xa;

	pefo->xout = xa + (((xdif << CEFO_BITS) - xdif) >> CEFO_BITS);

	if ( pefo->bgate )
	{
		// gate

		bool bgateon_prev = pefo->bgateon;

		// gate hysteresis

		if (bgateon_prev)
			// gate was on - it's off only if amp drops below thresh_off
			pefo->bgateon = ( pefo->xout >= pefo->thresh_off );
		else
			// gate was off - it's on only if amp > thresh
			pefo->bgateon = ( pefo->xout >= pefo->thresh );
		
		if ( pefo->bgateon )
		{
			// gate is on

			if ( bgateon_prev && RMP_HitEnd( &pefo->rmp_attack ))
				return x;		// gate is fully on

			if ( !bgateon_prev )
			{
				// gate just turned on, start ramp attack

				// start attack from previous decay ramp if active
	
				r = RMP_HitEnd( &pefo->rmp_decay ) ? 0 : RMP_GetNext( &pefo->rmp_decay );
				RMP_SetEnd( &pefo->rmp_decay);

				// DevMsg ("GATE ON \n");

				RMP_Init( &pefo->rmp_attack, pefo->attack_time, r, PMAX, false);
	
				return (x * r) >> PBITS;
			}
			
			if ( !RMP_HitEnd( &pefo->rmp_attack ) )
			{
				r = RMP_GetNext( &pefo->rmp_attack );

				// gate is on and ramping up

				return (x * r) >> PBITS;
			}
			
		}
		else
		{
			// gate is fully off

			if ( !bgateon_prev && RMP_HitEnd( &pefo->rmp_decay))
				return 0;

			if ( bgateon_prev )
			{
				// gate just turned off, start ramp decay

				// start decay from previous attack ramp if active
	
				r = RMP_HitEnd( &pefo->rmp_attack ) ? PMAX : RMP_GetNext( &pefo->rmp_attack );
				RMP_SetEnd( &pefo->rmp_attack);

				RMP_Init( &pefo->rmp_decay, pefo->decay_time, r, 0, false);
				
				// DevMsg ("GATE OFF \n");

				// if exponential set, gate has exponential ramp down. Otherwise linear ramp down.

				if ( pefo->bexp )
					return ( (((x * r) >> PBITS) * r ) >> PBITS );
				else
					return (x * r) >> PBITS;

			}
			else if ( !RMP_HitEnd( &pefo->rmp_decay ) )
			{
				// gate is off and ramping down

				r = RMP_GetNext( &pefo->rmp_decay );

				
				// if exponential set, gate has exponential ramp down. Otherwise linear ramp down.

				if ( pefo->bexp )
					return ( (((x * r) >> PBITS) * r ) >> PBITS );
				else
					return (x * r) >> PBITS;
			}
		}

		return x;
	}

	return pefo->xout;
}

// batch version for performance

inline void EFO_GetNextN( efo_t *pefo, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = EFO_GetNext( pefo, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = EFO_GetNext( pefo, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = EFO_GetNext( pefo, pb->left );
			pb++;
		}
		return;
	}
}
// parameter order

typedef enum
{
	efo_ithreshold,
	efo_iattack,	
	efo_idecay,		
	efo_iexp,

	efo_cparam			// # of params

} efo_e;

// parameter ranges

prm_rng_t efo_rng[] = {

	{efo_cparam,		0, 0},			// first entry is # of parameters

	{efo_ithreshold,	-140.0, 0.0},	// gate threshold in db. if 0.0 then no gate.
	{efo_iattack,		0.0, 20000.0},	// attack time in milliseconds
	{efo_idecay,		0.0, 20000.0},	// envelope decay time in milliseconds
	{efo_iexp,			0.0, 1.0},		// if 1, use exponential decay ramp (for more realistic reverb tail)

};

efo_t * EFO_Params ( prc_t *pprc )
{
	efo_t *penv;

	float threshold		= Gain_To_Amplitude( dB_To_Gain(pprc->prm[efo_ithreshold]) );
	float attack		= pprc->prm[efo_iattack]/1000.0;
	float decay			= pprc->prm[efo_idecay]/1000.0;
	float fexp			= pprc->prm[efo_iexp];
	bool bexp;

	// check for no gate

	if ( pprc->prm[efo_ithreshold] == 0.0 )
		threshold = 0.0;

	bexp = fexp > 0.0 ? 1 : 0;

	penv = EFO_Alloc ( threshold, attack, decay, bexp );
	return penv;
}

inline void * EFO_VParams ( void *p ) 
{
	PRC_CheckParams( (prc_t *)p, efo_rng ); 
	return (void *) EFO_Params ((prc_t *)p); 
}

inline void EFO_Mod ( void *p, float v ) { return; }


///////////////////////////////////////////
// Chorus - lfo modulated delay
///////////////////////////////////////////


#define CCRSS		64				// max number chorus' active

struct crs_t
{
	bool fused;

	mdy_t *pmdy;						// modulatable delay
	lfo_t *plfo;						// modulating lfo

	int lfoprev;						// previous modulator value from lfo

};

crs_t crss[CCRSS];

void CRS_Init( crs_t *pcrs ) { if (pcrs) Q_memset( pcrs, 0, sizeof (crs_t) ); };
void CRS_Free( crs_t *pcrs ) 
{
	if (pcrs)
	{
		MDY_Free ( pcrs->pmdy );
		LFO_Free ( pcrs->plfo );
		Q_memset( pcrs, 0, sizeof (crs_t) ); 
	}
}


void CRS_InitAll() { for (int i = 0; i < CCRSS; i++) CRS_Init( &crss[i] ); }
void CRS_FreeAll() { for (int i = 0; i < CCRSS; i++) CRS_Free( &crss[i] ); }

// fstep is base pitch shift, ie: floating point step value, where 1.0 = +1 octave, 0.5 = -1 octave 
// lfotype is LFO_SIN, LFO_RND, LFO_TRI etc (LFO_RND for chorus, LFO_SIN for flange)
// fHz is modulation frequency in Hz
// depth is modulation depth, 0-1.0
// mix is mix of chorus and clean signal

#define CRS_DELAYMAX			100		// max milliseconds of sweepable delay
#define CRS_RAMPTIME			5		// milliseconds to ramp between new delay values

crs_t * CRS_Alloc( int lfotype, float fHz, float fdepth, float mix ) 
{
	
	int i;
	crs_t *pcrs;
	dly_t *pdly;
	mdy_t *pmdy;
	lfo_t *plfo;
	float ramptime;
	int D;

	// find free chorus slot

	for ( i = 0; i < CCRSS; i++ )
	{
		if ( !crss[i].fused )
			break;
	}

	if ( i == CCRSS ) 
	{
		DevMsg ("DSP: Warning, failed to allocate chorus.\n" );
		return NULL;
	}

	pcrs = &crss[i];

	CRS_Init ( pcrs );

	D = fdepth * MSEC_TO_SAMPS(CRS_DELAYMAX);		// sweep from 0 - n milliseconds

	ramptime = (float) CRS_RAMPTIME / 1000.0;				// # milliseconds to ramp between new values
	
	pdly = DLY_Alloc ( D, 0, 1, DLY_LINEAR );

	pmdy = MDY_Alloc ( pdly, ramptime, 0.0, 0.0, mix );

	plfo = LFO_Alloc ( lfotype, fHz, false, 1.0 );
	
	if ( !plfo || !pmdy )
	{
		LFO_Free ( plfo );
		MDY_Free ( pmdy );
		DevMsg ("DSP: Warning, failed to allocate lfo or mdy for chorus.\n" );
		return NULL;
	}

	pcrs->pmdy = pmdy;
	pcrs->plfo = plfo;
	pcrs->fused = true;

	return pcrs;
}

// return next chorused sample (modulated delay) mixed with input sample

inline int CRS_GetNext( crs_t *pcrs, int x ) 
{
	int l;
	int y;
	
	// get current mod delay value

	y = MDY_GetNext ( pcrs->pmdy, x );

	// get next lfo value for modulation
	// note: lfo must return 0 as first value

	l = LFO_GetNext ( pcrs->plfo, x );

	// if modulator has changed, change mdy

	if ( l != pcrs->lfoprev )
	{
		// calculate new tap starts at D)

		int D = pcrs->pmdy->pdly->D0;
		int tap;

		// lfo should always output values 0 <= l <= LFOMAX

		if (l < 0)
			l = 0;

		tap = D - ((l * D) >> LFOBITS);

		MDY_ChangeVal ( pcrs->pmdy, tap );

		pcrs->lfoprev = l;
	}

	return y;
}

// batch version for performance

inline void CRS_GetNextN( crs_t *pcrs, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = CRS_GetNext( pcrs, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = CRS_GetNext( pcrs, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = CRS_GetNext( pcrs, pb->left );
			pb++;
		}
		return;
	}
}

// parameter order

typedef enum {

	crs_ilfotype,
	crs_irate,
	crs_idepth,
	crs_imix,
	
	crs_cparam

} crs_e;


// parameter ranges

prm_rng_t crs_rng[] = {

	{crs_cparam,	0, 0},				// first entry is # of parameters
		
	{crs_ilfotype,	0, LFO_MAX},		// lfotype is LFO_SIN, LFO_RND, LFO_TRI etc (LFO_RND for chorus, LFO_SIN for flange)
	{crs_irate,		0.0, 1000.0},		// rate is modulation frequency in Hz
	{crs_idepth,	0.0, 1.0},			// depth is modulation depth, 0-1.0
	{crs_imix,	    0.0, 1.0},			// mix is mix of chorus and clean signal

};

// uses pitch, lfowav, rate, depth

crs_t * CRS_Params ( prc_t *pprc )
{
	crs_t *pcrs;
	
	pcrs = CRS_Alloc ( pprc->prm[crs_ilfotype], pprc->prm[crs_irate], pprc->prm[crs_idepth], pprc->prm[crs_imix] );

	return pcrs;
}

inline void * CRS_VParams ( void *p ) 
{
	PRC_CheckParams ( (prc_t *)p, crs_rng ); 
	return (void *) CRS_Params ((prc_t *)p); 
}

inline void CRS_Mod ( void *p, float v ) { return; }


////////////////////////////////////////////////////
// amplifier - modulatable gain, distortion
////////////////////////////////////////////////////

#define CAMPS		64				// max number amps active

#define AMPSLEW		10				// milliseconds of slew time between gain changes

struct amp_t
{
	bool fused;

	int gain;					// amplification 0-6.0 * PMAX
	int gain_max;				// original gain setting
	int distmix;				// 0-1.0 mix of distortion with clean * PMAX
	int vfeed;					// 0-1.0 feedback with distortion * PMAX
	int vthresh;					// amplitude of clipping threshold 0..32768

	
	bool fchanging;				// true if modulating to new amp value
	float ramptime;				// ramp 'glide' time - time in seconds to change between values
	int mtime;					// time in samples between amp changes. 0 implies no self-modulating
	int mtimecur;				// current time in samples until next amp change
	int depth;					// modulate amp from A to A - (A*depth)  depth 0-1.0
	bool brand;					// if true, use random modulation otherwise alternate btwn max/min
	rmp_t rmp_interp;			// interpolation ramp 0...PMAX

};

amp_t amps[CAMPS];

void AMP_Init( amp_t *pamp ) { if (pamp) Q_memset( pamp, 0, sizeof (amp_t) ); };
void AMP_Free( amp_t *pamp ) 
{
	if (pamp)
	{
		Q_memset( pamp, 0, sizeof (amp_t) ); 
	}
}


void AMP_InitAll() { for (int i = 0; i < CAMPS; i++) AMP_Init( &amps[i] ); }
void AMP_FreeAll() { for (int i = 0; i < CAMPS; i++) AMP_Free( &amps[i] ); }


amp_t * AMP_Alloc( float gain, float vthresh, float distmix, float vfeed, float ramptime, float modtime, float depth, bool brand ) 
{
	int i;
	amp_t *pamp;

	// find free amp slot

	for ( i = 0; i < CAMPS; i++ )
	{
		if ( !amps[i].fused )
			break;
	}

	if ( i == CAMPS ) 
	{
		DevMsg ("DSP: Warning, failed to allocate amp.\n" );
		return NULL;
	}

	pamp = &amps[i];

	AMP_Init ( pamp );

	pamp->fused = true;

	pamp->gain = gain * PMAX;
	pamp->gain_max = gain * PMAX;
	pamp->distmix = distmix * PMAX;
	pamp->vfeed = vfeed * PMAX;
	pamp->vthresh = vthresh * 32767.0;

	// modrate,	0.01, 200.0},		// frequency at which amplitude values change to new random value. 0 is no self-modulation
	// moddepth,	0.0, 1.0},			// how much amplitude changes (decreases) from current value (0-1.0) 
	// modglide,	0.01, 100.0},		// glide time between mapcur and ampnew in milliseconds

	pamp->ramptime = ramptime;
	pamp->mtime = SEC_TO_SAMPS(modtime);
	pamp->mtimecur = pamp->mtime;
	pamp->depth = depth * PMAX;
	pamp->brand = brand;

	return pamp;
}

// return next amplified sample

inline int AMP_GetNext( amp_t *pamp, int x ) 
{
	int y = x;
	int d;

	// if distortion is on, add distortion, feedback

	if ( pamp->vthresh < PMAX && pamp->distmix )
	{
		int vthresh = pamp->vthresh; 

/* 		if ( pamp->vfeed > 0.0 )
		{
			// UNDONE: feedback 
		}
*/
		// clip distort

		d = ( y > vthresh ? vthresh : ( y < -vthresh ? -vthresh : y));

		// mix distorted with clean (1.0 = full distortion)

		if ( pamp->distmix < PMAX )
			y = y + (((d - y) * pamp->distmix ) >> PBITS);
		else
			y = d;
	}	

	// get output for current gain value

	int xout = (y * pamp->gain) >> PBITS;
	
	if ( !pamp->fchanging && !pamp->mtime )
	{
		// if not modulating and not self modulating, return right away

		return xout;
	}

	if (pamp->fchanging)
	{
		// modulating...
	
		// get next gain value

		pamp->gain = RMP_GetNext( &pamp->rmp_interp ); // 0...next gain

		if ( RMP_HitEnd( &pamp->rmp_interp ) )
		{
			// done. 

			pamp->fchanging = false;
		}
	}
	
	// if self-modulating and timer has expired, get next change

	if ( pamp->mtime && !pamp->mtimecur-- )
	{
		pamp->mtimecur = pamp->mtime;

		int gain_new;
		int G1;
		int G2 = pamp->gain_max;

		// modulate between 0 and 100% of gain_max
		
		G1 = pamp->gain_max - ((pamp->gain_max * pamp->depth) >> PBITS);

		if (pamp->brand)
		{
			gain_new = RandomInt( min(G1,G2), max(G1,G2) );
		}
		else
		{
			// alternate between min & max

			gain_new = (pamp->gain == G1 ? G2 : G1);
		}

		// set up modulation to new value

		pamp->fchanging = true;

		// init gain ramp - always hit target

		RMP_Init ( &pamp->rmp_interp, pamp->ramptime, pamp->gain, gain_new, false );
	}

	return xout;

}

// batch version for performance

inline void AMP_GetNextN( amp_t *pamp, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	int count = SampleCount;
	portable_samplepair_t *pb = pbuffer;
	
	switch (op)
	{
	default:
	case OP_LEFT:
		while (count--)
		{
			pb->left = AMP_GetNext( pamp, pb->left );
			pb++;
		}
		return;
	case OP_RIGHT:
		while (count--)
		{
			pb->right = AMP_GetNext( pamp, pb->right );
			pb++;
		}
		return;
	case OP_LEFT_DUPLICATE:
		while (count--)
		{
			pb->left = pb->right = AMP_GetNext( pamp, pb->left );
			pb++;
		}
		return;
	}
}

inline void AMP_Mod( amp_t *pamp, float v )
{
}


// parameter order

typedef enum {

	amp_gain,
	amp_vthresh,
	amp_distmix,
	amp_vfeed,
	amp_imodrate,
	amp_imoddepth,
	amp_imodglide,
	amp_irand,
	amp_cparam

} amp_e;


// parameter ranges

prm_rng_t amp_rng[] = {

	{amp_cparam,	0, 0},				// first entry is # of parameters
		
	{amp_gain,		0.0, 1000.0},		// amplification		
	{amp_vthresh,	0.0, 1.0},			// threshold for distortion (1.0 = no distortion)
	{amp_distmix,	0.0, 1.0},			// mix of clean and distortion (1.0 = full distortion, 0.0 = full clean)
	{amp_vfeed,	    0.0, 1.0},			// distortion feedback

	{amp_imodrate,	0.0, 200.0},		// frequency at which amplitude values change to new random value. 0 is no self-modulation
	{amp_imoddepth,	0.0, 1.0},			// how much amplitude changes (decreases) from current value (0-1.0) 
	{amp_imodglide,	0.01, 100.0},		// glide time between mapcur and ampnew in milliseconds
	{amp_irand,		0.0, 1.0},			// if 1, use random modulation otherwise alternate from max-min-max
};

amp_t * AMP_Params ( prc_t *pprc )
{
	amp_t *pamp;

	float ramptime = 0.0;
	float modtime = 0.0;
	float depth = 0.0;
	float rand = pprc->prm[amp_irand];
	bool brand;

	if (pprc->prm[amp_imodrate] > 0.0)
	{
		ramptime = pprc->prm[amp_imodglide] / 1000.0;			// get ramp time in seconds
		modtime = 1.0 / max((double)pprc->prm[amp_imodrate], 0.01);		// time between modulations in seconds
		depth = pprc->prm[amp_imoddepth];						// depth of modulations 0-1.0
	}

	brand = rand > 0.0 ? 1 : 0;

	pamp = AMP_Alloc ( pprc->prm[amp_gain], pprc->prm[amp_vthresh], pprc->prm[amp_distmix], pprc->prm[amp_vfeed], 
		ramptime, modtime, depth, brand );

	return pamp;
}

inline void * AMP_VParams ( void *p ) 
{
	PRC_CheckParams ( (prc_t *)p, amp_rng ); 
	return (void *) AMP_Params ((prc_t *)p); 
}


/////////////////
// NULL processor
/////////////////

struct nul_t
{
	int type;
};

nul_t nuls[] = {{0}};

void NULL_Init ( nul_t *pnul ) { }
void NULL_InitAll( ) { }
void NULL_Free ( nul_t *pnul ) { }
void NULL_FreeAll ( ) { }
nul_t *NULL_Alloc ( ) { return &nuls[0]; }

inline int NULL_GetNext ( void *p, int x) { return x; }

inline void NULL_GetNextN( nul_t *pnul, portable_samplepair_t *pbuffer, int SampleCount, int op ) { return; }

inline void NULL_Mod ( void *p, float v ) { return; }

inline void * NULL_VParams ( void *p ) { return (void *) (&nuls[0]); }

//////////////////////////
// DSP processors presets - see dsp_presets.txt
//////////////////////////




// init array of processors - first store pfnParam, pfnGetNext and pfnFree functions for type,
// then call the pfnParam function to initialize each processor

// prcs - an array of prc structures, all with initialized params
// count - number of elements in the array

// returns false if failed to init one or more processors

bool PRC_InitAll( prc_t *prcs, int count ) 
{ 
	int i;
	prc_Param_t pfnParam;			// allocation function - takes ptr to prc, returns ptr to specialized data struct for proc type
	prc_GetNext_t pfnGetNext;		// get next function
	prc_GetNextN_t pfnGetNextN;		// get next function, batch version
	prc_Free_t pfnFree;	
	prc_Mod_t pfnMod;	

	bool fok = true;;

	if ( count == 0 )
		count = 1;

	// set up pointers to XXX_Free, XXX_GetNext and XXX_Params functions

	for (i = 0; i < count; i++)
	{
		switch (prcs[i].type)
		{
		default:
		case PRC_NULL:
			pfnFree		= (prc_Free_t)NULL_Free;
			pfnGetNext	= (prc_GetNext_t)NULL_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)NULL_GetNextN;
			pfnParam	= NULL_VParams;
			pfnMod		= (prc_Mod_t)NULL_Mod;
			break;
		case PRC_DLY:
			pfnFree		= (prc_Free_t)DLY_Free;
			pfnGetNext	= (prc_GetNext_t)DLY_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)DLY_GetNextN;
			pfnParam	= DLY_VParams;
			pfnMod		= (prc_Mod_t)DLY_Mod;
			break;
		case PRC_RVA:
			pfnFree		= (prc_Free_t)RVA_Free;
			pfnGetNext	= (prc_GetNext_t)RVA_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)RVA_GetNextN;
			pfnParam	= RVA_VParams;
			pfnMod		= (prc_Mod_t)RVA_Mod;
			break;
		case PRC_FLT:
			pfnFree		= (prc_Free_t)FLT_Free;
			pfnGetNext	= (prc_GetNext_t)FLT_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)FLT_GetNextN;
			pfnParam	= FLT_VParams;
			pfnMod		= (prc_Mod_t)FLT_Mod;
			break;
		case PRC_CRS:
			pfnFree		= (prc_Free_t)CRS_Free;
			pfnGetNext	= (prc_GetNext_t)CRS_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)CRS_GetNextN;
			pfnParam	= CRS_VParams;
			pfnMod		= (prc_Mod_t)CRS_Mod;
			break;
		case PRC_PTC:
			pfnFree		= (prc_Free_t)PTC_Free;
			pfnGetNext	= (prc_GetNext_t)PTC_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)PTC_GetNextN;
			pfnParam	= PTC_VParams;
			pfnMod		= (prc_Mod_t)PTC_Mod;
			break;
		case PRC_ENV:
			pfnFree		= (prc_Free_t)ENV_Free;
			pfnGetNext	= (prc_GetNext_t)ENV_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)ENV_GetNextN;
			pfnParam	= ENV_VParams;
			pfnMod		= (prc_Mod_t)ENV_Mod;
			break;
		case PRC_LFO:
			pfnFree		= (prc_Free_t)LFO_Free;
			pfnGetNext	= (prc_GetNext_t)LFO_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)LFO_GetNextN;
			pfnParam	= LFO_VParams;
			pfnMod		= (prc_Mod_t)LFO_Mod;
			break;
		case PRC_EFO:
			pfnFree		= (prc_Free_t)EFO_Free;
			pfnGetNext	= (prc_GetNext_t)EFO_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)EFO_GetNextN;
			pfnParam	= EFO_VParams;
			pfnMod		= (prc_Mod_t)EFO_Mod;
			break;
		case PRC_MDY:
			pfnFree		= (prc_Free_t)MDY_Free;
			pfnGetNext	= (prc_GetNext_t)MDY_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)MDY_GetNextN;
			pfnParam	= MDY_VParams;
			pfnMod		= (prc_Mod_t)MDY_Mod;
			break;
		case PRC_DFR:
			pfnFree		= (prc_Free_t)DFR_Free;
			pfnGetNext	= (prc_GetNext_t)DFR_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)DFR_GetNextN;
			pfnParam	= DFR_VParams;
			pfnMod		= (prc_Mod_t)DFR_Mod;
			break;
		case PRC_AMP:
			pfnFree		= (prc_Free_t)AMP_Free;
			pfnGetNext	= (prc_GetNext_t)AMP_GetNext;
			pfnGetNextN	= (prc_GetNextN_t)AMP_GetNextN;
			pfnParam	= AMP_VParams;
			pfnMod		= (prc_Mod_t)AMP_Mod;
			break;
		}

		// set up function pointers

		prcs[i].pfnParam	= pfnParam;
		prcs[i].pfnGetNext	= pfnGetNext;
		prcs[i].pfnGetNextN	= pfnGetNextN;
		prcs[i].pfnFree		= pfnFree;
		prcs[i].pfnMod		= pfnMod;

		// call param function, store pdata for the processor type

		prcs[i].pdata = pfnParam ( (void *) (&prcs[i]) );

		if ( !prcs[i].pdata )
			fok = false;
	}

	return fok;
}

// free individual processor's data

void PRC_Free ( prc_t *pprc )
{
	if ( pprc->pfnFree && pprc->pdata )
		pprc->pfnFree ( pprc->pdata );
}

// free all processors for supplied array
// prcs - array of processors
// count - elements in array

void PRC_FreeAll ( prc_t *prcs, int count )
{
	for (int i = 0; i < count; i++)
		PRC_Free( &prcs[i] );
}

// get next value for processor - (usually called directly by PSET_GetNext)

inline int PRC_GetNext ( prc_t *pprc, int x )
{
	return pprc->pfnGetNext ( pprc->pdata, x );
}

// automatic parameter range limiting
// force parameters between specified min/max in param_rng

void PRC_CheckParams ( prc_t *pprc, prm_rng_t *prng )
{
	// first entry in param_rng is # of parameters

	int cprm = prng[0].iprm;

	for (int i = 0; i < cprm; i++)
	{
		// if parameter is 0.0, always allow it (this is 'off' for most params)

		if ( pprc->prm[i] != 0.0 && (pprc->prm[i] > prng[i+1].hi || pprc->prm[i] < prng[i+1].lo) )
		{
			DevMsg ("DSP: Warning, clamping out of range parameter.\n" );
			pprc->prm[i] = clamp (pprc->prm[i], prng[i+1].lo, prng[i+1].hi);
		}
	}
}


// DSP presets

// A dsp preset comprises one or more dsp processors in linear, parallel or feedback configuration

// preset configurations
//
#define PSET_SIMPLE		0

// x(n)--->P(0)--->y(n)

#define PSET_LINEAR		1

// x(n)--->P(0)-->P(1)-->...P(m)--->y(n)


#define PSET_PARALLEL2	5
	
// x(n)--->P(0)-->(+)-->y(n)
//      	       ^
//		           | 
// x(n)--->P(1)-----

#define PSET_PARALLEL4	6

// x(n)--->P(0)-->P(1)-->(+)-->y(n)
//      				  ^
//		                  | 
// x(n)--->P(2)-->P(3)-----

#define PSET_PARALLEL5	7

// x(n)--->P(0)-->P(1)-->(+)-->P(4)-->y(n)
//      				  ^
//		                  | 
// x(n)--->P(2)-->P(3)-----

#define PSET_FEEDBACK	8
 
// x(n)-P(0)--(+)-->P(1)-->P(2)---->y(n)
//             ^				|
//             |                v 
//		       -----P(4)<--P(3)--

#define PSET_FEEDBACK3	9
 
// x(n)---(+)-->P(0)--------->y(n)
//         ^                |
//         |                v 
//		   -----P(2)<--P(1)--

#define PSET_FEEDBACK4	10

// x(n)---(+)-->P(0)-------->P(3)--->y(n)
//         ^              |
//         |              v 
//		   ---P(2)<--P(1)--

#define PSET_MOD		11

//
// x(n)------>P(1)--P(2)--P(3)--->y(n)
//                    ^     
// x(n)------>P(0)....:

#define PSET_MOD2		12

//
// x(n)-------P(1)-->y(n)
//              ^     
// x(n)-->P(0)..:


#define PSET_MOD3		13

//
// x(n)-------P(1)-->P(2)-->y(n)
//              ^     
// x(n)-->P(0)..:


#define CPSETS			64				// max number of presets simultaneously active

#define CPSET_PRCS		5				// max # of processors per dsp preset
#define CPSET_STATES	(CPSET_PRCS+3)	// # of internal states

// NOTE: do not reorder members of pset_t - g_psettemplates relies on it!!!

struct pset_t
{
	int type;							// preset configuration type
	int cprcs;							// number of processors for this preset

	prc_t prcs[CPSET_PRCS];				// processor preset data

	float mix_min;						// min dsp mix at close range
	float mix_max;						// max dsp mix at long range
	float db_min;						// if sndlvl of a new sound is < db_min, reduce mix_min/max by db_mixdrop					
	float db_mixdrop;					// reduce mix_min/max by n% if sndlvl of new sound less than db_min
	float duration;						// if > 0, duration of preset in seconds (duration 0 = infinite)
	float fade;							// fade out time, exponential fade
	
	int csamp_duration;					// duration counter # samples

	int w[CPSET_STATES];				// internal states
	int fused;
};

pset_t psets[CPSETS];

pset_t *g_psettemplates = NULL;
int	g_cpsettemplates = 0;

// returns true if preset will expire after duration

bool PSET_IsOneShot( pset_t *ppset )
{
	return ppset->duration > 0.0;
}

// return true if preset is no longer active - duration has expired

bool PSET_HasExpired( pset_t *ppset )
{
	if (!PSET_IsOneShot( ppset ))
		return false;

	return ppset->csamp_duration <= 0;
}

// if preset is oneshot, update duration counter by SampleCount samples

void PSET_UpdateDuration( pset_t *ppset, int SampleCount )
{		
	if ( PSET_IsOneShot( ppset ) )
	{
		// if oneshot preset and not expired, decrement sample count

		if (ppset->csamp_duration > 0)
			ppset->csamp_duration -= SampleCount;
	}
}

// A dsp processor (prc) performs a single-sample function, such as pitch shift, delay, reverb, filter


// init a preset - just clear state array

void PSET_Init( pset_t *ppset ) 
{ 
	// clear state array

	if (ppset)
		Q_memset( ppset->w, 0, sizeof (int) * (CPSET_STATES) ); 
}

// clear runtime slots

void PSET_InitAll( void )
{
	for (int i = 0; i < CPSETS; i++)
		Q_memset( &psets[i], 0, sizeof(pset_t));
}

// free the preset - free all processors

void PSET_Free( pset_t *ppset ) 
{ 
	if (ppset)
	{
		// free processors

		PRC_FreeAll ( ppset->prcs, ppset->cprcs );

		// clear

		Q_memset( ppset, 0, sizeof (pset_t));
	}
}

void PSET_FreeAll() { for (int i = 0; i < CPSETS; i++) PSET_Free( &psets[i] ); };

// return preset struct, given index into preset template array
// NOTE: should not ever be more than 2 or 3 of these active simultaneously

pset_t * PSET_Alloc ( int ipsettemplate )
{
	pset_t *ppset;
	bool fok;

	// don't excede array bounds

	if ( ipsettemplate >= g_cpsettemplates)
		ipsettemplate = 0;

	// find free slot
	int i = 0;
	for (i = 0; i < CPSETS; i++)
	{
		if ( !psets[i].fused )
			break;
	}

	if ( i == CPSETS )
		return NULL;

	if (das_debug.GetInt())
	{
		int nSlots = 0;
		for ( int j = 0; j < CPSETS; j++)
		{
			if ( psets[j].fused )
				nSlots++;
		}
		DevMsg("total preset slots used: %d \n", nSlots );
	}
	

	ppset = &psets[i];
	
	// clear preset
	
	Q_memset(ppset, 0, sizeof(pset_t));

	// copy template into preset

	*ppset = g_psettemplates[ipsettemplate];

	ppset->fused = true;

	// clear state array

	PSET_Init ( ppset );
	
	// init all processors, set up processor function pointers

	fok = PRC_InitAll( ppset->prcs, ppset->cprcs );

	if ( !fok )
	{
		// failed to init one or more processors
		Warning( "Sound DSP: preset failed to init.\n");
		PRC_FreeAll ( ppset->prcs, ppset->cprcs );
		return NULL;
	}

	// if preset has duration, setup duration sample counter

	if ( PSET_IsOneShot( ppset ) )
	{
		ppset->csamp_duration = SEC_TO_SAMPS( ppset->duration );
	}

	return ppset;
}

// batch version of PSET_GetNext for linear array of processors.  For performance.

// ppset - preset array
// pbuffer - input sample data 
// SampleCount - size of input buffer
// OP:	OP_LEFT				- process left channel in place
//		OP_RIGHT			- process right channel in place
//		OP_LEFT_DUPLICATe	- process left channel, duplicate into right

inline void PSET_GetNextN( pset_t *ppset, portable_samplepair_t *pbuffer, int SampleCount, int op )
{
	portable_samplepair_t *pbf = pbuffer;
	prc_t *pprc;
	int count = ppset->cprcs;

	switch ( ppset->type )
	{
		default:
		case PSET_SIMPLE:
		{
			// x(n)--->P(0)--->y(n)

			ppset->prcs[0].pfnGetNextN (ppset->prcs[0].pdata, pbf, SampleCount, op);
			return;
		}
		case PSET_LINEAR:
		{

			//      w0     w1     w2 
			// x(n)--->P(0)-->P(1)-->...P(count-1)--->y(n)

			//      w0     w1     w2     w3     w4     w5
			// x(n)--->P(0)-->P(1)-->P(2)-->P(3)-->P(4)-->y(n)

			// call batch processors in sequence - no internal state for batch processing

			// point to first processor

			pprc = &ppset->prcs[0];

			for (int i = 0; i < count; i++)
			{
				pprc->pfnGetNextN (pprc->pdata, pbf, SampleCount, op);
				pprc++;
			}

		return;
		}	
	}
}


// Get next sample from this preset.  called once for every sample in buffer
// ppset is pointer to preset
// x is input sample

inline int PSET_GetNext ( pset_t *ppset, int x )
{

	// pset_simple and pset_linear have no internal state:
	// this is REQUIRED for all presets that have a batch getnextN equivalent!

	if ( ppset->type == PSET_SIMPLE )
	{
		// x(n)--->P(0)--->y(n)

		return ppset->prcs[0].pfnGetNext (ppset->prcs[0].pdata, x);
	}
	
	prc_t *pprc;
	int count = ppset->cprcs;
	
	if ( ppset->type == PSET_LINEAR )
	{
		int y = x; 

		//      w0     w1     w2 
		// x(n)--->P(0)-->P(1)-->...P(count-1)--->y(n)

		//      w0     w1     w2     w3     w4     w5
		// x(n)--->P(0)-->P(1)-->P(2)-->P(3)-->P(4)-->y(n)

		// call processors in reverse order, from count to 1
		
		//for (int i = count; i > 0; i--, pprc--)
		//	w[i] = pprc->pfnGetNext (pprc->pdata, w[i-1]);

		// return w[count];


		// point to first processor, update sequentially, no state preserved

		pprc = &ppset->prcs[0];

		switch (count)
		{
		default:
		case 5:
			y = pprc->pfnGetNext (pprc->pdata, y);
			pprc++;
		case 4:
			y = pprc->pfnGetNext (pprc->pdata, y);
			pprc++;
		case 3:
			y = pprc->pfnGetNext (pprc->pdata, y);
			pprc++;
		case 2:
			y = pprc->pfnGetNext (pprc->pdata, y);
			pprc++;
		case 1:
		case 0:
			y = pprc->pfnGetNext (pprc->pdata, y);
		}

		return y;	
	}

	// all other preset types have internal state:

	// initialize 0'th element of state array

	int *w = ppset->w;
	w[0] = x;

	switch ( ppset->type )
	{
	default:
	
	case PSET_PARALLEL2:
		{	//     w0      w1    w3
			// x(n)--->P(0)-->(+)-->y(n)
			//      	       ^
			//	   w0      w2  | 
			// x(n)--->P(1)-----

			pprc = &ppset->prcs[0];

			w[3] = w[1] + w[2];

			w[1] = pprc->pfnGetNext( pprc->pdata, w[0] );
			pprc++;
			w[2] = pprc->pfnGetNext( pprc->pdata, w[0] );

			return w[3];
		}

	case PSET_PARALLEL4:
		{	//     w0      w1     w2    w5
			// x(n)--->P(0)-->P(1)-->(+)-->y(n)
			//      				  ^
			//	   w0      w3     w4  | 
			// x(n)--->P(2)-->P(3)-----


			pprc = &ppset->prcs[0];

			w[5] = w[2] + w[4];

			w[2] = pprc[1].pfnGetNext( pprc[1].pdata, w[1] );
			w[4] = pprc[3].pfnGetNext( pprc[3].pdata, w[3] );

			w[1] = pprc[0].pfnGetNext( pprc[0].pdata, w[0] );
			w[3] = pprc[2].pfnGetNext( pprc[2].pdata, w[0] );

			return w[5];
		}

	case PSET_PARALLEL5:
		{	//     w0      w1     w2    w5     w6
			// x(n)--->P(0)-->P(1)-->(+)--P(4)-->y(n)
			//      				  ^
			//	   w0      w3     w4  | 
			// x(n)--->P(2)-->P(3)-----

			pprc = &ppset->prcs[0];

			w[5] = w[2] + w[4];

			w[2] = pprc[1].pfnGetNext( pprc[1].pdata, w[1] );
			w[4] = pprc[3].pfnGetNext( pprc[3].pdata, w[3] );

			w[1] = pprc[0].pfnGetNext( pprc[0].pdata, w[0] );
			w[3] = pprc[2].pfnGetNext( pprc[2].pdata, w[0] );

			return pprc[4].pfnGetNext( pprc[4].pdata, w[5] );
		}

	case PSET_FEEDBACK:
		{
			//    w0    w1   w2     w3      w4    w7
			// x(n)-P(0)--(+)-->P(1)-->P(2)-->---->y(n)
			//             ^				|
			//             |  w6     w5     v 
			//		       -----P(4)<--P(3)--

			pprc = &ppset->prcs[0];
			
			// start with adders
			
			w[2] = w[1] + w[6];

			// evaluate in reverse order

			w[6] = pprc[4].pfnGetNext( pprc[4].pdata, w[5] );
			w[5] = pprc[3].pfnGetNext( pprc[3].pdata, w[4] );

			w[4] = pprc[2].pfnGetNext( pprc[2].pdata, w[3] );
			w[3] = pprc[1].pfnGetNext( pprc[1].pdata, w[2] );
			w[1] = pprc[0].pfnGetNext( pprc[0].pdata, w[0] );

			return w[4];
		}
	case PSET_FEEDBACK3:
		{
			//     w0     w1     w2
			// x(n)---(+)-->P(0)--------->y(n)
			//         ^                |
			//         |  w4     w3     v 
			//		   -----P(2)<--P(1)--
			
			pprc = &ppset->prcs[0];
			
			// start with adders
			
			w[1] = w[0] + w[4];

			// evaluate in reverse order

			w[4] = pprc[2].pfnGetNext( pprc[2].pdata, w[3] );
			w[3] = pprc[1].pfnGetNext( pprc[1].pdata, w[2] );
			w[2] = pprc[0].pfnGetNext( pprc[0].pdata, w[1] );
			
			return w[2];
		}
	case PSET_FEEDBACK4:
		{
			//     w0    w1      w2           w5
			// x(n)---(+)-->P(0)-------->P(3)--->y(n)
			//         ^              |
			//         | w4     w3    v 
			//		   ---P(2)<--P(1)--

			pprc = &ppset->prcs[0];
			
			// start with adders
			
			w[1] = w[0] + w[4];

			// evaluate in reverse order

			w[5] = pprc[3].pfnGetNext( pprc[3].pdata, w[2] );
			w[4] = pprc[2].pfnGetNext( pprc[2].pdata, w[3] );
			w[3] = pprc[1].pfnGetNext( pprc[1].pdata, w[2] );
			w[2] = pprc[0].pfnGetNext( pprc[0].pdata, w[1] );
			
			return w[2];
		}
	case PSET_MOD:
		{
			//		w0		  w1    w3     w4
			// x(n)------>P(1)--P(2)--P(3)--->y(n)
			//      w0        w2  ^     
			// x(n)------>P(0)....:

			pprc = &ppset->prcs[0];

			w[4] = pprc[3].pfnGetNext( pprc[3].pdata, w[3] );

			w[3] = pprc[2].pfnGetNext( pprc[2].pdata, w[1] );

			// modulate processor 2

			pprc[2].pfnMod( pprc[2].pdata, ((float)w[2] / (float)PMAX));

			// get modulator output

			w[2] = pprc[0].pfnGetNext( pprc[0].pdata, w[0] );

			w[1] = pprc[1].pfnGetNext( pprc[1].pdata, w[0] );

			return w[4];
		}
	case PSET_MOD2:
		{
			//      w0           w2
			// x(n)---------P(1)-->y(n)
			//      w0    w1  ^     
			// x(n)-->P(0)....:

			pprc = &ppset->prcs[0];

			// modulate processor 1

			pprc[1].pfnMod( pprc[1].pdata, ((float)w[1] / (float)PMAX));

			// get modulator output

			w[1] = pprc[0].pfnGetNext( pprc[0].pdata, w[0] );

			w[2] = pprc[1].pfnGetNext( pprc[1].pdata, w[0] );

			return w[2];

		}
	case PSET_MOD3:
		{
			//      w0           w2      w3
			// x(n)----------P(1)-->P(2)-->y(n)
			//      w0    w1   ^     
			// x(n)-->P(0).....:

			pprc = &ppset->prcs[0];

			w[3] = pprc[2].pfnGetNext( pprc[2].pdata, w[2] );

			// modulate processor 1

			pprc[1].pfnMod( pprc[1].pdata, ((float)w[1] / (float)PMAX));

			// get modulator output

			w[1] = pprc[0].pfnGetNext( pprc[0].pdata, w[0] );

			w[2] = pprc[1].pfnGetNext( pprc[1].pdata, w[0] );

			return w[2];
		}
	}
}


/////////////
// DSP system
/////////////

// Main interface

//     Whenever the preset # changes on any of these processors, the old processor is faded out, new is faded in.
//     dsp_chan is optionally set when a sound is played - a preset is sent with the start_static/dynamic sound.
//  
// sound1---->dsp_chan-->  -------------(+)---->dsp_water--->dsp_player--->out
// sound2---->dsp_chan-->  |             |
// sound3--------------->  ----dsp_room---
//                         |             |		 
//                         --dsp_indirect-

//  dsp_room	- set this cvar to a preset # to change the room dsp.  room fx are more prevalent farther from player.
//					use: when player moves into a new room, all sounds played in room take on its reverberant character
//  dsp_water	- set this cvar (once) to a preset # for serial underwater sound.
//					use: when player goes under water, all sounds pass through this dsp (such as low pass filter)
//	dsp_player	- set this cvar to a preset # to cause all sounds to run through the effect (serial, in-line).
//					use: player is deafened, player fires special weapon, player is hit by special weapon.
//  dsp_facingaway- set this cvar to a preset # appropriate for sounds which are played facing away from player (weapon,voice)
//
//  dsp_spatial - set by system to create modulated spatial delays for left/right/front/back ears - delay value
//					modulates by distance to nearest l/r surface in world

// Dsp presets


ConVar dsp_room			("dsp_room", "0", FCVAR_DEMO );				// room dsp preset - sounds more distant from player (1ch)
ConVar dsp_water		("dsp_water", "14", FCVAR_DEMO );			// "14" underwater dsp preset - sound when underwater (1-2ch)
ConVar dsp_player		("dsp_player", "0", FCVAR_DEMO | FCVAR_SERVER_CAN_EXECUTE );			// dsp on player - sound when player hit by special device (1-2ch)
ConVar dsp_facingaway	("dsp_facingaway", "0", FCVAR_DEMO );		// "30" sounds that face away from player (weapons, voice) (1-4ch)
ConVar dsp_speaker		("dsp_speaker", "50", FCVAR_DEMO );			// "50" small distorted speaker sound (1ch)
ConVar dsp_spatial		("dsp_spatial", "40", FCVAR_DEMO );			// spatial delays for l/r front/rear ears
ConVar dsp_automatic	("dsp_automatic", "0", FCVAR_DEMO );			// automatic room type detection. if non zero, replaces dsp_room

int ipset_room_prev;
int ipset_water_prev;
int ipset_player_prev;
int ipset_facingaway_prev;
int ipset_speaker_prev;
int ipset_spatial_prev;
int ipset_automatic_prev;

// legacy room_type support

ConVar dsp_room_type		( "room_type", "0", FCVAR_DEMO );
int  ipset_room_typeprev;


// DSP processors

int idsp_room;
int idsp_water;
int idsp_player;
int idsp_facingaway;
int idsp_speaker;
int idsp_spatial;
int idsp_automatic;

ConVar dsp_off		("dsp_off", "0", FCVAR_CHEAT | FCVAR_ALLOWED_IN_COMPETITIVE );						// set to 1 to disable all dsp processing
ConVar dsp_slow_cpu ("dsp_slow_cpu", "0", FCVAR_ARCHIVE|FCVAR_DEMO );	// set to 1 if cpu bound - ie: does not process dsp_room fx
ConVar snd_profile	("snd_profile", "0", FCVAR_DEMO );					// 1 - profile dsp, 2 - mix, 3 - load sound, 4 - all sound
ConVar dsp_volume	("dsp_volume", "1.0", FCVAR_ARCHIVE|FCVAR_DEMO );	// 0.0 - 2.0; master dsp volume control
ConVar dsp_vol_5ch	("dsp_vol_5ch", "0.5", FCVAR_DEMO );					// 0.0 - 1.0; attenuate master dsp volume for 5ch surround
ConVar dsp_vol_4ch	("dsp_vol_4ch", "0.5", FCVAR_DEMO );					// 0.0 - 1.0; attenuate master dsp volume for 4ch surround
ConVar dsp_vol_2ch	("dsp_vol_2ch", "1.0", FCVAR_DEMO );					// 0.0 - 1.0; attenuate master dsp volume for 2ch surround

ConVar dsp_enhance_stereo("dsp_enhance_stereo", "0", FCVAR_ARCHIVE );	// 1) use dsp_spatial delays on all reverb channels

// DSP preset executor

#define CDSPS		32				// max number dsp executors active
#define DSPCHANMAX	5				// max number of channels dsp can process (allocs a separte processor for each chan)

struct dsp_t
{
	bool fused;
	int cchan;						// 1-5 channels, ie: mono, FrontLeft, FrontRight, RearLeft, RearRight, FrontCenter

	pset_t *ppset[DSPCHANMAX];		// current preset (1-5 channels)
	int ipset;						// current ipreset

	pset_t *ppsetprev[DSPCHANMAX];	// previous preset (1-5 channels)
	int ipsetprev;					// previous ipreset
	
	float xfade;					// crossfade time between previous preset and new
	float xfade_default;			// default xfade value, set in DSP_Alloc
	bool bexpfade;					// true if exponential crossfade

	int ipsetsav_oneshot;			// previous preset before one-shot preset was set

	rmp_t xramp;					// crossfade ramp
};

dsp_t dsps[CDSPS];

void DSP_Init( int idsp ) 
{ 
	dsp_t *pdsp;

	Assert( idsp < CDSPS );

	if (idsp < 0 || idsp >= CDSPS)
		return;
	
	pdsp = &dsps[idsp];

	Q_memset( pdsp, 0, sizeof (dsp_t) ); 
}

void DSP_Free( int idsp ) 
{
	dsp_t *pdsp;

	Assert( idsp < CDSPS );

	if (idsp < 0 || idsp >= CDSPS)
		return;
	
	pdsp = &dsps[idsp];

	for (int i = 0; i < pdsp->cchan; i++)
	{
		if ( pdsp->ppset[i] )
			PSET_Free( pdsp->ppset[i] );
		
		if ( pdsp->ppsetprev[i] )
			PSET_Free( pdsp->ppsetprev[i] );
	}

	Q_memset( pdsp, 0, sizeof (dsp_t) ); 
}

// Init all dsp processors - called once, during engine startup

void DSP_InitAll ( bool bLoadPresetFile )
{
	// only load template file on engine startup

	if ( bLoadPresetFile )
		DSP_LoadPresetFile();

	// order is important, don't rearange.

	FLT_InitAll();
	DLY_InitAll();
	RVA_InitAll();
	LFOWAV_InitAll();
	LFO_InitAll();
	
	CRS_InitAll();
	PTC_InitAll();
	ENV_InitAll();
	EFO_InitAll();
	MDY_InitAll();
	AMP_InitAll();

	PSET_InitAll();

	for (int idsp = 0; idsp < CDSPS; idsp++) 
		DSP_Init( idsp );
}

// free all resources associated with dsp - called once, during engine shutdown

void DSP_FreeAll (void)
{
	// order is important, don't rearange.

	for (int idsp = 0; idsp < CDSPS; idsp++) 
			DSP_Free( idsp );

	AMP_FreeAll();
	MDY_FreeAll();
	EFO_FreeAll();
	ENV_FreeAll();
	PTC_FreeAll();
	CRS_FreeAll();
	
	LFO_FreeAll();
	LFOWAV_FreeAll();
	RVA_FreeAll();
	DLY_FreeAll();
	FLT_FreeAll();
}


// allocate a new dsp processor chain, kill the old processor.  Called during dsp init only.
// ipset is new preset 
// xfade is crossfade time when switching between presets (milliseconds)
// cchan is how many simultaneous preset channels to allocate (1-4)
// return index to new dsp

int DSP_Alloc( int ipset, float xfade, int cchan )
{
	dsp_t *pdsp;
	int i;
	int idsp;
	int cchans = clamp( cchan, 1, DSPCHANMAX);

	// find free slot

	for ( idsp = 0; idsp < CDSPS; idsp++ )
	{
		if ( !dsps[idsp].fused )
			break;
	}

	if ( idsp >= CDSPS ) 
		return -1;

	pdsp = &dsps[idsp];

	DSP_Init ( idsp );
	
	pdsp->fused = true;

	pdsp->cchan = cchans;

	// allocate a preset processor for each channel

	pdsp->ipset = ipset;
	pdsp->ipsetprev = 0;
	pdsp->ipsetsav_oneshot = 0;

	for (i = 0; i < pdsp->cchan; i++)
	{
		pdsp->ppset[i] = PSET_Alloc ( ipset );
		pdsp->ppsetprev[i] = NULL;
	}

	// set up crossfade time in seconds

	pdsp->xfade = xfade / 1000.0;				
	pdsp->xfade_default = pdsp->xfade;

	RMP_SetEnd(&pdsp->xramp);

	return idsp;
}

// call modulation function of specified processor within dsp preset

// idsp - dsp preset
// channel - channel 1-5 (l,r,rl,rr,fc)
// iproc - which processor to change (normally 0)
// value - new parameter value for processor

// NOTE: routine returns with no result or error if any parameter is invalid.

void DSP_ChangePresetValue( int idsp, int channel, int iproc, float value )
{

	dsp_t *pdsp;		
	pset_t *ppset;		// preset
	prc_Mod_t pfnMod;	// modulation function

	if (idsp < 0 || idsp >= CDSPS)
		return;
	
	if (channel >= DSPCHANMAX)
		return;
	
	if (iproc >= CPSET_PRCS)
		return;

	// get ptr to processor preset

	pdsp = &dsps[idsp];

	// assert that this dsp processor has enough separate channels

	Assert(channel <= pdsp->cchan);	

	ppset = pdsp->ppset[channel];
	
	if (!ppset)
		return;

	// get ptr to modulation function

	pfnMod = ppset->prcs[iproc].pfnMod;

	if (!pfnMod)
		return;

	// call modulation function with new value

	pfnMod (ppset->prcs[iproc].pdata, value);
}


#define DSP_AUTOMATIC	1		// corresponds to Generic preset

// if dsp_room == DSP_AUTOMATIC, then use dsp_automatic value for dsp
// any subsequent reset of dsp_room will disable automatic room detection.

// return true if automatic room detection is enabled

bool DSP_CheckDspAutoEnabled( void )
{
	return (dsp_room.GetInt() == DSP_AUTOMATIC);	
}

// set dsp_automatic preset, used in place of dsp_room when automatic room detection enabled

void DSP_SetDspAuto( int dsp_preset )
{
	// set dsp_preset into dsp_automatic

	dsp_automatic.SetValue( dsp_preset );
}

// wrapper on dsp_room GetInt so that dsp_automatic can override

int dsp_room_GetInt ( void )
{
	// if dsp_automatic is not enabled, get room

	if (! DSP_CheckDspAutoEnabled())
		return dsp_room.GetInt();

	// automatic room detection is on, get dsp_automatic instead of dsp_room

	return dsp_automatic.GetInt();
}

// wrapper on idsp_room preset so that idsp_automatic can override

int Get_idsp_room ( void )
{

	// if dsp_automatic is not enabled, get room

	if ( !DSP_CheckDspAutoEnabled())
		return idsp_room;

	// automatic room detection is on, return dsp_automatic preset instead of dsp_room preset

	return idsp_automatic;
}


// free previous preset if not 0

inline void DSP_FreePrevPreset( dsp_t *pdsp )
{
	// free previous presets if non-null - ie: rapid change of preset just kills old without xfade

	if ( pdsp->ipsetprev )
	{
		for (int i = 0; i < pdsp->cchan; i++)
		{
			if ( pdsp->ppsetprev[i] )
			{
				PSET_Free( pdsp->ppsetprev[i] );
				pdsp->ppsetprev[i] = NULL;
			}
		}

		pdsp->ipsetprev = 0;
	}

}

extern ConVar dsp_mix_min;	
extern ConVar dsp_mix_max;
extern ConVar dsp_db_min;
extern ConVar dsp_db_mixdrop;

// alloc new preset if different from current
//		xfade from prev to new preset
//		free previous preset, copy current into previous, set up xfade from previous to new

void DSP_SetPreset( int idsp, int ipsetnew)
{
	dsp_t *pdsp;
	pset_t *ppsetnew[DSPCHANMAX];

	Assert (idsp >= 0 && idsp < CDSPS);

	pdsp = &dsps[idsp];

	// validate new preset range

	if ( ipsetnew >=  g_cpsettemplates || ipsetnew < 0 )
		return;

	// ignore if new preset is same as current preset

	if ( ipsetnew == pdsp->ipset )
		return;

	// alloc new presets (each channel is a duplicate preset)
	
	Assert (pdsp->cchan <= DSPCHANMAX);

	for (int i = 0; i < pdsp->cchan; i++)
	{
		ppsetnew[i] = PSET_Alloc ( ipsetnew );
		if ( !ppsetnew[i] )
		{
			DevMsg("WARNING: DSP preset failed to allocate.\n");
			return;
		}
	}

	Assert (pdsp);

	// free PREVIOUS previous preset if not 0

	DSP_FreePrevPreset( pdsp );

	for (int i = 0; i < pdsp->cchan; i++)
	{
		// current becomes previous

		pdsp->ppsetprev[i] = pdsp->ppset[i];
		
		// new becomes current

		pdsp->ppset[i] = ppsetnew[i];
	}
	
	pdsp->ipsetprev = pdsp->ipset;
	pdsp->ipset = ipsetnew;

	if ( idsp == idsp_room || idsp == idsp_automatic )
	{
		// set up new dsp mix min & max, db_min & db_drop params so that new channels get new mix values

		// NOTE: only new sounds will get the new mix min/max values set in their dspmix param
		// NOTE: so - no crossfade is needed between dspmix and dspmix prev, but this also means
		// NOTE: that currently playing ambients will not see changes to dspmix at all.
		
		float mix_min = pdsp->ppset[0]->mix_min;
		float mix_max = pdsp->ppset[0]->mix_max;
		float db_min =  pdsp->ppset[0]->db_min;
		float db_mixdrop = pdsp->ppset[0]->db_mixdrop;

		dsp_mix_min.SetValue( mix_min );
		dsp_mix_max.SetValue( mix_max );
		dsp_db_min.SetValue( db_min );
		dsp_db_mixdrop.SetValue( db_mixdrop );
	}

	RMP_SetEnd( &pdsp->xramp );
	
	// make sure previous dsp preset has data

	Assert (pdsp->ppsetprev[0]);

	// shouldn't be crossfading if current dsp preset == previous dsp preset

	Assert (pdsp->ipset != pdsp->ipsetprev);

	// if new preset is one-shot, keep previous preset to restore when one-shot times out
	// but: don't restore previous one-shots!

	pdsp->ipsetsav_oneshot = 0;

	if ( PSET_IsOneShot( pdsp->ppset[0] ) && !PSET_IsOneShot( pdsp->ppsetprev[0] ) )
			pdsp->ipsetsav_oneshot = pdsp->ipsetprev;
	
	// get new xfade time from previous preset (ie: fade out time). if 0 use default. if < 0, use exponential xfade

	if ( fabs(pdsp->ppsetprev[0]->fade) > 0.0 )
	{
		pdsp->xfade = fabs(pdsp->ppsetprev[0]->fade);
		pdsp->bexpfade = pdsp->ppsetprev[0]->fade < 0 ? 1 : 0;
	}
	else
	{
		// no previous preset - use defauts, set in DSP_Alloc

		pdsp->xfade = pdsp->xfade_default;
		pdsp->bexpfade = false;
	}

	RMP_Init( &(pdsp->xramp), pdsp->xfade, 0, PMAX, false );
}


#define DSP_AUTO_BASE		60		// presets 60-100 in g_psettemplates are reserved as autocreated presets
#define DSP_CAUTO_PRESETS	40		// must be same as DAS_CNODES!!!

// construct a dsp preset based on provided parameters,
// preset is constructed within g_psettemplates[] array.
// return preset #

// parameter batch

struct auto_params_t
{
	// passed in params

	bool bskyabove;			// true if sky is mostly above player
	int width;				// max width of room in inches
	int length;				// max length of room in inches (length always > width)
	int height;				// max height of room in inches
	float fdiffusion;		// diffusion of room 0..1.0
	float freflectivity;	// average reflectivity of all surfaces in room 0..1.0
	float surface_refl[6];	// reflectivity for left,right,front,back,ceiling,floor surfaces 0.0 for open surface (sky or no hit)

	// derived params

	int shape;				// ADSP_ROOM, etc 0...4
	int size;				// ADSP_SIZE_SMALL, etc	0...3
	int len;				// ADSP_LENGTH_SHORT, etc 0...3
	int wid;				// ADSP_WIDTH_NARROW, etc 0...3
	int ht;					// ADSP_HEIGHT_LOW, etc 0...3
	int reflectivity;		// ADSP_DULL, etc 0..3
	int diffusion;			// ADSP_EMPTY, etc 0...3
};


// select type 1..5 based on params
	// 1:simple reverb
	// 2:diffusor + reverb
	// 3:diffusor + delay + reverb
	// 4:simple delay
	// 5:diffusor + delay

#define AROOM_SMALL			(10.0 * 12.0)		// small room
#define	AROOM_MEDIUM		(20.0 * 12.0)		// medium room
#define AROOM_LARGE			(40.0 * 12.0)		// large room
#define AROOM_HUGE			(100.0 * 12.0)		// huge room
#define AROOM_GIGANTIC		(200.0 * 12.0)		// gigantic room

#define AROOM_DUCT_WIDTH	(4.0 * 12.0)		// max width for duct
#define AROOM_DUCT_HEIGHT	(6.0 * 12.0)

#define AROOM_HALL_WIDTH	(8.0 * 12.0)		// max width for hall
#define AROOM_HALL_HEIGHT	(16.0 * 12.0)		// max height for hall

#define AROOM_TUNNEL_WIDTH	(20.0 * 12.0)		// max width for tunnel
#define AROOM_TUNNEL_HEIGHT	(30.0 * 12.0)		// max height for tunnel

#define AROOM_STREET_WIDTH	(12.0 * 12.0)		// min width for street

#define AROOM_SHORT_LENGTH	(12.0 * 12.0)		// max length for short hall
#define AROOM_MEDIUM_LENGTH	(24.0 * 12.0)		// min length for medium hall
#define AROOM_LONG_LENGTH	(48.0 * 12.0)		// min length for long hall
#define AROOM_VLONG_LENGTH	(96.0 * 12.0)		// min length for very long hall
#define AROOM_XLONG_LENGTH	(192.0 * 12.0)		// min length for huge hall

#define AROOM_LOW_HEIGHT	(4.0 * 12.0)		// short ceiling
#define AROOM_MEDIUM_HEIGHT	(128)				// medium ceiling
#define AROOM_TALL_HEIGHT	(18.0 * 12.0)		// tall ceiling
#define AROOM_VTALL_HEIGHT	(32.0 * 12.0)		// very tall ceiling
#define AROOM_XTALL_HEIGHT   (64.0 * 12.0)		// huge tall ceiling

#define AROOM_NARROW_WIDTH	(6.0 * 12.0)		// narrow width
#define AROOM_MEDIUM_WIDTH	(12.0 * 12.0)		// medium width
#define AROOM_WIDE_WIDTH	(24.0 * 12.0)		// wide width
#define AROOM_VWIDE_WIDTH	(48.0 * 12.0)		// very wide
#define AROOM_XWIDE_WIDTH	(96.0 * 12.0)		// huge width

#define BETWEEN(a,b,c)			( ((a) > (b)) && ((a) <= (c)) )

#define ADSP_IsShaft(pa)		(pa->height > (3.0 * pa->length)) 
#define ADSP_IsRoom(pa)			(pa->length <= (2.5 * pa->width))
#define ADSP_IsHall(pa)			((pa->length > (2.5 * pa->width)) && (BETWEEN(pa->width, AROOM_DUCT_WIDTH, AROOM_HALL_WIDTH)))
#define ADSP_IsTunnel(pa)		((pa->length > (4.0 * pa->width)) && (pa->width > AROOM_HALL_WIDTH))
#define ADSP_IsDuct(pa)			((pa->length > (4.0 * pa->width)) && (pa->width <= AROOM_DUCT_WIDTH))

#define ADSP_IsCourtyard(pa)	(pa->length <= (2.5 * pa->width))
#define ADSP_IsAlley(pa)		((pa->length > (2.5 * pa->width)) && (pa->width <= AROOM_STREET_WIDTH))
#define ADSP_IsStreet(pa)		((pa->length > (2.5 * pa->width)) && (pa->width > AROOM_STREET_WIDTH))

#define ADSP_IsSmallRoom(pa)	(pa->length <= AROOM_SMALL)
#define ADSP_IsMediumRoom(pa)	((BETWEEN(pa->length, AROOM_SMALL, AROOM_MEDIUM)) ) // && (BETWEEN(pa->width, AROOM_SMALL, AROOM_MEDIUM)))
#define ADSP_IsLargeRoom(pa)	(BETWEEN(pa->length, AROOM_MEDIUM, AROOM_LARGE) ) // && BETWEEN(pa->width, AROOM_MEDIUM, AROOM_LARGE))
#define ADSP_IsHugeRoom(pa)		(BETWEEN(pa->length, AROOM_LARGE, AROOM_HUGE) ) // && BETWEEN(pa->width, AROOM_LARGE, AROOM_HUGE))
#define ADSP_IsGiganticRoom(pa)	((pa->length > AROOM_HUGE) ) // && (pa->width > AROOM_HUGE))

#define ADSP_IsShortLength(pa)	(pa->length <= AROOM_SHORT_LENGTH)
#define ADSP_IsMediumLength(pa)	(BETWEEN(pa->length, AROOM_SHORT_LENGTH, AROOM_MEDIUM_LENGTH))
#define ADSP_IsLongLength(pa)	(BETWEEN(pa->length, AROOM_MEDIUM_LENGTH, AROOM_LONG_LENGTH))
#define ADSP_IsVLongLength(pa)	(BETWEEN(pa->length, AROOM_LONG_LENGTH, AROOM_VLONG_LENGTH))
#define ADSP_IsXLongLength(pa)	(pa->length > AROOM_VLONG_LENGTH)

#define ADSP_IsLowHeight(pa)	(pa->height <= AROOM_LOW_HEIGHT)
#define ADSP_IsMediumHeight(pa)	(BETWEEN(pa->height, AROOM_LOW_HEIGHT, AROOM_MEDIUM_HEIGHT))
#define ADSP_IsTallHeight(pa)	(BETWEEN(pa->height, AROOM_MEDIUM_HEIGHT, AROOM_TALL_HEIGHT))
#define ADSP_IsVTallHeight(pa)	(BETWEEN(pa->height, AROOM_TALL_HEIGHT, AROOM_VTALL_HEIGHT))
#define ADSP_IsXTallHeight(pa)	(pa->height > AROOM_VTALL_HEIGHT)

#define ADSP_IsNarrowWidth(pa)	(pa->width <= AROOM_NARROW_WIDTH)
#define ADSP_IsMediumWidth(pa)	(BETWEEN(pa->width, AROOM_NARROW_WIDTH, AROOM_MEDIUM_WIDTH))
#define ADSP_IsWideWidth(pa)	(BETWEEN(pa->width, AROOM_MEDIUM_WIDTH, AROOM_WIDE_WIDTH))
#define ADSP_IsVWideWidth(pa)	(BETWEEN(pa->width, AROOM_WIDE_WIDTH, AROOM_VWIDE_WIDTH))
#define ADSP_IsXWideWidth(pa)	(pa->width > AROOM_VWIDE_WIDTH)

#define ADSP_IsInside(pa)		(!(pa->bskyabove))

// room diffusion

#define ADSP_EMPTY			0
#define ADSP_SPARSE			1
#define ADSP_CLUTTERED		2
#define ADSP_FULL			3
#define ADSP_DIFFUSION_MAX	4

#define AROOM_DIF_EMPTY		0.01	// 1% of space by volume is other objects
#define AROOM_DIF_SPARSE	0.1		// 10% "
#define AROOM_DIF_CLUTTERED	0.3		// 30% "
#define AROOM_DIF_FULL		0.5		// 50% "

#define ADSP_IsEmpty(pa)		(pa->fdiffusion <= AROOM_DIF_EMPTY)
#define ADSP_IsSparse(pa)		(BETWEEN(pa->fdiffusion, AROOM_DIF_EMPTY, AROOM_DIF_SPARSE))
#define ADSP_IsCluttered(pa)	(BETWEEN(pa->fdiffusion, AROOM_DIF_SPARSE, AROOM_DIF_CLUTTERED))
#define ADSP_IsFull(pa)			(pa->fdiffusion > AROOM_DIF_CLUTTERED)

#define ADSP_IsDiffuse(pa)		(pa->diffusion > ADSP_SPARSE)

// room acoustic reflectivity

	// tile									0.3  * 3.3 = 0.99
	// metal								0.25 * 3.3 = 0.83
	// concrete,rock,brick,glass,gravel		0.2  * 3.3 = 0.66
	// metal panel/vent, wood, water		0.1	 * 3.3 = 0.33
	// carpet,sand,snow,dirt				0.01 * 3.3 = 0.03		

#define ADSP_DULL				0
#define ADSP_FLAT				1
#define ADSP_REFLECTIVE			2
#define ADSP_BRIGHT				3
#define ADSP_REFLECTIVITY_MAX	4

#define AROOM_REF_DULL			0.04
#define AROOM_REF_FLAT			0.50
#define AROOM_REF_REFLECTIVE	0.80
#define AROOM_REF_BRIGHT		0.99

#define ADSP_IsDull(pa)			(pa->freflectivity <= AROOM_REF_DULL)
#define ADSP_IsFlat(pa)			(BETWEEN(pa->freflectivity, AROOM_REF_DULL, AROOM_REF_FLAT))
#define ADSP_IsReflective(pa)	(BETWEEN(pa->freflectivity, AROOM_REF_FLAT, AROOM_REF_REFLECTIVE))
#define ADSP_IsBright(pa)		(pa->freflectivity > AROOM_REF_REFLECTIVE)

#define ADSP_IsRefl(pa)			(pa->reflectivity > ADSP_FLAT)

// room shapes

#define ADSP_ROOM			0
#define ADSP_DUCT			1
#define ADSP_HALL			2
#define ADSP_TUNNEL			3
#define ADSP_STREET			4
#define ADSP_ALLEY			5
#define ADSP_COURTYARD		6
#define ADSP_OPEN_SPACE		7		// NOTE: 7..10 must remain in order !!!
#define ADSP_OPEN_WALL		8
#define ADSP_OPEN_STREET	9
#define ADSP_OPEN_COURTYARD	10

// room sizes

#define ADSP_SIZE_SMALL		0		// NOTE: must remain 0..4!!!
#define ADSP_SIZE_MEDIUM	1
#define ADSP_SIZE_LARGE		2
#define ADSP_SIZE_HUGE		3
#define ADSP_SIZE_GIGANTIC	4
#define ADSP_SIZE_MAX		5

#define ADSP_LENGTH_SHORT	0	
#define ADSP_LENGTH_MEDIUM	1
#define ADSP_LENGTH_LONG	2
#define ADSP_LENGTH_VLONG	3
#define ADSP_LENGTH_XLONG	4
#define ADSP_LENGTH_MAX		5

#define ADSP_WIDTH_NARROW	0
#define ADSP_WIDTH_MEDIUM	1
#define ADSP_WIDTH_WIDE		2
#define ADSP_WIDTH_VWIDE	3
#define ADSP_WIDTH_XWIDE	4
#define ADSP_WIDTH_MAX		5

#define ADSP_HEIGHT_LOW		0
#define ADSP_HEIGTH_MEDIUM	1
#define ADSP_HEIGHT_TALL	2
#define ADSP_HEIGHT_VTALL	3
#define ADSP_HEIGHT_XTALL	4
#define ADSP_HEIGHT_MAX		5


// convert numeric size params to #defined size params

void ADSP_GetSize( auto_params_t *pa )
{
	pa->size =	((ADSP_IsSmallRoom(pa) ? 1 : 0)		* ADSP_SIZE_SMALL) +
				((ADSP_IsMediumRoom(pa) ? 1 : 0)	* ADSP_SIZE_MEDIUM) +
				((ADSP_IsLargeRoom(pa) ? 1 : 0)		* ADSP_SIZE_LARGE) +
				((ADSP_IsHugeRoom(pa) ? 1 : 0)		* ADSP_SIZE_HUGE) +
				((ADSP_IsGiganticRoom(pa) ? 1 : 0)	* ADSP_SIZE_GIGANTIC);

	pa->len =	((ADSP_IsShortLength(pa) ? 1 : 0)	* ADSP_LENGTH_SHORT) +
				((ADSP_IsMediumLength(pa) ? 1 : 0)	* ADSP_LENGTH_MEDIUM) +
				((ADSP_IsLongLength(pa) ? 1 : 0)	* ADSP_LENGTH_LONG) +
				((ADSP_IsVLongLength(pa) ? 1 : 0)	* ADSP_LENGTH_VLONG) + 
				((ADSP_IsXLongLength(pa) ? 1 : 0)	* ADSP_LENGTH_XLONG);
				
	pa->wid =	((ADSP_IsNarrowWidth(pa) ? 1 : 0)	* ADSP_WIDTH_NARROW) +
				((ADSP_IsMediumWidth(pa) ? 1 : 0)	* ADSP_WIDTH_MEDIUM) +
				((ADSP_IsWideWidth(pa) ? 1 : 0)		* ADSP_WIDTH_WIDE) +
				((ADSP_IsVWideWidth(pa) ? 1 : 0)	* ADSP_WIDTH_VWIDE) +
				((ADSP_IsXWideWidth(pa) ? 1 : 0)	* ADSP_WIDTH_XWIDE);

	pa->ht =	((ADSP_IsLowHeight(pa) ? 1 : 0)		* ADSP_HEIGHT_LOW) +
				((ADSP_IsMediumHeight(pa) ? 1 : 0)	* ADSP_HEIGTH_MEDIUM) +
				((ADSP_IsTallHeight(pa) ? 1 : 0)	* ADSP_HEIGHT_TALL) +
				((ADSP_IsVTallHeight(pa) ? 1 : 0)	* ADSP_HEIGHT_VTALL) +
				((ADSP_IsXTallHeight(pa) ? 1 : 0)	* ADSP_HEIGHT_XTALL);

	pa->reflectivity = 
				((ADSP_IsDull(pa) ? 1 : 0)			* ADSP_DULL) +
				((ADSP_IsFlat(pa) ? 1 : 0)			* ADSP_FLAT) +
				((ADSP_IsReflective(pa) ? 1 : 0)	* ADSP_REFLECTIVE) +
				((ADSP_IsBright(pa) ? 1 : 0)		* ADSP_BRIGHT);

	pa->diffusion = 
				((ADSP_IsEmpty(pa) ? 1 : 0)			* ADSP_EMPTY) +
				((ADSP_IsSparse(pa) ? 1 : 0)		* ADSP_SPARSE) +
				((ADSP_IsCluttered(pa) ? 1 : 0)		* ADSP_CLUTTERED) +
				((ADSP_IsFull(pa) ? 1 : 0)			* ADSP_FULL);

	Assert(pa->size < ADSP_SIZE_MAX);
	Assert(pa->len  < ADSP_LENGTH_MAX);
	Assert(pa->wid  < ADSP_WIDTH_MAX);
	Assert(pa->ht   < ADSP_HEIGHT_MAX);
	Assert(pa->reflectivity < ADSP_REFLECTIVITY_MAX);
	Assert(pa->diffusion < ADSP_DIFFUSION_MAX);

	if ( pa->shape != ADSP_COURTYARD && pa->shape != ADSP_OPEN_COURTYARD )
	{
		// fix up size for streets, alleys, halls, ducts, tunnelsy

		if (pa->shape == ADSP_STREET || pa->shape == ADSP_ALLEY )
			pa->size = pa->wid;
		else
			pa->size = (pa->len + pa->wid) / 2;

	}

}

void ADSP_GetOutsideSize( auto_params_t *pa )
{
	ADSP_GetSize( pa );
}

// return # of sides that had max length or sky hits (out of 6 sides).

int ADSP_COpenSides( auto_params_t *pa )
{
	int count = 0;

	// only look at left,right,front,back walls - ignore floor, ceiling

	for (int i = 0; i < 4; i++)
	{
		if (pa->surface_refl[i] == 0.0)
			count++;
	}

	return count;
}

// given auto params, return shape and size of room

void ADSP_GetAutoShape( auto_params_t *pa )
{

	// INSIDE: 
	// shapes: duct, hall, tunnel, shaft (vertical duct, hall or tunnel)
	//		sizes: short->long, narrow->wide, low->tall
	// shapes: room
	//		sizes: small->large, low->tall

	// OUTSIDE: 
	// shapes: street, alley
	//		sizes: short->long, narrow->wide
	// shapes: courtyard
	//		sizes: small->large

	// shapes: open_space, wall, open_street, open_corner, open_courtyard
	//		sizes: open, narrow->wide

	bool bshaft = false;
	int t;

	if (ADSP_IsInside(pa))
	{
		if (ADSP_IsShaft(pa))
		{
			// temp swap height and length

			bshaft = true;
			t = pa->height;
			pa->height = pa->length;
			pa->length = t;
			if (das_debug.GetInt() > 1)
				DevMsg("VERTICAL SHAFT Detected \n");
		}

		// get shape

		if (ADSP_IsDuct(pa))
		{
			pa->shape = ADSP_DUCT;
			ADSP_GetSize( pa );
			if (das_debug.GetInt() > 1)
				DevMsg("DUCT Detected \n");
			goto autoshape_exit;
		}

		if (ADSP_IsHall(pa))
		{
			// get size
			pa->shape = ADSP_HALL;
			ADSP_GetSize( pa );

			if (das_debug.GetInt() > 1)
				DevMsg("HALL Detected \n");

			goto autoshape_exit;
		}

		if (ADSP_IsTunnel(pa))
		{
			// get size
			pa->shape = ADSP_TUNNEL;
			ADSP_GetSize( pa );

			if (das_debug.GetInt() > 1)
				DevMsg("TUNNEL Detected \n");

			goto autoshape_exit;
		}
		
		// default
		// (ADSP_IsRoom(pa))
		{
			// get size
			pa->shape = ADSP_ROOM;
			ADSP_GetSize( pa );

			if (das_debug.GetInt() > 1)
				DevMsg("ROOM Detected \n");

			goto autoshape_exit;
		}
	}

	// outside:

	if (ADSP_COpenSides(pa) > 0)	// side hit sky, or side has max length
	{
		// get shape - courtyard, street, wall or open space
		// 10..7
		pa->shape = ADSP_OPEN_COURTYARD - (ADSP_COpenSides(pa) - 1);
		ADSP_GetOutsideSize( pa );
		
		if (das_debug.GetInt() > 1)
				DevMsg("OPEN SIDED OUTDOOR AREA Detected \n");

		goto autoshape_exit;
	}

	// all sides closed:

	// get shape - closed street or alley or courtyard

	if (ADSP_IsCourtyard(pa))
	{
		pa->shape = ADSP_COURTYARD;
		ADSP_GetOutsideSize( pa );
		
		if (das_debug.GetInt() > 1)
				DevMsg("OUTSIDE COURTYARD Detected \n");

		goto autoshape_exit;
	}

	if (ADSP_IsAlley(pa))
	{
		pa->shape = ADSP_ALLEY;
		ADSP_GetOutsideSize( pa );

		if (das_debug.GetInt() > 1)
				DevMsg("OUTSIDE ALLEY Detected \n");
		goto autoshape_exit;
	}
	
	// default to 'street' if sides are closed

	// if (ADSP_IsStreet(pa))
	{
		pa->shape = ADSP_STREET;
		ADSP_GetOutsideSize( pa );
		if (das_debug.GetInt() > 1)
				DevMsg("OUTSIDE STREET Detected \n");
		goto autoshape_exit;
	}
	
autoshape_exit:

	// swap height & length if needed

	if (bshaft)
	{
		t = pa->height;
		pa->height = pa->length;
		pa->length = t;
	}
}

int MapReflectivityToDLYCutoff[] = 
{
	1000,	// DULL
	2000,   // FLAT
	4000,   // REFLECTIVE
	6000	// BRIGHT
};

float MapSizeToDLYFeedback[] = 
{
	0.9, // 0.6,	// SMALL	
	0.8, // 0.5,	// MEDIUM	
	0.7, // 0.4,	// LARGE	
	0.6, // 0.3,	// HUGE		
	0.5, // 0.2,	// GIGANTIC	
};

void ADSP_SetupAutoDelay( prc_t *pprc_dly, auto_params_t *pa )
{
	// shapes: 
	// inside: duct, long hall, long tunnel, large room
	// outside: open courtyard, street wall, space
	// outside: closed courtyard, alley, street

	// size 0..4
	// len 0..3
	// wid 0..3
	// reflectivity: 0..3
	// diffusion 0..3

	// dtype: delay type DLY_PLAIN, DLY_LOWPASS, DLY_ALLPASS	
	// delay: delay in milliseconds (room max size in feet)
	// feedback: feedback 0-1.0
	// gain: final gain of output stage, 0-1.0

	int size = pa->length * 2.0;
	
	if (pa->shape == ADSP_ALLEY || pa->shape == ADSP_STREET || pa->shape == ADSP_OPEN_STREET)
		size = pa->width * 2.0;

	pprc_dly->type = PRC_DLY;

	pprc_dly->prm[dly_idtype]		= DLY_LOWPASS;		// delay with feedback

	pprc_dly->prm[dly_idelay]		= clamp((size / 12.0), 5.0, 500.0);
	
	pprc_dly->prm[dly_ifeedback]	= MapSizeToDLYFeedback[pa->len];

	// reduce gain based on distance reflection travels
//	float g = 1.0 - ( clamp(pprc_dly->prm[dly_idelay], 10.0, 1000.0) / (1000.0 - 10.0) );
//	pprc_dly->prm[dly_igain]		= g;

	pprc_dly->prm[dly_iftype]		= FLT_LP;
	if (ADSP_IsInside(pa))
		pprc_dly->prm[dly_icutoff]	= MapReflectivityToDLYCutoff[pa->reflectivity];
	else
		pprc_dly->prm[dly_icutoff]	= (int)((float)(MapReflectivityToDLYCutoff[pa->reflectivity]) * 0.75);

	pprc_dly->prm[dly_iqwidth]		= 0;

	pprc_dly->prm[dly_iquality]		= QUA_LO;

	float l = clamp((pa->length * 2.0 / 12.0), 14.0, 500.0);
	float w = clamp((pa->width * 2.0 / 12.0), 14.0, 500.0);

	// convert to multitap delay

	pprc_dly->prm[dly_idtype] = DLY_LOWPASS_4TAP;

	pprc_dly->prm[dly_idelay]	= l;
	pprc_dly->prm[dly_itap1]	= w;
	pprc_dly->prm[dly_itap2]	= l; // max(7, l * 0.7 );
	pprc_dly->prm[dly_itap3]	= l; // max(7, w * 0.7 );
		
	pprc_dly->prm[dly_igain]		= 1.0;
}

int MapReflectivityToRVACutoff[] = 
{
	1000,	// DULL
	2000,   // FLAT
	4000,   // REFLECTIVE
	6000	// BRIGHT
};

float MapSizeToRVANumDelays[] = 
{
	3,	// SMALL	3 reverbs
	6,	// MEDIUM	6 reverbs
	6,	// LARGE	6 reverbs
	9,	// HUGE		9 reverbs
	12,	// GIGANTIC	12 reverbs
};

float MapSizeToRVAFeedback[] =
{
	0.75,	// SMALL	
	0.8,	// MEDIUM	
	0.9,	// LARGE	
	0.95,	// HUGE		
	0.98,	// GIGANTIC
};

void ADSP_SetupAutoReverb( prc_t *pprc_rva, auto_params_t *pa )
{
	// shape: hall, tunnel or room
	// size 0..4
	// reflectivity: 0..3
	// diffusion 0..3

	// size: 0-2.0 scales nominal delay parameters (18 to 47 ms * scale = delay)
	// numdelays: 0-12 controls # of parallel or series delays
	// decay: 0-2.0 scales feedback parameters (.7 to .9 * scale/2.0 = feedback)
	// fparallel: if true, filters are built into delays, otherwise filter output only
	// fmoddly: if true, all delays are modulating delays
	float gain = 1.0;

	pprc_rva->type = PRC_RVA;

	pprc_rva->prm[rva_size_max]			= 50.0;
	pprc_rva->prm[rva_size_min]			= 30.0;
	
	if (ADSP_IsRoom(pa))
		pprc_rva->prm[rva_inumdelays]	= MapSizeToRVANumDelays[pa->size];
	else
		pprc_rva->prm[rva_inumdelays]	= MapSizeToRVANumDelays[pa->len];

	pprc_rva->prm[rva_ifeedback]	= 0.9;
	
	pprc_rva->prm[rva_icutoff]		= MapReflectivityToRVACutoff[pa->reflectivity];
	
	pprc_rva->prm[rva_ifparallel]	= 1;
	pprc_rva->prm[rva_imoddly]		= ADSP_IsEmpty(pa) ? 0 : 4;
	pprc_rva->prm[rva_imodrate]		= 3.48;

	pprc_rva->prm[rva_iftaps]		= 0;	// 0.1 // use extra delay taps to increase density

	pprc_rva->prm[rva_width]		= clamp( ((float)(pa->width) / 12.0), 6.0, 500.0);	// in feet
	pprc_rva->prm[rva_depth]		= clamp( ((float)(pa->length) / 12.0), 6.0, 500.0);
	pprc_rva->prm[rva_height]		= clamp( ((float)(pa->height) / 12.0), 6.0, 500.0);

	// room
	pprc_rva->prm[rva_fbwidth]		= 0.9; // MapSizeToRVAFeedback[pa->size];	// larger size = more feedback
	pprc_rva->prm[rva_fbdepth]		= 0.9; // MapSizeToRVAFeedback[pa->size];	
	pprc_rva->prm[rva_fbheight]		= 0.5; // MapSizeToRVAFeedback[pa->size];

	// feedback is based on size of room:
	
	if (ADSP_IsInside(pa))
	{
		if (pa->shape == ADSP_HALL)
		{
			pprc_rva->prm[rva_fbwidth]		= 0.7; //MapSizeToRVAFeedback[pa->wid];
			pprc_rva->prm[rva_fbdepth]		= -0.5; //MapSizeToRVAFeedback[pa->len];	
			pprc_rva->prm[rva_fbheight]		= 0.3; //MapSizeToRVAFeedback[pa->ht];
		}

		if (pa->shape == ADSP_TUNNEL)
		{
			pprc_rva->prm[rva_fbwidth]		= 0.9;	
			pprc_rva->prm[rva_fbdepth]		= -0.8;	// fixed pre-delay, no feedback
			pprc_rva->prm[rva_fbheight]		= 0.3;	
		}
	}
	else
	{
		if  (pa->shape == ADSP_ALLEY)
		{
			pprc_rva->prm[rva_fbwidth]		= 0.9; 
			pprc_rva->prm[rva_fbdepth]		= -0.8; // fixed pre-delay, no feedback	
			pprc_rva->prm[rva_fbheight]		= 0.0; 
		}
	}

	if (!ADSP_IsInside(pa))
		pprc_rva->prm[rva_fbheight]		= 0.0;
	
	pprc_rva->prm[rva_igain] = gain;
}

// diffusor templates for auto create 

		// size: 0-1.0 scales all delays				(13ms to 41ms * scale = delay)
		// numdelays: 0-4.0 controls # of series delays
		// decay: 0-1.0 scales all feedback parameters	

//					prctype		size	#dly	feedback

#if 0
#define PRC_DFRA_S	{PRC_DFR,	{0.5,	2,		0.10},	NULL,NULL,NULL,NULL,NULL}	// S room
#define PRC_DFRA_M	{PRC_DFR,	{0.75,	2,		0.12},	NULL,NULL,NULL,NULL,NULL}	// M room
#define PRC_DFRA_L	{PRC_DFR,	{1.0,	3,		0.13},	NULL,NULL,NULL,NULL,NULL}	// L room
#define PRC_DFRA_VL	{PRC_DFR,	{1.0,	3,		0.15},	NULL,NULL,NULL,NULL,NULL}	// VL room

prc_t g_prc_dfr_auto[] = {PRC_DFRA_S, PRC_DFRA_M, PRC_DFRA_L, PRC_DFRA_VL, PRC_DFRA_VL};

//$BUGBUGBUG: I think this should be sizeof(prc_t), not sizeof(pset_t)...
#define CDFRTEMPLATES  (sizeof(g_prc_dfr_auto)/sizeof(pset_t))		// number of diffusor templates

// copy diffusor template from preset list, based on room size

void ADSP_SetupAutoDiffusor( prc_t *pprc_dfr, auto_params_t *pa )
{
	int i = clamp(pa->size, 0, (int)CDFRTEMPLATES - 1);

	// copy diffusor preset based on size

	*pprc_dfr = g_prc_dfr_auto[i];
}
#endif

// return index to processor given processor type and preset
// skips N processors of similar type
// returns -1 if type not found

int ADSP_FindProc( pset_t *ppset, int proc_type, int skip )
{
	int skipcount = skip;

	for (int i = 0; i < ppset->cprcs; i++)
	{
		// look for match on processor type

		if ( ppset->prcs[i].type == proc_type )
		{
			// skip first N procs of similar type, 
			
			// return index to processor

			if (!skipcount)
				return i;

			skipcount--;
		}

	}

	return -1;
}

// interpolate parameter:
// pnew - target preset
// pmin - preset with parameter with min value
// pmax - preset with parameter with max value
// proc_type - type of processor to look for ie: PRC_RVA or PRC_DLY
// skipprocs - skip n processors of type
// iparam - which parameter within processor to interpolate
// index - 
// index_max:  use index/index_max as interpolater between pmin param and pmax param
// if bexp is true, interpolate exponentially as (index/index_max)^2

// NOTE: returns with no result if processor type is not found in all presets.

void ADSP_InterpParam( pset_t *pnew, pset_t *pmin, pset_t *pmax, int proc_type, int skipprocs, int iparam, int index, int index_max, bool bexp )
{
	// find processor index in pnew
	int iproc_new = ADSP_FindProc( pnew, proc_type, skipprocs);
	int iproc_min = ADSP_FindProc( pmin, proc_type, skipprocs);
	int iproc_max = ADSP_FindProc( pmax, proc_type, skipprocs);

	// make sure processor type found in all presets

	if ( iproc_new < 0 || iproc_min < 0 || iproc_max < 0 )
		return;
	
	float findex = (float)index/(float)index_max;
	float vmin = pmin->prcs[iproc_min].prm[iparam];
	float vmax = pmax->prcs[iproc_max].prm[iparam];
	float vinterp;

	// interpolate

	if (!bexp)
		vinterp = vmin + (vmax - vmin) * findex;
	else
		vinterp = vmin + (vmax - vmin) * findex * findex;

	pnew->prcs[iproc_new].prm[iparam] = vinterp;

	return;
}

// directly set parameter

void ADSP_SetParam( pset_t *pnew, int proc_type, int skipprocs, int iparam, float value )
{
	int iproc_new = ADSP_FindProc( pnew, proc_type, skipprocs);

	if (iproc_new >= 0)
		pnew->prcs[iproc_new].prm[iparam] = value;
}

// directly set parameter if min or max is negative

void ADSP_SetParamIfNegative( pset_t *pnew, pset_t *pmin, pset_t *pmax, int proc_type, int skipprocs, int iparam, int index, int index_max, bool bexp, float value )
{
	// find processor index in pnew
	int iproc_new = ADSP_FindProc( pnew, proc_type, skipprocs);
	int iproc_min = ADSP_FindProc( pmin, proc_type, skipprocs);
	int iproc_max = ADSP_FindProc( pmax, proc_type, skipprocs);

	// make sure processor type found in all presets

	if ( iproc_new < 0 || iproc_min < 0 || iproc_max < 0 )
		return;
	
	float vmin = pmin->prcs[iproc_min].prm[iparam];
	float vmax = pmax->prcs[iproc_max].prm[iparam];

	if ( vmin < 0.0 || vmax < 0.0 )
		ADSP_SetParam( pnew, proc_type, skipprocs, iparam, value );
	else
		ADSP_InterpParam( pnew, pmin, pmax, proc_type, skipprocs, iparam, index, index_max, bexp);

	return;	
}

// given min and max preset and auto parameters, create new preset
// NOTE: the # and type of processors making up pmin and pmax presets must be identical!

void ADSP_InterpolatePreset( pset_t *pnew, pset_t *pmin, pset_t *pmax, auto_params_t *pa, int iskip )
{
	int i;

	// if size > mid size, then copy basic processors from MAX preset,
	// otherwise, copy from MIN preset

	if ( !iskip )
	{
		// only copy on 1st call

		if ( pa->size > ADSP_SIZE_MEDIUM )
		{
			*pnew = *pmax;
		}
		else
		{
			*pnew = *pmin;
		}
	}

	// DFR

	// interpolate all DFR params on size

	for (i = 0; i < dfr_cparam; i++)
		ADSP_InterpParam( pnew, pmin, pmax, PRC_DFR, iskip, i, pa->size, ADSP_SIZE_MAX , 0);

	// RVA

	// interpolate size_max, size_min, feedback, #delays, moddly, imodrate, based on ap size 

	ADSP_InterpParam( pnew, pmin, pmax, PRC_RVA, iskip, rva_ifeedback, pa->size, ADSP_SIZE_MAX, 0);
	ADSP_InterpParam( pnew, pmin, pmax, PRC_RVA, iskip, rva_size_min, pa->size, ADSP_SIZE_MAX, 1);
	ADSP_InterpParam( pnew, pmin, pmax, PRC_RVA, iskip, rva_size_max, pa->size, ADSP_SIZE_MAX, 1);
	ADSP_InterpParam( pnew, pmin, pmax, PRC_RVA, iskip, rva_igain, pa->size, ADSP_SIZE_MAX, 0);	
	ADSP_InterpParam( pnew, pmin, pmax, PRC_RVA, iskip, rva_inumdelays, pa->size, ADSP_SIZE_MAX , 0);
	ADSP_InterpParam( pnew, pmin, pmax, PRC_RVA, iskip, rva_imoddly, pa->size, ADSP_SIZE_MAX , 0);
	ADSP_InterpParam( pnew, pmin, pmax, PRC_RVA, iskip, rva_imodrate, pa->size, ADSP_SIZE_MAX , 0);

	// interpolate width,depth,height based on ap width length & height - exponential interpolation
	// if pmin or pmax parameters are < 0, directly set value from w/l/h

	float w	= clamp( ((float)(pa->width) / 12.0), 6.0, 500.0);	// in feet
	float l = clamp( ((float)(pa->length) / 12.0), 6.0, 500.0);
	float h	= clamp( ((float)(pa->height) / 12.0), 6.0, 500.0);

	ADSP_SetParamIfNegative( pnew, pmin, pmax, PRC_RVA, iskip, rva_width, pa->wid, ADSP_WIDTH_MAX, 1, w);
	ADSP_SetParamIfNegative( pnew, pmin, pmax, PRC_RVA, iskip, rva_depth, pa->len, ADSP_LENGTH_MAX, 1, l);
	ADSP_SetParamIfNegative( pnew, pmin, pmax, PRC_RVA, iskip, rva_height, pa->ht, ADSP_HEIGHT_MAX, 1, h);

	// interpolate w/d/h feedback based on ap w/d/f

	ADSP_InterpParam( pnew, pmin, pmax, PRC_RVA, iskip, rva_fbwidth, pa->wid, ADSP_WIDTH_MAX , 0);
	ADSP_InterpParam( pnew, pmin, pmax, PRC_RVA, iskip, rva_fbdepth, pa->len, ADSP_LENGTH_MAX , 0);
	ADSP_InterpParam( pnew, pmin, pmax, PRC_RVA, iskip, rva_fbheight, pa->ht, ADSP_HEIGHT_MAX , 0);

	// interpolate cutoff based on ap reflectivity
	// NOTE: cutoff goes from max to min! ie: small bright - large dull

	ADSP_InterpParam( pnew, pmax, pmin, PRC_RVA, iskip, rva_icutoff, pa->reflectivity, ADSP_REFLECTIVITY_MAX , 0);

	// don't interpolate: fparallel, ftaps

	// DLY
	
	// directly set delay value from pa->length if pmin or pmax value is < 0
	
	l = clamp((pa->length * 2.0 / 12.0), 14.0, 500.0);
	w = clamp((pa->width * 2.0 / 12.0), 14.0, 500.0);

	ADSP_SetParamIfNegative( pnew, pmin, pmax, PRC_DLY, iskip, dly_idelay, pa->len, ADSP_LENGTH_MAX, 1, l);

	// interpolate feedback, gain, based on max size (length)

	ADSP_InterpParam( pnew, pmin, pmax, PRC_DLY, iskip, dly_ifeedback, pa->len, ADSP_LENGTH_MAX , 0);
	ADSP_InterpParam( pnew, pmin, pmax, PRC_DLY, iskip, dly_igain, pa->len, ADSP_LENGTH_MAX , 0);

	// directly set tap value from pa->width if pmin or pmax value is < 0

	ADSP_SetParamIfNegative( pnew, pmin, pmax, PRC_DLY, iskip, dly_itap1, pa->len, ADSP_LENGTH_MAX, 1, w);
	ADSP_SetParamIfNegative( pnew, pmin, pmax, PRC_DLY, iskip, dly_itap2, pa->len, ADSP_LENGTH_MAX, 1, l);
	ADSP_SetParamIfNegative( pnew, pmin, pmax, PRC_DLY, iskip, dly_itap3, pa->len, ADSP_LENGTH_MAX, 1, l);
	
	// interpolate cutoff and qwidth based on reflectivity NOTE: this can affect gain!
	// NOTE: cutoff goes from max to min! ie: small bright - large dull

	ADSP_InterpParam( pnew, pmax, pmin, PRC_DLY, iskip, dly_icutoff, pa->len, ADSP_LENGTH_MAX , 0);
	ADSP_InterpParam( pnew, pmax, pmin, PRC_DLY, iskip, dly_iqwidth, pa->len, ADSP_LENGTH_MAX , 0);
	
	// interpolate all other parameters for all other processor types based on size
	
	// PRC_MDY, PRC_AMP, PRC_FLT, PTC, CRS, ENV, EFO, LFO

	for (i = 0; i < mdy_cparam; i++)
		ADSP_InterpParam( pnew, pmin, pmax, PRC_MDY, iskip, i, pa->len, ADSP_LENGTH_MAX , 0);
	
	for (i = 0; i < amp_cparam; i++)
		ADSP_InterpParam( pnew, pmin, pmax, PRC_AMP, iskip, i, pa->size, ADSP_SIZE_MAX , 0);

	for (i = 0; i < flt_cparam; i++)
		ADSP_InterpParam( pnew, pmin, pmax, PRC_FLT, iskip, i, pa->size, ADSP_SIZE_MAX , 0);

	for (i = 0; i < ptc_cparam; i++)
		ADSP_InterpParam( pnew, pmin, pmax, PRC_PTC, iskip, i, pa->size, ADSP_SIZE_MAX , 0);

	for (i = 0; i < crs_cparam; i++)
		ADSP_InterpParam( pnew, pmin, pmax, PRC_CRS, iskip, i, pa->size, ADSP_SIZE_MAX , 0);

	for (i = 0; i < env_cparam; i++)
		ADSP_InterpParam( pnew, pmin, pmax, PRC_ENV, iskip, i, pa->size, ADSP_SIZE_MAX , 0);

	for (i = 0; i < efo_cparam; i++)
		ADSP_InterpParam( pnew, pmin, pmax, PRC_EFO, iskip, i, pa->size, ADSP_SIZE_MAX , 0);

	for (i = 0; i < lfo_cparam; i++)
		ADSP_InterpParam( pnew, pmin, pmax, PRC_LFO, iskip, i, pa->size, ADSP_SIZE_MAX , 0);

}

// these convars store the index to the first preset for each shape type in dsp_presets.txt

ConVar adsp_room_min			("adsp_room_min",		"102");
ConVar adsp_duct_min			("adsp_duct_min",		"106");
ConVar adsp_hall_min			("adsp_hall_min",		"110");
ConVar adsp_tunnel_min			("adsp_tunnel_min",		"114");
ConVar adsp_street_min			("adsp_street_min",		"118");
ConVar adsp_alley_min			("adsp_alley_min",		"122");
ConVar adsp_courtyard_min		("adsp_courtyard_min",	"126");
ConVar adsp_openspace_min		("adsp_openspace_min",	"130");
ConVar adsp_openwall_min		("adsp_openwall_min",	"130");
ConVar adsp_openstreet_min		("adsp_openstreet_min",	"118");
ConVar adsp_opencourtyard_min	("adsp_opencourtyard_min", "126");

// given room parameters, construct and return a dsp preset representing the room.
// bskyabove, width, length, height, fdiffusion, freflectivity are all passed-in room parameters
// psurf_refl is a passed-in array of reflectivity values for 6 surfaces
// inode is the location within g_psettemplates[] that the dsp preset will be constructed (inode = dsp preset#)
// cnode should always = DSP_CAUTO_PRESETS
// returns idsp preset.

int DSP_ConstructPreset( bool bskyabove, int width, int length, int height, float fdiffusion, float freflectivity, float *psurf_refl, int inode, int cnodes )
{
	auto_params_t ap;
	auto_params_t *pa;
	
	pset_t new_pset;	// preset
	pset_t pset_min;
	pset_t pset_max;

	int ipreset;
	int ipset_min;
	int ipset_max;

	if (inode >= DSP_CAUTO_PRESETS)
	{
		Assert(false);	// check DAS_CNODES == DSP_CAUTO_PRESETS!!!
		return 0;
	}
	
	// fill parameter struct

	ap.bskyabove = bskyabove;
	ap.width = width;
	ap.length = length;
	ap.height = height;
	ap.fdiffusion = fdiffusion;
	ap.freflectivity = freflectivity;

	for (int i = 0; i < 6; i++)
		ap.surface_refl[i] = psurf_refl[i];

	if (ap.bskyabove)
		ap.surface_refl[4] = 0.0;
	
	// select shape, size based on params

	ADSP_GetAutoShape( &ap );
	
	// set up min/max presets based on shape

	switch ( ap.shape )
	{
	default:
	case ADSP_ROOM:			ipset_min = adsp_room_min.GetInt(); break;
	case ADSP_DUCT:			ipset_min = adsp_duct_min.GetInt();  break;
	case ADSP_HALL:			ipset_min = adsp_hall_min.GetInt();  break;
	case ADSP_TUNNEL:		ipset_min = adsp_tunnel_min.GetInt();  break;
	case ADSP_STREET:		ipset_min = adsp_street_min.GetInt();  break;
	case ADSP_ALLEY:		ipset_min = adsp_alley_min.GetInt();  break;
	case ADSP_COURTYARD:	ipset_min = adsp_courtyard_min.GetInt();  break;
	case ADSP_OPEN_SPACE:	ipset_min = adsp_openspace_min.GetInt();  break;
	case ADSP_OPEN_WALL:	ipset_min = adsp_openwall_min.GetInt();  break;
	case ADSP_OPEN_STREET:	ipset_min = adsp_openstreet_min.GetInt();  break;
	case ADSP_OPEN_COURTYARD: ipset_min = adsp_opencourtyard_min.GetInt();  break;
	}

	// presets in dsp_presets.txt are ordered as:

	// <shape><empty><min>
	// <shape><empty><max>
	// <shape><diffuse><min>
	// <shape><diffuse><max>
	pa = &ap;
	if ( ADSP_IsDiffuse(pa) )
			ipset_min += 2;

	ipset_max = ipset_min + 1;

	pset_min = g_psettemplates[ipset_min];
	pset_max = g_psettemplates[ipset_max];

	// given min and max preset and auto parameters, create new preset

	// interpolate between 1st instances of each processor type (ie: PRC_DLY) appearing in preset

	ADSP_InterpolatePreset( &new_pset, &pset_min, &pset_max, &ap, 0 );

	// interpolate between 2nd instances of each processor type (ie: PRC_DLY) appearing in preset

	ADSP_InterpolatePreset( &new_pset, &pset_min, &pset_max, &ap, 1 );

	// copy constructed preset back into node's template location

	ipreset = DSP_AUTO_BASE + inode;

	g_psettemplates[ipreset] = new_pset;

	return ipreset;
}	

///////////////////////////////////////
// Helpers: called only from DSP_Process
///////////////////////////////////////


// return true if batch processing version of preset exists

inline bool FBatchPreset( pset_t *ppset )
{
	
	switch (ppset->type)
	{
	case PSET_LINEAR: 
		return true;
	case PSET_SIMPLE: 
		return true;
	default:
		return false;
	}
}

// Helper: called only from DSP_Process
// mix front stereo buffer to mono buffer, apply dsp fx

inline void DSP_ProcessStereoToMono(dsp_t *pdsp, portable_samplepair_t *pbfront, portable_samplepair_t *pbrear, int sampleCount, bool bcrossfading )
{
	portable_samplepair_t *pbf = pbfront;		// pointer to buffer of front stereo samples to process
	int count = sampleCount;
	int av;
	int x;

	if ( !bcrossfading ) 
	{
		if ( !pdsp->ipset ) 
			return;

		if ( FBatchPreset(pdsp->ppset[0]))
		{
			// convert Stereo to Mono in place, then batch process fx: perf KDB

			// front->left + front->right / 2 into front->left, front->right duplicated.

			while ( count-- )
			{
				pbf->left = (pbf->left + pbf->right) >> 1;
				pbf++;
			}
			
			// process left (mono), duplicate output into right

			PSET_GetNextN( pdsp->ppset[0], pbfront, sampleCount, OP_LEFT_DUPLICATE);
		}
		else
		{
			// avg left and right -> mono fx -> duplcate out left and right
			while ( count-- )
			{
				av = ( ( pbf->left + pbf->right ) >> 1 );
				x = PSET_GetNext( pdsp->ppset[0], av );
				x = CLIP_DSP( x );
				pbf->left = pbf->right = x;
				pbf++;
			}
		}
		return;
	}

	// crossfading to current preset from previous preset	

	if ( bcrossfading )
	{
		int r;
		int fl;
		int fr;
		int flp;
		int frp;
		int xf_fl;
		int xf_fr;
		bool bexp = pdsp->bexpfade;
		bool bfadetostereo = (pdsp->ipset == 0);
		bool bfadefromstereo = (pdsp->ipsetprev == 0);

		Assert ( !(bfadetostereo && bfadefromstereo) );	// don't call if ipset & ipsetprev both 0!

		if ( bfadetostereo || bfadefromstereo )
		{
			// special case if fading to or from preset 0, stereo passthrough

			while ( count-- )
			{
				av = ( ( pbf->left + pbf->right ) >> 1 );

				// get current preset values
				
				if ( pdsp->ipset )
				{
					fl = fr = PSET_GetNext( pdsp->ppset[0], av );
				}
				else
				{
					fl = pbf->left;
					fr = pbf->right;
				}
				
				// get previous preset values

				if ( pdsp->ipsetprev )
				{
					frp = flp = PSET_GetNext( pdsp->ppsetprev[0], av );
				}
				else
				{
					flp = pbf->left;
					frp = pbf->right;
				}
				
				fl = CLIP_DSP(fl);
				fr = CLIP_DSP(fr);
				flp = CLIP_DSP(flp);
				frp = CLIP_DSP(frp);

				// get current ramp value

				r = RMP_GetNext( &pdsp->xramp );	

				// crossfade from previous to current preset
				
				if (!bexp)
				{
					xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
					xf_fr = XFADE(fr, frp, r);	// crossfade front left previous to front left
				}
				else
				{
					xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left
					xf_fr = XFADE_EXP(fr, frp, r);	// crossfade front left previous to front left
				}

				pbf->left =  xf_fl;			// crossfaded front left, duplicate in right channel
				pbf->right = xf_fr;
			
				pbf++;
			}

			return;
		}

		// crossfade mono to mono preset

		while ( count-- )
		{
			av = ( ( pbf->left + pbf->right ) >> 1 );

			// get current preset values
			
			fl = PSET_GetNext( pdsp->ppset[0], av );
			
			// get previous preset values

			flp = PSET_GetNext( pdsp->ppsetprev[0], av );
			
			fl = CLIP_DSP(fl);
			flp = CLIP_DSP(flp);
			
			// get current ramp value

			r = RMP_GetNext( &pdsp->xramp );	

			// crossfade from previous to current preset
			
			if (!bexp)
				xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
			else
				xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left

			pbf->left =  xf_fl;			// crossfaded front left, duplicate in right channel
			pbf->right = xf_fl;
		
			pbf++;
		}
	}
}

// Helper: called only from DSP_Process
// DSP_Process stereo in to stereo out (if more than 2 procs, ignore them)

inline void DSP_ProcessStereoToStereo(dsp_t *pdsp, portable_samplepair_t *pbfront, portable_samplepair_t *pbrear, int sampleCount, bool bcrossfading )
{
	portable_samplepair_t *pbf = pbfront;		// pointer to buffer of front stereo samples to process
	int count = sampleCount;
	int fl, fr;

	if ( !bcrossfading ) 
	{

		if ( !pdsp->ipset ) 
			return;

		if ( FBatchPreset(pdsp->ppset[0]) && FBatchPreset(pdsp->ppset[1]) )
		{

			// process left & right

			PSET_GetNextN( pdsp->ppset[0], pbfront, sampleCount, OP_LEFT );
			PSET_GetNextN( pdsp->ppset[1], pbfront, sampleCount, OP_RIGHT );
		}
		else
		{
			// left -> left fx, right -> right fx
			while ( count-- )
			{
				fl = PSET_GetNext( pdsp->ppset[0], pbf->left );
				fr = PSET_GetNext( pdsp->ppset[1], pbf->right );

				fl = CLIP_DSP( fl );
				fr = CLIP_DSP( fr );

				pbf->left =  fl;
				pbf->right = fr;
				pbf++;
			}
		}
		return;
	}

	// crossfading to current preset from previous preset	

	if ( bcrossfading )
	{
		int r;
		int flp, frp;
		int xf_fl, xf_fr;
		bool bexp = pdsp->bexpfade;

		while ( count-- )
		{
			// get current preset values

			fl = PSET_GetNext( pdsp->ppset[0], pbf->left );
			fr = PSET_GetNext( pdsp->ppset[1], pbf->right );
	
			// get previous preset values

			flp = PSET_GetNext( pdsp->ppsetprev[0], pbf->left );
			frp = PSET_GetNext( pdsp->ppsetprev[1], pbf->right );
		
			// get current ramp value

			r = RMP_GetNext( &pdsp->xramp );	

			fl = CLIP_DSP( fl );
			fr = CLIP_DSP( fr );
			flp = CLIP_DSP( flp );
			frp = CLIP_DSP( frp );

			// crossfade from previous to current preset
			if (!bexp)
			{
				xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
				xf_fr = XFADE(fr, frp, r);
			}
			else
			{
				xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left
				xf_fr = XFADE_EXP(fr, frp, r);
			}
			
			pbf->left =  xf_fl;			// crossfaded front left
			pbf->right = xf_fr;
		
			pbf++;
		}
	}
}

// Helper: called only from DSP_Process
// DSP_Process quad in to mono out (front left = front right)

inline void DSP_ProcessQuadToMono(dsp_t *pdsp, portable_samplepair_t *pbfront, portable_samplepair_t *pbrear, int sampleCount, bool bcrossfading )
{
	portable_samplepair_t *pbf = pbfront;		// pointer to buffer of front stereo samples to process
	portable_samplepair_t *pbr = pbrear;		// pointer to buffer of rear stereo samples to process
	int count = sampleCount;
	int x;
	int av;

	if ( !bcrossfading ) 
	{
		if ( !pdsp->ipset ) 
			return;

		if ( FBatchPreset(pdsp->ppset[0]) )
		{

			// convert Quad to Mono in place, then batch process fx: perf KDB

			// left front + rear -> left, right front + rear -> right
			while ( count-- )
			{
				pbf->left = ((pbf->left + pbf->right + pbr->left + pbr->right) >> 2);
				pbf++;
				pbr++;
			}
			
			// process left (mono), duplicate into right

			PSET_GetNextN( pdsp->ppset[0], pbfront, sampleCount, OP_LEFT_DUPLICATE);

			// copy processed front to rear

			count = sampleCount;
			
			pbf = pbfront;
			pbr = pbrear;

			while ( count-- )
			{
				pbr->left = pbf->left;
				pbr->right = pbf->right;
				pbf++;
				pbr++;
			}

		}
		else
		{
			// avg fl,fr,rl,rr into mono fx, duplicate on all channels
			while ( count-- )
			{
				av = ((pbf->left + pbf->right + pbr->left + pbr->right) >> 2);
				x = PSET_GetNext( pdsp->ppset[0], av );
				x = CLIP_DSP( x );
				pbr->left = pbr->right = pbf->left = pbf->right = x;
				pbf++;
				pbr++;
			}
		}
			return;
	}

	if ( bcrossfading )
	{
		int r;
		int fl, fr, rl, rr;
		int flp, frp, rlp, rrp;
		int xf_fl, xf_fr, xf_rl, xf_rr;
		bool bexp = pdsp->bexpfade;
		bool bfadetoquad = (pdsp->ipset == 0);
		bool bfadefromquad = (pdsp->ipsetprev == 0);

		if ( bfadetoquad || bfadefromquad )
		{
			// special case if previous or current preset is 0 (quad passthrough)

			while ( count-- )
			{
				av = ((pbf->left + pbf->right + pbr->left + pbr->right) >> 2);

				// get current preset values
		
				// current preset is 0, which implies fading to passthrough quad output
				// need to fade from mono to quad
				
				if ( pdsp->ipset )
				{
					rl = rr = fl = fr = PSET_GetNext( pdsp->ppset[0], av );
				}
				else
				{
					fl = pbf->left;
					fr = pbf->right;
					rl = pbr->left;
					rr = pbr->right;
				}
				
				// get previous preset values

				if ( pdsp->ipsetprev )
				{
					rrp = rlp = frp = flp = PSET_GetNext( pdsp->ppsetprev[0], av );
				}
				else
				{
					flp = pbf->left;
					frp = pbf->right;
					rlp = pbr->left;
					rrp = pbr->right;
				}
				
				fl = CLIP_DSP(fl);
				fr = CLIP_DSP(fr);
				flp = CLIP_DSP(flp);
				frp = CLIP_DSP(frp);
				rl = CLIP_DSP(rl);
				rr = CLIP_DSP(rr);
				rlp = CLIP_DSP(rlp);
				rrp = CLIP_DSP(rrp);

				// get current ramp value

				r = RMP_GetNext( &pdsp->xramp );	

				// crossfade from previous to current preset
				
				if (!bexp)
				{
					xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
					xf_fr = XFADE(fr, frp, r);	// crossfade front left previous to front left
					xf_rl = XFADE(rl, rlp, r);	// crossfade front left previous to front left
					xf_rr = XFADE(rr, rrp, r);	// crossfade front left previous to front left
				}
				else
				{
					xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left
					xf_fr = XFADE_EXP(fr, frp, r);	// crossfade front left previous to front left
					xf_rl = XFADE_EXP(rl, rlp, r);	// crossfade front left previous to front left
					xf_rr = XFADE_EXP(rr, rrp, r);	// crossfade front left previous to front left
				}

				pbf->left =  xf_fl;			
				pbf->right = xf_fr;
				pbr->left = xf_rl;
				pbr->right = xf_rr;

				pbf++;
				pbr++;
			}

			return;
		}

		while ( count-- )
		{
			
			av = ((pbf->left + pbf->right + pbr->left + pbr->right) >> 2);

			// get current preset values

			fl = PSET_GetNext( pdsp->ppset[0], av );
			
			// get previous preset values

			flp = PSET_GetNext( pdsp->ppsetprev[0], av );
			
			// get current ramp value

			r = RMP_GetNext( &pdsp->xramp );	

			fl = CLIP_DSP( fl );
			flp = CLIP_DSP( flp );

			// crossfade from previous to current preset
			if (!bexp)
				xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
			else
				xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left
			
			pbf->left =  xf_fl;			// crossfaded front left, duplicated to all channels
			pbf->right = xf_fl;
			pbr->left =  xf_fl;			
			pbr->right = xf_fl;

			pbf++;
			pbr++;
		}
	}
}

// Helper: called only from DSP_Process
// DSP_Process quad in to stereo out (preserve stereo spatialization, throw away front/rear)

inline void DSP_ProcessQuadToStereo(dsp_t *pdsp, portable_samplepair_t *pbfront, portable_samplepair_t *pbrear, int sampleCount, bool bcrossfading )
{
	portable_samplepair_t *pbf = pbfront;		// pointer to buffer of front stereo samples to process
	portable_samplepair_t *pbr = pbrear;		// pointer to buffer of rear stereo samples to process
	int count = sampleCount;
	int fl, fr;
	
	if ( !bcrossfading ) 
	{
		if ( !pdsp->ipset ) 
			return;

		if ( FBatchPreset(pdsp->ppset[0]) && FBatchPreset(pdsp->ppset[1]) )
		{

			// convert Quad to Stereo in place, then batch process fx: perf KDB

			// left front + rear -> left, right front + rear -> right

			while ( count-- )
			{
				pbf->left =  (pbf->left + pbr->left) >> 1;
				pbf->right = (pbf->right + pbr->right) >> 1;
				pbf++;
				pbr++;
			}
			
			// process left & right

			PSET_GetNextN( pdsp->ppset[0], pbfront, sampleCount, OP_LEFT);
			PSET_GetNextN( pdsp->ppset[1], pbfront, sampleCount, OP_RIGHT );

			// copy processed front to rear

			count = sampleCount;
			
			pbf = pbfront;
			pbr = pbrear;

			while ( count-- )
			{
				pbr->left = pbf->left;
				pbr->right = pbf->right;
				pbf++;
				pbr++;
			}

		}	
		else
		{
			// left front + rear -> left fx, right front + rear -> right fx
			while ( count-- )
			{
				fl = PSET_GetNext( pdsp->ppset[0], (pbf->left + pbr->left) >> 1);
				fr = PSET_GetNext( pdsp->ppset[1], (pbf->right + pbr->right) >> 1);
				fl = CLIP_DSP( fl );
				fr = CLIP_DSP( fr );

				pbr->left =  pbf->left =  fl;
				pbr->right = pbf->right = fr;
				pbf++;
				pbr++;
			}
		}
		return;
	}

	// crossfading to current preset from previous preset	

	if ( bcrossfading )
	{
		int r;
		int rl, rr;
		int flp, frp, rlp, rrp;
		int xf_fl, xf_fr, xf_rl, xf_rr;
		int avl, avr;
		bool bexp = pdsp->bexpfade;
		bool bfadetoquad = (pdsp->ipset == 0);
		bool bfadefromquad = (pdsp->ipsetprev == 0);

		if ( bfadetoquad || bfadefromquad )
		{
			// special case if previous or current preset is 0 (quad passthrough)

			while ( count-- )
			{
				avl = (pbf->left + pbr->left) >> 1;
				avr = (pbf->right + pbr->right) >> 1;

				// get current preset values
		
				// current preset is 0, which implies fading to passthrough quad output
				// need to fade from stereo to quad
				
				if ( pdsp->ipset )
				{
					rl = fl = PSET_GetNext( pdsp->ppset[0], avl );
					rr = fr = PSET_GetNext( pdsp->ppset[0], avr );
				}
				else
				{
					fl = pbf->left;
					fr = pbf->right;
					rl = pbr->left;
					rr = pbr->right;
				}
				
				// get previous preset values

				if ( pdsp->ipsetprev )
				{
					rlp = flp = PSET_GetNext( pdsp->ppsetprev[0], avl );
					rrp = frp = PSET_GetNext( pdsp->ppsetprev[0], avr );
				}
				else
				{
					flp = pbf->left;
					frp = pbf->right;
					rlp = pbr->left;
					rrp = pbr->right;
				}
				
				fl = CLIP_DSP(fl);
				fr = CLIP_DSP(fr);
				flp = CLIP_DSP(flp);
				frp = CLIP_DSP(frp);
				rl = CLIP_DSP(rl);
				rr = CLIP_DSP(rr);
				rlp = CLIP_DSP(rlp);
				rrp = CLIP_DSP(rrp);

				// get current ramp value

				r = RMP_GetNext( &pdsp->xramp );	

				// crossfade from previous to current preset
				
				if (!bexp)
				{
					xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
					xf_fr = XFADE(fr, frp, r);	// crossfade front left previous to front left
					xf_rl = XFADE(rl, rlp, r);	// crossfade front left previous to front left
					xf_rr = XFADE(rr, rrp, r);	// crossfade front left previous to front left
				}
				else
				{
					xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left
					xf_fr = XFADE_EXP(fr, frp, r);	// crossfade front left previous to front left
					xf_rl = XFADE_EXP(rl, rlp, r);	// crossfade front left previous to front left
					xf_rr = XFADE_EXP(rr, rrp, r);	// crossfade front left previous to front left
				}

				pbf->left =  xf_fl;			
				pbf->right = xf_fr;
				pbr->left = xf_rl;
				pbr->right = xf_rr;

				pbf++;
				pbr++;
			}

			return;
		}

		while ( count-- )
		{
			avl = (pbf->left + pbr->left) >> 1;
			avr = (pbf->right + pbr->right) >> 1;

			// get current preset values

			fl = PSET_GetNext( pdsp->ppset[0], avl );
			fr = PSET_GetNext( pdsp->ppset[1], avr );
			
			// get previous preset values

			flp = PSET_GetNext( pdsp->ppsetprev[0], avl );
			frp = PSET_GetNext( pdsp->ppsetprev[1], avr );
			

			fl = CLIP_DSP( fl );
			fr = CLIP_DSP( fr );
			
			// get previous preset values

			flp = CLIP_DSP( flp );
			frp = CLIP_DSP( frp );

			// get current ramp value

			r = RMP_GetNext( &pdsp->xramp );	

			// crossfade from previous to current preset
			if (!bexp)
			{
				xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
				xf_fr = XFADE(fr, frp, r);
			}
			else
			{
				xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left
				xf_fr = XFADE_EXP(fr, frp, r);
			}
			
			pbf->left =  xf_fl;			// crossfaded front left
			pbf->right = xf_fr;

			pbr->left =  xf_fl;			// duplicate front channel to rear channel
			pbr->right = xf_fr;

			pbf++;
			pbr++;
		}
	}
}

// Helper: called only from DSP_Process
// DSP_Process quad in to quad out

inline void DSP_ProcessQuadToQuad(dsp_t *pdsp, portable_samplepair_t *pbfront, portable_samplepair_t *pbrear, int sampleCount, bool bcrossfading )
{
	portable_samplepair_t *pbf = pbfront;		// pointer to buffer of front stereo samples to process
	portable_samplepair_t *pbr = pbrear;		// pointer to buffer of rear stereo samples to process
	int count = sampleCount;
	int fl, fr, rl, rr;

	if ( !bcrossfading ) 
	{
		if ( !pdsp->ipset ) 
			return;

		// each channel gets its own processor

		if ( FBatchPreset(pdsp->ppset[0]) && FBatchPreset(pdsp->ppset[1]) && FBatchPreset(pdsp->ppset[2]) && FBatchPreset(pdsp->ppset[3]))
		{	
			// batch process fx front & rear, left & right: perf KDB

			PSET_GetNextN( pdsp->ppset[0], pbfront, sampleCount, OP_LEFT);
			PSET_GetNextN( pdsp->ppset[1], pbfront, sampleCount, OP_RIGHT );
			PSET_GetNextN( pdsp->ppset[2], pbrear,  sampleCount, OP_LEFT );
			PSET_GetNextN( pdsp->ppset[3], pbrear,  sampleCount, OP_RIGHT );
		}	
		else
		{
			while ( count-- )
			{
				fl = PSET_GetNext( pdsp->ppset[0], pbf->left );
				fr = PSET_GetNext( pdsp->ppset[1], pbf->right );
				rl = PSET_GetNext( pdsp->ppset[2], pbr->left );
				rr = PSET_GetNext( pdsp->ppset[3], pbr->right );
				
				pbf->left =  CLIP_DSP( fl );
				pbf->right = CLIP_DSP( fr );
				pbr->left =  CLIP_DSP( rl );
				pbr->right = CLIP_DSP( rr );

				pbf++;
				pbr++;
			}
		}
		return;
	}

	// crossfading to current preset from previous preset	

	if ( bcrossfading )
	{
		int r;
		int flp, frp, rlp, rrp;
		int xf_fl, xf_fr, xf_rl, xf_rr;
		bool bexp = pdsp->bexpfade;

		while ( count-- )
		{
			// get current preset values

			fl = PSET_GetNext( pdsp->ppset[0], pbf->left );
			fr = PSET_GetNext( pdsp->ppset[1], pbf->right );
			rl = PSET_GetNext( pdsp->ppset[2], pbr->left );
			rr = PSET_GetNext( pdsp->ppset[3], pbr->right );

			// get previous preset values

			flp = PSET_GetNext( pdsp->ppsetprev[0], pbf->left );
			frp = PSET_GetNext( pdsp->ppsetprev[1], pbf->right );
			rlp = PSET_GetNext( pdsp->ppsetprev[2], pbr->left );
			rrp = PSET_GetNext( pdsp->ppsetprev[3], pbr->right );

			// get current ramp value

			r = RMP_GetNext( &pdsp->xramp );	

			// crossfade from previous to current preset
			if (!bexp)
			{
				xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
				xf_fr = XFADE(fr, frp, r);
				xf_rl = XFADE(rl, rlp, r);
				xf_rr = XFADE(rr, rrp, r);
			}
			else
			{
				xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left
				xf_fr = XFADE_EXP(fr, frp, r);
				xf_rl = XFADE_EXP(rl, rlp, r);
				xf_rr = XFADE_EXP(rr, rrp, r);
			}
			
			pbf->left =  CLIP_DSP(xf_fl);			// crossfaded front left
			pbf->right = CLIP_DSP(xf_fr);
			pbr->left =  CLIP_DSP(xf_rl);
			pbr->right = CLIP_DSP(xf_rr);

			pbf++;
			pbr++;
		}
	}
}


// Helper: called only from DSP_Process
// DSP_Process quad + center in to mono out (front left = front right)

inline void DSP_Process5To1(dsp_t *pdsp, portable_samplepair_t *pbfront, portable_samplepair_t *pbrear, portable_samplepair_t *pbcenter, int sampleCount, bool bcrossfading )
{
	portable_samplepair_t *pbf = pbfront;		// pointer to buffer of front stereo samples to process
	portable_samplepair_t *pbr = pbrear;		// pointer to buffer of rear stereo samples to process
	portable_samplepair_t *pbc = pbcenter;		// pointer to buffer of center mono samples to process	
	int count = sampleCount;
	int x;
	int av;

	if ( !bcrossfading ) 
	{
		if ( !pdsp->ipset ) 
			return;

		if ( FBatchPreset(pdsp->ppset[0]) )
		{

			// convert Quad + Center to Mono in place, then batch process fx: perf KDB

			// left front + rear -> left, right front + rear -> right
			while ( count-- )
			{
				// pbf->left = ((pbf->left + pbf->right + pbr->left + pbr->right + pbc->left) / 5);

				av = (pbf->left + pbf->right + pbr->left + pbr->right + pbc->left) * 51;  // 51/255 = 1/5
				av >>= 8;
				pbf->left = av;
				pbf++;
				pbr++;
				pbc++;
			}
			
			// process left (mono), duplicate into right

			PSET_GetNextN( pdsp->ppset[0], pbfront, sampleCount, OP_LEFT_DUPLICATE);

			// copy processed front to rear & center

			count = sampleCount;
			
			pbf = pbfront;
			pbr = pbrear;
			pbc = pbcenter;

			while ( count-- )
			{
				pbr->left = pbf->left;
				pbr->right = pbf->right;
				pbc->left = pbf->left;
				pbf++;
				pbr++;
				pbc++;
			}

		}
		else
		{
			// avg fl,fr,rl,rr,fc into mono fx, duplicate on all channels
			while ( count-- )
			{
				// av = ((pbf->left + pbf->right + pbr->left + pbr->right + pbc->left) / 5);
				av = (pbf->left + pbf->right + pbr->left + pbr->right + pbc->left) * 51;  // 51/255 = 1/5
				av >>= 8;
				x = PSET_GetNext( pdsp->ppset[0], av );
				x = CLIP_DSP( x );
				pbr->left = pbr->right = pbf->left = pbf->right = pbc->left = x;
				pbf++;
				pbr++;
				pbc++;
			}
		}
			return;
	}

	if ( bcrossfading )
	{
		int r;
		int fl, fr, rl, rr, fc;
		int flp, frp, rlp, rrp, fcp;
		int xf_fl, xf_fr, xf_rl, xf_rr, xf_fc;
		bool bexp = pdsp->bexpfade;
		bool bfadetoquad = (pdsp->ipset == 0);
		bool bfadefromquad = (pdsp->ipsetprev == 0);

		if ( bfadetoquad || bfadefromquad )
		{
			// special case if previous or current preset is 0 (quad passthrough)

			while ( count-- )
			{
				// av = ((pbf->left + pbf->right + pbr->left + pbr->right) >> 2);

				av = (pbf->left + pbf->right + pbr->left + pbr->right + pbc->left) * 51;  // 51/255 = 1/5
				av >>= 8;

				// get current preset values
		
				// current preset is 0, which implies fading to passthrough quad output
				// need to fade from mono to quad
				
				if ( pdsp->ipset )
				{
					fc = rl = rr = fl = fr = PSET_GetNext( pdsp->ppset[0], av );
				}
				else
				{
					fl = pbf->left;
					fr = pbf->right;
					rl = pbr->left;
					rr = pbr->right;
					fc = pbc->left;
				}
				
				// get previous preset values

				if ( pdsp->ipsetprev )
				{
					fcp = rrp = rlp = frp = flp = PSET_GetNext( pdsp->ppsetprev[0], av );
				}
				else
				{
					flp = pbf->left;
					frp = pbf->right;
					rlp = pbr->left;
					rrp = pbr->right;
					fcp = pbc->left;
				}
				
				fl = CLIP_DSP(fl);
				fr = CLIP_DSP(fr);
				flp = CLIP_DSP(flp);
				frp = CLIP_DSP(frp);
				rl = CLIP_DSP(rl);
				rr = CLIP_DSP(rr);
				rlp = CLIP_DSP(rlp);
				rrp = CLIP_DSP(rrp);
				fc = CLIP_DSP(fc);
				fcp = CLIP_DSP(fcp);

				// get current ramp value

				r = RMP_GetNext( &pdsp->xramp );	

				// crossfade from previous to current preset
				
				if (!bexp)
				{
					xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
					xf_fr = XFADE(fr, frp, r);	// crossfade front left previous to front left
					xf_rl = XFADE(rl, rlp, r);	// crossfade front left previous to front left
					xf_rr = XFADE(rr, rrp, r);	// crossfade front left previous to front left
					xf_fc = XFADE(fc, fcp, r);	// crossfade front left previous to front left
				}
				else
				{
					xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left
					xf_fr = XFADE_EXP(fr, frp, r);	// crossfade front left previous to front left
					xf_rl = XFADE_EXP(rl, rlp, r);	// crossfade front left previous to front left
					xf_rr = XFADE_EXP(rr, rrp, r);	// crossfade front left previous to front left
					xf_fc = XFADE_EXP(fc, fcp, r);	// crossfade front left previous to front left
				}

				pbf->left =  xf_fl;			
				pbf->right = xf_fr;
				pbr->left = xf_rl;
				pbr->right = xf_rr;
				pbc->left = xf_fc;

				pbf++;
				pbr++;
				pbc++;
			}

			return;
		}

		while ( count-- )
		{
			
			// av = ((pbf->left + pbf->right + pbr->left + pbr->right) >> 2);
			av = (pbf->left + pbf->right + pbr->left + pbr->right + pbc->left) * 51;  // 51/255 = 1/5
			av >>= 8;

			// get current preset values

			fl = PSET_GetNext( pdsp->ppset[0], av );
			
			// get previous preset values

			flp = PSET_GetNext( pdsp->ppsetprev[0], av );
			
			// get current ramp value

			r = RMP_GetNext( &pdsp->xramp );	

			fl = CLIP_DSP( fl );
			flp = CLIP_DSP( flp );

			// crossfade from previous to current preset
			if (!bexp)
				xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
			else
				xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left
			
			pbf->left =  xf_fl;			// crossfaded front left, duplicated to all channels
			pbf->right = xf_fl;
			pbr->left =  xf_fl;			
			pbr->right = xf_fl;
			pbc->left =  xf_fl;

			pbf++;
			pbr++;
			pbc++;
		}
	}
}

// Helper: called only from DSP_Process
// DSP_Process quad + center in to quad + center out

inline void DSP_Process5To5(dsp_t *pdsp, portable_samplepair_t *pbfront, portable_samplepair_t *pbrear, portable_samplepair_t *pbcenter, int sampleCount, bool bcrossfading )
{
	portable_samplepair_t *pbf = pbfront;		// pointer to buffer of front stereo samples to process
	portable_samplepair_t *pbr = pbrear;		// pointer to buffer of rear stereo samples to process
	portable_samplepair_t *pbc = pbcenter;		// pointer to buffer of center mono samples to process

	int count = sampleCount;
	int fl, fr, rl, rr, fc;

	if ( !bcrossfading ) 
	{
		if ( !pdsp->ipset ) 
			return;

		// each channel gets its own processor

		if ( FBatchPreset(pdsp->ppset[0]) && FBatchPreset(pdsp->ppset[1]) && FBatchPreset(pdsp->ppset[2]) && FBatchPreset(pdsp->ppset[3]))
		{	
			// batch process fx front & rear, left & right: perf KDB

			PSET_GetNextN( pdsp->ppset[0], pbfront, sampleCount, OP_LEFT);
			PSET_GetNextN( pdsp->ppset[1], pbfront, sampleCount, OP_RIGHT );
			PSET_GetNextN( pdsp->ppset[2], pbrear,  sampleCount, OP_LEFT );
			PSET_GetNextN( pdsp->ppset[3], pbrear,  sampleCount, OP_RIGHT );
			PSET_GetNextN( pdsp->ppset[4], pbcenter,  sampleCount, OP_LEFT );
		}	
		else
		{
			while ( count-- )
			{
				fl = PSET_GetNext( pdsp->ppset[0], pbf->left );
				fr = PSET_GetNext( pdsp->ppset[1], pbf->right );
				rl = PSET_GetNext( pdsp->ppset[2], pbr->left );
				rr = PSET_GetNext( pdsp->ppset[3], pbr->right );
				fc = PSET_GetNext( pdsp->ppset[4], pbc->left );
				
				pbf->left =  CLIP_DSP( fl );
				pbf->right = CLIP_DSP( fr );
				pbr->left =  CLIP_DSP( rl );
				pbr->right = CLIP_DSP( rr );
				pbc->left =  CLIP_DSP( fc );

				pbf++;
				pbr++;
				pbc++;
			}
		}
		return;
	}

	// crossfading to current preset from previous preset	

	if ( bcrossfading )
	{
		int r;
		int flp, frp, rlp, rrp, fcp;
		int xf_fl, xf_fr, xf_rl, xf_rr, xf_fc;
		bool bexp = pdsp->bexpfade;

		while ( count-- )
		{
			// get current preset values

			fl = PSET_GetNext( pdsp->ppset[0], pbf->left );
			fr = PSET_GetNext( pdsp->ppset[1], pbf->right );
			rl = PSET_GetNext( pdsp->ppset[2], pbr->left );
			rr = PSET_GetNext( pdsp->ppset[3], pbr->right );
			fc = PSET_GetNext( pdsp->ppset[4], pbc->left );

			// get previous preset values

			flp = PSET_GetNext( pdsp->ppsetprev[0], pbf->left );
			frp = PSET_GetNext( pdsp->ppsetprev[1], pbf->right );
			rlp = PSET_GetNext( pdsp->ppsetprev[2], pbr->left );
			rrp = PSET_GetNext( pdsp->ppsetprev[3], pbr->right );
			fcp = PSET_GetNext( pdsp->ppsetprev[4], pbc->left );

			// get current ramp value

			r = RMP_GetNext( &pdsp->xramp );	

			// crossfade from previous to current preset
			if (!bexp)
			{
				xf_fl = XFADE(fl, flp, r);	// crossfade front left previous to front left
				xf_fr = XFADE(fr, frp, r);
				xf_rl = XFADE(rl, rlp, r);
				xf_rr = XFADE(rr, rrp, r);
				xf_fc = XFADE(fc, fcp, r);
			}
			else
			{
				xf_fl = XFADE_EXP(fl, flp, r);	// crossfade front left previous to front left
				xf_fr = XFADE_EXP(fr, frp, r);
				xf_rl = XFADE_EXP(rl, rlp, r);
				xf_rr = XFADE_EXP(rr, rrp, r);
				xf_fc = XFADE_EXP(fc, fcp, r);
			}
			
			pbf->left =  CLIP_DSP(xf_fl);			// crossfaded front left
			pbf->right = CLIP_DSP(xf_fr);
			pbr->left =  CLIP_DSP(xf_rl);
			pbr->right = CLIP_DSP(xf_rr);
			pbc->left =  CLIP_DSP(xf_fc);

			pbf++;
			pbr++;
			pbc++;
		}
	}
}

// This is an evil hack, but we need to restore the old presets after letting the sound system update for a few frames, so we just
//  "defer" the restore until the top of the next call to CheckNewDspPresets.  I put in a bit of warning in case we ever have code
//  outside of this time period modifying any of the dsp convars.  It doesn't seem to be an issue just save/loading between levels
static bool g_bNeedPresetRestore = false;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct PreserveDSP_t
{
	ConVar *cvar;
	float	oldvalue;
};

static PreserveDSP_t g_PreserveDSP[] =
{
	{ &dsp_room },
	{ &dsp_water },
	{ &dsp_player },
	{ &dsp_facingaway },
	{ &dsp_speaker },
	{ &dsp_spatial },
	{ &dsp_automatic }
};

//-----------------------------------------------------------------------------
// Purpose: Called at the top of CheckNewDspPresets to restore ConVars to real values
//-----------------------------------------------------------------------------
void DSP_CheckRestorePresets()
{
	if ( !g_bNeedPresetRestore )
		return;

	g_bNeedPresetRestore = false;

	int i;
	int c = ARRAYSIZE( g_PreserveDSP );

	// Restore
	for ( i = 0 ; i < c; ++i )
	{
		PreserveDSP_t& slot = g_PreserveDSP[ i ];

		ConVar *cv = slot.cvar;
		Assert( cv );
		if ( cv->GetFloat() != 0.0f )
		{
			// NOTE: dsp_speaker is being (correctly) save/restored by maps, which would trigger this warning
			//Warning( "DSP_CheckRestorePresets:  Value of %s was changed between DSP_ClearState and CheckNewDspPresets, not restoring to old value\n", cv->GetName() );
			continue;
		}
		cv->SetValue( slot.oldvalue );
	}

	// reinit all dsp processors (only load preset file on engine init, however)

	AllocDsps( false );

	// flush dsp automatic nodes

	g_bdas_init_nodes = 0;
	g_bdas_room_init = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DSP_ClearState()
{
	// if we already cleared dsp state, and a restore is pending, 
	// don't clear again

	if ( g_bNeedPresetRestore )
		return;

	// always save a cleared dsp automatic value to force reset of all adsp code

	dsp_automatic.SetValue(0);

	// Tracker 7155:  YWB:  This is a pretty ugly hack to zero out all of the dsp convars and bootstrap the dsp system into using them for a few frames
	
	int i;
	int c = ARRAYSIZE( g_PreserveDSP );

	for ( i = 0 ; i < c; ++i )
	{
		PreserveDSP_t& slot = g_PreserveDSP[ i ];

		ConVar *cv = slot.cvar;
		Assert( cv );
		slot.oldvalue = cv->GetFloat();
		cv->SetValue( 0 );
	}
	
	// force all dsp presets to end crossfades, end one-shot presets, & release and reset all resources 
	// immediately.
	
	FreeDsps( false ); // free all dsp states, but don't discard preset templates

	// This forces the ConVars which we set to zero above to be reloaded to their old values at the time we issue the CheckNewDspPresets
	//  command.  This seems to happen early enough in level changes were we don't appear to be trying to stomp real settings...

	g_bNeedPresetRestore = true;
}

// return true if dsp's preset is one-shot and it has expired

bool DSP_HasExpired( int idsp )
{
	dsp_t *pdsp;

	Assert( idsp < CDSPS );

	if (idsp < 0 || idsp >= CDSPS)
		return false;

	pdsp = &dsps[idsp];

	// if first preset has expired, dsp has expired

	if ( PSET_IsOneShot( pdsp->ppset[0] ) )
		return PSET_HasExpired( pdsp->ppset[0] );	
	else
		return false;
}

// returns true if dsp is crossfading from previous dsp preset

bool DSP_IsCrossfading( int idsp )
{
	dsp_t *pdsp;

	Assert( idsp < CDSPS );

	if (idsp < 0 || idsp >= CDSPS)
		return false;

	pdsp = &dsps[idsp];

	return !RMP_HitEnd( &pdsp->xramp );

}

// returns previous preset # before oneshot preset was set

int DSP_OneShotPrevious( int idsp )
{
	dsp_t *pdsp;
	int idsp_prev;

	Assert( idsp < CDSPS );

	if (idsp < 0 || idsp >= CDSPS)
		return 0;

	pdsp = &dsps[idsp];
	
	idsp_prev = pdsp->ipsetsav_oneshot;

	return idsp_prev;
}

// given idsp (processor index), return true if 
// both current and previous presets are 0 for this processor

bool DSP_PresetIsOff( int idsp )
{
	dsp_t *pdsp;

	if (idsp < 0 || idsp >= CDSPS)
		return true;

	Assert ( idsp < CDSPS );					// make sure idsp is valid

	pdsp = &dsps[idsp];

	// if current and previous preset 0, return - preset 0 is 'off'

	return ( !pdsp->ipset && !pdsp->ipsetprev );
}

// returns true if dsp is off for room effects

bool DSP_RoomDSPIsOff()
{
	return DSP_PresetIsOff( Get_idsp_room() );
}

// Main DSP processing routine:
// process samples in buffers using pdsp processor
// continue crossfade between 2 dsp processors if crossfading on switch
// pfront - front stereo buffer to process
// prear - rear stereo buffer to process (may be NULL)
// pcenter - front center mono buffer (may be NULL)
// sampleCount - number of samples in pbuf to process
// This routine also maps the # processing channels in the pdsp to the number of channels 
// supplied.  ie: if the pdsp has 4 channels and pbfront and pbrear are both non-null, the channels
// map 1:1 through the processors.

void DSP_Process( int idsp, portable_samplepair_t *pbfront, portable_samplepair_t *pbrear, portable_samplepair_t *pbcenter, int sampleCount )
{
	bool bcrossfading;
	int cchan_in;								// input channels (2,4 or 5)
	int cprocs;									// output cannels (1, 2 or 4)
	dsp_t *pdsp;

	if (idsp < 0 || idsp >= CDSPS)
		return;

	// Don't pull dsp data in if player is not connected (during load/level change)
	if ( !g_pSoundServices->IsConnected() )
		return;

	Assert ( idsp < CDSPS );					// make sure idsp is valid

	pdsp = &dsps[idsp];

	Assert (pbfront);

	// return right away if fx processing is turned off

	if ( dsp_off.GetInt() )
		return;

	// if current and previous preset 0, return - preset 0 is 'off'

	if ( !pdsp->ipset && !pdsp->ipsetprev )
		return;

	if ( sampleCount < 0 )
		return;

	bcrossfading = !RMP_HitEnd( &pdsp->xramp );

	// if not crossfading, and previous channel is not null, free previous
	
	if ( !bcrossfading )
		DSP_FreePrevPreset( pdsp );

	// if current and previous preset 0 (ie: just freed previous), return - preset 0 is 'off'

	if ( !pdsp->ipset && !pdsp->ipsetprev )
		return;

	cchan_in = (pbrear ? 4 : 2) + (pbcenter ? 1 : 0);
	cprocs = pdsp->cchan;

	Assert(cchan_in == 2 || cchan_in == 4 || cchan_in == 5 );

	// if oneshot preset, update the duration counter (only update front left counter)

	PSET_UpdateDuration( pdsp->ppset[0], sampleCount );

	// NOTE: when mixing between different channel sizes, 
	// always AVERAGE down to fewer channels and DUPLICATE up more channels.
	// The following routines always process cchan_in channels. 
	// ie: QuadToMono still updates 4 values in buffer

	// DSP_Process stereo in to mono out (ie: left and right are averaged)

	if ( cchan_in == 2 && cprocs == 1)
	{
		DSP_ProcessStereoToMono( pdsp, pbfront, pbrear, sampleCount, bcrossfading );
		return;
	}

	// DSP_Process stereo in to stereo out (if more than 2 procs, ignore them)

	if ( cchan_in == 2 && cprocs >= 2)
	{
		DSP_ProcessStereoToStereo( pdsp, pbfront, pbrear, sampleCount, bcrossfading );
		return;
	}


	// DSP_Process quad in to mono out

	if ( cchan_in == 4 && cprocs == 1)
	{
		DSP_ProcessQuadToMono( pdsp, pbfront, pbrear, sampleCount, bcrossfading );
		return;
	}


	// DSP_Process quad in to stereo out (preserve stereo spatialization, loose front/rear)

	if ( cchan_in == 4 && cprocs == 2)
	{
		DSP_ProcessQuadToStereo( pdsp, pbfront, pbrear, sampleCount, bcrossfading );
		return;
	}


	// DSP_Process quad in to quad out

	if ( cchan_in == 4 && cprocs == 4)
	{
		DSP_ProcessQuadToQuad( pdsp, pbfront, pbrear, sampleCount, bcrossfading );
		return;
	}

	// DSP_Process quad + center in to mono out

	if ( cchan_in == 5 && cprocs == 1)
	{
		DSP_Process5To1( pdsp, pbfront, pbrear, pbcenter, sampleCount, bcrossfading );
		return;
	}

	if ( cchan_in == 5 && cprocs == 2)
	{
		// undone: not used in AllocDsps
		Assert(false);
		//DSP_Process5to2( pdsp, pbfront, pbrear, pbcenter, sampleCount, bcrossfading );
		return;
	}

	if ( cchan_in == 5 && cprocs == 4)
	{
		// undone: not used in AllocDsps
		Assert(false);
		//DSP_Process5to4( pdsp, pbfront, pbrear, pbcenter, sampleCount, bcrossfading );
		return;
	}

	// DSP_Process quad + center in to quad + center out

	if ( cchan_in == 5 && cprocs == 5)
	{
		DSP_Process5To5( pdsp, pbfront, pbrear, pbcenter, sampleCount, bcrossfading );
		return;
	}

}

// DSP helpers

// free all dsp processors 

void FreeDsps( bool bReleaseTemplateMemory )
{

	DSP_Free(idsp_room);
	DSP_Free(idsp_water);
	DSP_Free(idsp_player);
	DSP_Free(idsp_facingaway);
	DSP_Free(idsp_speaker);
	DSP_Free(idsp_spatial);
	DSP_Free(idsp_automatic);

	idsp_room = 0;
	idsp_water = 0;
	idsp_player = 0;
	idsp_facingaway = 0;
	idsp_speaker = 0;
	idsp_spatial = 0;
	idsp_automatic = 0;

	for ( int i = SOUND_BUFFER_SPECIAL_START; i < g_paintBuffers.Count(); ++i )
	{
		paintbuffer_t *pSpecialBuffer = MIX_GetPPaintFromIPaint( i );
		if ( pSpecialBuffer->nSpecialDSP != 0 )
		{
			DSP_Free( pSpecialBuffer->idsp_specialdsp );
			pSpecialBuffer->idsp_specialdsp = 0;
			pSpecialBuffer->nPrevSpecialDSP = 0;
			pSpecialBuffer->nSpecialDSP = 0;
		}
	}

	DSP_FreeAll();
	
	// only unlock and free psettemplate memory on engine shutdown

	if ( bReleaseTemplateMemory )
		DSP_ReleaseMemory();
}

// alloc dsp processors, load dsp preset array from file on engine init only

bool AllocDsps( bool bLoadPresetFile )
{
	int csurround = (g_AudioDevice->IsSurround() ? 2: 0);		// surround channels to allocate
	int ccenter = (g_AudioDevice->IsSurroundCenter() ? 1 : 0);	// center channels to allocate

	DSP_InitAll( bLoadPresetFile );

	idsp_room = -1;
	idsp_water = -1;
	idsp_player = -1;
	idsp_facingaway = -1;
	idsp_speaker = -1;
	idsp_spatial = -1;
	idsp_automatic = -1;

	// alloc dsp room channel (mono, stereo if dsp_stereo is 1)
	
	// dsp room is mono, 300ms default fade time

	idsp_room = DSP_Alloc( dsp_room.GetInt(), 200, 1 ); 

	// dsp automatic overrides dsp_room, if dsp_room set to DSP_AUTOMATIC (1)

	idsp_automatic = DSP_Alloc( dsp_automatic.GetInt(), 200, 1 ) ; 

	// alloc stereo or quad series processors for player or water
	
	// water and player presets are mono

	idsp_water = DSP_Alloc( dsp_water.GetInt(), 100, 1 );
	idsp_player = DSP_Alloc( dsp_player.GetInt(), 100, 1 );

	// alloc facing away filters (stereo, quad or 5ch)

	idsp_facingaway = DSP_Alloc( dsp_facingaway.GetInt(), 100, 2 + csurround + ccenter );

	// alloc speaker preset (mono)

	idsp_speaker = DSP_Alloc( dsp_speaker.GetInt(), 300, 1 );

	// alloc spatial preset (2-5 chan)

	idsp_spatial = DSP_Alloc( dsp_spatial.GetInt(), 300, 2 + csurround + ccenter );

	// init prev values

	ipset_room_prev			= dsp_room.GetInt();
	ipset_water_prev		= dsp_water.GetInt();
	ipset_player_prev		= dsp_player.GetInt();
	ipset_facingaway_prev	= dsp_facingaway.GetInt();
	ipset_room_typeprev		= dsp_room_type.GetInt();
	ipset_speaker_prev		= dsp_speaker.GetInt();
	ipset_spatial_prev		= dsp_spatial.GetInt();
	ipset_automatic_prev	= dsp_automatic.GetInt();

	if (idsp_room < 0 || idsp_water < 0 || idsp_player < 0 || idsp_facingaway < 0 || idsp_speaker < 0 || idsp_spatial < 0 || idsp_automatic < 0)
	{
		DevMsg ("WARNING: DSP processor failed to initialize! \n" );

		FreeDsps( true );
		return false;
	}
		
	return true; 
}

// count number of dsp presets specified in preset file
// counts outer {} pairs, ignoring inner {} pairs.

int DSP_CountFilePresets( const char *pstart )
{
	int cpresets = 0;
	bool binpreset = false;
	bool blookleft = false;

	while ( 1 )
	{
		pstart = COM_Parse( pstart );
		
		if ( strlen(com_token) <= 0)
			break;

		if ( com_token[0] == '{' )  // left paren
		{	
			if (!binpreset)
			{
				cpresets++;			// found preset:
				blookleft = true;	// look for another left
				binpreset = true;
			}
			else 
			{
				blookleft = false; // inside preset: next, look for matching right paren
			}

			continue;
		} 

		if ( com_token[0] == '}' )  // right paren
		{
			if (binpreset)
			{
				if (!blookleft)		// looking for right paren
				{
					blookleft = true; // found it, now look for another left
				}
				else
				{
					// expected inner left paren, found outer right - end of preset definition
					binpreset = false;
					blookleft = true;
				}
			}
			else
			{
				// error - unexpected } paren
				DevMsg("PARSE ERROR!!! dsp_presets.txt: unexpected '}' \n");
				continue;
			}
		}

	}

	return cpresets;
}

struct dsp_stringmap_t
{
	char sz[33];
	int i;
};

// token map for dsp_preset.txt

dsp_stringmap_t gdsp_stringmap[] = 
{
	// PROCESSOR TYPE:
	{"NULL",		PRC_NULL},
	{"DLY",			PRC_DLY},
	{"RVA",			PRC_RVA},
	{"FLT",			PRC_FLT},
	{"CRS",			PRC_CRS},
	{"PTC",			PRC_PTC},
	{"ENV",			PRC_ENV},
	{"LFO",			PRC_LFO},
	{"EFO",			PRC_EFO},
	{"MDY",			PRC_MDY},
	{"DFR",			PRC_DFR},
	{"AMP",			PRC_AMP},

	// FILTER TYPE: 
	{"LP",			FLT_LP},
	{"HP",			FLT_HP},
	{"BP",			FLT_BP},

	// FILTER QUALITY:
	{"LO",			QUA_LO},
	{"MED",			QUA_MED},
	{"HI",			QUA_HI},
	{"VHI",			QUA_VHI},

	// DELAY TYPE:
	{"PLAIN",		DLY_PLAIN},
	{"ALLPASS",		DLY_ALLPASS},
	{"LOWPASS",		DLY_LOWPASS},
	{"DLINEAR",		DLY_LINEAR},
	{"FLINEAR",		DLY_FLINEAR},
	{"LOWPASS_4TAP",DLY_LOWPASS_4TAP},
	{"PLAIN_4TAP",	DLY_PLAIN_4TAP},

	// LFO TYPE: 	
	{"SIN",			LFO_SIN},
	{"TRI",			LFO_TRI},
	{"SQR",			LFO_SQR},
	{"SAW",			LFO_SAW},
	{"RND",			LFO_RND},
	{"LOG_IN",		LFO_LOG_IN},
	{"LOG_OUT",		LFO_LOG_OUT},
	{"LIN_IN",		LFO_LIN_IN},
	{"LIN_OUT",		LFO_LIN_OUT},

	// ENVELOPE TYPE:
	{"LIN",			ENV_LIN},
	{"EXP",			ENV_EXP},

	// PRESET CONFIGURATION TYPE:
	{"SIMPLE",		PSET_SIMPLE},
	{"LINEAR",		PSET_LINEAR},
	{"PARALLEL2",	PSET_PARALLEL2},
	{"PARALLEL4",	PSET_PARALLEL4},
	{"PARALLEL5",	PSET_PARALLEL5},
	{"FEEDBACK",	PSET_FEEDBACK},
	{"FEEDBACK3",	PSET_FEEDBACK3},
	{"FEEDBACK4",	PSET_FEEDBACK4},
	{"MOD1",		PSET_MOD},
	{"MOD2",		PSET_MOD2},
	{"MOD3",		PSET_MOD3}
};

int gcdsp_stringmap = sizeof(gdsp_stringmap) / sizeof (dsp_stringmap_t);

#define isnumber(c) (c == '+' || c == '-' || c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7'|| c == '8' || c == '9')\

// given ptr to null term. string, return integer or float value from g_dsp_stringmap

float DSP_LookupStringToken( char *psz, int ipset )
{
	int i;	
	float fipset = (float)ipset;

	if (isnumber(psz[0]))
		return atof(psz);

	for (i = 0; i < gcdsp_stringmap; i++)
	{
		if (!strcmpi(gdsp_stringmap[i].sz, psz))
			return gdsp_stringmap[i].i;
	}

	// not found

	DevMsg("DSP PARSE ERROR! token not found in dsp_presets.txt. Preset: %3.0f \n", fipset );
	return 0;
}

// load dsp preset file, parse presets into g_psettemplate array
// format for each preset:
// { <preset #> <preset type> <#processors> <gain> { <processor type> <param0>...<param15> } {...} {...} }

#define CHAR_LEFT_PAREN		'{'
#define CHAR_RIGHT_PAREN	'}'

// free preset template memory

void DSP_ReleaseMemory( void )
{
	if (g_psettemplates)
	{
		delete[] g_psettemplates;
		g_psettemplates = NULL;
	}
}

bool DSP_LoadPresetFile( void )
{
	char szFile[ MAX_OSPATH ];
	char *pbuffer;
	const char *pstart;
	bool bResult = false;
	int cpresets;	
	int ipreset;
	int itype;
	int cproc;
	float mix_min;
	float mix_max;
	float db_min;
	float db_mixdrop;
	int j;
	bool fdone;
	float duration;
	float fadeout;

	Q_snprintf( szFile, sizeof( szFile ), "scripts/dsp_presets.txt" );

	MEM_ALLOC_CREDIT();

	CUtlBuffer buf;

	if ( !g_pFullFileSystem->ReadFile( szFile, "GAME", buf ) )
	{
		Error( "DSP_LoadPresetFile: unable to open '%s'\n", szFile );
		return bResult;
	}
	pbuffer = (char *)buf.PeekGet(); // Use malloc - free at end of this routine

	pstart = pbuffer;

	// figure out how many presets we're loading - count outer parens.

	cpresets = DSP_CountFilePresets( pstart );
	
	g_cpsettemplates = cpresets;

	g_psettemplates = new pset_t[cpresets];
	if (!g_psettemplates) 
	{ 
		Warning( "DSP Preset Loader: Out of memory.\n");
		goto load_exit; 
	}
	memset (g_psettemplates, 0, cpresets * sizeof(pset_t));


	// parse presets into g_psettemplates array

	pstart = pbuffer;

	// for each preset...

	for ( j = 0; j < cpresets; j++)
	{
		// check for end of file or next CHAR_LEFT_PAREN

		while (1)
		{
			pstart = COM_Parse( pstart );
		
			if ( strlen(com_token) <= 0)
				break;

			if ( com_token[0] != CHAR_LEFT_PAREN )
				continue;

			break;
		}

		// found start of a new preset definition

		// get preset #, type, cprocessors, gain
		
		pstart = COM_Parse( pstart );
		ipreset = atoi( com_token );

		pstart = COM_Parse( pstart );
		itype = (int)DSP_LookupStringToken( com_token , ipreset);

		pstart = COM_Parse( pstart );
		mix_min = atof( com_token );

		pstart = COM_Parse( pstart );
		mix_max = atof( com_token );

		pstart = COM_Parse( pstart );
		duration = atof( com_token );

		pstart = COM_Parse( pstart );
		fadeout = atof( com_token );
		
		pstart = COM_Parse( pstart );
		db_min = atof( com_token );

		pstart = COM_Parse( pstart );
		db_mixdrop = atof( com_token );


		g_psettemplates[ipreset].fused = true;
		g_psettemplates[ipreset].mix_min = mix_min;
		g_psettemplates[ipreset].mix_max = mix_max;
		g_psettemplates[ipreset].duration = duration;
		g_psettemplates[ipreset].fade = fadeout;
		g_psettemplates[ipreset].db_min = db_min;
		g_psettemplates[ipreset].db_mixdrop = db_mixdrop;
		
		// parse each processor for this preset
		
		fdone = false;
		cproc = 0;

		while (1)
		{
			// find CHAR_LEFT_PAREN - start of new processor

			while (1)
			{
				pstart = COM_Parse( pstart );

				if ( strlen(com_token) <= 0)
					break;

				if (com_token[0] == CHAR_LEFT_PAREN)
					break;

				if (com_token[0] == CHAR_RIGHT_PAREN)
				{
					// if found right paren, no more processors: done with this preset
					fdone = true;
					break;
				}
			}
	
			if ( fdone )
				break;

			// get processor type

			pstart = COM_Parse( pstart );
			g_psettemplates[ipreset].prcs[cproc].type = (int)DSP_LookupStringToken( com_token, ipreset );

			// get param 0..n or stop when hit closing CHAR_RIGHT_PAREN

			int ip = 0;

			while (1)
			{
				pstart = COM_Parse( pstart );

				if ( strlen(com_token) <= 0)
					break;

				if ( com_token[0] == CHAR_RIGHT_PAREN )
					break;

				g_psettemplates[ipreset].prcs[cproc].prm[ip++] = DSP_LookupStringToken( com_token, ipreset );
				
				// cap at max params

				ip = min(ip, CPRCPARAMS);
			}
			
			cproc++;
			if (cproc > CPSET_PRCS)
				DevMsg("DSP PARSE ERROR!!! dsp_presets.txt: missing } or too many processors in preset #: %d \n", ipreset);
			cproc = min(cproc, CPSET_PRCS); // don't overflow # procs
		}
		
		// if cproc == 1, type is always SIMPLE

		if ( cproc == 1)
			itype = PSET_SIMPLE;

		g_psettemplates[ipreset].type = itype;
		g_psettemplates[ipreset].cprcs = cproc;
	
	}

	bResult = true;

load_exit:
	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: Called by client on level shutdown to clear ear ringing dsp effects
//  could be extended to other stuff
//-----------------------------------------------------------------------------
void DSP_FastReset( int dspType )
{
	int c = ARRAYSIZE( g_PreserveDSP );

	// Restore
	for ( int i = 0 ; i < c; ++i )
	{
		PreserveDSP_t& slot = g_PreserveDSP[ i ];

		if ( slot.cvar == &dsp_player )
		{
			slot.oldvalue = dspType;
			return;
		}
	}
}

// Helper to check for change in preset of any of 4 processors
// if switching to a new preset, alloc new preset, simulate both presets in DSP_Process & xfade,
// called a few times per frame.

void CheckNewDspPresets( void )
{
	bool b_slow_cpu = dsp_slow_cpu.GetInt() == 0 ? false : true;

	DSP_CheckRestorePresets();

	//  room fx are on only if cpu is not slow

	int iroom			= b_slow_cpu ? 0 : dsp_room.GetInt() ;	
	int ifacingaway		= b_slow_cpu ? 0 : dsp_facingaway.GetInt();
	int iroomtype		= b_slow_cpu ? 0 : dsp_room_type.GetInt();
	int ispatial		= b_slow_cpu ? 0 : dsp_spatial.GetInt();
	int iautomatic		= b_slow_cpu ? 0 : dsp_automatic.GetInt();

	// always use dsp to process these

	int iwater			= dsp_water.GetInt();
	int iplayer			= dsp_player.GetInt();
	int	ispeaker		= dsp_speaker.GetInt();

	// check for expired one-shot presets on player and room.
	// Only check if a) no new preset has been set and b) not crossfading from previous preset (ie; previous is null)

	if ( iplayer == ipset_player_prev && !DSP_IsCrossfading( idsp_player ) )
	{
		if ( DSP_HasExpired ( idsp_player ) )
		{
			iplayer = DSP_OneShotPrevious( idsp_player);	// preset has expired - revert to previous preset before one-shot
			dsp_player.SetValue(iplayer);
		}
	}

	if ( iroom == ipset_room_prev && !DSP_IsCrossfading( idsp_room ) )
	{
		if ( DSP_HasExpired ( idsp_room ) )
		{
			iroom = DSP_OneShotPrevious( idsp_room );		// preset has expired - revert to previous preset before one-shot
			dsp_room.SetValue(iroom);
		}
	}


	// legacy code support for "room_type" Cvar

	if ( iroomtype != ipset_room_typeprev )
	{
		// force dsp_room = room_type
		
		ipset_room_typeprev = iroomtype;
		dsp_room.SetValue(iroomtype);
	}

	// NOTE: don't change presets if currently crossfading from a previous preset

	if ( iroom != ipset_room_prev && !DSP_IsCrossfading( idsp_room) )
	{
		DSP_SetPreset( idsp_room, iroom );
		ipset_room_prev = iroom;

		// force room_type = dsp_room

		dsp_room_type.SetValue(iroom);
		ipset_room_typeprev = iroom;
	}

	if ( iwater != ipset_water_prev && !DSP_IsCrossfading( idsp_water) )
	{
		DSP_SetPreset( idsp_water, iwater );
		ipset_water_prev = iwater;
	}

	if ( iplayer != ipset_player_prev && !DSP_IsCrossfading( idsp_player))
	{
		DSP_SetPreset( idsp_player, iplayer );
		ipset_player_prev = iplayer;
	}

	if ( ifacingaway != ipset_facingaway_prev && !DSP_IsCrossfading( idsp_facingaway) )
	{
		DSP_SetPreset( idsp_facingaway, ifacingaway );
		ipset_facingaway_prev = ifacingaway;
	}

	if ( ispeaker != ipset_speaker_prev && !DSP_IsCrossfading( idsp_speaker) )
	{
		DSP_SetPreset( idsp_speaker, ispeaker );
		ipset_speaker_prev = ispeaker;
	}

	if ( ispatial != ipset_spatial_prev && !DSP_IsCrossfading( idsp_spatial) )
	{
		DSP_SetPreset( idsp_spatial, ispatial );
		ipset_spatial_prev = ispatial;
	}

	if ( iautomatic != ipset_automatic_prev && !DSP_IsCrossfading( idsp_automatic) )
	{
		DSP_SetPreset( idsp_automatic, iautomatic );
		ipset_automatic_prev = iautomatic;
	}

	for ( int i = SOUND_BUFFER_SPECIAL_START; i < g_paintBuffers.Count(); ++i )
	{
		paintbuffer_t *pSpecialBuffer = MIX_GetPPaintFromIPaint( i );
		if ( pSpecialBuffer->nSpecialDSP != pSpecialBuffer->nPrevSpecialDSP && !DSP_IsCrossfading( pSpecialBuffer->idsp_specialdsp ) )
		{
			DSP_SetPreset( pSpecialBuffer->idsp_specialdsp, pSpecialBuffer->nSpecialDSP );
			pSpecialBuffer->nPrevSpecialDSP = pSpecialBuffer->nSpecialDSP;
		}
	}
}

// create idsp_room preset from set of values, reload the preset.
// modifies psettemplates in place.

// ipreset is the preset # ie: 40
// iproc is the processor to modify within the preset (typically 0)
// pvalues is an array of floating point parameters
// cparams is the # of elements in pvalues

// USED FOR DEBUG ONLY.

void DSP_DEBUGSetParams(int ipreset, int iproc, float *pvalues, int cparams)
{
	pset_t new_pset;	// preset
	int cparam = clamp (cparams, 0, CPRCPARAMS);
	prc_t *pprct;

	// copy template preset from template array

	new_pset = g_psettemplates[ipreset];

	// get iproc processor

	pprct = &(new_pset.prcs[iproc]);

	// copy parameters in to processor

	for (int i = 0; i < cparam; i++)
	{
		pprct->prm[i] = pvalues[i];
	}

	// copy constructed preset back into template location

	g_psettemplates[ipreset] = new_pset;

	// setup new preset

	dsp_room.SetValue( 0 );

	CheckNewDspPresets();

	dsp_room.SetValue( ipreset );

	CheckNewDspPresets();
}

// reload entire preset file, reset all current dsp presets
// NOTE: this is debug code only.  It doesn't do all mem free work correctly!

void DSP_DEBUGReloadPresetFile( void )
{
	int iroom			= dsp_room.GetInt();
	int iwater			= dsp_water.GetInt();
	int iplayer			= dsp_player.GetInt();
//	int ifacingaway		= dsp_facingaway.GetInt();
//	int iroomtype		= dsp_room_type.GetInt();
	int	ispeaker		= dsp_speaker.GetInt();
	int ispatial		= dsp_spatial.GetInt();
//	int iautomatic		= dsp_automatic.GetInt();

	// reload template array

	DSP_ReleaseMemory();

	DSP_LoadPresetFile();

	// force presets to reload

	dsp_room.SetValue( 0 );
	dsp_water.SetValue( 0 );
	dsp_player.SetValue( 0 );
	//dsp_facingaway.SetValue( 0 );
	//dsp_room_type.SetValue( 0 );
	dsp_speaker.SetValue( 0 );
	dsp_spatial.SetValue( 0 );
	//dsp_automatic.SetValue( 0 );

	CUtlVector< int > specialDSPs;
	for ( int i = SOUND_BUFFER_SPECIAL_START; i < g_paintBuffers.Count(); ++i )
	{
		paintbuffer_t *pSpecialBuffer = MIX_GetPPaintFromIPaint( i );

		specialDSPs.AddToTail( pSpecialBuffer->nSpecialDSP );
		pSpecialBuffer->nSpecialDSP = 0;
	}
	
	CheckNewDspPresets();

	dsp_room.SetValue( iroom );
	dsp_water.SetValue( iwater );
	dsp_player.SetValue( iplayer );
	//dsp_facingaway.SetValue( ifacingaway );
	//dsp_room_type.SetValue( iroomtype );
	dsp_speaker.SetValue( ispeaker );
	dsp_spatial.SetValue( ispatial );
	//dsp_automatic.SetValue( iautomatic );

	int nSpecialDSPNum = 0;
	for ( int i = SOUND_BUFFER_SPECIAL_START; i < g_paintBuffers.Count(); ++i )
	{
		paintbuffer_t *pSpecialBuffer = MIX_GetPPaintFromIPaint( i );

		pSpecialBuffer->nSpecialDSP = specialDSPs[ nSpecialDSPNum ];
		nSpecialDSPNum++;
	}

	CheckNewDspPresets();

	// flush dsp automatic nodes

	g_bdas_init_nodes = 0;
	g_bdas_room_init = 0;
}

// UNDONE: stock reverb presets: 

// carpet hallway
// tile hallway
// wood hallway
// metal hallway

// train tunnel
// sewer main tunnel
// concrete access tunnel
// cave tunnel
// sand floor cave tunnel

// metal duct shaft
// elevator shaft
// large elevator shaft

// parking garage
// aircraft hangar
// cathedral
// train station

// small cavern
// large cavern
// huge cavern
// watery cavern
// long, low cavern

// wood warehouse
// metal warehouse
// concrete warehouse

// small closet room
// medium drywall room
// medium wood room
// medium metal room

// elevator
// small metal room
// medium metal room
// large metal room
// huge metal room

// small metal room dense
// medium metal room dense
// large metal room dense
// huge metal room dense

// small concrete room
// medium concrete room
// large concrete room
// huge concrete room

// small concrete room dense
// medium concrete room dense
// large concrete room dense
// huge concrete room dense

// soundproof room
// carpet lobby
// swimming pool
// open park
// open courtyard
// wide parkinglot
// narrow street
// wide street, short buildings
// wide street, tall buildings
// narrow canyon
// wide canyon
// huge canyon
// small valley
// wide valley
// wreckage & rubble
// small building cluster
// wide open plain
// high vista

// alien interior small
// alien interior medium
// alien interior large
// alien interior huge

// special fx presets:

// alien citadel 

// teleport aftershock (these presets all ADSR timeout and reset the dsp_* to 0)
// on target teleport
// off target teleport
// death fade
// beam stasis
// scatterbrain
// pulse only
// slomo
// hypersensitive
// supershocker
// physwhacked
// forcefieldfry
// juiced
// zoomed in
// crabbed
// barnacle gut
// bad transmission

////////////////////////
// Dynamics processing
////////////////////////

// compressor defines
#define COMP_MAX_AMP	32767			// abs max amplitude
#define COMP_THRESH		20000			// start compressing at this threshold

// compress input value - smoothly limit output y to -32767 <= y <= 32767
// UNDONE: not tested or used

inline int S_Compress( int xin )
{

	return CLIP( xin >> 2 );	// DEBUG - disabled


	float Yn, Xn, Cn, Fn;
	float C0 = 20000;	// threshold
	float p = .3;		// compression ratio
	float g = 1;		// gain after compression
	
	Xn = (float)xin;

	// Compressor formula:
	// Cn = l*Cn-1 + (1-l)*|Xn|				// peak detector with memory
	// f(Cn) = (Cn/C0)^(p-1)	for Cn > C0	// gain function above threshold
	// f(Cn) = 1				for C <= C0	// unity gain below threshold
	// Yn = f(Cn) * Xn						// compressor output
	
	// UNDONE: curves discontinuous at threshold, causes distortion, try catmul-rom

	//float l = .5;		// compressor memory
	//Cn = l * (*pCnPrev) + (1 - l) * fabs((float)xin);
	//*pCnPrev = Cn;
	
	Cn = fabs((float)xin);

	if (Cn < C0)
		Fn = 1;
	else
		Fn = powf((Cn / C0),(p - 1));
		
	Yn = Fn * Xn * g;
	
	//if (Cn > 0)
	//	Msg("%d -> %d\n", xin, (int)Yn);	// DEBUG

	//if (fabs(Yn) > 32767)
	//	Yn = Yn;			// DEBUG

	return (CLIP((int)Yn));
}

