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


//=============================================================================
//							TREE-ENUMERATION PROCEDURES
//=============================================================================

#define ASSERT_AND_ABORT(f, sz)							\
	if (!(f))											\
	{													\
		ASSERT_MBOX(FALSE, sz);							\
		cleanup( );										\
		return TREE_ABORT;								\
	}


// Integer constants for this class
enum
	{
	MAX_NAME_CHARS			= 70,
	UNDESIRABLE_NODE_MARKER	= -7777
	};

// For keeping info about each (non-ignored) 3dsMax node in the tree
typedef struct
{
	char		szNodeName[MAX_NAME_CHARS];	// usefull for lookups
	Matrix3		mat3NodeTM;					// node's transformation matrix (at time zero)
	Matrix3		mat3ObjectTM;				// object-offset transformation matrix (at time zero)
	int			imaxnodeParent;				// cached index of parent node
} MaxNode;

typedef struct
{
	float		flDist;
	float		flWeight;
} MaxVertWeight;

//===================================================================
// Class that implements the scene-export.
//
class VWeightExportClass : public SceneExport
{
	friend BOOL CALLBACK ExportOptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	friend class CollectModelTEP;

public:
	CONSTRUCTOR				VWeightExportClass	(void);
	DESTRUCTOR				~VWeightExportClass	(void);

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
	virtual int		DoExport(const TCHAR *name,ExpInterface *ei,Interface *i, BOOL suppressPrompts=FALSE,DWORD options=0); // Export	file

	MaxNode		m_MaxNode[512];		// array of nodes
	long		m_cMaxNode;		// # of nodes

	MaxVertWeight *m_MaxVertex[10000];
	long		m_cMaxVertex;

	// Animation metrics (gleaned from 3dsMax and cached for convenience)
	Interval	m_intervalOfAnimation;
	TimeValue	m_tvStart;
	TimeValue	m_tvEnd;
	int			m_tpf;		// ticks-per-frame

private:
	void					CollectNodes( INode *pnode );
	BOOL					CollectModel( ExpInterface *pexpiface );
};


//===================================================================
// Basically just a ClassFactory for communicating with 3DSMAX.
//
class VWeightExportClassDesc : public ClassDesc
{
public:
	int				IsPublic		(void)					{ return TRUE;								}
	void *			Create			(BOOL loading=FALSE)	{ return new VWeightExportClass;				}
	const TCHAR *	ClassName		(void)					{ return _T("VWeightExport");					}
	SClass_ID 		SuperClassID	(void)					{ return SCENE_EXPORT_CLASS_ID;				}
	Class_ID 		ClassID			(void)					{ return Class_ID(0x554b1092, 0x206a444f);	}
	const TCHAR *	Category		(void)					{ return _T("");							}
};


//===================================================================
// Tree Enumeration Callback
//		Just counts the nodes in the node tree
//
class CountNodesTEP : public ITreeEnumProc
{
public:
	virtual int				callback(INode *node);
	int						m_cNodes;		// running count of nodes
};


//===================================================================
// Tree Enumeration Callback
//		Collects the nodes in the tree into the global array
//
class CollectNodesTEP : public ITreeEnumProc
{
public:
	virtual int				callback(INode *node);
	VWeightExportClass			*m_phec;
};



//===================================================================
// Tree Enumeration Callback
//		Dumps the triangle meshes to a file.
//
class CollectModelTEP : public ITreeEnumProc
{
public:
	virtual int				callback(INode *node);
	void					cleanup(void);
	// FILE					*m_pfile;		// write to this file
	TimeValue				m_tvToDump;		// dump snapshot at this frame time
	VWeightExportClass			*m_phec;
	IPhyContextExport		*m_mcExport;
	IPhysiqueExport			*m_phyExport;
    Modifier				*m_phyMod;
	Modifier				*m_bonesProMod;
	BonesPro_WeightArray	*m_wa;
private:
	int						CollectWeights( int iVertex, MaxVertWeight *pweight );
};


//===================================================================
// Prototype declarations
//
void ResetINodeMap( void );
int BuildINodeMap(INode *pnode);
void AddINode( INode *pnode );
int GetIndexOfNodeName( TCHAR *szNodeName, BOOL fAssertPropExists = TRUE );
int GetIndexOfINode( INode *pnode, BOOL fAssertPropExists = TRUE);

BOOL FUndesirableNode( INode *pnode);
BOOL FNodeMarkedToSkip( INode *pnode);
Modifier *FindPhysiqueModifier( INode *nodePtr );
Modifier *FindBonesProModifier( INode *nodePtr );
int AssertFailedFunc(char *sz);
#define ASSERT_MBOX(f, sz) ((f) ? 1 : AssertFailedFunc(sz))
void GetUnifiedCoord( Point3 &p1, MaxVertWeight pweight[], MaxNode maxNode[], int cMaxNode );

int GetBoneWeights( IPhyContextExport *mcExport, int iVertex, MaxVertWeight *pweight);
int GetBoneWeights( Modifier * bonesProMod, int iVertex, MaxVertWeight *pweight);
void SetBoneWeights( Modifier * bonesProMod, int iVertex, MaxVertWeight *pweight);

