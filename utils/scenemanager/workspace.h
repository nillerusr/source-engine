//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WORKSPACE_H
#define WORKSPACE_H
#ifdef _WIN32
#pragma once
#endif

class CProject;
class mxTreeView;

#include "itreeitem.h"

class CWorkspace : public ITreeItem
{
public:
	CWorkspace( char const *filename );
	~CWorkspace();

	char const	*GetName() const;
	char const	*GetFileName() const { return m_szFile; }

	bool		IsDirty( void ) const;
	void		SetDirty( bool dirty );

	int			GetProjectCount() const;
	CProject	*GetProject( int index ) const;
	void		AddProject( CProject *project );
	CProject	*FindProjectFile( char const *filename ) const;
	void		RemoveProject( CProject *project );

	void		ValidateTree( mxTreeView *tree, mxTreeViewItem *parent );

	bool		CanClose( void );

	void		SaveChanges();

	virtual CWorkspace	*GetWorkspace() { return this; }
	virtual CProject	*GetProject() { return NULL; }
	virtual CScene		*GetScene() { return NULL; }
	virtual CVCDFile	*GetVCDFile() { return NULL; }
	virtual CSoundEntry	*GetSoundEntry() { return NULL; }
	virtual CWaveFile	*GetWaveFile() { return NULL; }


	char const	*GetVSSUserName() const;
	char const	*GetVSSProject() const;

	void		SetVSSUserName( char const *username );
	void		SetVSSProject( char const *projectname );

	virtual void Checkout( bool updatestateicons = true );
	virtual void Checkin( bool updatestateicons = true );

	bool		IsCheckedOut() const;
	int			GetIconIndex() const;

	virtual void MoveChildUp( ITreeItem *item );
	virtual void MoveChildDown( ITreeItem *item );

	virtual bool		IsChildFirst( ITreeItem *child );
	virtual bool		IsChildLast( ITreeItem *child );

private:

	void LoadFromFile();
	void SaveToFile();

	enum
	{
		MAX_WORKSPACE_NAME = 128,
		MAX_WORKSPACE_FILENAME = 256
	};

	char		m_szName[ MAX_WORKSPACE_NAME ];
	char		m_szFile[ MAX_WORKSPACE_FILENAME ];

	char		m_szVSSUserName[ MAX_WORKSPACE_NAME ];
	char		m_szVSSProject[ MAX_WORKSPACE_NAME ];

	bool		m_bDirty;

	CUtlVector< CProject * >	m_Projects;
};

#endif // WORKSPACE_H
