//========== Copyright © 2007, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#undef AI_BEHAVIOR_METHOD_0
#undef AI_BEHAVIOR_METHOD_0V
#undef AI_BEHAVIOR_METHOD_1
#undef AI_BEHAVIOR_METHOD_1V
#undef AI_BEHAVIOR_METHOD_2
#undef AI_BEHAVIOR_METHOD_2V
#undef AI_BEHAVIOR_METHOD_3
#undef AI_BEHAVIOR_METHOD_3V
#undef AI_BEHAVIOR_METHOD_4
#undef AI_BEHAVIOR_METHOD_4V
#undef AI_BEHAVIOR_METHOD_5
#undef AI_BEHAVIOR_METHOD_5V
#undef AI_BEHAVIOR_METHOD_6
#undef AI_BEHAVIOR_METHOD_6V
#undef AI_BEHAVIOR_METHOD_7
#undef AI_BEHAVIOR_METHOD_7V
#undef AI_BEHAVIOR_METHOD_8
#undef AI_BEHAVIOR_METHOD_8V
#undef AI_BEHAVIOR_METHOD_9
#undef AI_BEHAVIOR_METHOD_9V
#undef AI_BEHAVIOR_METHOD_11V

#undef AI_BEHAVIOR_METHOD_0C
#undef AI_BEHAVIOR_METHOD_0VC
#undef AI_BEHAVIOR_METHOD_1C
#undef AI_BEHAVIOR_METHOD_1VC
#undef AI_BEHAVIOR_METHOD_2C
#undef AI_BEHAVIOR_METHOD_2VC
#undef AI_BEHAVIOR_METHOD_3C
#undef AI_BEHAVIOR_METHOD_3VC
#undef AI_BEHAVIOR_METHOD_4C
#undef AI_BEHAVIOR_METHOD_4VC
#undef AI_BEHAVIOR_METHOD_5C
#undef AI_BEHAVIOR_METHOD_5VC
#undef AI_BEHAVIOR_METHOD_6C
#undef AI_BEHAVIOR_METHOD_6VC
#undef AI_BEHAVIOR_METHOD_7C
#undef AI_BEHAVIOR_METHOD_7VC
#undef AI_BEHAVIOR_METHOD_8C
#undef AI_BEHAVIOR_METHOD_8VC
#undef AI_BEHAVIOR_METHOD_9C
#undef AI_BEHAVIOR_METHOD_9VC

#ifdef AI_GENERATE_HOST_METHODS
#undef AI_GENERATE_HOST_METHODS

#define AI_BEHAVIOR_METHOD_0( RetType, FuncName ) RetType FuncName()	{ if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName(); else return BaseClass::FuncName(); }
#define AI_BEHAVIOR_METHOD_0V( FuncName ) void FuncName()	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName(); else BaseClass::FuncName(); }
#define AI_BEHAVIOR_METHOD_1( RetType, FuncName, ArgType1 ) RetType FuncName( ArgType1 a1 )	{ if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1 ); else return BaseClass::FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_1V( FuncName, ArgType1 ) void FuncName( ArgType1 a1 )	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1 ); else BaseClass::FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_2( RetType, FuncName, ArgType1, ArgType2 ) RetType FuncName( ArgType1 a1, ArgType2 a2 )	{ if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2 ); else return BaseClass::FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_2V( FuncName, ArgType1, ArgType2 ) void FuncName( ArgType1 a1, ArgType2 a2 )	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2 ); else BaseClass::FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_3( RetType, FuncName, ArgType1, ArgType2, ArgType3 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	{ if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3 ); else return BaseClass::FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_3V( FuncName, ArgType1, ArgType2, ArgType3 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3 ); else BaseClass::FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_4( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	{ if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4 ); else return BaseClass::FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_4V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4 ); else BaseClass::FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_5( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	{ if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5 ); else return BaseClass::FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_5V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5 ); else BaseClass::FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_6( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	{ if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6 ); else return BaseClass::FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_6V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6 ); else BaseClass::FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_7( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	{ if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7 ); else return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_7V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7 ); else BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_8( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	{ if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); else return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_8V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); else BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_9( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	{ if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); else return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_9V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); else BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_11V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ArgType10, ArgType11 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9, ArgType10 a10, ArgType11 a11 )	{ if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 ); else BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 ); }

#define AI_BEHAVIOR_METHOD_0C( RetType, FuncName ) RetType FuncName()	const { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName(); else return BaseClass::FuncName(); }
#define AI_BEHAVIOR_METHOD_0VC( FuncName ) void FuncName()	const { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName(); else BaseClass::FuncName(); }
#define AI_BEHAVIOR_METHOD_1C( RetType, FuncName, ArgType1 ) RetType FuncName( ArgType1 a1 )	const { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1 ); else return BaseClass::FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_1VC( FuncName, ArgType1 ) void FuncName( ArgType1 a1 )	const { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1 ); else BaseClass::FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_2C( RetType, FuncName, ArgType1, ArgType2 ) RetType FuncName( ArgType1 a1, ArgType2 a2 )	const { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2 ); else return BaseClass::FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_2VC( FuncName, ArgType1, ArgType2 ) void FuncName( ArgType1 a1, ArgType2 a2 )	const { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2 ); else BaseClass::FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_3C( RetType, FuncName, ArgType1, ArgType2, ArgType3 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	const { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3 ); else return BaseClass::FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_3VC( FuncName, ArgType1, ArgType2, ArgType3 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	const { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3 ); else BaseClass::FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_4C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	const { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4 ); else return BaseClass::FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_4VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	const { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4 ); else BaseClass::FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_5C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	const { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5 ); else return BaseClass::FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_5VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	const { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5 ); else BaseClass::FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_6C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	const { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6 ); else return BaseClass::FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_6VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	const { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6 ); else BaseClass::FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_7C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	const { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7 ); else return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_7VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	const { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7 ); else BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_8C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	const { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); else return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_8VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	const { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); else BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_9C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	const { if ( this->m_pPrimaryBehavior ) return this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); else return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_9VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	const { if ( this->m_pPrimaryBehavior ) this->m_pPrimaryBehavior->FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); else BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }

#endif

#ifdef AI_GENERATE_BRIDGES
#undef AI_GENERATE_BRIDGES

#define AI_BEHAVIOR_METHOD_0( RetType, FuncName ) RetType BehaviorBridge_##FuncName()	{ return BaseClass::FuncName(); }
#define AI_BEHAVIOR_METHOD_0V( FuncName ) void BehaviorBridge_##FuncName()	{ BaseClass::FuncName(); }
#define AI_BEHAVIOR_METHOD_1( RetType, FuncName, ArgType1 ) RetType BehaviorBridge_##FuncName( ArgType1 a1 )	{ return BaseClass::FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_1V( FuncName, ArgType1 ) void BehaviorBridge_##FuncName( ArgType1 a1 )	{ BaseClass::FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_2( RetType, FuncName, ArgType1, ArgType2 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 )	{ return BaseClass::FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_2V( FuncName, ArgType1, ArgType2 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 )	{ BaseClass::FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_3( RetType, FuncName, ArgType1, ArgType2, ArgType3 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	{ return BaseClass::FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_3V( FuncName, ArgType1, ArgType2, ArgType3 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	{ BaseClass::FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_4( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	{ return BaseClass::FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_4V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	{ BaseClass::FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_5( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	{ return BaseClass::FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_5V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_6( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	{ return BaseClass::FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_6V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_7( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	{ return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_7V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_8( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	{ return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_8V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_9( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	{ return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_9V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_11V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ArgType10, ArgType11 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9, ArgType10 a10, ArgType11 a11 )	{ BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 ); }

#define AI_BEHAVIOR_METHOD_0C( RetType, FuncName ) RetType BehaviorBridge_##FuncName()	const { return BaseClass::FuncName(); }
#define AI_BEHAVIOR_METHOD_0VC( FuncName ) void BehaviorBridge_##FuncName()	const { BaseClass::FuncName(); }
#define AI_BEHAVIOR_METHOD_1C( RetType, FuncName, ArgType1 ) RetType BehaviorBridge_##FuncName( ArgType1 a1 )	const { return BaseClass::FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_1VC( FuncName, ArgType1 ) void BehaviorBridge_##FuncName( ArgType1 a1 )	const { BaseClass::FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_2C( RetType, FuncName, ArgType1, ArgType2 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 )	const { return BaseClass::FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_2VC( FuncName, ArgType1, ArgType2 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 )	const { BaseClass::FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_3C( RetType, FuncName, ArgType1, ArgType2, ArgType3 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	const { return BaseClass::FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_3VC( FuncName, ArgType1, ArgType2, ArgType3 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	const { BaseClass::FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_4C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	const { return BaseClass::FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_4VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	const { BaseClass::FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_5C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	const { return BaseClass::FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_5VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	const { BaseClass::FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_6C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	const { return BaseClass::FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_6VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	const { BaseClass::FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_7C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	const { return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_7VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	const { BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_8C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	const { return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_8VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	const { BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_9C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	const { return BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_9VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	const { BaseClass::FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }

#endif

#ifdef AI_GENERATE_BRIDGE_INTERFACE
#undef AI_GENERATE_BRIDGE_INTERFACE

#define AI_BEHAVIOR_METHOD_0( RetType, FuncName ) virtual RetType BehaviorBridge_##FuncName() { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_0V( FuncName ) virtual void BehaviorBridge_##FuncName() {}
#define AI_BEHAVIOR_METHOD_1( RetType, FuncName, ArgType1 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1 ) { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_1V( FuncName, ArgType1 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1 ) {}
#define AI_BEHAVIOR_METHOD_2( RetType, FuncName, ArgType1, ArgType2 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 ) { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_2V( FuncName, ArgType1, ArgType2 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 ) {}
#define AI_BEHAVIOR_METHOD_3( RetType, FuncName, ArgType1, ArgType2, ArgType3 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 ) { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_3V( FuncName, ArgType1, ArgType2, ArgType3 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 ) {}
#define AI_BEHAVIOR_METHOD_4( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 ) { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_4V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 ) {}
#define AI_BEHAVIOR_METHOD_5( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 ) { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_5V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 ) {}
#define AI_BEHAVIOR_METHOD_6( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 ) { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_6V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 ) {}
#define AI_BEHAVIOR_METHOD_7( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 ) { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_7V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 ) {}
#define AI_BEHAVIOR_METHOD_8( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 ) { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_8V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 ) {}
#define AI_BEHAVIOR_METHOD_9( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 ) { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_9V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 ) {}
#define AI_BEHAVIOR_METHOD_11V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ArgType10, ArgType11 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9, ArgType10 a10, ArgType11 a11 ) {}

#define AI_BEHAVIOR_METHOD_0C( RetType, FuncName ) virtual RetType BehaviorBridge_##FuncName() const { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_0VC( FuncName ) virtual void BehaviorBridge_##FuncName() const {}
#define AI_BEHAVIOR_METHOD_1C( RetType, FuncName, ArgType1 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1 ) const { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_1VC( FuncName, ArgType1 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1 ) const {}
#define AI_BEHAVIOR_METHOD_2C( RetType, FuncName, ArgType1, ArgType2 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 ) const { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_2VC( FuncName, ArgType1, ArgType2 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 ) const {}
#define AI_BEHAVIOR_METHOD_3C( RetType, FuncName, ArgType1, ArgType2, ArgType3 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 ) const { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_3VC( FuncName, ArgType1, ArgType2, ArgType3 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 ) const {}
#define AI_BEHAVIOR_METHOD_4C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 ) const { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_4VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 ) const {}
#define AI_BEHAVIOR_METHOD_5C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 ) const { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_5VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 ) const {}
#define AI_BEHAVIOR_METHOD_6C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 ) const { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_6VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 ) const {}
#define AI_BEHAVIOR_METHOD_7C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 ) const { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_7VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 ) const {}
#define AI_BEHAVIOR_METHOD_8C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 ) const { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_8VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 ) const {}
#define AI_BEHAVIOR_METHOD_9C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) virtual RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 ) const { return (RetType)0; }
#define AI_BEHAVIOR_METHOD_9VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) virtual void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 ) const {}

#endif

#ifdef AI_GENERATE_BASE_METHODS
#undef AI_GENERATE_BASE_METHODS

#define AI_BEHAVIOR_METHOD_0( RetType, FuncName ) virtual RetType FuncName()	{ return m_pBackBridge->BehaviorBridge_##FuncName(); }
#define AI_BEHAVIOR_METHOD_0V( FuncName ) virtual void FuncName()	{ m_pBackBridge->BehaviorBridge_##FuncName(); }
#define AI_BEHAVIOR_METHOD_1( RetType, FuncName, ArgType1 ) virtual RetType FuncName( ArgType1 a1 )	{ return m_pBackBridge->BehaviorBridge_##FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_1V( FuncName, ArgType1 ) virtual void FuncName( ArgType1 a1 )	{ m_pBackBridge->BehaviorBridge_##FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_2( RetType, FuncName, ArgType1, ArgType2 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2 )	{ return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_2V( FuncName, ArgType1, ArgType2 ) virtual void FuncName( ArgType1 a1, ArgType2 a2 )	{ m_pBackBridge->BehaviorBridge_##FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_3( RetType, FuncName, ArgType1, ArgType2, ArgType3 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	{ return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_3V( FuncName, ArgType1, ArgType2, ArgType3 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	{ m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_4( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	{ return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_4V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	{ m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_5( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	{ return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_5V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	{ m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_6( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	{ return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_6V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	{ m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_7( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	{ return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_7V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	{ m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_8( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	{ return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_8V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	{ m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_9( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	{ return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_9V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	{ m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_11V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ArgType10, ArgType11 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9, ArgType10 a10, ArgType11 a11 )	{ m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 ); }

#define AI_BEHAVIOR_METHOD_0C( RetType, FuncName ) virtual RetType FuncName()	const { return m_pBackBridge->BehaviorBridge_##FuncName(); }
#define AI_BEHAVIOR_METHOD_0VC( FuncName ) virtual void FuncName()	const { m_pBackBridge->BehaviorBridge_##FuncName(); }
#define AI_BEHAVIOR_METHOD_1C( RetType, FuncName, ArgType1 ) virtual RetType FuncName( ArgType1 a1 )	const { return m_pBackBridge->BehaviorBridge_##FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_1VC( FuncName, ArgType1 ) virtual void FuncName( ArgType1 a1 )	const { m_pBackBridge->BehaviorBridge_##FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_2C( RetType, FuncName, ArgType1, ArgType2 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2 )	const { return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_2VC( FuncName, ArgType1, ArgType2 ) virtual void FuncName( ArgType1 a1, ArgType2 a2 )	const { m_pBackBridge->BehaviorBridge_##FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_3C( RetType, FuncName, ArgType1, ArgType2, ArgType3 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	const { return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_3VC( FuncName, ArgType1, ArgType2, ArgType3 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	const { m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_4C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	const { return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_4VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	const { m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_5C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	const { return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_5VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	const { m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_6C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	const { return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_6VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	const { m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_7C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	const { return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_7VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	const { m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_8C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	const { return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_8VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	const { m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_9C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) virtual RetType FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	const { return m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_9VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) virtual void FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	const { m_pBackBridge->BehaviorBridge_##FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }

#endif

#ifdef AI_GENERATE_BEHAVIOR_BRIDGES
#undef AI_GENERATE_BEHAVIOR_BRIDGES

#define AI_BEHAVIOR_METHOD_0( RetType, FuncName ) RetType BehaviorBridge_##FuncName()	{ return FuncName(); }
#define AI_BEHAVIOR_METHOD_0V( FuncName ) void BehaviorBridge_##FuncName()	{ FuncName(); }
#define AI_BEHAVIOR_METHOD_1( RetType, FuncName, ArgType1 ) RetType BehaviorBridge_##FuncName( ArgType1 a1 )	{ return FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_1V( FuncName, ArgType1 ) void BehaviorBridge_##FuncName( ArgType1 a1 )	{ FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_2( RetType, FuncName, ArgType1, ArgType2 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 )	{ return FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_2V( FuncName, ArgType1, ArgType2 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 )	{ FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_3( RetType, FuncName, ArgType1, ArgType2, ArgType3 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	{ return FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_3V( FuncName, ArgType1, ArgType2, ArgType3 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	{ FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_4( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	{ return FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_4V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	{ FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_5( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	{ return FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_5V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	{ FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_6( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	{ return FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_6V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	{ FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_7( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	{ return FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_7V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	{ FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_8( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	{ return FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_8V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	{ FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_9( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	{ return FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_9V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	{ FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_11V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ArgType10, ArgType11 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9, ArgType10 a10, ArgType11 a11 )	{ FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 ); }

#define AI_BEHAVIOR_METHOD_0C( RetType, FuncName ) RetType BehaviorBridge_##FuncName()	const { return FuncName(); }
#define AI_BEHAVIOR_METHOD_0VC( FuncName ) void BehaviorBridge_##FuncName()	const { FuncName(); }
#define AI_BEHAVIOR_METHOD_1C( RetType, FuncName, ArgType1 ) RetType BehaviorBridge_##FuncName( ArgType1 a1 )	const { return FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_1VC( FuncName, ArgType1 ) void BehaviorBridge_##FuncName( ArgType1 a1 )	const { FuncName( a1 ); }
#define AI_BEHAVIOR_METHOD_2C( RetType, FuncName, ArgType1, ArgType2 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 )	const { return FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_2VC( FuncName, ArgType1, ArgType2 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2 )	const { FuncName( a1, a2 ); }
#define AI_BEHAVIOR_METHOD_3C( RetType, FuncName, ArgType1, ArgType2, ArgType3 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	const { return FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_3VC( FuncName, ArgType1, ArgType2, ArgType3 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3 )	const { FuncName( a1, a2, a3 ); }
#define AI_BEHAVIOR_METHOD_4C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	const { return FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_4VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4 )	const { FuncName( a1, a2, a3, a4 ); }
#define AI_BEHAVIOR_METHOD_5C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	const { return FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_5VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5 )	const { FuncName( a1, a2, a3, a4, a5 ); }
#define AI_BEHAVIOR_METHOD_6C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	const { return FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_6VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6 )	const { FuncName( a1, a2, a3, a4, a5, a6 ); }
#define AI_BEHAVIOR_METHOD_7C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	const { return FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_7VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7 )	const { FuncName( a1, a2, a3, a4, a5, a6, a7 ); }
#define AI_BEHAVIOR_METHOD_8C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	const { return FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_8VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8 )	const { FuncName( a1, a2, a3, a4, a5, a6, a7, a8 ); }
#define AI_BEHAVIOR_METHOD_9C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) RetType BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	const { return FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }
#define AI_BEHAVIOR_METHOD_9VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 ) void BehaviorBridge_##FuncName( ArgType1 a1, ArgType2 a2, ArgType3 a3, ArgType4 a4, ArgType5 a5, ArgType6 a6, ArgType7 a7, ArgType8 a8, ArgType9 a9 )	const { FuncName( a1, a2, a3, a4, a5, a6, a7, a8, a9 ); }

#endif


#ifndef AI_BEHAVIOR_METHOD_0

#define AI_BEHAVIOR_METHOD_0( RetType, FuncName )
#define AI_BEHAVIOR_METHOD_0V( FuncName )
#define AI_BEHAVIOR_METHOD_1( RetType, FuncName, ArgType1 )
#define AI_BEHAVIOR_METHOD_1V( FuncName, ArgType1 )
#define AI_BEHAVIOR_METHOD_2( RetType, FuncName, ArgType1, ArgType2 )
#define AI_BEHAVIOR_METHOD_2V( FuncName, ArgType1, ArgType2 )
#define AI_BEHAVIOR_METHOD_3( RetType, FuncName, ArgType1, ArgType2, ArgType3 )
#define AI_BEHAVIOR_METHOD_3V( FuncName, ArgType1, ArgType2, ArgType3 )
#define AI_BEHAVIOR_METHOD_4( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 )
#define AI_BEHAVIOR_METHOD_4V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 )
#define AI_BEHAVIOR_METHOD_5( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 )
#define AI_BEHAVIOR_METHOD_5V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 )
#define AI_BEHAVIOR_METHOD_6( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 )
#define AI_BEHAVIOR_METHOD_6V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 )
#define AI_BEHAVIOR_METHOD_7( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 )
#define AI_BEHAVIOR_METHOD_7V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 )
#define AI_BEHAVIOR_METHOD_8( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 )
#define AI_BEHAVIOR_METHOD_8V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 )
#define AI_BEHAVIOR_METHOD_9( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 )
#define AI_BEHAVIOR_METHOD_9V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 )
#define AI_BEHAVIOR_METHOD_11V( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9, ArgType10, ArgType11 )

#define AI_BEHAVIOR_METHOD_0C( RetType, FuncName )
#define AI_BEHAVIOR_METHOD_0VC( FuncName )
#define AI_BEHAVIOR_METHOD_1C( RetType, FuncName, ArgType1 )
#define AI_BEHAVIOR_METHOD_1VC( FuncName, ArgType1 )
#define AI_BEHAVIOR_METHOD_2C( RetType, FuncName, ArgType1, ArgType2 )
#define AI_BEHAVIOR_METHOD_2VC( FuncName, ArgType1, ArgType2 )
#define AI_BEHAVIOR_METHOD_3C( RetType, FuncName, ArgType1, ArgType2, ArgType3 )
#define AI_BEHAVIOR_METHOD_3VC( FuncName, ArgType1, ArgType2, ArgType3 )
#define AI_BEHAVIOR_METHOD_4C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4 )
#define AI_BEHAVIOR_METHOD_4VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4 )
#define AI_BEHAVIOR_METHOD_5C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 )
#define AI_BEHAVIOR_METHOD_5VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5 )
#define AI_BEHAVIOR_METHOD_6C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 )
#define AI_BEHAVIOR_METHOD_6VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6 )
#define AI_BEHAVIOR_METHOD_7C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 )
#define AI_BEHAVIOR_METHOD_7VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7 )
#define AI_BEHAVIOR_METHOD_8C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 )
#define AI_BEHAVIOR_METHOD_8VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8 )
#define AI_BEHAVIOR_METHOD_9C( RetType, FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 )
#define AI_BEHAVIOR_METHOD_9VC( FuncName, ArgType1, ArgType2, ArgType3, ArgType4, ArgType5, ArgType6, ArgType7, ArgType8, ArgType9 )

#endif

