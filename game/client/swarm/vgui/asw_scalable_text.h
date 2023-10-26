#ifndef _INCLUDED_ASW_SCALABLE_TEXT_H
#define _INCLUDED_ASW_SCALABLE_TEXT_H
#ifdef _WIN32
#pragma once
#endif

#define NUM_SCALABLE_LETTERS 26

class CASW_Scalable_Text
{
public:
	CASW_Scalable_Text();
	virtual ~CASW_Scalable_Text();

	int DrawSetLetterTexture( char ch, bool bGlow );

	bool m_bLetterSupported[ NUM_SCALABLE_LETTERS ];
	int m_nLetterTextureID[ NUM_SCALABLE_LETTERS ];
	const char *m_pszMaterialFilename[ NUM_SCALABLE_LETTERS ];

	int m_nGlowLetterTextureID[ NUM_SCALABLE_LETTERS ];
	const char *m_pszGlowMaterialFilename[ NUM_SCALABLE_LETTERS ];
};

CASW_Scalable_Text* ASWScalableText();

#endif // _INCLUDED_ASW_SCALABLE_TEXT_H














