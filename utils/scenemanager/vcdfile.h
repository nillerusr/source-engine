//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VCDFILE_H
#define VCDFILE_H
#ifdef _WIN32
#pragma once
#endif

class CSoundEntry;
class CScene;

#include "itreeitem.h"
#include "ichoreoeventcallback.h"

class CChoreoScene;

class CVCDFile : public ITreeItem, public IChoreoEventCallback
{
public:
	CVCDFile( CScene *scene, char const *filename );
	~CVCDFile();

	CScene		*GetOwnerScene();

	char const *GetName() const;
	char const *GetComments();
	void		SetComments( char const *comments );

	int			GetSoundEntryCount() const;
	CSoundEntry	*GetSoundEntry( int index );

	void		ValidateTree( mxTreeView *tree, mxTreeViewItem* parent );

	// ITreeItem
	virtual CWorkspace	*GetWorkspace() { return NULL; }
	virtual CProject	*GetProject() { return NULL; }
	virtual CScene		*GetScene() { return NULL; }
	virtual CVCDFile	*GetVCDFile() { return this; }
	virtual CSoundEntry	*GetSoundEntry() { return NULL; }
	virtual CWaveFile	*GetWaveFile() { return NULL; }

	// IChoreoEventCallback stubs
	virtual void StartEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event ) {}
	// Only called for events with HasEndTime() == true
	virtual void EndEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event ) {}
	// Called for events which have been started but aren't done yet
	virtual void ProcessEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event ) {}
	// Called for events that are part of a pause condition
	virtual bool CheckEvent( float currenttime, CChoreoScene *scene, CChoreoEvent *event ) { return false; }

	virtual void Checkout( bool updatestateicons = true );
	virtual void Checkin( bool updatestateicons = true );

	bool		IsCheckedOut() const;
	int			GetIconIndex() const;

	virtual void MoveChildUp( ITreeItem *child );
	virtual void MoveChildDown( ITreeItem *child );

	void		SetDirty( bool dirty );

	virtual bool		IsChildFirst( ITreeItem *child );
	virtual bool		IsChildLast( ITreeItem *child );

private:

	CChoreoScene		*LoadScene( char const *filename );

	void				LoadSoundsFromScene( CChoreoScene *scene );

	enum
	{
		MAX_VCD_NAME = 128,
	};

	char	m_szName[ MAX_VCD_NAME ];
	char	*m_pszComments;

	CUtlVector< CSoundEntry * >	m_Sounds;

	CChoreoScene	*m_pScene;

	CScene			*m_pOwner;
};

#endif // VCDFILE_H
