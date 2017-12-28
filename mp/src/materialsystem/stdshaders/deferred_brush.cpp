
#include "deferred_includes.h"

#include "tier0/memdbgon.h"

// DEFINE_FALLBACK_SHADER( WorldVertexTransition, DEFERRED_BRUSH )
// DEFINE_FALLBACK_SHADER( LightmappedGeneric, DEFERRED_BRUSH )

BEGIN_VS_SHADER( DEFERRED_BRUSH, "" )
	BEGIN_SHADER_PARAMS

		SHADER_PARAM( BUMPMAP, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( SSBUMP, SHADER_PARAM_TYPE_BOOL, "", "" )

		SHADER_PARAM( LITFACE, SHADER_PARAM_TYPE_BOOL, "0", "" )

		SHADER_PARAM( PHONG_SCALE, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( PHONG_EXP, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( PHONG_EXP2, SHADER_PARAM_TYPE_FLOAT, "", "" )
		SHADER_PARAM( PHONG_MAP, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( PHONG_FRESNEL, SHADER_PARAM_TYPE_BOOL, "", "" )

		SHADER_PARAM( FRESNELRANGES, SHADER_PARAM_TYPE_VEC3, "", "" )
		SHADER_PARAM( ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "", "" )

		SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_env", "envmap" )
		SHADER_PARAM( ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "envmap tint" )
		SHADER_PARAM( ENVMAPCONTRAST, SHADER_PARAM_TYPE_FLOAT, "0.0", "contrast 0 == normal 1 == color*color" )
		SHADER_PARAM( ENVMAPSATURATION, SHADER_PARAM_TYPE_FLOAT, "1.0", "saturation 0 == greyscale 1 == normal" )
		SHADER_PARAM( ENVMAPMASK, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_envmask", "envmap mask" )

		SHADER_PARAM( SELFILLUMTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Self-illumination tint" )
		SHADER_PARAM( SELFILLUM_ENVMAPMASK_ALPHA, SHADER_PARAM_TYPE_BOOL, "0", "defines that self illum value comes from env map mask alpha" )
		SHADER_PARAM( SELFILLUMFRESNEL, SHADER_PARAM_TYPE_BOOL, "0", "Self illum fresnel" )
		SHADER_PARAM( SELFILLUMMASK, SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", "If we bind a texture here, it overrides base alpha (if any) for self illum" )

		SHADER_PARAM( BASETEXTURE2, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( BUMPMAP2, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( ENVMAPMASK2, SHADER_PARAM_TYPE_TEXTURE, "shadertest/shadertest_envmask", "envmap mask" )
		SHADER_PARAM( BLENDMODULATETEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "texture to use r/g channels for blend range for" )
		SHADER_PARAM( BLENDMASKTRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "$blendmodulatetexture texcoord transform" )

		SHADER_PARAM( ENVMAPPARALLAX, SHADER_PARAM_TYPE_MATRIX, "[1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1]", "" )
		SHADER_PARAM( ENVMAPORIGIN, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "The world space position of the env_cubemap being corrected" )

	END_SHADER_PARAMS

	void SetupParmsGBuffer( defParms_gBuffer &p )
	{
		p.bModel = false;

		p.iAlbedo = BASETEXTURE;
#if DEFCFG_DEFERRED_SHADING == 1
		p.iAlbedo2 = BASETEXTURE2;
#endif
		p.iBumpmap = BUMPMAP;
		p.iBumpmap2 = BUMPMAP2;

		p.iPhongmap = PHONG_MAP;

		p.iAlphatestRef = ALPHATESTREFERENCE;
		p.iLitface = LITFACE;
		p.iPhongExp = PHONG_EXP;
		p.iPhongExp2 = PHONG_EXP2;
		p.iSSBump = SSBUMP;

		p.iBlendmodulate = BLENDMODULATETEXTURE;
		p.iBlendmodulateTransform = BLENDMASKTRANSFORM;
	}

	void SetupParmsShadow( defParms_shadow &p )
	{
		p.bModel = false;
		p.iAlbedo = BASETEXTURE;
		p.iAlbedo2 = BASETEXTURE2;

		p.iAlphatestRef = ALPHATESTREFERENCE;
	}

	void SetupParmsComposite( defParms_composite &p )
	{
		p.bModel = false;
		p.iAlbedo = BASETEXTURE;
		p.iAlbedo2 = BASETEXTURE2;

		p.iEnvmap = ENVMAP;
		p.iEnvmapMask = ENVMAPMASK;
		p.iEnvmapMask2 = ENVMAPMASK2;
		p.iEnvmapTint = ENVMAPTINT;
		p.iEnvmapContrast = ENVMAPCONTRAST;
		p.iEnvmapSaturation = ENVMAPSATURATION;

		p.iSelfIllumTint = SELFILLUMTINT;
		p.iSelfIllumMaskInEnvmapAlpha = SELFILLUM_ENVMAPMASK_ALPHA;
		p.iSelfIllumFresnelModulate = SELFILLUMFRESNEL;
		p.iSelfIllumMask = SELFILLUMMASK;

		p.iAlphatestRef = ALPHATESTREFERENCE;

		p.iPhongScale = PHONG_SCALE;
		p.iPhongFresnel = PHONG_FRESNEL;

		p.iBlendmodulate = BLENDMODULATETEXTURE;
		p.iBlendmodulateTransform = BLENDMASKTRANSFORM;

		p.iFresnelRanges = FRESNELRANGES;

		p.iEnvmapParallax = ENVMAPPARALLAX;
		p.iEnvmapOrigin = ENVMAPORIGIN;
	}

	bool DrawToGBuffer( IMaterialVar **params )
	{
#if DEFCFG_DEFERRED_SHADING == 1
		return true;
#else
		const bool bIsDecal = IS_FLAG_SET( MATERIAL_VAR_DECAL );
		const bool bTranslucent = IS_FLAG_SET( MATERIAL_VAR_TRANSLUCENT );

		return !bTranslucent && !bIsDecal;
#endif
	}

	SHADER_INIT_PARAMS()
	{
		// for fallback shaders
		// SWARMS VBSP BETTER FUCKING CALL MODINIT NOW
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_LIGHTMAP );
		if ( PARM_DEFINED( BUMPMAP ) )
			SET_FLAGS2( MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP );

		const bool bDrawToGBuffer = DrawToGBuffer( params );

		if ( bDrawToGBuffer )
		{
			defParms_gBuffer parms_gbuffer;
			SetupParmsGBuffer( parms_gbuffer );
			InitParmsGBuffer( parms_gbuffer, this, params );
		
			defParms_shadow parms_shadow;
			SetupParmsShadow( parms_shadow );
			InitParmsShadowPass( parms_shadow, this, params );
		}

		defParms_composite parms_composite;
		SetupParmsComposite( parms_composite );
		InitParmsComposite( parms_composite, this, params );
	}

	SHADER_INIT
	{
		const bool bDrawToGBuffer = DrawToGBuffer( params );

		if ( bDrawToGBuffer )
		{
			defParms_gBuffer parms_gbuffer;
			SetupParmsGBuffer( parms_gbuffer );
			InitPassGBuffer( parms_gbuffer, this, params );

			defParms_shadow parms_shadow;
			SetupParmsShadow( parms_shadow );
			InitPassShadowPass( parms_shadow, this, params );
		}

		defParms_composite parms_composite;
		SetupParmsComposite( parms_composite );
		InitPassComposite( parms_composite, this, params );
	}

	SHADER_FALLBACK
	{
		/*if ( !GetDeferredExt()->IsDeferredLightingEnabled() )
			return "LightmappedGeneric";

		const bool bTranslucent = IS_FLAG_SET( MATERIAL_VAR_TRANSLUCENT );
		const bool bIsDecal = IS_FLAG_SET( MATERIAL_VAR_DECAL );

		if ( bTranslucent && !bIsDecal )
			return "LightmappedGeneric";*/

		return 0;
	}

	SHADER_DRAW
	{
		if ( pShaderAPI != NULL && *pContextDataPtr == NULL )
			*pContextDataPtr = new CDeferredPerMaterialContextData();

		CDeferredPerMaterialContextData *pDefContext = reinterpret_cast< CDeferredPerMaterialContextData* >(*pContextDataPtr);

		const int iDeferredRenderStage = pShaderAPI ?
			pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE )
			: DEFERRED_RENDER_STAGE_INVALID;

		const bool bDrawToGBuffer = DrawToGBuffer( params );

		Assert( pShaderAPI == NULL ||
			iDeferredRenderStage != DEFERRED_RENDER_STAGE_INVALID );

		if ( bDrawToGBuffer )
		{
			if ( pShaderShadow != NULL ||
				iDeferredRenderStage == DEFERRED_RENDER_STAGE_GBUFFER )
			{
				defParms_gBuffer parms_gbuffer;
				SetupParmsGBuffer( parms_gbuffer );
				DrawPassGBuffer( parms_gbuffer, this, params, pShaderShadow, pShaderAPI,
					vertexCompression, pDefContext );
			}
			else
				Draw(false);

			if ( pShaderShadow != NULL ||
				iDeferredRenderStage == DEFERRED_RENDER_STAGE_SHADOWPASS )
			{
				defParms_shadow parms_shadow;
				SetupParmsShadow( parms_shadow );
				DrawPassShadowPass( parms_shadow, this, params, pShaderShadow, pShaderAPI,
					vertexCompression, pDefContext );
			}
			else
				Draw(false);
		}

#if ( DEFCFG_DEFERRED_SHADING == 0 )
		if ( pShaderShadow != NULL ||
			iDeferredRenderStage == DEFERRED_RENDER_STAGE_COMPOSITION )
		{
			defParms_composite parms_composite;
			SetupParmsComposite( parms_composite );
			DrawPassComposite( parms_composite, this, params, pShaderShadow, pShaderAPI,
				vertexCompression, pDefContext );
		}
		else
			Draw(false);
#endif

		if ( pShaderAPI != NULL && pDefContext->m_bMaterialVarsChanged )
			pDefContext->m_bMaterialVarsChanged = false;
	}

END_SHADER