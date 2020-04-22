//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//

#include "tier0/platform.h"

#ifdef PLATFORM_WINDOWS

#include "studiorender.h"
#include "studio.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imorph.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "optimize.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include <malloc.h>
#include "mathlib/vmatrix.h"
#include "studiorendercontext.h"
#include "tier2/tier2.h"
#include "tier0/vprof.h"
//#include "tier0/miniprofiler.h"
#include <algorithm>
#include "filesystem.h"

#define PROFILE_THIS_FILE 0


//DLL_IMPORT CLinkedMiniProfiler *g_pOtherMiniProfilers;
#if PROFILE_THIS_FILE

#if !ENABLE_HARDWARE_PROFILER
#error "can't profile without profiler enabled"
#endif

CLinkedMiniProfiler g_mp_morph_Vx("morph_Vx", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_Vw("morph_Vw", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_lower_bound("morph_lower_bound", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph("morph", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V1("morph_V1", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V2("morph_V2", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V3("morph_V3", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V4("morph_V4", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V5("morph_V5", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V6("morph_V6", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V7("morph_V7", &g_pOtherMiniProfilers);

CLinkedMiniProfiler* g_mp_ComputeFlexedVertex_StreamOffset[8] = 
{
	NULL,
	&g_mp_morph_V1,
	&g_mp_morph_V2,
	&g_mp_morph_V3,
	&g_mp_morph_V4,
	&g_mp_morph_V5,
	&g_mp_morph_V6,
	&g_mp_morph_V7
};
#else
uint32 g_mp_morph_Vx[2];
uint32 g_mp_morph_Vw[2];
#endif

#ifdef _X360
ConVar g_cv_morph_path("morph_path", "7");
#ifdef _DEBUG
ConVar g_cv_morph_debug("morph_debug", "0");
#endif // _DEBUG
#endif // _X360


#ifdef _X360
const ALIGN16 int32 g_perm_speed_side[4] = {0x12, 0x13, 0x12, 0x13};
const ALIGN16 int32 g_perm_delta[4] = {0x14150000, 0x16170000, 0x18190000, 0};
const ALIGN16 int32 g_perm_delta_wrinkle[4] = {0x14150000, 0x16170000, 0x18190000, 0x10110000}; // includes the f3PreDelta's W that's in the X component
const ALIGN16 int32 g_perm_ndelta[4] = {0x1A1B0000, 0x1C1D0000, 0x1E1F0000, 0};
//const ALIGN16 int32 g_perm_w0[4]     = {0x00010203,0x08090A0B,0x00010203,0x08090A0B};
const ALIGN16 int32 g_perm_w1[4]     = {0x0C0D0E0F,0x0C0D0E0F,0x04050607,0x04050607};
const fltx4 g_sc256_255_special = {256.0f/255.0f,256.0f/255.0f,-256.0f/255.0f,-256.0f/255.0f};
const fltx4 g_f40011 = {0,0,1,1};
fltx4 g_dummy2[2];

int g_nStreamOffset_prefetch = 256;














//
// V4 rolled - latency of x4, manually scheduled for nearly optimal dual-issue and no automatic stalls
// the ~15 nops mean 1 instruction is issued at that cycle, instead of theoretically possible 2 per cycle
//
__declspec(naked) int ComputeFlexedVertex_StreamOffset_V7(
	int nThinFlexVertexCount,		//r3
	CachedPosNorm_t *pThinFlexVerts,//r4
	int32 *pFirstThinFlexIndex,		//r5
	mstudiovertanim_t * pVert,		//r6
	uint32 nCurrentTag,				//r7
	uint32 numVertsToProcess,		//r8
	fltx4 w1234						//vr1
	)
{
	__asm
	{
		std	r14, -0x08(r1)
		std r15, -0x10(r1)
		std r16, -0x18(r1)
		std r17, -0x20(r1)
		std r18, -0x28(r1)
		std r19, -0x30(r1)
		std r20, -0x38(r1)
		std r21, -0x40(r1)
		std r22, -0x48(r1)
		std r23, -0x50(r1)
		std r24, -0x58(r1)
		std r25, -0x60(r1)

		// let the compiler schedule the instructions, just use several registers to avoid dependencies
		lau r14, g_sc256_255_special
		lal r14, r14, g_sc256_255_special
		lvx vr2, r0,r14

		lau r15, g_f40011
		lal r15, r15, g_f40011
		lvx vr3, r0,r15

		lau r16, g_perm_speed_side
		lal r16, r16, g_perm_speed_side
		lvx vr4, r0,r16

		lau r17, g_perm_delta
		lal r17, r17, g_perm_delta
		lvx vr5, r0,r17

		lau r18, g_perm_ndelta
		lal r18, r18, g_perm_ndelta
		lvx vr6, r0,r18

		lau r20, g_dummy2
		lal r20,r20, g_dummy2
		mr r21, r20
		mr r22, r21
		mr r23, r22

		li r10, -1
		rldicl r7,r7,0,32   // currentTag &= 0xFFFFFFFF ; just to make sure we don't mess up isCacheInvalid computation
		rldicl r10,r10,0,48 // r10 = 0x0000FFFF

		vxor vr8,vr8,vr8

		li r15, 16
		
		li r11,0x100
		li r24, MAXSTUDIOFLEXVERTS - 4

		mtctr r8
		mftb r25
		vxor vr19,vr19,vr19
		vxor vr20,vr20,vr20
		nop	// align!
		nop
		nop

label_start_V7: // 52 instructions run in 45 cycles, although compiler predicts 38 cycles
		////////////////
		// IMPORTANT: DO NOT REMOVE NOPS UNLESS YOU KNOW WHAT YOU ARE DOING AND WHY!
		// nops are essential here, removing them will make the code about 2% slower because dual-issue will be broken
		////////////////
		lhz r14, 0(r6) // int n = pVert->index;	
		addi r16, r3, 2	
		dcbt r11,r6
		cmpw r3, r24     // compare nThinFlexVertexCount to MAXSTUDIOFLEXVERTS - 2
		lvlx vr9,r0,r6	
		rldicl r14, r14, 2, 0 // r14 = n*4	
		lvrx vr10,r15,r6	
		rldicl r16, r16, 5, 0 // r16 = (nThinFlexVertexCount+2) * 32 + pThinFlexVerts	
		vor vr9,vr9,vr10  // vr9 = packedVert = LoadUnalignedSIMD(pVert)	
			addi r31,r31,0//vpermwi128 vr40,vr40,0x1B //mr r31,r31
		add r16, r16, r4	
			vpermwi128 vr40,vr40,0x1B //mr r30,r30
		addi r6, r6, 0x10 // pVert++	
			vpermwi128 vr41,vr41,0x1B//nop
		lwzx r17, r14, r5    // r17 = oldCache	
			//addi r30,r30,0//nop
		vperm vr10, vr8, vr9, vr4	
			//addi r29,r29,0//nop
		xor r18, r17, r7     // cacheVertexIndex = oldCache^nCurrentTag	
		vperm vr11, vr8, vr9, vr5	
		stvx vr8, r0,r16	
		/*S:2*/		vmsum4fp128 vr29,vr19, vr1  // vr29 = scWeight 
		subf r18,r18,r10     // (0xFFFF-cacheVertexIndex) >> 32	
		/*S:1*/	vpermwi128 vr25, vr20, 0x22 // depends on vmadd vr20 = f4sb
		stvx vr8, r15,r16	
		/*S:1*/	vpermwi128 vr26, vr20, 0xF5		
		vcsxwfp vr10,vr10,8		
		or r19,r3,r7		
		vperm vr12, vr8, vr9, vr6		
		sradi r18,r18,32     // r18 = isCacheInvalid : form mask		
		/*S:3*/			stvx vr30, r0,r23
			//nop
		/*S:3*/			stvx vr31, r15,r23
			//nop
		andc r17, r17, r18   // r17 = oldCache & ~isCacheInvalid		
			//nop
		subf r3, r18, r3  // nThinFlexVertexCount = nThinFlexVertexCount + (isCacheInvalid&1);	
			//nop
		and r19,r19,r18      // r19 = newCache & isCacheInvalid		
			//nop
		/*S:2*/mr r23,r22
			//nop
		or r19, r19, r17     // r19 = updateCache		
		/*S:2*/	lvx vr13, r0,r22    // vr13 = vfPosition	
		/*S:2*/	lvx vr14, r15,r22    // vr14 = vfNormal	
			//nop
		rldicl r17, r19, 5,43 // r17 = (updateCache & 0xFFFF) * 32   = nVertexIndex * 32		
			//nop
		/*S:1*/	vmulfp128 vr19, vr25, vr26		
		/*S:1*/mr r22, r21
		vmaddfp vr20, vr10, vr2, vr3 // vr20 = f4sb	
		add r21, r17, r4      // r21 = pFlexedVertex, goes to Stage:1
		/*S:2*/	vmaddfp vr30, vr29, vr21, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition)
		stwx r19, r14, r5
		/*S:2*/	vmaddfp vr31, vr29, vr22, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)
		/*S:1*/	vpermwi128 vr21, vr32, 0x1B
		/*S:1*/	vpermwi128 vr22, vr33, 0x1B
		vcsxwfp128 vr32, vr11, 28
			//nop
		vcsxwfp128 vr33, vr12, 28
		bgt label_end_V7
		dcbt r11, r21
		bdnz label_start_V7
label_end_V7:
		
		/*S:2*/		vmsum4fp128 vr29,vr19, vr1  // vr29 = scWeight 
		/*S:1*/	vpermwi128 vr25, vr20, 0x22 // depends on vmadd vr20 = f4sb
		/*S:1*/	vpermwi128 vr26, vr20, 0xF5		
		/*S:3*/			stvx vr30, r0,r23
		/*S:3*/			stvx vr31, r15,r23
		/*S:2*/mr r23,r22
		/*S:2*/	lvx vr13, r0,r22    // vr13 = vfPosition	
		/*S:2*/	lvx vr14, r15,r22    // vr14 = vfNormal	
		/*S:1*/	vmulfp128 vr19, vr25, vr26		
		/*S:1*/mr r22, r21
		/*S:2*/		vmaddfp vr30, vr29, vr21, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition)
		/*S:2*/		vmaddfp vr31, vr29, vr22, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)
		/*S:1*/	vpermwi128 vr21, vr32, 0x1B
		/*S:1*/	vpermwi128 vr22, vr33, 0x1B


		/*S:2*/		vmsum4fp128 vr29,vr19, vr1  // vr29 = scWeight 
		/*S:3*/			stvx vr30, r0,r23
		/*S:3*/			stvx vr31, r15,r23
		/*S:2*/mr r23,r22
		/*S:2*/	lvx vr13, r0,r22    // vr13 = vfPosition	
		/*S:2*/	lvx vr14, r15,r22    // vr14 = vfNormal	
		/*S:2*/		vmaddfp vr30, vr29, vr21, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition)
		/*S:2*/		vmaddfp vr31, vr29, vr22, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)

		/*S:3*/			stvx vr30, r0,r23
		/*S:3*/			stvx vr31, r15,r23

		mftb r17
		subf r17, r25, r17
		lau r18, g_mp_morph_Vx
		lal r18, r18, g_mp_morph_Vx
		lwz r23, 0(r18)
		add r23,r23,r17
		stw r23, 0(r18)
		lwz r23, 4(r18)
		add r23,r23,r8
		stw r23, 4(r18)

		ld r14, -0x08(r1)
		ld r15, -0x10(r1)
		ld r16, -0x18(r1)
		ld r17, -0x20(r1)
		ld r18, -0x28(r1)
		ld r19, -0x30(r1)
		ld r20, -0x38(r1)
		ld r21, -0x40(r1)
		ld r22, -0x48(r1)
		ld r23, -0x50(r1)
		ld r24, -0x58(r1)
		ld r25, -0x60(r1)

		blr
	}
}




__declspec(naked) int ComputeFlexedVertexWrinkle_StreamOffset_V7(
	int nThinFlexVertexCount,		//r3
	CachedPosNorm_t *pThinFlexVerts,//r4
	int32 *pFirstThinFlexIndex,		//r5
	mstudiovertanim_wrinkle_t * pVert,		//r6
	uint32 nCurrentTag,				//r7
	uint32 numVertsToProcess,		//r8
	fltx4 w1234						//vr1
	)
{
	__asm
	{
		std	r14, -0x08(r1)
		std r15, -0x10(r1)
		std r16, -0x18(r1)
		std r17, -0x20(r1)
		std r18, -0x28(r1)
		std r19, -0x30(r1)
		std r20, -0x38(r1)
		std r21, -0x40(r1)
		std r22, -0x48(r1)
		std r23, -0x50(r1)
		std r24, -0x58(r1)
		std r25, -0x60(r1)

		// let the compiler schedule the instructions, just use several registers to avoid dependencies
		lau r14, g_sc256_255_special
		lal r14, r14, g_sc256_255_special
		lvx vr2, r0,r14

		lau r15, g_f40011
		lal r15, r15, g_f40011
		lvx vr3, r0,r15

		lau r16, g_perm_speed_side
		lal r16, r16, g_perm_speed_side
		lvx vr4, r0,r16

		lau r17, g_perm_delta_wrinkle
		lal r17, r17, g_perm_delta_wrinkle
		lvx vr5, r0,r17

		lau r18, g_perm_ndelta
		lal r18, r18, g_perm_ndelta
		lvx vr6, r0,r18

		lau r20, g_dummy2
		lal r20,r20, g_dummy2
		mr r21, r20
		mr r22, r21
		mr r23, r22

		li r10, -1
		rldicl r7,r7,0,32   // currentTag &= 0xFFFFFFFF ; just to make sure we don't mess up isCacheInvalid computation
		rldicl r10,r10,0,48 // r10 = 0x0000FFFF

		vxor vr8,vr8,vr8

		li r15, 16
		
		li r11,0x100
		li r24, MAXSTUDIOFLEXVERTS - 4

		mtctr r8
		mftb r25
		vxor vr19,vr19,vr19
		vxor vr20,vr20,vr20
		nop	// align!
		nop
		nop

label_start_V7: // 52 instructions run in 45 cycles, although compiler predicts 38 cycles
		////////////////
		// IMPORTANT: DO NOT REMOVE NOPS UNLESS YOU KNOW WHAT YOU ARE DOING AND WHY!
		// nops are essential here, removing them will make the code about 2% slower because dual-issue will be broken
		////////////////
		lhz r14, 0(r6) // int n = pVert->index;	
		addi r16, r3, 2	
		dcbt r11,r6
		cmpw r3, r24     // compare nThinFlexVertexCount to MAXSTUDIOFLEXVERTS - 2
		lvlx vr9,r0,r6	
		rldicl r14, r14, 2, 0 // r14 = n*4	
		lvrx vr10,r15,r6	
		rldicl r16, r16, 5, 0 // r16 = (nThinFlexVertexCount+2) * 32 + pThinFlexVerts	
		lvlx vr27,r15,r6  // f3PreDelta
		vor vr9,vr9,vr10  // vr9 = packedVert = LoadUnalignedSIMD(pVert)	
			addi r31,r31,0//vpermwi128 vr40,vr40,0x1B //mr r31,r31
		add r16, r16, r4	
			vpermwi128 vr40,vr40,0x1B //mr r30,r30
		addi r6, r6, 0x12 // pVert++	
			vpermwi128 vr41,vr41,0x1B//nop
		lwzx r17, r14, r5    // r17 = oldCache	
			//addi r30,r30,0//nop
		vperm vr10, vr8, vr9, vr4 //__vperm(f4Zero, packedVert, permuteSpeedSide)	
		vrlimi128 vr27,vr9,7,0// f3PreDelta
		xor r18, r17, r7     // cacheVertexIndex = oldCache^nCurrentTag	
		vperm vr12, vr8, vr9, vr6 //f3NDelta = __vperm(f4Zero, packedVert, permuteNDelta)
		stvx vr8, r0,r16	
		/*S:2*/		vmsum4fp128 vr29,vr19, vr1  // vr29 = scWeight 
		subf r18,r18,r10     // (0xFFFF-cacheVertexIndex) >> 32	
		/*S:1*/	vpermwi128 vr25, vr20, 0x22 // depends on vmadd vr20 = f4sb
		stvx vr8, r15,r16	
		/*S:1*/	vpermwi128 vr26, vr20, 0xF5		
		vcsxwfp vr10,vr10,8		
		or r19,r3,r7		
		vperm vr11, vr8, vr27, vr5 //f3Delta = __vperm(f4Zero, f3PreDelta, permuteDelta)	
		sradi r18,r18,32     // r18 = isCacheInvalid : form mask		
		/*S:3*/			stvx vr30, r0,r23
			//nop
		/*S:3*/			stvx vr31, r15,r23
			//nop
		andc r17, r17, r18   // r17 = oldCache & ~isCacheInvalid		
			//nop
		subf r3, r18, r3  // nThinFlexVertexCount = nThinFlexVertexCount + (isCacheInvalid&1);	
			//nop
		and r19,r19,r18      // r19 = newCache & isCacheInvalid		
			//nop
		/*S:2*/mr r23,r22
			//nop
		or r19, r19, r17     // r19 = updateCache		
		/*S:2*/	lvx vr13, r0,r22    // vr13 = vfPosition	
		/*S:2*/	lvx vr14, r15,r22    // vr14 = vfNormal	
			//nop
		rldicl r17, r19, 5,43 // r17 = (updateCache & 0xFFFF) * 32   = nVertexIndex * 32		
			//nop
		/*S:1*/	vmulfp128 vr19, vr25, vr26		
		/*S:1*/mr r22, r21
		vmaddfp vr20, vr10, vr2, vr3 // vr20 = f4sb	
		add r21, r17, r4      // r21 = pFlexedVertex, goes to Stage:1
		/*S:2*/	vmaddfp vr30, vr29, vr21, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition)
		stwx r19, r14, r5
		/*S:2*/	vmaddfp vr31, vr29, vr22, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)
		/*S:1*/	vpermwi128 vr21, vr32, 0x1B
		/*S:1*/	vpermwi128 vr22, vr33, 0x1B
		vcsxwfp128 vr32, vr11, 28
			//nop
		vcsxwfp128 vr33, vr12, 28
		bgt label_end_V7
		dcbt r11, r21
		bdnz label_start_V7
label_end_V7:
		
		/*S:2*/		vmsum4fp128 vr29,vr19, vr1  // vr29 = scWeight 
		/*S:1*/	vpermwi128 vr25, vr20, 0x22 // depends on vmadd vr20 = f4sb
		/*S:1*/	vpermwi128 vr26, vr20, 0xF5		
		/*S:3*/			stvx vr30, r0,r23
		/*S:3*/			stvx vr31, r15,r23
		/*S:2*/mr r23,r22
		/*S:2*/	lvx vr13, r0,r22    // vr13 = vfPosition	
		/*S:2*/	lvx vr14, r15,r22    // vr14 = vfNormal	
		/*S:1*/	vmulfp128 vr19, vr25, vr26		
		/*S:1*/mr r22, r21
		/*S:2*/		vmaddfp vr30, vr29, vr21, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition)
		/*S:2*/		vmaddfp vr31, vr29, vr22, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)
		/*S:1*/	vpermwi128 vr21, vr32, 0x1B
		/*S:1*/	vpermwi128 vr22, vr33, 0x1B


		/*S:2*/		vmsum4fp128 vr29,vr19, vr1  // vr29 = scWeight 
		/*S:3*/			stvx vr30, r0,r23
		/*S:3*/			stvx vr31, r15,r23
		/*S:2*/mr r23,r22
		/*S:2*/	lvx vr13, r0,r22    // vr13 = vfPosition	
		/*S:2*/	lvx vr14, r15,r22    // vr14 = vfNormal	
		/*S:2*/		vmaddfp vr30, vr29, vr21, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition)
		/*S:2*/		vmaddfp vr31, vr29, vr22, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)

		/*S:3*/			stvx vr30, r0,r23
		/*S:3*/			stvx vr31, r15,r23

		mftb r17
		subf r17, r25, r17
		lau r18, g_mp_morph_Vw
		lal r18, r18, g_mp_morph_Vw
		lwz r23, 0(r18)
		add r23,r23,r17
		stw r23, 0(r18)
		lwz r23, 4(r18)
		add r23,r23,r8
		stw r23, 4(r18)

		ld r14, -0x08(r1)
		ld r15, -0x10(r1)
		ld r16, -0x18(r1)
		ld r17, -0x20(r1)
		ld r18, -0x28(r1)
		ld r19, -0x30(r1)
		ld r20, -0x38(r1)
		ld r21, -0x40(r1)
		ld r22, -0x48(r1)
		ld r23, -0x50(r1)
		ld r24, -0x58(r1)
		ld r25, -0x60(r1)

		blr
	}
}




// V4 rolled - latency of x3
__declspec(naked) int ComputeFlexedVertex_StreamOffset_V6(
	int nThinFlexVertexCount,		//r3
	CachedPosNorm_t *pThinFlexVerts,//r4
	int32 *pFirstThinFlexIndex,		//r5
	mstudiovertanim_t * pVert,		//r6
	uint32 nCurrentTag,				//r7
	uint32 numVertsToProcess,		//r8
	fltx4 w1234						//vr1
	)
{
	__asm
	{
			std	r14, -0x08(r1)
			std r15, -0x10(r1)
			std r16, -0x18(r1)
			std r17, -0x20(r1)
			std r18, -0x28(r1)
			std r19, -0x30(r1)
			std r20, -0x38(r1)
			std r21, -0x40(r1)
			std r22, -0x48(r1)
			std r23, -0x50(r1)
			std r24, -0x58(r1)

			// let the compiler schedule the instructions, just use several registers to avoid dependencies
			lau r14, g_sc256_255_special
			lal r14, r14, g_sc256_255_special
			lvx vr2, r0,r14

			lau r15, g_f40011
			lal r15, r15, g_f40011
			lvx vr3, r0,r15

			lau r16, g_perm_speed_side
			lal r16, r16, g_perm_speed_side
			lvx vr4, r0,r16

			lau r17, g_perm_delta
			lal r17, r17, g_perm_delta
			lvx vr5, r0,r17

			lau r18, g_perm_ndelta
			lal r18, r18, g_perm_ndelta
			lvx vr6, r0,r18

			lau r20, g_dummy2
			lal r20,r20, g_dummy2
			mr r21, r20
			mr r22, r21

			li r10, -1
			rldicl r7,r7,0,32   // currentTag &= 0xFFFFFFFF ; just to make sure we don't mess up isCacheInvalid computation
			rldicl r10,r10,0,48 // r10 = 0x0000FFFF

			vxor vr8,vr8,vr8

			li r15, 16
			
			lau r14,g_nStreamOffset_prefetch
			lal r14,r14,g_nStreamOffset_prefetch
			lwz r11,0(r14)

			li r24, MAXSTUDIOFLEXVERTS - 2

			mtctr r8
			mftb r23

label_start:
			lhz r14, 0(r6) // int n = pVert->index;	
			dcbt r11,r6
			addi r16, r3, 2	
			cmpw r3, r24     // compare nThinFlexVertexCount to MAXSTUDIOFLEXVERTS - 2
			lvlx vr9,r0,r6	
			lvrx vr10,r15,r6	
			rldicl r14, r14, 2, 0 // r14 = n*4	
			rldicl r16, r16, 5, 0 // r16 = (nThinFlexVertexCount+2) * 32 + pThinFlexVerts	
			add r16, r16, r4	
			vor vr9,vr9,vr10  // vr9 = packedVert = LoadUnalignedSIMD(pVert)	
			stvx vr8, r0,r16	
			lwzx r17, r14, r5    // r17 = oldCache	
			stvx vr8, r15,r16	
				vmsum4fp128 vr19,vr19, vr1   // vr15 = scWeight 
			vperm vr10, vr8, vr9, vr4	
			xor r18, r17, r7     // cacheVertexIndex = oldCache^nCurrentTag	
			vperm vr11, vr8, vr9, vr5	
			subf r18,r18,r10     // (0xFFFF-cacheVertexIndex) >> 32	
			vcsxwfp vr10,vr10,8		
			vperm vr12, vr8, vr9, vr6		
					stvx vr23, r0,r22
			sradi r18,r18,32     // r18 = isCacheInvalid : form mask		
			vmaddfp vr10, vr10, vr2, vr3 // vr10 = f4sb		
					stvx vr24, r15,r22
			or r19,r3,r7		
			andc r17, r17, r18   // r17 = oldCache & ~isCacheInvalid		
			and r19,r19,r18      // r19 = newCache & isCacheInvalid		
			vpermwi128 vr15, vr10, 0x22		
			or r19, r19, r17     // r19 = updateCache		
			vpermwi128 vr16, vr10, 0xF5		
			rldicl r17, r19, 5,43 // r17 = (updateCache & 0xFFFF) * 32   = nVertexIndex * 32		
				vmaddfp vr24, vr19, vr22, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)
				vmaddfp vr23, vr19, vr21, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition)
			vmulfp128 vr19, vr15, vr16		
			add r17, r17, r4      // r17 = pFlexedVertex		
			stwx r19, r14, r5		
			subf r3, r18, r3// nThinFlexVertexCount = nThinFlexVertexCount + (isCacheInvalid&1);	
			lvx vr13, r0,r17       // vr13 = vfPosition	
			addi r6, r6, 0x10 // pVert++	
			lvx vr14, r15,r17     // vr14 = vfNormal	
			vcsxwfp vr21, vr11, 28	
			mr r22,r21
			vcsxwfp vr22, vr12, 28	
			mr r21,r17
			bgt label_end
			dcbt r11, r17

			bdnz label_start
label_end:
			
			mftb r17
			subf r17, r23, r17
			lau r18, g_mp_morph_Vx
			lal r18, r18, g_mp_morph_Vx
			lwz r23, 0(r18)
			add r23,r23,r17
			stw r23, 0(r18)
			lwz r23, 4(r18)
			add r23,r23,r8
			stw r23, 4(r18)


			vmsum4fp128 vr19,vr19, vr1   // vr15 = scWeight 
			stvx vr23, r0,r22
			stvx vr24, r15,r22
			vmaddfp vr24, vr19, vr22, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)
			vmaddfp vr23, vr19, vr21, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition)
			stvx vr23, r0,r21
			stvx vr24, r15,r21

			ld r14, -0x08(r1)
			ld r15, -0x10(r1)
			ld r16, -0x18(r1)
			ld r17, -0x20(r1)
			ld r18, -0x28(r1)
			ld r19, -0x30(r1)
			ld r20, -0x38(r1)
			ld r21, -0x40(r1)
			ld r22, -0x48(r1)
			ld r23, -0x50(r1)
			ld r24, -0x58(r1)

			blr
	}
}



// 2-stages
__declspec(naked) int ComputeFlexedVertex_StreamOffset_V5(
	int nThinFlexVertexCount,		//r3
	CachedPosNorm_t *pThinFlexVerts,//r4
	int32 *pFirstThinFlexIndex,		//r5
	mstudiovertanim_t * pVert,		//r6
	uint32 nCurrentTag,				//r7
	uint32 numVertsToProcess,		//r8
	fltx4 w1234						//vr1
	)
{
	__asm
	{
		std	r14, -0x08(r1)
			std r15, -0x10(r1)
			std r16, -0x18(r1)
			std r17, -0x20(r1)
			std r18, -0x28(r1)
			std r19, -0x30(r1)
			std r20, -0x38(r1)

			// let the compiler schedule the instructions, just use several registers to avoid dependencies
			lau r14, g_sc256_255_special
			lal r14, r14, g_sc256_255_special
			lvx vr2, r0,r14

			lau r15, g_f40011
			lal r15, r15, g_f40011
			lvx vr3, r0,r15

			lau r16, g_perm_speed_side
			lal r16, r16, g_perm_speed_side
			lvx vr4, r0,r16

			lau r17, g_perm_delta
			lal r17, r17, g_perm_delta
			lvx vr5, r0,r17

			lau r18, g_perm_ndelta
			lal r18, r18, g_perm_ndelta
			lvx vr6, r0,r18

			lau r20, g_dummy2
			lal r20,r20, g_dummy2

			vxor vr8,vr8,vr8
			li r10, -1
			rldicl r7,r7,0,32   // currentTag &= 0xFFFFFFFF ; just to make sure we don't mess up isCacheInvalid computation
			rldicl r10,r10,0,48 // r10 = 0x0000FFFF
			mtctr r8

			li r15, 16

label_start_schlp:
		lhz r14, 0(r6) // int n = pVert->index;
			addi r16, r3, 2       // r16 = (nThinFlexVertexCount+2) * 32 + pThinFlexVerts
			lvlx vr9,r0,r6
			rldicl r14, r14, 2, 0 // r14 = n*4
			lvrx vr10,r15,r6
			rldicl r16, r16, 5, 0 // r16 = (nThinFlexVertexCount+2) * 32 + pThinFlexVerts

			vor vr9,vr9,vr10  // vr9 = packedVert = LoadUnalignedSIMD(pVert)

			add r16, r16, r4

			vperm vr10, vr8, vr9, vr4	//__vperm(f4Zero, packedVert, permuteSpeedSide)
			addi r6, r6, 0x10 // pVert++
			vcsxwfp vr10,vr10,8

			vmaddfp vr17, vr15, vr11, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition) - stage 1
			vmaddfp vr18, vr15, vr12, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)  - stage 1

			vperm vr11, vr8, vr9, vr5	//f3Delta = __vperm(f4Zero, packedVert, permuteDelta)
			vcsxwfp vr11, vr11, 28
			vperm vr12, vr8, vr9, vr6	//f3NDelta = __vperm(f4Zero, packedVert, permuteNDelta)
			vcsxwfp vr12, vr12, 28

			vmaddfp vr10, vr10, vr2, vr3 // vr10 = f4sb

			lwzx r17, r14, r5      // r17 = oldCache
			xor r18, r17, r7      // cacheVertexIndex = oldCache^nCurrentTag
			subf r18,r18,r10     // (0xFFFF-cacheVertexIndex) >> 32

			or r19,r3,r7		 // newCache = nCurrentTag | nThinFlexVertexCount
			sradi r18,r18,32     // r18 = isCacheInvalid : form mask
			vpermwi128 vr15, vr10, 0x22
			and r19,r19,r18      // r19 = newCache & isCacheInvalid
			vpermwi128 vr16, vr10, 0xF5
			andc r17, r17, r18   // r17 = oldCache & ~isCacheInvalid
			stvx vr8, r0, r16
			or r19, r19, r17     // r19 = updateCache
			stvx vr8, r15, r16

			rldicl r17, r19, 5,43 // r17 = (updateCache & 0xFFFF) * 32   = nVertexIndex * 32
			add r17, r17, r4      // r17 = pFlexedVertex
			vmulfp128 vr15, vr15, vr16
			lvx vr13, r0,r17       // vr13 = vfPosition
			lvx vr14, r15,r17     // vr14 = vfNormal

			vmsum4fp128 vr15,vr15, vr1   // vr15 = scWeight 

			stwx r19, r14, r5	  // pFirstThinFlexIndex[n] = updateCache
			subf r3, r18, r3// nThinFlexVertexCount = nThinFlexVertexCount + (isCacheInvalid&1);

			stvx vr17, r0,r20     // stage 1
			stvx vr18, r15,r20    // stage 1

			mr r20, r17

			bdnz label_start_schlp

			vmaddfp vr17, vr15, vr11, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition) - stage 1
			vmaddfp vr18, vr15, vr12, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)  - stage 1
			stvx vr17, r0,r20	 // stage 1; deferred storing saves 15 cycles (10%!)
			stvx vr18, r15,r20

			ld r14, -0x08(r1) 
			ld r15, -0x10(r1)
			ld r16, -0x18(r1)
			ld r17, -0x20(r1)
			ld r18, -0x28(r1)
			ld r19, -0x30(r1)
			ld r20, -0x38(r1)

			blr
	}
}

// V3 in asm
__declspec(naked) int ComputeFlexedVertex_StreamOffset_V4(
	int nThinFlexVertexCount,		//r3
	CachedPosNorm_t *pThinFlexVerts,//r4
	int32 *pFirstThinFlexIndex,		//r5
	mstudiovertanim_t * pVert,		//r6
	uint32 nCurrentTag,				//r7
	uint32 numVertsToProcess,		//r8
	fltx4 w1234						//vr1
	)
{
	__asm
	{
		std	r14, -0x08(r1)
			std r15, -0x10(r1)
			std r16, -0x18(r1)
			std r17, -0x20(r1)
			std r18, -0x28(r1)
			std r19, -0x30(r1)

			// let the compiler schedule the instructions, just use several registers to avoid dependencies
			lau r14, g_sc256_255_special
			lal r14, r14, g_sc256_255_special
			lvx vr2, r0,r14

			lau r15, g_f40011
			lal r15, r15, g_f40011
			lvx vr3, r0,r15

			lau r16, g_perm_speed_side
			lal r16, r16, g_perm_speed_side
			lvx vr4, r0,r16

			lau r17, g_perm_delta
			lal r17, r17, g_perm_delta
			lvx vr5, r0,r17

			lau r18, g_perm_ndelta
			lal r18, r18, g_perm_ndelta
			lvx vr6, r0,r18

			li r10, -1
			rldicl r7,r7,0,32   // currentTag &= 0xFFFFFFFF ; just to make sure we don't mess up isCacheInvalid computation
			rldicl r10,r10,0,48 // r10 = 0x0000FFFF

			lau r14,g_nStreamOffset_prefetch
			lal r14,r14,g_nStreamOffset_prefetch
			lwz r11,0(r14)

			vxor vr8,vr8,vr8

			li r15, 16
			li r24, MAXSTUDIOFLEXVERTS - 3 // critical number at which to stop processing 

			mtctr r8
label_start:
		lhz r14, 0(r6) // int n = pVert->index;
			dcbt r11,r16
			rldicl r14, r14, 2, 0 // r14 = n*4
			

			addi r16, r3, 2
			rldicl r16, r16, 5, 0 // r16 = (nThinFlexVertexCount+2) * 32 + pThinFlexVerts
			add r16, r16, r4
			stvx vr8, r0,r16
			stvx vr8, r15,r16

			lvlx vr9,r0,r6
			lvrx vr10,r15,r6
			vor vr9,vr9,vr10  // vr9 = packedVert = LoadUnalignedSIMD(pVert)

			vperm vr10, vr8, vr9, vr4    //__vperm(f4Zero, packedVert, permuteSpeedSide)
			vcsxwfp vr10,vr10,8
			vmaddfp vr10, vr10, vr2, vr3 // vr10 = f4sb

			vperm vr11, vr8, vr9, vr5 //f3Delta = __vperm(f4Zero, packedVert, permuteDelta)
			vcsxwfp vr11, vr11, 28
			vperm vr12, vr8, vr9, vr6 //f3NDelta = __vperm(f4Zero, packedVert, permuteNDelta)
			vcsxwfp vr12, vr12, 28

			lwzx r17, r14, r5    // r17 = oldCache
			xor r18, r17, r7     // cacheVertexIndex = oldCache^nCurrentTag
			subf r18,r18,r10     // (0xFFFF-cacheVertexIndex) >> 32
			sradi r18,r18,32     // r18 = isCacheInvalid : form mask

			or r19,r3,r7          // newCache = nCurrentTag | nThinFlexVertexCount
			and r19,r19,r18      // r19 = newCache & isCacheInvalid
			andc r17, r17, r18   // r17 = oldCache & ~isCacheInvalid
			or r19, r19, r17     // r19 = updateCache

			rldicl r17, r19, 5,43 // r17 = (updateCache & 0xFFFF) * 32   = nVertexIndex * 32
			add r17, r17, r4      // r17 = pFlexedVertex
			lvx vr13, r0,r17      // vr13 = vfPosition
			lvx vr14, r15,r17     // vr14 = vfNormal
			dcbt r11,r17

			vpermwi128 vr15, vr10, 0x22
			vpermwi128 vr16, vr10, 0xF5
			vmulfp128 vr15, vr15, vr16
			vmsum4fp128 vr15,vr15, vr1   // vr15 = scWeight 

			stwx r19, r14, r5     // pFirstThinFlexIndex[n] = updateCache
			subf r3, r18, r3      // nThinFlexVertexCount = nThinFlexVertexCount + (isCacheInvalid&1);

			vmaddfp vr14, vr15, vr12, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)
			vmaddfp vr13, vr15, vr11, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition)

			stvx vr13, r0,r17
			stvx vr14, r15,r17

			cmpw r3, r24
			bgt label_end

			addi r6, r6, 0x10     // pVert++
			bdnz label_start
label_end:

			ld r14, -0x08(r1)
			ld r15, -0x10(r1)
			ld r16, -0x18(r1)
			ld r17, -0x20(r1)
			ld r18, -0x28(r1)
			ld r19, -0x30(r1)

			blr
	}
}



// V3 in asm
__declspec(naked) int ComputeFlexedVertexWrinkle_StreamOffset_V4(
	int nThinFlexVertexCount,		//r3
	CachedPosNorm_t *pThinFlexVerts,//r4
	int32 *pFirstThinFlexIndex,		//r5
	mstudiovertanim_wrinkle_t * pVert,//r6
	uint32 nCurrentTag,				//r7
	uint32 numVertsToProcess,		//r8
	fltx4 w1234						//vr1
	)
{
	__asm
	{
		std	r14, -0x08(r1)
		std r15, -0x10(r1)
		std r16, -0x18(r1)
		std r17, -0x20(r1)
		std r18, -0x28(r1)
		std r19, -0x30(r1)

		// let the compiler schedule the instructions, just use several registers to avoid dependencies
		lau r14, g_sc256_255_special
		lal r14, r14, g_sc256_255_special
		lvx vr2, r0,r14

		lau r15, g_f40011
		lal r15, r15, g_f40011
		lvx vr3, r0,r15

		lau r16, g_perm_speed_side
		lal r16, r16, g_perm_speed_side
		lvx vr4, r0,r16

		lau r17, g_perm_delta_wrinkle
		lal r17, r17, g_perm_delta_wrinkle
		lvx vr5, r0,r17

		lau r18, g_perm_ndelta
		lal r18, r18, g_perm_ndelta
		lvx vr6, r0,r18

		li r10, -1
		rldicl r7,r7,0,32   // currentTag &= 0xFFFFFFFF ; just to make sure we don't mess up isCacheInvalid computation
		rldicl r10,r10,0,48 // r10 = 0x0000FFFF

		lau r14,g_nStreamOffset_prefetch
		lal r14,r14,g_nStreamOffset_prefetch
		lwz r11,0(r14)

		vxor vr8,vr8,vr8

		li r15, 16
		li r24, MAXSTUDIOFLEXVERTS - 3 // critical number at which to stop processing 

		mtctr r8
	label_start:
		lhz r14, 0(r6) // int n = pVert->index;
		dcbt r11,r16
		rldicl r14, r14, 2, 0 // r14 = n*4


		addi r16, r3, 2
		rldicl r16, r16, 5, 0 // r16 = (nThinFlexVertexCount+2) * 32 + pThinFlexVerts
		add r16, r16, r4
		stvx vr8, r0,r16
		stvx vr8, r15,r16

		lvlx vr27,r15,r6  // f3PreDelta
		lvlx vr9,r0,r6
		lvrx vr10,r15,r6
		vor vr9,vr9,vr10  // vr9 = packedVert = LoadUnalignedSIMD(pVert)
		vrlimi128 vr27,vr9,7,0// f3PreDelta

		vperm vr10, vr8, vr9, vr4    //__vperm(f4Zero, packedVert, permuteSpeedSide)
		vcsxwfp vr10,vr10,8
		vmaddfp vr10, vr10, vr2, vr3 // vr10 = f4sb

		vperm vr11, vr8, vr27, vr5 //f3Delta = __vperm(f4Zero, f3PreDelta, permuteDelta)
		vcsxwfp vr11, vr11, 28
		vperm vr12, vr8, vr9, vr6 //f3NDelta = __vperm(f4Zero, packedVert, permuteNDelta)
		vcsxwfp vr12, vr12, 28

		lwzx r17, r14, r5    // r17 = oldCache
		xor r18, r17, r7     // cacheVertexIndex = oldCache^nCurrentTag
		subf r18,r18,r10     // (0xFFFF-cacheVertexIndex) >> 32
		sradi r18,r18,32     // r18 = isCacheInvalid : form mask

		or r19,r3,r7          // newCache = nCurrentTag | nThinFlexVertexCount
		and r19,r19,r18      // r19 = newCache & isCacheInvalid
		andc r17, r17, r18   // r17 = oldCache & ~isCacheInvalid
		or r19, r19, r17     // r19 = updateCache

		rldicl r17, r19, 5,43 // r17 = (updateCache & 0xFFFF) * 32   = nVertexIndex * 32
		add r17, r17, r4      // r17 = pFlexedVertex
		lvx vr13, r0,r17      // vr13 = vfPosition
		lvx vr14, r15,r17     // vr14 = vfNormal
		dcbt r11,r17

		vpermwi128 vr15, vr10, 0x22
		vpermwi128 vr16, vr10, 0xF5
		vmulfp128 vr15, vr15, vr16
		vmsum4fp128 vr15,vr15, vr1   // vr15 = scWeight 

		stwx r19, r14, r5     // pFirstThinFlexIndex[n] = updateCache
		subf r3, r18, r3      // nThinFlexVertexCount = nThinFlexVertexCount + (isCacheInvalid&1);

		vmaddfp vr14, vr15, vr12, vr14 // MaddSIMD(scWeight,f3NDelta, vfNormal)
		vmaddfp vr13, vr15, vr11, vr13 // MaddSIMD(scWeight,f3Delta, vfPosition)

		stvx vr13, r0,r17
		stvx vr14, r15,r17

		cmpw r3, r24
		bgt label_end

		addi r6, r6, 0x12     // pVert++
		bdnz label_start
	label_end:

		ld r14, -0x08(r1)
		ld r15, -0x10(r1)
		ld r16, -0x18(r1)
		ld r17, -0x20(r1)
		ld r18, -0x28(r1)
		ld r19, -0x30(r1)

		blr
	}
}



// base for asm
int ComputeFlexedVertex_StreamOffset_V3(int nThinFlexVertexCount, CachedPosNorm_t *pThinFlexVerts, int32 *pFirstThinFlexIndex, mstudiovertanim_t * pVert, uint32 nCurrentTag, uint32 numVertsToProcess, fltx4 w1234)
{
	fltx4 sc256_255_special = g_sc256_255_special;
	fltx4 f40011 = g_f40011;
	fltx4 permuteSpeedSide = LoadAlignedSIMD((const float*)g_perm_speed_side);
	fltx4 permuteDelta  = LoadAlignedSIMD((const float*)g_perm_delta);
	fltx4 permuteNDelta = LoadAlignedSIMD((const float*)g_perm_ndelta);
	//fltx4 permuteW0     = LoadAlignedSIMD((const float*)g_perm_w0);
	//fltx4 permuteW1     = LoadAlignedSIMD((const float*)g_perm_w1);
	fltx4 f4Zero = Four_Zeros;

	do
	{
		int n = pVert->index;
		pThinFlexVerts[nThinFlexVertexCount+2].m_Position.InitZero();
		pThinFlexVerts[nThinFlexVertexCount+2].m_Normal.InitZero();
		fltx4 packedVert = LoadUnalignedSIMD((const float*)pVert);
		fltx4 f4sb = MaddSIMD(__vcfsx(__vperm(f4Zero, packedVert, permuteSpeedSide), 8), sc256_255_special, f40011);
		// f4sb = {s,b,1-s,1-b}

		fltx4 f3Delta = __vcfsx(__vperm(f4Zero, packedVert, permuteDelta), 12+16);
		fltx4 f3NDelta = __vcfsx(__vperm(f4Zero, packedVert, permuteNDelta), 12+16);
		uint64 oldCache = uint32(pFirstThinFlexIndex[n]);
		uint64 cacheVertexIndex = oldCache^nCurrentTag; // if there is trash in high (2^16) bits, we need to update the cache
		int64 isCacheInvalid = int64(0xFFFF-cacheVertexIndex)>>32; // the second shift must be arithmetic to form a valid mask
		int64 isCacheValid = ~isCacheInvalid;

		int64 newCache = nCurrentTag | nThinFlexVertexCount;
		int64 updateCache = (newCache & isCacheInvalid) | (oldCache & isCacheValid);
		nThinFlexVertexCount = nThinFlexVertexCount - isCacheInvalid;

		int nVertexIndex = updateCache & 0xFFFF;

		CachedPosNorm_t *pFlexedVertex = pThinFlexVerts + nVertexIndex; // will be overridden
		fltx4 vfNormal = LoadAlignedSIMD((float*)&pFlexedVertex->m_Normal);
		fltx4 vfPosition = LoadAlignedSIMD((float*)&pFlexedVertex->m_Position);

		// here we need to form the following vector to compute final w:
		// {s(1-b), (1-s)(1-b), sb, (1-s)b}
		//fltx4 f4sbProd = MulSIMD(__vperm(f4sb,f4sb,permuteW0), __vperm(f4sb,f4sb,permuteW1));
		fltx4 f4sbProd = MulSIMD(__vpermwi(f4sb,0x22), __vpermwi(f4sb,0xF5));
		fltx4 scWeight = __vmsum4fp(f4sbProd,w1234);

		pFirstThinFlexIndex[n] = updateCache;
		StoreAlignedSIMD((float*)&pFlexedVertex->m_Normal, MaddSIMD(scWeight,f3NDelta, vfNormal));
		StoreAlignedSIMD((float*)&pFlexedVertex->m_Position, MaddSIMD(scWeight,f3Delta, vfPosition));

		pVert ++;
	}
	while(--numVertsToProcess); // why doesn't this use bdnz??

	return nThinFlexVertexCount;
}


// base for asm
int ComputeFlexedVertexWrinkle_StreamOffset_V3(int nThinFlexVertexCount, CachedPosNorm_t *pThinFlexVerts, int32 *pFirstThinFlexIndex, mstudiovertanim_wrinkle_t * pVert, uint32 nCurrentTag, uint32 numVertsToProcess, fltx4 w1234)
{
	fltx4 sc256_255_special = g_sc256_255_special;
	fltx4 f40011 = g_f40011;
	fltx4 permuteSpeedSide = LoadAlignedSIMD((const float*)g_perm_speed_side);
	fltx4 permuteDelta  = LoadAlignedSIMD((const float*)g_perm_delta_wrinkle);
	fltx4 permuteNDelta = LoadAlignedSIMD((const float*)g_perm_ndelta);
	//fltx4 permuteW0     = LoadAlignedSIMD((const float*)g_perm_w0);
	//fltx4 permuteW1     = LoadAlignedSIMD((const float*)g_perm_w1);
	fltx4 f4Zero = Four_Zeros;

	do
	{
		int n = pVert->index;
		pThinFlexVerts[nThinFlexVertexCount+2].m_Position.InitZero();
		pThinFlexVerts[nThinFlexVertexCount+2].m_Normal.InitZero();
		fltx4 packedVert = LoadUnalignedSIMD((const float*)pVert);
		fltx4 f3PreDelta = __lvlx(pVert, 16); // f3Delta now contains only packed W component in high X halfword...
		fltx4 f4sb = MaddSIMD(__vcfsx(__vperm(f4Zero, packedVert, permuteSpeedSide), 8), sc256_255_special, f40011);
		// f4sb = {s,b,1-s,1-b}


		f3PreDelta = __vrlimi(f3PreDelta, packedVert, 7, 0); // don't rotate and move bytes 4..15 from packed vert to f3PreDelta
		fltx4 f3NDelta = __vcfsx(__vperm(f4Zero, packedVert, permuteNDelta), 12+16);
		fltx4 f3Delta = __vcfsx(__vperm(f4Zero, f3PreDelta, permuteDelta), 12+16);
		uint64 oldCache = uint32(pFirstThinFlexIndex[n]);
		uint64 cacheVertexIndex = oldCache^nCurrentTag; // if there is trash in high (2^16) bits, we need to update the cache
		int64 isCacheInvalid = int64(0xFFFF-cacheVertexIndex)>>32; // the second shift must be arithmetic to form a valid mask
		int64 isCacheValid = ~isCacheInvalid;

		int64 newCache = nCurrentTag | nThinFlexVertexCount;
		int64 updateCache = (newCache & isCacheInvalid) | (oldCache & isCacheValid);
		nThinFlexVertexCount = nThinFlexVertexCount - isCacheInvalid;

		int nVertexIndex = updateCache & 0xFFFF;

		CachedPosNorm_t *pFlexedVertex = pThinFlexVerts + nVertexIndex; // will be overridden
		fltx4 vfNormal = LoadAlignedSIMD((float*)&pFlexedVertex->m_Normal);
		fltx4 vfPosition = LoadAlignedSIMD((float*)&pFlexedVertex->m_Position);

		// here we need to form the following vector to compute final w:
		// {s(1-b), (1-s)(1-b), sb, (1-s)b}
		//fltx4 f4sbProd = MulSIMD(__vperm(f4sb,f4sb,permuteW0), __vperm(f4sb,f4sb,permuteW1));
		fltx4 f4sbProd = MulSIMD(__vpermwi(f4sb,0x22), __vpermwi(f4sb,0xF5));
		fltx4 scWeight = __vmsum4fp(f4sbProd,w1234);

		pFirstThinFlexIndex[n] = updateCache;
		StoreAlignedSIMD((float*)&pFlexedVertex->m_Normal, MaddSIMD(scWeight,f3NDelta, vfNormal));
		StoreAlignedSIMD((float*)&pFlexedVertex->m_Position, MaddSIMD(scWeight,f3Delta, vfPosition));

		pVert ++;
	}
	while(--numVertsToProcess); // why doesn't this use bdnz??

	return nThinFlexVertexCount;
}

// tried to pipeline in C++
int ComputeFlexedVertex_StreamOffset_V2(int nThinFlexVertexCount, CachedPosNorm_t *pThinFlexVerts, int32 *pFirstThinFlexIndex, mstudiovertanim_t * pVert, uint32 nCurrentTag, uint32 numVertsToProcess, fltx4 w1234)
{
	Assert(0 == (uint32(pVert) & 0xF));
	fltx4 sc256_255_special = g_sc256_255_special;
	fltx4 f40011 = g_f40011;
	fltx4 permuteSpeedSide = LoadAlignedSIMD((const float*)g_perm_speed_side);
	fltx4 permuteDelta  = LoadAlignedSIMD((const float*)g_perm_delta);
	fltx4 permuteNDelta = LoadAlignedSIMD((const float*)g_perm_ndelta);
	//fltx4 permuteW0     = LoadAlignedSIMD((const float*)g_perm_w0);
	//fltx4 permuteW1     = LoadAlignedSIMD((const float*)g_perm_w1);
	fltx4 f4Zero = Four_Zeros;

	fltx4 f4sb_st1, f3Delta_st1, f3NDelta_st1;
	int32 updateCache_st1;
	mstudiovertanim_t *pVertEnd = pVert + numVertsToProcess;
	{
		// stage 0
		int n = pVert->index;
		pThinFlexVerts[nThinFlexVertexCount+2].m_Position.InitZero();
		pThinFlexVerts[nThinFlexVertexCount+2].m_Normal.InitZero();
		fltx4 packedVert = LoadUnalignedSIMD((const float*)pVert);
		fltx4 f4sb = MaddSIMD(__vcfsx(__vperm(f4Zero, packedVert, permuteSpeedSide), 8), sc256_255_special, f40011); // to be completely correct, we'll ned to multiply this with 256/255
		// f4sb = {s,b,1-s,1-b}

		fltx4 f3Delta = __vcfsx(__vperm(f4Zero, packedVert, permuteDelta), 12+16);
		fltx4 f3NDelta = __vcfsx(__vperm(f4Zero, packedVert, permuteNDelta), 12+16);
		uint64 oldCache = uint32(pFirstThinFlexIndex[n]);
		uint64 cacheVertexIndex = oldCache^nCurrentTag; // if there is trash in high (2^16) bits, we need to update the cache
		int64 isCacheInvalid = int64(0xFFFF-cacheVertexIndex)>>32; // the second shift must be arithmetic to form a valid mask
		int64 isCacheValid = ~isCacheInvalid;

		int64 newCache = nCurrentTag | nThinFlexVertexCount;
		int64 updateCache = (newCache & isCacheInvalid) | (oldCache & isCacheValid);
		nThinFlexVertexCount = nThinFlexVertexCount - isCacheInvalid;

		pFirstThinFlexIndex[n] = updateCache;

		// prime next stage 1
		f4sb_st1 = f4sb;
		f3Delta_st1 = f3Delta;
		f3NDelta_st1 = f3NDelta;
		updateCache_st1 = updateCache;

		pVert ++;
	}

	while(pVert < pVertEnd)
	{
		// stage 1
		{
			int nVertexIndex = updateCache_st1 & 0xFFFF;

			CachedPosNorm_t *pFlexedVertex = pThinFlexVerts + nVertexIndex; // will be overridden

			fltx4 vfNormal = LoadAlignedSIMD((float*)&pFlexedVertex->m_Normal);
			fltx4 vfPosition = LoadAlignedSIMD((float*)&pFlexedVertex->m_Position);

			// here we need to form the following vector to compute final w:
			// {s(1-b), (1-s)(1-b), sb, (1-s)b}
			//fltx4 f4sbProd = MulSIMD(__vperm(f4sb_st1,f4sb_st1,permuteW0), __vperm(f4sb_st1,f4sb_st1,permuteW1));
			fltx4 f4sbProd = MulSIMD(__vpermwi(f4sb_st1,0x22), __vpermwi(f4sb_st1,0xF5));
			fltx4 scWeight = __vmsum4fp(f4sbProd,w1234);

			StoreAlignedSIMD((float*)&pFlexedVertex->m_Normal, MaddSIMD(scWeight,f3NDelta_st1, vfNormal));
			StoreAlignedSIMD((float*)&pFlexedVertex->m_Position, MaddSIMD(scWeight,f3Delta_st1, vfPosition));
		}

		// stage 0
		{
			int n = pVert->index;
			pThinFlexVerts[nThinFlexVertexCount+2].m_Position.InitZero();
			pThinFlexVerts[nThinFlexVertexCount+2].m_Normal.InitZero();
			fltx4 packedVert = LoadUnalignedSIMD((const float*)pVert);
			fltx4 f4sb = MaddSIMD(__vcfsx(__vperm(f4Zero, packedVert, permuteSpeedSide), 8), sc256_255_special, f40011); // to be completely correct, we'll ned to multiply this with 256/255
			// f4sb = {s,b,1-s,1-b}

			fltx4 f3Delta = __vcfsx(__vperm(f4Zero, packedVert, permuteDelta), 12+16);
			fltx4 f3NDelta = __vcfsx(__vperm(f4Zero, packedVert, permuteNDelta), 12+16);
			uint64 oldCache = uint32(pFirstThinFlexIndex[n]);
			uint64 cacheVertexIndex = oldCache^nCurrentTag; // if there is trash in high (2^16) bits, we need to update the cache
			int64 isCacheInvalid = int64(0xFFFF-cacheVertexIndex)>>32; // the second shift must be arithmetic to form a valid mask
			int64 isCacheValid = ~isCacheInvalid;

			int64 newCache = nCurrentTag | nThinFlexVertexCount;
			int64 updateCache = (newCache & isCacheInvalid) | (oldCache & isCacheValid);
			nThinFlexVertexCount = nThinFlexVertexCount - isCacheInvalid;

			pFirstThinFlexIndex[n] = updateCache; // this may be put wherever it doesn't mess up the other stores

			// prime next stage 1
			f4sb_st1 = f4sb;
			updateCache_st1 = updateCache;
			f3Delta_st1 = f3Delta;
			f3NDelta_st1 = f3NDelta;
		}

		pVert ++;
	}

	// stage 1
	{
		int nVertexIndex = updateCache_st1 & 0xFFFF;

		CachedPosNorm_t *pFlexedVertex = pThinFlexVerts + nVertexIndex; // will be overridden

		fltx4 vfNormal = LoadAlignedSIMD((float*)&pFlexedVertex->m_Normal);
		fltx4 vfPosition = LoadAlignedSIMD((float*)&pFlexedVertex->m_Position);

		// here we need to form the following vector to compute final w:
		// {s(1-b), (1-s)(1-b), sb, (1-s)b}
		//fltx4 f4sbProd = MulSIMD(__vperm(f4sb_st1,f4sb_st1,permuteW0), __vperm(f4sb_st1,f4sb_st1,permuteW1));
		fltx4 f4sbProd = MulSIMD(__vpermwi(f4sb_st1,0x22), __vpermwi(f4sb_st1,0xF5));
		fltx4 scWeight = __vmsum4fp(f4sbProd,w1234);

		StoreAlignedSIMD((float*)&pFlexedVertex->m_Normal, MaddSIMD(scWeight,f3NDelta_st1, vfNormal));
		StoreAlignedSIMD((float*)&pFlexedVertex->m_Position, MaddSIMD(scWeight,f3Delta_st1, vfPosition));
	}
	return nThinFlexVertexCount;
}

// branchless
int ComputeFlexedVertex_StreamOffset_V1(int nThinFlexVertexCount, CachedPosNorm_t *pThinFlexVerts, int32 *pFirstThinFlexIndex, mstudiovertanim_t * pVert, uint32 nCurrentTag, uint32 numVertsToProcess, fltx4 w1234)
{
	Assert(0 == (uint32(pVert) & 0xF));
	fltx4 sc256_255_special = g_sc256_255_special;
	fltx4 f40011 = g_f40011;
	fltx4 permuteSpeedSide = LoadAlignedSIMD((const float*)g_perm_speed_side);
	fltx4 permuteDelta  = LoadAlignedSIMD((const float*)g_perm_delta);
	fltx4 permuteNDelta = LoadAlignedSIMD((const float*)g_perm_ndelta);
	//fltx4 permuteW0     = LoadAlignedSIMD((const float*)g_perm_w0);
	//fltx4 permuteW1     = LoadAlignedSIMD((const float*)g_perm_w1);
	fltx4 f4Zero = Four_Zeros;
	mstudiovertanim_t *pVertEnd = pVert + numVertsToProcess;
	do
	{
		int n = pVert->index;
		pThinFlexVerts[nThinFlexVertexCount].m_Position.InitZero();
		pThinFlexVerts[nThinFlexVertexCount].m_Normal.InitZero();
		fltx4 packedVert = LoadUnalignedSIMD((const float*)pVert);
		fltx4 f4sb = MaddSIMD(__vcfsx(__vperm(f4Zero, packedVert, permuteSpeedSide), 8), sc256_255_special, f40011);
		// f4sb = {s,b,1-s,1-b}

		fltx4 f3Delta = __vcfsx(__vperm(f4Zero, packedVert, permuteDelta), 12+16);
		fltx4 f3NDelta = __vcfsx(__vperm(f4Zero, packedVert, permuteNDelta), 12+16);
		uint64 oldCache = uint32(pFirstThinFlexIndex[n]);
		uint64 cacheVertexIndex = oldCache^nCurrentTag; // if there is trash in high (2^16) bits, we need to update the cache
		int64 isCacheInvalid = int64(0xFFFF-cacheVertexIndex)>>32; // the second shift must be arithmetic to form a valid mask
		int32 isCacheValid = ~isCacheInvalid;

		int32 newCache = nCurrentTag | nThinFlexVertexCount;
		int32 updateCache = (newCache & isCacheInvalid) | (oldCache & isCacheValid);
		nThinFlexVertexCount = nThinFlexVertexCount - isCacheInvalid;

		int nVertexIndex = updateCache & 0xFFFF;

		CachedPosNorm_t *pFlexedVertex = pThinFlexVerts + nVertexIndex; // will be overridden
		fltx4 vfNormal = LoadAlignedSIMD((float*)&pFlexedVertex->m_Normal);
		fltx4 vfPosition = LoadAlignedSIMD((float*)&pFlexedVertex->m_Position);

		// here we need to form the following vector to compute final w:
		// {s(1-b), (1-s)(1-b), sb, (1-s)b}
		//fltx4 f4sbProd = MulSIMD(__vperm(f4sb,f4sb,permuteW0), __vperm(f4sb,f4sb,permuteW1));
		fltx4 f4sbProd = MulSIMD(__vpermwi(f4sb,0x22), __vpermwi(f4sb,0xF5));
		fltx4 scWeight = __vmsum4fp(f4sbProd,w1234);

		pFirstThinFlexIndex[n] = updateCache;
		StoreAlignedSIMD((float*)&pFlexedVertex->m_Normal, MaddSIMD(scWeight,f3NDelta, vfNormal));
		StoreAlignedSIMD((float*)&pFlexedVertex->m_Position, MaddSIMD(scWeight,f3Delta, vfPosition));

		pVert ++;
	}
	while(pVert < pVertEnd); // why doesn't this use CTR??

	return nThinFlexVertexCount;
}


typedef int (*Fn_ComputeFlexedVertex_StreamOffset)(int nThinFlexVertexCount, CachedPosNorm_t *pThinFlexVerts, int32 *pFirstThinFlexIndex, mstudiovertanim_t * pVert, uint32 nCurrentTag, uint32 numVertsToProcess, fltx4 w1234);
Fn_ComputeFlexedVertex_StreamOffset g_fn_ComputeFlexedVertex_StreamOffset[8] = 
{
	NULL,
	ComputeFlexedVertex_StreamOffset_V1,
	ComputeFlexedVertex_StreamOffset_V2,
	ComputeFlexedVertex_StreamOffset_V3,
	ComputeFlexedVertex_StreamOffset_V4,
	ComputeFlexedVertex_StreamOffset_V5,
	ComputeFlexedVertex_StreamOffset_V6,
	ComputeFlexedVertex_StreamOffset_V7
};

typedef int (*Fn_ComputeFlexedVertexWrinkle_StreamOffset)(int nThinFlexVertexCount, CachedPosNorm_t *pThinFlexVerts, int32 *pFirstThinFlexIndex, mstudiovertanim_wrinkle_t * pVert, uint32 nCurrentTag, uint32 numVertsToProcess, fltx4 w1234);
Fn_ComputeFlexedVertexWrinkle_StreamOffset g_fn_ComputeFlexedVertexWrinkle_StreamOffset[8] = 
{
	NULL,
	ComputeFlexedVertexWrinkle_StreamOffset_V3,
	ComputeFlexedVertexWrinkle_StreamOffset_V3,
	ComputeFlexedVertexWrinkle_StreamOffset_V3,
	ComputeFlexedVertexWrinkle_StreamOffset_V4,
	ComputeFlexedVertexWrinkle_StreamOffset_V4,
	ComputeFlexedVertexWrinkle_StreamOffset_V4,
	ComputeFlexedVertexWrinkle_StreamOffset_V7
};


inline float Diff(const CachedPosNorm_t&a, const CachedPosNorm_t&b)
{
	return a.m_Position.DistTo(b.m_Position) + a.m_Normal.DistTo(b.m_Normal);
}

bool g_bBreakOnAssert = true;
void AlwaysAssert(bool mustBeTrue)
{
	if(!mustBeTrue)
	{
		Plat_DebugString("AlwaysAssert\n");
		if(g_bBreakOnAssert)
			DebugBreak();
	}
}

#endif

template
void CCachedRenderData::ComputeFlexedVertex_StreamOffset<mstudiovertanim_t>( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, 
														 mstudiovertanim_t *pvanim, int vertCount, float w1, float w2, float w3, float w4 );
template
void CCachedRenderData::ComputeFlexedVertex_StreamOffset<mstudiovertanim_wrinkle_t>( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, 
														 mstudiovertanim_wrinkle_t *pvanim, int vertCount, float w1, float w2, float w3, float w4 );

// vectorized
void CCachedRenderData::ComputeFlexedVertex_StreamOffset_Optimized( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, mstudiovertanim_t *pvanim, int vertCount, float w1, float w2, float w3, float w4 )
{
#if PROFILE_THIS_FILE
	CMiniProfilerGuard mpguard(&g_mp_morph);
#endif
#ifdef _X360
	int nMorphPath = g_cv_morph_path.GetInt();
	if(nMorphPath)
	{
		mstudiovertanim_t vertCountStruct;
		vertCountStruct.index = vertCount;
		/*for(uint32 i = 1; i< pflex->numverts; ++i)
		if(pvanim[i-1].index > pvanim[i].index)
		DebugBreak();*/

		mstudiovertanim_t * pVertEnd;
		{
#if PROFILE_THIS_FILE
			CMiniProfilerGuard mpguard_lower_bound(&g_mp_morph_lower_bound);
#endif
			pVertEnd = std::lower_bound(pvanim, pvanim + pflex->numverts, vertCountStruct, mstudiovertanim_t::CSortByIndex());
		}

		if(pvanim < pVertEnd)
		{
			union
			{
				fltx4 f4;
				float f1[4];
			} weights;
			weights.f1[0] = w1;
			weights.f1[1] = w2;
			weights.f1[2] = w3;
			weights.f1[3] = w4;
			uint32 nCurrentTag = uint32(m_CurrentTag)<<16;
			int nThinFlexVertexCount =  m_ThinFlexVertexCount;
			int32 *pFirstThinFlexIndex = (int32*)m_pFirstThinFlexIndex;
			CachedPosNorm_t *pThinFlexVerts = m_pThinFlexVerts;
			uint64 numVertsToProcess = pVertEnd - pvanim;
			nMorphPath = MIN(7,nMorphPath);

			/*static int maxVertsSaved = 0;
			if(numVertsToProcess > maxVertsSaved)
			{
				maxVertsSaved = numVertsToProcess;
				
				FileHandle_t fh = g_pFullFileSystem->Open( "vertices.bin", "wb" );
				if(fh != FILESYSTEM_INVALID_HANDLE)
				{
					g_pFullFileSystem->Write(pvanim, sizeof(*pvanim) * numVertsToProcess, fh);
					g_pFullFileSystem->Close(fh);
				}
			}*/


#ifdef _DEBUG
			if(0 == g_cv_morph_debug.GetInt())
#endif
			{
				for(uint32 i = 0; i < 2; ++i) // reset the first 2 positions here as it's required by the algorithm..
				{
					pThinFlexVerts[nThinFlexVertexCount+i].m_Position.InitZero();
					pThinFlexVerts[nThinFlexVertexCount+i].m_Normal.InitZero();
				}
				nThinFlexVertexCount = g_fn_ComputeFlexedVertex_StreamOffset[nMorphPath](nThinFlexVertexCount,pThinFlexVerts,pFirstThinFlexIndex,pvanim,nCurrentTag, numVertsToProcess, weights.f4);
			}
#ifdef _DEBUG
			else // Validation path inactive in release, since these static arrays consume 1MB
			{
				bool repeat = false;
				static CachedPosNorm_t backupThinFlexVerts[MAXSTUDIOFLEXVERTS+1], checkThinFlexVerts[MAXSTUDIOFLEXVERTS+1];
				static CacheIndex_t	backupFirstThinFlexIndex[MAXSTUDIOVERTS+1],checkFirstThinFlexIndex[MAXSTUDIOVERTS+1];
				int newThinFlexVertexCount ;
				static int numRuns = 0;
				++numRuns;
				memcpy(backupThinFlexVerts, m_pThinFlexVerts, sizeof(m_pThinFlexVerts));
				memcpy(backupFirstThinFlexIndex, m_pThinFlexIndex, sizeof(m_pThinFlexIndex));
				do
				{
					for(uint32 i = 0; i < 2; ++i) // reset the first 2 positions here as it's required by the algorithm..
					{
						pThinFlexVerts[nThinFlexVertexCount+i].m_Position.InitZero();
						pThinFlexVerts[nThinFlexVertexCount+i].m_Normal.InitZero();
					}

					newThinFlexVertexCount = g_fn_ComputeFlexedVertex_StreamOffset[nMorphPath](nThinFlexVertexCount,pThinFlexVerts,pFirstThinFlexIndex,pvanim,nCurrentTag, numVertsToProcess, weights.f4);
					memcpy(checkThinFlexVerts, m_pThinFlexVerts, sizeof(m_pThinFlexVerts));
					memcpy(checkFirstThinFlexIndex, m_pThinFlexIndex, sizeof(m_pThinFlexIndex));
					memcpy(m_pThinFlexVerts, backupThinFlexVerts, sizeof(m_pThinFlexVerts));
					memcpy(m_pThinFlexIndex, backupFirstThinFlexIndex, sizeof(m_pThinFlexIndex));

					ComputeFlexedVertex_StreamOffset( pStudioHdr, pflex, pvanim, vertCount, w1, w2, w3, w4);
					AlwaysAssert(m_ThinFlexVertexCount == newThinFlexVertexCount);
					for(int i = 0; i < newThinFlexVertexCount; ++i)
						AlwaysAssert(Diff(checkThinFlexVerts[i], m_pThinFlexVerts[i]) < 1e-5f);
					int indexOffset = m_pFirstThinFlexIndex - m_pThinFlexIndex;
					for(int i = 0; i < numVertsToProcess; ++i)
						AlwaysAssert(*(int*)&checkFirstThinFlexIndex[indexOffset + pvanim[i].index]  == *(int*)&m_pThinFlexIndex[indexOffset + pvanim[i].index]);

					if(repeat)
					{
						m_ThinFlexVertexCount = nThinFlexVertexCount;
						memcpy(m_pThinFlexVerts, backupThinFlexVerts, sizeof(m_pThinFlexVerts));
						memcpy(m_pThinFlexIndex, backupFirstThinFlexIndex, sizeof(m_pThinFlexIndex));
					}
				}
				while(repeat);
				nThinFlexVertexCount = newThinFlexVertexCount;
			}
#endif
			m_ThinFlexVertexCount = nThinFlexVertexCount;
		}
	}
	else
#endif
	{
		ComputeFlexedVertex_StreamOffset( pStudioHdr, pflex, pvanim, vertCount, w1, w2, w3, w4);
	}
}


void CCachedRenderData::ComputeFlexedVertexWrinkle_StreamOffset_Optimized( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, mstudiovertanim_wrinkle_t *pvanim, int vertCount, float w1, float w2, float w3, float w4)
{
#if PROFILE_THIS_FILE
	CMiniProfilerGuard mpguard(&g_mp_morph);
#endif

#ifdef _X360
	int nMorphPath = g_cv_morph_path.GetInt();
	if(nMorphPath)
	{
		mstudiovertanim_wrinkle_t vertCountStruct;
		vertCountStruct.index = vertCount;

		mstudiovertanim_wrinkle_t * pVertEnd;
		{
#if PROFILE_THIS_FILE
			CMiniProfilerGuard mpguard_lower_bound(&g_mp_morph_lower_bound);
#endif
			pVertEnd = std::lower_bound(pvanim, pvanim + pflex->numverts, vertCountStruct, mstudiovertanim_wrinkle_t::CSortByIndex());
		}

		if(pvanim < pVertEnd)
		{
			union
			{
				fltx4 f4;
				float f1[4];
			} weights;
			weights.f1[0] = w1;
			weights.f1[1] = w2;
			weights.f1[2] = w3;
			weights.f1[3] = w4;
			uint32 nCurrentTag = uint32(m_CurrentTag)<<16;
			int nThinFlexVertexCount =  m_ThinFlexVertexCount;
			int32 *pFirstThinFlexIndex = (int32*)m_pFirstThinFlexIndex;
			CachedPosNorm_t *pThinFlexVerts = m_pThinFlexVerts;
			uint64 numVertsToProcess = pVertEnd - pvanim;
			nMorphPath = MIN(7,nMorphPath);

#ifdef _DEBUG
			if(0 == g_cv_morph_debug.GetInt())
#endif
			{
				for(uint32 i = 0; i < 2; ++i) // reset the first 2 positions here as it's required by the algorithm..
				{
					pThinFlexVerts[nThinFlexVertexCount+i].m_Position.InitZero();
					pThinFlexVerts[nThinFlexVertexCount+i].m_Normal.InitZero();
				}
				nThinFlexVertexCount = g_fn_ComputeFlexedVertexWrinkle_StreamOffset[nMorphPath](nThinFlexVertexCount,pThinFlexVerts,pFirstThinFlexIndex,pvanim,nCurrentTag, numVertsToProcess, weights.f4);
			}
#ifdef _DEBUG
			else // Validation path inactive in release, since these static arrays consume 1MB
			{
				bool repeat = false;
				static CachedPosNorm_t backupThinFlexVerts[MAXSTUDIOFLEXVERTS+1], checkThinFlexVerts[MAXSTUDIOFLEXVERTS+1];
				static CacheIndex_t	backupFirstThinFlexIndex[MAXSTUDIOVERTS+1],checkFirstThinFlexIndex[MAXSTUDIOVERTS+1];
				int newThinFlexVertexCount ;
				static int numRuns = 0;
				++numRuns;
				memcpy(backupThinFlexVerts, m_pThinFlexVerts, sizeof(m_pThinFlexVerts));
				memcpy(backupFirstThinFlexIndex, m_pThinFlexIndex, sizeof(m_pThinFlexIndex));
				do
				{
					for(uint32 i = 0; i < 2; ++i) // reset the first 2 positions here as it's required by the algorithm..
					{
						pThinFlexVerts[nThinFlexVertexCount+i].m_Position.InitZero();
						pThinFlexVerts[nThinFlexVertexCount+i].m_Normal.InitZero();
					}

					newThinFlexVertexCount = g_fn_ComputeFlexedVertexWrinkle_StreamOffset[nMorphPath](nThinFlexVertexCount,pThinFlexVerts,pFirstThinFlexIndex,pvanim,nCurrentTag, numVertsToProcess, weights.f4);
					memcpy(checkThinFlexVerts, m_pThinFlexVerts, sizeof(m_pThinFlexVerts));
					memcpy(checkFirstThinFlexIndex, m_pThinFlexIndex, sizeof(m_pThinFlexIndex));
					memcpy(m_pThinFlexVerts, backupThinFlexVerts, sizeof(m_pThinFlexVerts));
					memcpy(m_pThinFlexIndex, backupFirstThinFlexIndex, sizeof(m_pThinFlexIndex));

					ComputeFlexedVertex_StreamOffset( pStudioHdr, pflex, pvanim, vertCount, w1, w2, w3, w4);
					AlwaysAssert(m_ThinFlexVertexCount == newThinFlexVertexCount);
					for(int i = 0; i < newThinFlexVertexCount; ++i)
						AlwaysAssert(Diff(checkThinFlexVerts[i], m_pThinFlexVerts[i]) < 1e-5f);
					int indexOffset = m_pFirstThinFlexIndex - m_pThinFlexIndex;
					for(int i = 0; i < numVertsToProcess; ++i)
						AlwaysAssert(*(int*)&checkFirstThinFlexIndex[indexOffset + pvanim[i].index]  == *(int*)&m_pThinFlexIndex[indexOffset + pvanim[i].index]);

					if(repeat)
					{
						m_ThinFlexVertexCount = nThinFlexVertexCount;
						memcpy(m_pThinFlexVerts, backupThinFlexVerts, sizeof(m_pThinFlexVerts));
						memcpy(m_pThinFlexIndex, backupFirstThinFlexIndex, sizeof(m_pThinFlexIndex));
					}
				}
				while(repeat);
				nThinFlexVertexCount = newThinFlexVertexCount;
			}
#endif
			m_ThinFlexVertexCount = nThinFlexVertexCount;
		}
	}
	else
#endif
	{
		ComputeFlexedVertex_StreamOffset( pStudioHdr, pflex, pvanim, vertCount, w1, w2, w3, w4);
	}
}

#endif // PLATFORM_WINDOWS