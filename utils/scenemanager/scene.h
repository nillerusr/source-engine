//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SCENE_H
#define SCENE_H
#ifdef _WIN32
#pragma once
#endif

class CVCDFile;
class CProject;

#include "itreeitem.h"

class CScene : public ITreeItem
{
public:
				CScene( CProject *proj, char const *name );
				~CScene();

	CProject	*GetOwnerProject();

	void		SetComments( char const *comments );
	char const	*GetComments( void ) const;
	char const	*GetName() const;

	int			GetVCDCount() const;
	CVCDFile	*GetVCD( int index );
	void		AddVCD( CVCDFile *vcd );
	void		RemoveVCD( CVCDFile *vcd );
	CVCDFile	*FindVCD( char const *filename );

	void		ValidateTree( mxTreeView *tree, mxTreeViewItem* parent );

	virtual CWorkspace	*GetWorkspace() { return NULL; }
	virtual CProject	*GetProject() { return NULL; }
	virtual CScene		*GetScene() { return this; }
	virtual CVCDFile	*GetVCDFile() { return NULL; }
	virtual CSoundEntry	*GetSoundEntry() { return NULL; }
	virtual CWaveFile	*GetWaveFile() { return NULL; }

	bool		IsCheckedOut() const;
	int			GetIconIndex() const;

	virtual void Checkout( bool updatestateicons = true );
	virtual void Checkin( bool updatestateicons = true );

	virtual void MoveChildUp( ITreeItem *child );
	virtual void MoveChildDown( ITreeItem *child );

	void		SetDirty( bool dirty )
	{
	}

	virtual bool		IsChildFirst( ITreeItem *child );
	virtual bool		IsChildLast( ITreeItem *child );

private:
	enum
	{
		MAX_SCENE_NAME = 128,
	};

	char	m_szName[ MAX_SCENE_NAME ];
	char	*m_pszComments;

	CUtlVector< CVCDFile * >	m_Files;

	CProject			*m_pOwner;
};

#endif // SCENE_H
