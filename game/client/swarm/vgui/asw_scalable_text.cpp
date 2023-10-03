#include "cbase.h"
#include <vgui_controls/Panel.h>
#include "asw_scalable_text.h"
#include <vgui/isurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheASWScalableText )
PRECACHE( MATERIAL, "vgui/letters/letter_a" )
PRECACHE( MATERIAL, "vgui/letters/letter_c" )
PRECACHE( MATERIAL, "vgui/letters/letter_d" )
PRECACHE( MATERIAL, "vgui/letters/letter_e" )
PRECACHE( MATERIAL, "vgui/letters/letter_f" )
PRECACHE( MATERIAL, "vgui/letters/letter_g" )
PRECACHE( MATERIAL, "vgui/letters/letter_i" )
PRECACHE( MATERIAL, "vgui/letters/letter_l" )
PRECACHE( MATERIAL, "vgui/letters/letter_m" )
PRECACHE( MATERIAL, "vgui/letters/letter_n" )
PRECACHE( MATERIAL, "vgui/letters/letter_o" )
PRECACHE( MATERIAL, "vgui/letters/letter_p" )
PRECACHE( MATERIAL, "vgui/letters/letter_s" )
PRECACHE( MATERIAL, "vgui/letters/letter_t" )
PRECACHE( MATERIAL, "vgui/letters/letter_a_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_c_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_d_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_e_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_f_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_g_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_i_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_l_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_m_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_n_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_o_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_p_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_s_glow" )
PRECACHE( MATERIAL, "vgui/letters/letter_t_glow" )
PRECACHE_REGISTER_END()

CASW_Scalable_Text* g_pScalableText = NULL;
CASW_Scalable_Text* ASWScalableText()
{
	if ( !g_pScalableText )
	{
		g_pScalableText = new CASW_Scalable_Text();
	}
	return g_pScalableText;
}

CASW_Scalable_Text::CASW_Scalable_Text()
{
	for ( int i = 0; i < NUM_SCALABLE_LETTERS; i++ )
	{
		m_bLetterSupported[i] = false;
		m_nLetterTextureID[i] = -1;
		m_nGlowLetterTextureID[i] = -1;
	}
	const char *szSupportedLetters = "ACDEFGILMNOPST";
	for ( int i = 0; i < Q_strlen( szSupportedLetters ); i++ )
	{
		m_bLetterSupported[ szSupportedLetters[i] - 'A' ] = true;
	}
	m_pszMaterialFilename[ 'A' - 'A' ] = "vgui/letters/letter_a";
	m_pszMaterialFilename[ 'C' - 'A' ] = "vgui/letters/letter_c";
	m_pszMaterialFilename[ 'D' - 'A' ] = "vgui/letters/letter_d";
	m_pszMaterialFilename[ 'E' - 'A' ] = "vgui/letters/letter_e";
	m_pszMaterialFilename[ 'F' - 'A' ] = "vgui/letters/letter_f";
	m_pszMaterialFilename[ 'G' - 'A' ] = "vgui/letters/letter_g";
	m_pszMaterialFilename[ 'I' - 'A' ] = "vgui/letters/letter_i";
	m_pszMaterialFilename[ 'L' - 'A' ] = "vgui/letters/letter_l";
	m_pszMaterialFilename[ 'M' - 'A' ] = "vgui/letters/letter_m";
	m_pszMaterialFilename[ 'N' - 'A' ] = "vgui/letters/letter_n";
	m_pszMaterialFilename[ 'O' - 'A' ] = "vgui/letters/letter_o";
	m_pszMaterialFilename[ 'P' - 'A' ] = "vgui/letters/letter_p";
	m_pszMaterialFilename[ 'S' - 'A' ] = "vgui/letters/letter_s";
	m_pszMaterialFilename[ 'T' - 'A' ] = "vgui/letters/letter_t";
	m_pszGlowMaterialFilename[ 'A' - 'A' ] = "vgui/letters/letter_a_glow";
	m_pszGlowMaterialFilename[ 'C' - 'A' ] = "vgui/letters/letter_c_glow";
	m_pszGlowMaterialFilename[ 'D' - 'A' ] = "vgui/letters/letter_d_glow";
	m_pszGlowMaterialFilename[ 'E' - 'A' ] = "vgui/letters/letter_e_glow";
	m_pszGlowMaterialFilename[ 'F' - 'A' ] = "vgui/letters/letter_f_glow";
	m_pszGlowMaterialFilename[ 'G' - 'A' ] = "vgui/letters/letter_g_glow";
	m_pszGlowMaterialFilename[ 'I' - 'A' ] = "vgui/letters/letter_i_glow";
	m_pszGlowMaterialFilename[ 'L' - 'A' ] = "vgui/letters/letter_l_glow";
	m_pszGlowMaterialFilename[ 'M' - 'A' ] = "vgui/letters/letter_m_glow";
	m_pszGlowMaterialFilename[ 'N' - 'A' ] = "vgui/letters/letter_n_glow";
	m_pszGlowMaterialFilename[ 'O' - 'A' ] = "vgui/letters/letter_o_glow";
	m_pszGlowMaterialFilename[ 'P' - 'A' ] = "vgui/letters/letter_p_glow";
	m_pszGlowMaterialFilename[ 'S' - 'A' ] = "vgui/letters/letter_s_glow";
	m_pszGlowMaterialFilename[ 'T' - 'A' ] = "vgui/letters/letter_t_glow";
}

CASW_Scalable_Text::~CASW_Scalable_Text()
{

}

int CASW_Scalable_Text::DrawSetLetterTexture( char ch, bool bGlow )
{
	// clamp requested character into the supported range
	if ( ch < 'A' || ch > 'Z' || !m_bLetterSupported[ ch - 'A' ] )
	{
		ch = 'A';
	}

	if ( bGlow )
	{
		if ( m_nGlowLetterTextureID[ ch - 'A' ] == -1 )
		{
			m_nGlowLetterTextureID[ ch - 'A' ] = vgui::surface()->CreateNewTextureID();
		}
		vgui::surface()->DrawSetTextureFile( m_nGlowLetterTextureID[ ch - 'A' ], m_pszGlowMaterialFilename[ ch - 'A' ], true, false );

		return m_nGlowLetterTextureID[ ch - 'A' ];
	}

	if ( m_nLetterTextureID[ ch - 'A' ] == -1 )
	{
		m_nLetterTextureID[ ch - 'A' ] = vgui::surface()->CreateNewTextureID();
	}
	vgui::surface()->DrawSetTextureFile( m_nLetterTextureID[ ch - 'A' ], m_pszMaterialFilename[ ch - 'A' ], true, false );

	return m_nLetterTextureID[ ch - 'A' ];
}