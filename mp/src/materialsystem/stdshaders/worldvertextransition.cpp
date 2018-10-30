//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"
#include "convar.h"

#include "lightmappedgeneric_dx9_helper.h"

#include "IDeferredExt.h"

static LightmappedGeneric_DX9_Vars_t s_info;

BEGIN_VS_SHADER( WorldVertexTransition_V2, "Help for WorldVertexTransition" )

	BEGIN_SHADER_PARAMS
		SHADER_PARAM( ALBEDO, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "albedo (Base texture with no baked lighting)" )
		SHADER_PARAM( SELFILLUMTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Self-illumination tint" )
		SHADER_PARAM( DETAIL, SHADER_PARAM_TYPE_TEXTURE, "shadertest/detail", "detail texture" )
		SHADER_PARAM( DETAILFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $detail" )
		SHADER_PARAM( DETAILSCALE, SHADER_PARAM_TYPE_FLOAT, "4", "scale of the detail texture" )

		// detail (multi-) texturing
		SHADER_PARAM( DETAILBLENDMODE, SHADER_PARAM_TYPE_INTEGER, "0", "mode for combining detail texture with base. 0=normal, 1= additive, 2=alpha blend detail over base, 3=crossfade" )
		SHADER_PARAM( DETAILBLENDFACTOR, SHADER_PARAM_TYPE_FLOAT, "1", "blend amount for detail texture." )
		SHADER_PARAM( DETAILTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "detail texture tint" )

		SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_env", "envmap" )
		SHADER_PARAM( ENVMAPFRAME, SHADER_PARAM_TYPE_INTEGER, "", "" )
		SHADER_PARAM( ENVMAPMASK, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_envmask", "envmap mask" )
		SHADER_PARAM( ENVMAPMASKFRAME, SHADER_PARAM_TYPE_INTEGER, "", "" )
		SHADER_PARAM( ENVMAPMASKTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$envmapmask texcoord transform" )
		SHADER_PARAM( ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "envmap tint" )
		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader1_normal", "bump map" )
		SHADER_PARAM( BUMPFRAME, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
		SHADER_PARAM( BUMPTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$bumpmap texcoord transform" )
		SHADER_PARAM( ENVMAPCONTRAST, SHADER_PARAM_TYPE_FLOAT, "0.0", "contrast 0 == normal 1 == color*color" )
		SHADER_PARAM( ENVMAPSATURATION, SHADER_PARAM_TYPE_FLOAT, "1.0", "saturation 0 == greyscale 1 == normal" )
		SHADER_PARAM( FRESNELREFLECTION, SHADER_PARAM_TYPE_FLOAT, "1.0", "1.0 == mirror, 0.0 == water" )
		SHADER_PARAM( NODIFFUSEBUMPLIGHTING, SHADER_PARAM_TYPE_INTEGER, "0", "0 == Use diffuse bump lighting, 1 = No diffuse bump lighting" )
		SHADER_PARAM( BUMPMAP2, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader3_normal", "bump map" )
		SHADER_PARAM( BUMPFRAME2, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $bumpmap" )
		SHADER_PARAM( BUMPTRANSFORM2, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$bumpmap texcoord transform" )
		SHADER_PARAM( BUMPMASK, SHADER_PARAM_TYPE_TEXTURE, "models/shadertest/shader1_normal", "bump map" )
		SHADER_PARAM( BASETEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "shadertest/detail", "detail texture" )
		SHADER_PARAM( FRAME2, SHADER_PARAM_TYPE_INTEGER, "0", "frame number for $basetexture2" )
		SHADER_PARAM( BASETEXTURENOENVMAP, SHADER_PARAM_TYPE_BOOL, "0", "" )
		SHADER_PARAM( BASETEXTURE2NOENVMAP, SHADER_PARAM_TYPE_BOOL, "0", "" )
		SHADER_PARAM( DETAIL_ALPHA_MASK_BASE_TEXTURE, SHADER_PARAM_TYPE_BOOL, "0",
			"If this is 1, then when detail alpha=0, no base texture is blended and when "
			"detail alpha=1, you get detail*base*lightmap" )
		SHADER_PARAM( LIGHTWARPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "light munging lookup texture" )
		SHADER_PARAM( BLENDMODULATETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "texture to use r/g channels for blend range for" )
		SHADER_PARAM( BLENDMASKTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$blendmodulatetexture texcoord transform" )
		SHADER_PARAM( MASKEDBLENDING, SHADER_PARAM_TYPE_INTEGER, "0", "blend using texture with no vertex alpha. For using texture blending on non-displacements" )
		SHADER_PARAM( SSBUMP, SHADER_PARAM_TYPE_INTEGER, "0", "whether or not to use alternate bumpmap format with height" )
		SHADER_PARAM( SEAMLESS_SCALE, SHADER_PARAM_TYPE_FLOAT, "0", "Scale factor for 'seamless' texture mapping. 0 means to use ordinary mapping" )

		SHADER_PARAM( PHONG_EXP, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( PHONG_EXP2, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "0.0", "" )
	END_SHADER_PARAMS

	void SetupVars( LightmappedGeneric_DX9_Vars_t& info )
	{
		info.m_nBaseTexture = BASETEXTURE;
		info.m_nBaseTextureFrame = FRAME;
		info.m_nBaseTextureTransform = BASETEXTURETRANSFORM;
		info.m_nAlbedo = ALBEDO;
		info.m_nSelfIllumTint = SELFILLUMTINT;

		info.m_nDetail = DETAIL;
		info.m_nDetailFrame = DETAILFRAME;
		info.m_nDetailScale = DETAILSCALE;
		info.m_nDetailTextureCombineMode = DETAILBLENDMODE;
		info.m_nDetailTextureBlendFactor = DETAILBLENDFACTOR;
		info.m_nDetailTint = DETAILTINT;

		info.m_nEnvmap = ENVMAP;
		info.m_nEnvmapFrame = ENVMAPFRAME;
		info.m_nEnvmapMask = ENVMAPMASK;
		info.m_nEnvmapMaskFrame = ENVMAPMASKFRAME;
		info.m_nEnvmapMaskTransform = ENVMAPMASKTRANSFORM;
		info.m_nEnvmapTint = ENVMAPTINT;
		info.m_nBumpmap = BUMPMAP;
		info.m_nBumpFrame = BUMPFRAME;
		info.m_nBumpTransform = BUMPTRANSFORM;
		info.m_nEnvmapContrast = ENVMAPCONTRAST;
		info.m_nEnvmapSaturation = ENVMAPSATURATION;
		info.m_nFresnelReflection = FRESNELREFLECTION;
		info.m_nNoDiffuseBumpLighting = NODIFFUSEBUMPLIGHTING;
		info.m_nBumpmap2 = BUMPMAP2;
		info.m_nBumpFrame2 = BUMPFRAME2;
		info.m_nBaseTexture2 = BASETEXTURE2;
		info.m_nBaseTexture2Frame = FRAME2;
		info.m_nBumpTransform2 = BUMPTRANSFORM2;
		info.m_nBumpMask = BUMPMASK;
		info.m_nBaseTextureNoEnvmap = BASETEXTURENOENVMAP;
		info.m_nBaseTexture2NoEnvmap = BASETEXTURE2NOENVMAP;
		info.m_nDetailAlphaMaskBaseTexture = DETAIL_ALPHA_MASK_BASE_TEXTURE;
		info.m_nFlashlightTexture = FLASHLIGHTTEXTURE;
		info.m_nFlashlightTextureFrame = FLASHLIGHTTEXTUREFRAME;
		info.m_nLightWarpTexture = LIGHTWARPTEXTURE;
		info.m_nBlendModulateTexture = BLENDMODULATETEXTURE;
		info.m_nBlendMaskTransform = BLENDMASKTRANSFORM;
		info.m_nMaskedBlending = MASKEDBLENDING;
		info.m_nSelfShadowedBumpFlag = SSBUMP;
		info.m_nSeamlessMappingScale = SEAMLESS_SCALE;
		info.m_nAlphaTestReference = -1;
	}

	void SetupParmsGBuffer( defParms_gBuffer &p )
	{
		p.bModel = false;

		p.iAlbedo = BASETEXTURE;
		p.iAlbedo2 = BASETEXTURE2;
		p.iBumpmap = BUMPMAP;
		p.iBumpmap2 = BUMPMAP2;
		p.iSSBump = SSBUMP;

		p.iPhongExp = PHONG_EXP;
		p.iPhongExp2 = PHONG_EXP2;

		p.iAlphatestRef = ALPHATESTREFERENCE;
	}

	void SetupParmsShadow( defParms_shadow &p )
	{
		p.bModel = false;
		p.iAlbedo = BASETEXTURE;
		p.iAlbedo2 = BASETEXTURE2;

		p.iAlphatestRef = ALPHATESTREFERENCE;
	}

	bool DrawToGBuffer( IMaterialVar **params )
	{
		const bool bIsDecal = IS_FLAG_SET( MATERIAL_VAR_DECAL );
		const bool bTranslucent = IS_FLAG_SET( MATERIAL_VAR_TRANSLUCENT );

		return !bTranslucent && !bIsDecal;
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT_PARAMS()
	{
		SetupVars( s_info );
		InitParamsLightmappedGeneric_DX9( this, params, pMaterialName, s_info );

		if ( GetDeferredExt()->IsDeferredLightingEnabled() )
		{
			defParms_gBuffer parms_gbuffer;
			SetupParmsGBuffer( parms_gbuffer );
			InitParmsGBuffer( parms_gbuffer, this, params );

			defParms_shadow parms_shadow;
			SetupParmsShadow( parms_shadow );
			InitParmsShadowPass( parms_shadow, this, params );
		}
	}

	SHADER_INIT
	{
		SetupVars( s_info );
		InitLightmappedGeneric_DX9( this, params, s_info );

		if ( GetDeferredExt()->IsDeferredLightingEnabled() )
		{
			defParms_gBuffer parms_gbuffer;
			SetupParmsGBuffer( parms_gbuffer );
			InitPassGBuffer( parms_gbuffer, this, params );

			defParms_shadow parms_shadow;
			SetupParmsShadow( parms_shadow );
			InitPassShadowPass( parms_shadow, this, params );
		}
	}

	SHADER_DRAW
	{
		bool bDrawComposite = true;
		const bool bDeferredActive = GetDeferredExt()->IsDeferredLightingEnabled();
		if ( bDeferredActive )
		{
			if ( IsSnapshotting() )
				pShaderShadow->EnableCulling( false );

			if ( pShaderAPI != NULL && *pContextDataPtr == NULL )
				*pContextDataPtr = new CLightmappedGeneric_DX9_Context();

			CDeferredPerMaterialContextData *pDefContext = reinterpret_cast< CDeferredPerMaterialContextData* >( *pContextDataPtr );

			const int iDeferredRenderStage = pShaderAPI ? pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE ) : DEFERRED_RENDER_STAGE_INVALID;

			const bool bDrawToGBuffer = DrawToGBuffer( params );

			Assert( pShaderAPI == NULL || iDeferredRenderStage != DEFERRED_RENDER_STAGE_INVALID );

			if ( bDrawToGBuffer )
			{
				if ( pShaderShadow != NULL || iDeferredRenderStage == DEFERRED_RENDER_STAGE_GBUFFER )
				{
					defParms_gBuffer parms_gbuffer;
					SetupParmsGBuffer( parms_gbuffer );
					DrawPassGBuffer( parms_gbuffer, this, params, pShaderShadow, pShaderAPI,
									 vertexCompression, pDefContext );
				}
				else
					Draw( false );

				if ( pShaderShadow != NULL || iDeferredRenderStage == DEFERRED_RENDER_STAGE_SHADOWPASS )
				{
					defParms_shadow parms_shadow;
					SetupParmsShadow( parms_shadow );
					DrawPassShadowPass( parms_shadow, this, params, pShaderShadow, pShaderAPI,
										vertexCompression, pDefContext );
				}
				else
					Draw( false );
			}

			bDrawComposite = ( pShaderShadow != NULL || iDeferredRenderStage == DEFERRED_RENDER_STAGE_COMPOSITION );

			if ( IsSnapshotting() && ( params[FLAGS]->GetIntValue() & MATERIAL_VAR_NOCULL ) == 0 )
				pShaderShadow->EnableCulling( true );
		}

		if ( pShaderShadow != NULL || bDrawComposite )
			DrawLightmappedGeneric_DX9( this, params, pShaderAPI, pShaderShadow, s_info, pContextDataPtr, bDeferredActive );
		else
			Draw( false );
	}
END_SHADER

