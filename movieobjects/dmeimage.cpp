//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeimage.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "bitmap/imageformat.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeImage, CDmeImage );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeImage::OnConstruction()
{
	m_Width.Init( this, "width" );
	m_Height.Init( this, "height" );
	m_Format.Init( this, "format" );
	m_Bits.Init( this, "bits" );
}

void CDmeImage::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Image format
//-----------------------------------------------------------------------------
ImageFormat CDmeImage::Format() const
{
	return (ImageFormat)( m_Format.Get() );
}

const char *CDmeImage::FormatName() const
{
	return ImageLoader::GetName( Format() );
}

