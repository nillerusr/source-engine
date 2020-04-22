//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

//===================================================================
// Useful macros
//
#define CONSTRUCTOR
#define DESTRUCTOR

#define EXPORT_THIS		__declspec(dllexport)

#define DEFAULT_EXT		_T("vvw")

#define FStrEq(sz1, sz2) (strcmp((sz1), (sz2)) == 0)


//===================================================================
// Class that implements the scene-export.
//
class VWeightImportClass : public SceneImport
{
	// friend BOOL CALLBACK ImportOptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	// friend class DumpModelTEP;
	// friend class DumpDeformsTEP;

public:
	CONSTRUCTOR				VWeightImportClass	(void);
	DESTRUCTOR				~VWeightImportClass	(void);

	// Required by classes derived from SceneExport
	virtual int				ExtCount		(void)		{ return 1;						}
	virtual const TCHAR*	Ext				(int i)		{ return DEFAULT_EXT;			}
	virtual const TCHAR*	LongDesc		(void)		{ return _T("Valve Skeletal Model Exporter for 3D Studio Max");	}
	virtual const TCHAR*	ShortDesc		(void)		{ return _T("Valve VVW");			}
	virtual const TCHAR*	AuthorName		(void)		{ return _T("Valve, LLC");			}
	virtual const TCHAR*	CopyrightMessage(void)		{ return _T("Copyright (c) 1998, Valve LLC");			}
	virtual const TCHAR*	OtherMessage1	(void)		{ return _T("");				}
	virtual const TCHAR*	OtherMessage2	(void)		{ return _T("");				}
	virtual unsigned int	Version			(void)		{ return 201;					}
	virtual void			ShowAbout		(HWND hWnd)	{ return;						}
	// virtual int				DoExport		(const TCHAR *name, ExpInterface *ei, Interface *i);
	virtual int		DoImport(const TCHAR *name,ImpInterface *ii,Interface *i, BOOL suppressPrompts=FALSE); // Export	file

	MaxNode		m_SrcMaxNode[512];		// array of nodes
	long		m_cSrcMaxNodes;		// # of nodes

	MaxVertWeight *m_SrcMaxVertex[10000];
	long		m_cSrcMaxVertex;

	MaxNode		m_MaxNode[512];		// array of nodes
	long		m_cMaxNodes;		// # of nodes

	int			m_iToSrc[512];	// translate src nodes to local bones

	// Animation metrics (gleaned from 3dsMax and cached for convenience)
	Interval	m_intervalOfAnimation;
	TimeValue	m_tvStart;
	TimeValue	m_tvEnd;
	int			m_tpf;		// ticks-per-frame

private:
	void		CollectNodes( INode *pnode );
	void		NodeEnum( INode *node );
	int			UpdateModel( INode *node );
	void		CollectWeights(int iVertex, MaxVertWeight *pweight);
	void		RemapSrcWeights( void );

	IPhyContextExport		*m_mcExport;
	IPhysiqueExport			*m_phyExport;
    Modifier				*m_phyMod;
	Modifier				*m_bonesProMod;
	BonesPro_WeightArray	*m_wa;

};


//===================================================================
// Basically just a ClassFactory for communicating with 3DSMAX.
//
class VWeightImportClassDesc : public ClassDesc
{
public:
	int				IsPublic		(void)					{ return TRUE;								}
	void *			Create			(BOOL loading=FALSE)	{ return new VWeightImportClass;				}
	const TCHAR *	ClassName		(void)					{ return _T("VWeightImport");					}
	SClass_ID 		SuperClassID	(void)					{ return SCENE_IMPORT_CLASS_ID;				}
	Class_ID 		ClassID			(void)					{ return Class_ID(0x554b1092, 0x206a444f);	}
	const TCHAR *	Category		(void)					{ return _T("");							}
};


//===================================================================
// Tree Enumeration Callback
//		Collects the nodes in the tree into the global array
//
class UpdateNodesTEP : public ITreeEnumProc
{
public:
	virtual int				callback(INode *node);
	VWeightImportClass		*m_phec;
};


//===================================================================
// Tree Enumeration Callback
//		Dumps the triangle meshes to a file.
//
class UpdateModelTEP : public ITreeEnumProc
{
public:
	virtual int				callback(INode *node);
	void					cleanup(void);
	// FILE					*m_pfile;		// write to this file
	TimeValue				m_tvToDump;		// dump snapshot at this frame time
	VWeightImportClass		*m_phec;
	IPhyContextExport		*m_mcExport;
	IPhysiqueExport			*m_phyExport;
    Modifier				*m_phyMod;
	Modifier				*m_bonesProMod;
	BonesPro_WeightArray	*m_wa;
private:
	void					CollectWeights( int iVertex, MaxVertWeight *pweight );
};


