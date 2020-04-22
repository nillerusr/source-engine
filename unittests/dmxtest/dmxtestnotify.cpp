//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for DMX testing (testing the Notify subsystem)
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "datamodel/dmelement.h"
#include "datamodel/idatamodel.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"
#include "datamodel/dmehandle.h"
#include "tier2/tier2.h"

class CNotifyTest : public IDmNotify
{
public:
	CNotifyTest() : m_nValueCount(0), m_nTopologyCount(0), m_nArrayCount(0) {}

	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
	{
		if ( nNotifyFlags & NOTIFY_CHANGE_ATTRIBUTE_VALUE )
		{
			m_nValueCount++;
		}
		if ( nNotifyFlags & NOTIFY_CHANGE_ATTRIBUTE_ARRAY_SIZE )
		{
			m_nArrayCount++;
		}
		if ( nNotifyFlags & NOTIFY_CHANGE_TOPOLOGICAL )
		{
			m_nTopologyCount++;
		}
	}

	int m_nTopologyCount;
	int m_nArrayCount;
	int m_nValueCount;
};


DEFINE_TESTCASE_NOSUITE( DmxNotifyTest )
{
	Msg( "Running dmx notify tests...\n" );

	CNotifyTest test1, test2;

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( "<RunNotifyTests>" );

	g_pDataModel->InstallNotificationCallback( &test1 );

	CDmElement *element = NULL;

	{
		CUndoScopeGuard guard( NOTIFY_SOURCE_APPLICATION, 0, "create" );
		element = CreateElement< CDmElement >( "test", fileid );
	}
	
	Shipping_Assert( test1.m_nTopologyCount == 1 );
	Shipping_Assert( test1.m_nArrayCount == 0 );

	g_pDataModel->Undo();

	Shipping_Assert( test1.m_nTopologyCount == 2 );
	Shipping_Assert( test1.m_nArrayCount == 0 );

	{
		CNotifyScopeGuard notify( "test1", NOTIFY_SOURCE_APPLICATION, 0, &test2 );
		CDisableUndoScopeGuard guard;
		element = CreateElement< CDmElement >( "test", fileid );
	}

	Shipping_Assert( test1.m_nTopologyCount == 3 );
	Shipping_Assert( test1.m_nArrayCount == 0 );
	Shipping_Assert( test2.m_nTopologyCount == 1 );
	Shipping_Assert( test2.m_nArrayCount == 0 );

	{
		CDisableUndoScopeGuard guard;

		// NOTE: Nested scope guards referring to the same callback shouldn't double call it
		CNotifyScopeGuard notify( "test2", NOTIFY_SOURCE_APPLICATION, 0, &test2 );
		{
			CNotifyScopeGuard notify( "test3", NOTIFY_SOURCE_APPLICATION, 0, &test2 );
			DestroyElement( element );
		}
	}

	Shipping_Assert( test1.m_nTopologyCount == 4 );
	Shipping_Assert( test1.m_nArrayCount == 0 );
	Shipping_Assert( test2.m_nTopologyCount == 2 );
	Shipping_Assert( test2.m_nArrayCount == 0 );

	{
		CUndoScopeGuard guard( NOTIFY_SOURCE_APPLICATION, 0, "create" );
		{
			element = CreateElement< CDmElement >( "test", fileid );
			element->SetValue( "test", 1.0f );
		}
		guard.Abort();
	}

	Shipping_Assert( test1.m_nTopologyCount == 4 );
	Shipping_Assert( test1.m_nArrayCount == 0 );
	Shipping_Assert( test2.m_nTopologyCount == 2 );
	Shipping_Assert( test2.m_nArrayCount == 0 );

	g_pDataModel->RemoveNotificationCallback( &test1 );
	g_pDataModel->RemoveFileId( fileid );
}
