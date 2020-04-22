//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// PakDoc.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPakDoc document

typedef struct
{ char magic[4];	// Name of the new WAD format
  long diroffset;   // Position of WAD directory from start of file
  long dirsize;     // Number of entries * 0x40 (64 char)
} pakheader_t;

typedef struct
{ char filename[50];       // Name of the file, Unix style, with extension,
                               // 50 chars, padded with '\0'.
  long offset;                 // Position of the entry in PACK file
  long size;                   // Size of the entry in PACK file
} pakentry_t;

class CPakDoc : public CDocument
{
protected:
	CPakDoc();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CPakDoc)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPakDoc)
	public:
	virtual void Serialize(CArchive& ar);   // overridden for document i/o
	protected:
	virtual BOOL OnNewDocument();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CPakDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CPakDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
