//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Definition of the info_remarkable entity.
//			This entity is a quick and dirty hack to provide writers 
//			with some kind of object that characters can remark upon.
//			It is not performant, because it relies upon each character
//			polling over each of these for visibility.  A better approach
//			will be an object that is notified by the engine when it is within
//			a character's view.
//
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "inforemarkable.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


/// A global list of entities that can be remarked upon.
/// @todo This is an awful, brute force solution. Fix it.
class CRemarkableEntityList : public CAutoGameSystem
{
public:
	CRemarkableEntityList( char const *name ) : CAutoGameSystem( name )
	{}

	virtual void LevelShutdownPostEntity()  
	{ 
		m_list.Purge();
	}

	void AddEntity( CInfoRemarkable *pItem )
	{
		m_list.AddToTail( pItem );
	}

	void RemoveEntity( CInfoRemarkable *pItem )
	{
		m_list.FindAndRemove( pItem );
	}
	CUtlLinkedList< CInfoRemarkable * > m_list;
};

CRemarkableEntityList g_RemarkableList( "CRemarkableEntityList" );


CUtlLinkedList< CInfoRemarkable * > *CInfoRemarkable::GetListOfAllThatIsRemarkable( void )
{
	return &g_RemarkableList.m_list;
}

CInfoRemarkable::~CInfoRemarkable()
{
	g_RemarkableList.RemoveEntity( this );
}

void CInfoRemarkable::Spawn( void )
{
	g_RemarkableList.AddEntity(this);
	m_iTimesRemarkedUpon = 0;
}



//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( info_remarkable, CInfoRemarkable );


//--------------------------------------------------------------------------------------------------------
BEGIN_DATADESC( CInfoRemarkable )
DEFINE_KEYFIELD( m_szRemarkContext,	FIELD_STRING, "contextsubject" ),

END_DATADESC()
