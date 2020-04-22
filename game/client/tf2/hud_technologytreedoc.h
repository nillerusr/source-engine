//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( TF_TECHNOLOGYTREEDOC_H )
#define TF_TECHNOLOGYTREEDOC_H
#ifdef _WIN32
#pragma once
#endif

// Forward declarations
class CTechnologyTree;

//-----------------------------------------------------------------------------
// Purpose: Container for views into technology tree
//-----------------------------------------------------------------------------
class CTechnologyTreeDoc
{
public:
	// Construction
								CTechnologyTreeDoc( void );
	virtual						~CTechnologyTreeDoc( void );

	virtual void				Init( void );
	virtual void				ReloadTechTree( void );

	virtual void				LevelInit( void );
	virtual void				LevelShutdown( void );

	inline CTechnologyTree		*GetTechnologyTree() {return m_pTree;}

	void						AddTechnologyFile( char *sFilename );

	// Network input
	int							MsgFunc_Technology( bf_read &msg );
	int							MsgFunc_Resource( bf_read &msg );

private:
	// The underlying technology data tree
	CTechnologyTree				*m_pTree;
};

// Expose Document to rest of dll
extern CTechnologyTreeDoc& GetTechnologyTreeDoc();


#endif // TF_TECHNOLOGYTREEDOC_H