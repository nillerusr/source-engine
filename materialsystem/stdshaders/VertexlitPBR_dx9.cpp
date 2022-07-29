//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// Example shader that can be applied to models
//
//==================================================================================================

#include "BaseVSShader.h"
#include "convar.h"
#include "vertexlitpbr_dx9_helper.h"

#ifdef STDSHADER
BEGIN_VS_SHADER(VertexLitPBR,
	"Help for VertexLitPBR")
#else
BEGIN_VS_SHADER(VertexLitPBR,
	"Help for VertexLitPBR")
#endif

BEGIN_SHADER_PARAMS
SHADER_PARAM(ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0.0", "")
SHADER_PARAM(ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_env", "envmap")
SHADER_PARAM(BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader1_normal", "bump map")

SHADER_PARAM(BRDF, SHADER_PARAM_TYPE_TEXTURE, "models/PBRTest/BRDF", "")
SHADER_PARAM(NOISE, SHADER_PARAM_TYPE_TEXTURE, "shaders/bluenoise", "")
SHADER_PARAM(ROUGHNESS, SHADER_PARAM_TYPE_TEXTURE, "", "")
SHADER_PARAM(METALLIC, SHADER_PARAM_TYPE_TEXTURE, "", "")
SHADER_PARAM(AO, SHADER_PARAM_TYPE_TEXTURE, "", "")
SHADER_PARAM(EMISSIVE, SHADER_PARAM_TYPE_TEXTURE, "", "")
SHADER_PARAM(LIGHTMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "lightmap texture--will be bound by the engine")

SHADER_PARAM(USESMOOTHNESS, SHADER_PARAM_TYPE_BOOL, "0", "Invert roughness")
END_SHADER_PARAMS

void SetupVars(VertexLitPBR_DX9_Vars_t& info)
{
	info.m_nBaseTexture = BASETEXTURE;
	info.m_nBaseTextureFrame = FRAME;
	info.m_nBaseTextureTransform = BASETEXTURETRANSFORM;
	info.m_nAlphaTestReference = ALPHATESTREFERENCE;
	info.m_nRoughness = ROUGHNESS;
	info.m_nMetallic = METALLIC;
	info.m_nAO = AO;
	info.m_nEmissive = EMISSIVE;
	info.m_nEnvmap = ENVMAP;
	info.m_nBumpmap = BUMPMAP;
	info.m_nFlashlightTexture = FLASHLIGHTTEXTURE;
	info.m_nFlashlightTextureFrame = FLASHLIGHTTEXTUREFRAME;
	info.m_nBRDF = BRDF;
	info.m_nUseSmoothness = USESMOOTHNESS;
	info.m_nLightmap = LIGHTMAP;
}

SHADER_INIT_PARAMS()
{
	VertexLitPBR_DX9_Vars_t info;
	SetupVars(info);
	InitParamsVertexLitPBR_DX9(this, params, pMaterialName, info);
}

SHADER_FALLBACK
{
	return 0;
}

SHADER_INIT
{
	VertexLitPBR_DX9_Vars_t info;
	SetupVars(info);
	InitVertexLitPBR_DX9(this, params, info);
}

SHADER_DRAW
{
	bool hasFlashlight = UsingFlashlight(params);
	
	VertexLitPBR_DX9_Vars_t info;
	SetupVars(info);
	DrawVertexLitPBR_DX9(this, params, pShaderAPI, pShaderShadow, hasFlashlight, info, vertexCompression, pContextDataPtr);

}

END_SHADER

