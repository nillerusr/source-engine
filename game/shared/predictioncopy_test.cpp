//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"

#if !defined( NO_ENTITY_PREDICTION )

#include "IGameSystem.h"
#include <typeinfo.h>
#include "cdll_int.h"
#endif
#include <memory.h>
#include <stdarg.h>
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "predictioncopy.h"
#include "engine/ivmodelinfo.h"
#include "tier1/fmtstr.h"
#include "utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( COPY_CHECK_STRESSTEST )

class CPredictionCopyTester : public CBaseGameSystem
{
public:

	virtual char const *Name() { return "CPredictionCopyTester"; }
	// Init, shutdown
	virtual bool Init()
	{
		RunTests();
		Remove( this );
		return true;
	}

	virtual void Shutdown() {}

	// Level init, shutdown
	virtual void LevelInit() {}
	// The level is shutdown in two parts
	virtual void LevelShutdownPreEntity() {}
	// Entities are deleted / released here...
	virtual void LevelShutdownPostEntity() {}
	// end of level shutdown

	// Called before rendering
	virtual void PreRender ( ) {}

	// Called after rendering
	virtual void PostRender() {}

	// Gets called each frame
	virtual void Update( float frametime ) {}

private:

	void RunTests( void );
};

IGameSystem* GetPredictionCopyTester( void )
{
	static CPredictionCopyTester s_PredictionCopyTesterSystem;
	return &s_PredictionCopyTesterSystem;
}

class CCopyTesterData
{
public:

	CCopyTesterData()
	{
		m_CharValue = 'a';
		m_ShortValue = (short)100;
		m_IntValue = (int)100;
		m_FloatValue = 1.0f;
		Q_strncpy( m_szValue, "primarydata", sizeof( m_szValue ) );
		m_Vector = Vector( 100, 100, 100 );
		AngleQuaternion( QAngle( 0, 45, 0 ), m_Quaternion );
		m_Bool = false;
		m_Clr.r = m_Clr.g = m_Clr.b = m_Clr.a = 255;

		m_Ptr = (void *)0xfedcba98;
		//	m_hEHandle = NULL;
	}

	void MakeDifferent( void )
	{
		m_CharValue = 'd';
		m_ShortValue = (short)400;
		m_IntValue = (int)400;
		m_FloatValue = 4.0f;
		Q_strncpy( m_szValue, "secondarydata", sizeof( m_szValue ) );
		m_Vector = Vector( 400, 400, 400 );
		AngleQuaternion( QAngle( 60, 0, 0 ), m_Quaternion );
		m_Bool = true;
		m_Clr.r = m_Clr.g = m_Clr.b = m_Clr.a = 1;
		m_Ptr = (void *)0x00000001;
		//	m_hEHandle = (C_BaseEntity *)0x00000001;
	}

	DECLARE_PREDICTABLE();

	int		m_IntValue;
	float	m_FloatValue;
	Vector	m_Vector;
	char	m_CharValue;
	Quaternion m_Quaternion;
	color32	m_Clr;
	bool	m_Bool;
	void	*m_Ptr;
	short	m_ShortValue;
	char	m_szValue[ 128 ];

	//	EHANDLE	m_hEHandle;

};

BEGIN_PREDICTION_DATA_NO_BASE( CCopyTesterData )

DEFINE_FIELD( m_CharValue, FIELD_CHARACTER ),
DEFINE_FIELD( m_ShortValue, FIELD_SHORT ),
DEFINE_FIELD( m_IntValue, FIELD_INTEGER ),
DEFINE_FIELD( m_FloatValue, FIELD_FLOAT ),
DEFINE_FIELD( m_szValue, FIELD_STRING ),
DEFINE_FIELD( m_Vector, FIELD_VECTOR ),
DEFINE_FIELD( m_Quaternion, FIELD_QUATERNION ),
DEFINE_FIELD( m_Bool, FIELD_BOOLEAN ),
DEFINE_FIELD( m_Clr, FIELD_COLOR32 ),
//	DEFINE_FIELD( m_hEHandle, FIELD_EHANDLE ),

END_PREDICTION_DATA()

class CCopyTesterData2 : public C_BaseEntity
{
	DECLARE_CLASS( CCopyTesterData2, C_BaseEntity );

public:
	CCopyTesterData2()
	{
		m_CharValueA = 'b';
		m_ShortValueA = (short)200;
		m_IntValueA = (int)200;
		m_FloatValueA = 2.0f;
	}

	void MakeDifferent( void )
	{
		m_CharValueA = 'e';
		m_ShortValueA = (short)500;
		m_IntValueA = (int)500;
		m_FloatValueA = 5.0f;
		m_FooData.MakeDifferent();
	}

	DECLARE_PREDICTABLE();

	char	m_CharValueA;
	CCopyTesterData	m_FooData;
	int		m_IntValueA;
	short	m_ShortValueA;
	float	m_FloatValueA;

};

BEGIN_PREDICTION_DATA_NO_BASE( CCopyTesterData2 )

DEFINE_FIELD( m_CharValueA, FIELD_CHARACTER ),
DEFINE_FIELD( m_ShortValueA, FIELD_SHORT ),
DEFINE_PRED_TYPEDESCRIPTION( m_FooData, CCopyTesterData ),
DEFINE_FIELD( m_IntValueA, FIELD_INTEGER ),
DEFINE_FIELD( m_FloatValueA, FIELD_FLOAT ),

END_PREDICTION_DATA()

void CPredictionCopyTester::RunTests( void )
{
	CCopyTesterData2 *foo1, *foo2, *foo3;

	foo1 = new CCopyTesterData2;
	foo2 = new CCopyTesterData2;
	foo3 = new CCopyTesterData2;

	foo2->MakeDifferent();

	CPredictionCopy::PrepareDataMap( foo1->GetPredDescMap() );
	CPredictionCopy::PrepareDataMap( foo2->GetPredDescMap() );
	CPredictionCopy::PrepareDataMap( foo2->GetPredDescMap() );

	// foo1 == foo3
	// foo1 != foo2
	{
		Msg( "Comparing and copying == objects, should have zero diffcount\n" );

		// Compare foo1 and foo3, should be equal
		CPredictionCopy tester( PC_NON_NETWORKED_ONLY, foo1, false, foo3, false, CPredictionCopy::TRANSFERDATA_ERRORCHECK_SPEW );
		int diff_count = 0;
		diff_count = tester.TransferData( "test1", -1, foo3->GetPredDescMap() );

		Msg( "diff_count == %i\n", diff_count );
		Assert( !diff_count );
	}

	{
		Msg( "Simple compare of != objects, should spew and have non-zero diffcount\n" );

		// Compare foo1 and foo2, should differ
		CPredictionCopy tester( PC_NON_NETWORKED_ONLY, foo1, false, foo2, false, CPredictionCopy::TRANSFERDATA_ERRORCHECK_SPEW );
		int diff_count = 0;
		diff_count = tester.TransferData( "test2", -1, foo2->GetPredDescMap() );

		Msg( "diff_count == %i (should be 13)\n", diff_count );
		Assert( diff_count == 13 );
	}

	{
		Msg( "Comparing and copying same objects, should spew and have non-zero diffcount\n" );

		// Compare foo1 and foo2 while overriting foo2, should have differences but leave objects ==
		CPredictionCopy tester( PC_NON_NETWORKED_ONLY, foo1, false, foo2, false, CPredictionCopy::TRANSFERDATA_COPYONLY );
		tester.TransferData( "test2", -1, foo1->GetPredDescMap() );
	}

	{
		Msg( "Comparing and copying objects which were just made to coincide, should have zero diffcount\n" );

		// Make sure foo1 is now == foo2
		CPredictionCopy tester( PC_NON_NETWORKED_ONLY, foo1, false, foo2, false, CPredictionCopy::TRANSFERDATA_ERRORCHECK_SPEW );
		int diff_count = 0;
		diff_count = tester.TransferData( "test4", -1, foo2->GetPredDescMap() );

		Msg( "diff_count == %i\n", diff_count );
		Assert( !diff_count );
	}

	delete foo3;
	delete foo2;
	delete foo1;

}

#endif // COPY_CHECK_STRESSTEST