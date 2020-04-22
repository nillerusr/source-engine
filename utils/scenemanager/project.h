//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PROJECT_H
#define PROJECT_H
#ifdef _WIN32
#pragma once
#endif

class CScene;
class mxTreeView;
class CWorkspace;

#include "itreeitem.h"

class CProject : public ITreeItem
{
public:
	CProject( CWorkspace *ws, char const *filename );
	~CProject();

	CWorkspace	*GetOwnerWorkspace();

	char const	*GetName() const;
	char const	*GetFileName() const;


	bool		IsDirty( void ) const;
	void		SetDirty( bool dirty );

	void		SetComments( char const *comments );
	char const	*GetComments( void ) const;

	int			GetSceneCount() const;
	CScene		*GetScene( int index ) const;
	void		AddScene( CScene *scene );
	void		RemoveScene( CScene *scene );

	void		ValidateTree( mxTreeView *tree, mxTreeViewItem* parent );

	void		SaveChanges();

	virtual CWorkspace	*GetWorkspace() { return NULL; }
	virtual CProject	*GetProject() { return this; }
	virtual CScene		*GetScene() { return NULL; }
	virtual CVCDFile	*GetVCDFile() { return NULL; }
	virtual CSoundEntry	*GetSoundEntry() { return NULL; }
	virtual CWaveFile	*GetWaveFile() { return NULL; }

	virtual void Checkout( bool updatestateicons = true );
	virtual void Checkin( bool updatestateicons = true );

	bool		IsCheckedOut() const;
	int			GetIconIndex() const;

	virtual void MoveChildUp( ITreeItem *child );
	virtual void MoveChildDown( ITreeItem *child );

	virtual bool		IsChildFirst( ITreeItem *child );
	virtual bool		IsChildLast( ITreeItem *child );

private:

	void		LoadFromFile();
	void		SaveToFile();

	enum
	{
		MAX_PROJECT_NAME = 128,
		MAX_PROJECT_FILENAME = 256
	};

	char		m_szName[ MAX_PROJECT_NAME ];
	char		m_szFile[ MAX_PROJECT_FILENAME ];

	char		*m_pszComments;

	bool		m_bDirty;

	CUtlVector< CScene * >	m_Scenes;

	CWorkspace	*m_pOwner;
};

#endif // PROJECT_H
