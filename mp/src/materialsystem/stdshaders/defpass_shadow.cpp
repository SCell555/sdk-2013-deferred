
#include "deferred_includes.h"

#include "shadowpass_vs30.inc"
#include "shadowpass_ps30.inc"

#include "tier0/memdbgon.h"

static CCommandBufferBuilder< CFixedCommandStorageBuffer< 512 > > tmpBuf;

void InitParmsShadowPass( const defParms_shadow &info, CBaseVSShader *pShader, IMaterialVar **params )
{
	if ( PARM_NO_DEFAULT( info.iAlphatestRef ) ||
		PARM_VALID( info.iAlphatestRef ) && PARM_FLOAT( info.iAlphatestRef ) == 0.0f )
		params[ info.iAlphatestRef ]->SetFloatValue( DEFAULT_ALPHATESTREF );

	InitIntParam( info.iTreeSway, params, 0 );
	InitFloatParam( info.iTreeSwayHeight, params, 1000.0f );
	InitFloatParam( info.iTreeSwayStartHeight, params, 0.1f );
	InitFloatParam( info.iTreeSwayRadius, params, 300.0f );
	InitFloatParam( info.iTreeSwayStartRadius, params, 0.2f );
	InitFloatParam( info.iTreeSwaySpeed, params, 1.0f );
	InitFloatParam( info.iTreeSwaySpeedHighWindMultiplier, params, 2.0f );
	InitFloatParam( info.iTreeSwayStrength, params, 10.0f );
	InitFloatParam( info.iTreeSwayScrumbleSpeed, params, 5.0f );
	InitFloatParam( info.iTreeSwayScrumbleStrength, params, 10.0f );
	InitFloatParam( info.iTreeSwayScrumbleFrequency, params, 12.0f );
	InitFloatParam( info.iTreeSwayFalloffExp, params, 1.5f );
	InitFloatParam( info.iTreeSwayScrumbleFalloffExp, params, 1.0f );
	InitFloatParam( info.iTreeSwaySpeedLerpStart, params, 3.0f );
	InitFloatParam( info.iTreeSwaySpeedLerpEnd, params, 6.0f );
}

void InitPassShadowPass( const defParms_shadow &info, CBaseVSShader *pShader, IMaterialVar **params )
{
	if ( PARM_DEFINED( info.iAlbedo ) )
		pShader->LoadTexture( info.iAlbedo );

	if ( PARM_DEFINED( info.iAlbedo2 ) )
		pShader->LoadTexture( info.iAlbedo2 );
}

void DrawPassShadowPass( const defParms_shadow &info, CBaseVSShader *pShader, IMaterialVar **params,
	IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI,
	VertexCompressionType_t vertexCompression, CDeferredPerMaterialContextData *pDeferredContext )
{
	const bool bModel = info.bModel;
	const bool bIsDecal = IS_FLAG_SET( MATERIAL_VAR_DECAL );
	const bool bFastVTex = g_pHardwareConfig->HasFastVertexTextures();
	const bool bNoCull = IS_FLAG_SET( MATERIAL_VAR_NOCULL );

	const bool bAlbedo = PARM_TEX( info.iAlbedo );
	const bool bAlbedo2 = PARM_TEX( info.iAlbedo2 );
	const bool bAlphatest = IS_FLAG_SET( MATERIAL_VAR_ALPHATEST ) && bAlbedo;

	const int nTreeSwayMode = clamp( GetIntParam( info.iTreeSway, params, 0 ), 0, 2 );
	const bool bTreeSway = nTreeSwayMode != 0;

	SHADOW_STATE
	{
		pShaderShadow->SetDefaultState();

		pShaderShadow->EnableSRGBWrite( false );

		if ( bNoCull )
		{
			pShaderShadow->EnableCulling( false );
		}

		int iVFmtFlags = VERTEX_POSITION | VERTEX_NORMAL;
		int iUserDataSize = 0;

		int *pTexCoordDim;
		int iTexCoordNum;
		GetTexcoordSettings( ( bModel && bIsDecal && bFastVTex ),
							 iTexCoordNum, &pTexCoordDim );

		if ( bModel )
		{
			iVFmtFlags |= VERTEX_FORMAT_COMPRESSED;
		}

#ifndef DEFCFG_ENABLE_RADIOSITY
		if ( bAlphatest )
#endif
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			if ( bAlbedo2 )
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
		}

#if DEFCFG_BILATERAL_DEPTH_TEST
		pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
#endif

		pShaderShadow->VertexShaderVertexFormat( iVFmtFlags, iTexCoordNum, pTexCoordDim, iUserDataSize );

		DECLARE_STATIC_VERTEX_SHADER( shadowpass_vs30 );
		SET_STATIC_VERTEX_SHADER_COMBO( MODEL, bModel );
		SET_STATIC_VERTEX_SHADER_COMBO( MORPHING_VTEX, bModel && bFastVTex );
		SET_STATIC_VERTEX_SHADER_COMBO( MULTITEXTURE, bAlbedo2 );
		SET_STATIC_VERTEX_SHADER_COMBO( TREESWAY, nTreeSwayMode );
		SET_STATIC_VERTEX_SHADER( shadowpass_vs30 );

		DECLARE_STATIC_PIXEL_SHADER( shadowpass_ps30 );
		SET_STATIC_PIXEL_SHADER_COMBO( ALPHATEST, bAlphatest );
		SET_STATIC_PIXEL_SHADER_COMBO( MULTITEXTURE, bAlbedo2 );
		SET_STATIC_PIXEL_SHADER( shadowpass_ps30 );
	}
	DYNAMIC_STATE
	{
		Assert( pDeferredContext != NULL );
		const int shadowMode = pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_MODE );
		const int radiosityOutput = DEFCFG_ENABLE_RADIOSITY != 0
			&& pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_RADIOSITY );

		if ( pDeferredContext->m_bMaterialVarsChanged || !pDeferredContext->HasCommands( CDeferredPerMaterialContextData::DEFSTAGE_SHADOW ) )
		{
			tmpBuf.Reset();

			if ( bAlphatest )
			{
				PARM_VALIDATE( info.iAlphatestRef );

				tmpBuf.SetPixelShaderConstant1( 0, PARM_FLOAT( info.iAlphatestRef ) );
#ifndef DEFCFG_ENABLE_RADIOSITY
				tmpBuf.BindTexture( pShader, SHADER_SAMPLER0, info.iAlbedo );
#endif
			}

#if DEFCFG_ENABLE_RADIOSITY
			if ( bAlbedo )
				tmpBuf.BindTexture( pShader, SHADER_SAMPLER0, info.iAlbedo );
			else
				tmpBuf.BindStandardTexture( SHADER_SAMPLER0, TEXTURE_WHITE );

			if ( bAlbedo2 )
				tmpBuf.BindTexture( pShader, SHADER_SAMPLER1, info.iAlbedo2 );
#endif

			if ( bTreeSway )
			{
				float flParams[4];
				flParams[0] = GetFloatParam( info.iTreeSwaySpeedHighWindMultiplier, params, 2.0f );
				flParams[1] = GetFloatParam( info.iTreeSwayScrumbleFalloffExp, params, 1.0f );
				flParams[2] = GetFloatParam( info.iTreeSwayFalloffExp, params, 1.0f );
				flParams[3] = GetFloatParam( info.iTreeSwayScrumbleSpeed, params, 3.0f );
				tmpBuf.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, flParams );

				flParams[0] = GetFloatParam( info.iTreeSwayHeight, params, 1000.0f );
				flParams[1] = GetFloatParam( info.iTreeSwayStartHeight, params, 0.1f );
				flParams[2] = GetFloatParam( info.iTreeSwayRadius, params, 300.0f );
				flParams[3] = GetFloatParam( info.iTreeSwayStartRadius, params, 0.2f );
				tmpBuf.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, flParams );

				flParams[0] = GetFloatParam( info.iTreeSwaySpeed, params, 1.0f );
				flParams[1] = GetFloatParam( info.iTreeSwayStrength, params, 10.0f );
				flParams[2] = GetFloatParam( info.iTreeSwayScrumbleFrequency, params, 12.0f );
				flParams[3] = GetFloatParam( info.iTreeSwayScrumbleStrength, params, 10.0f );
				tmpBuf.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, flParams );

				flParams[0] = GetFloatParam( info.iTreeSwaySpeedLerpStart, params, 3.0f );
				flParams[1] = GetFloatParam( info.iTreeSwaySpeedLerpEnd, params, 6.0f );
				tmpBuf.SetVertexShaderConstant( 219, flParams );
			}

			tmpBuf.End();

			pDeferredContext->SetCommands( CDeferredPerMaterialContextData::DEFSTAGE_SHADOW, tmpBuf.Copy() );
		}

		pShaderAPI->SetDefaultState();

#if DEFCFG_BILATERAL_DEPTH_TEST
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, GetDeferredExt()->GetWorldToCameraDepthTexBase(), 4, true );
#endif

		if ( bModel && bFastVTex )
			pShader->SetHWMorphVertexShaderState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_10, VERTEX_SHADER_SHADER_SPECIFIC_CONST_11, SHADER_VERTEXTEXTURE_SAMPLER0 );

		DECLARE_DYNAMIC_VERTEX_SHADER( shadowpass_vs30 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (bModel && (int)vertexCompression) ? 1 : 0 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, (bModel && pShaderAPI->GetCurrentNumBones() > 0) ? 1 : 0 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( MORPHING, (bModel && pShaderAPI->IsHWMorphingEnabled()) ? 1 : 0 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( SHADOW_MODE, shadowMode );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( RADIOSITY, radiosityOutput );
		SET_DYNAMIC_VERTEX_SHADER( shadowpass_vs30 );

		DECLARE_DYNAMIC_PIXEL_SHADER( shadowpass_ps30 );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( SHADOW_MODE, shadowMode );
		SET_DYNAMIC_PIXEL_SHADER_COMBO( RADIOSITY, radiosityOutput );
		SET_DYNAMIC_PIXEL_SHADER( shadowpass_ps30 );

		if ( bModel && bFastVTex )
		{
			bool bUnusedTexCoords[3] = { false, true, !pShaderAPI->IsHWMorphingEnabled() || !bIsDecal };
			pShaderAPI->MarkUnusedVertexFields( 0, 3, bUnusedTexCoords );
		}

		if ( bTreeSway )
		{
			float fTempConst[4];
			fTempConst[0] = 0; // unused
			fTempConst[1] = pShaderAPI->CurrentTime();
			const Vector& windDir = pShaderAPI->GetVectorRenderingParameter( VECTOR_RENDERPARM_WIND_DIRECTION );
			fTempConst[2] = windDir.x;
			fTempConst[3] = windDir.y;
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_4, fTempConst );
		}

		pShaderAPI->ExecuteCommandBuffer( pDeferredContext->GetCommands( CDeferredPerMaterialContextData::DEFSTAGE_SHADOW ) );

		switch ( shadowMode )
		{
		case DEFERRED_SHADOW_MODE_ORTHO:
			{
				CommitShadowcastingConstants_Ortho( pShaderAPI,
					pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_INDEX ),
					VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, VERTEX_SHADER_SHADER_SPECIFIC_CONST_1,
					VERTEX_SHADER_SHADER_SPECIFIC_CONST_2
					);
			}
			break;
		case DEFERRED_SHADOW_MODE_PROJECTED:
			{
				CommitShadowcastingConstants_Proj( pShaderAPI,
					pShaderAPI->GetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_INDEX ),
					VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, VERTEX_SHADER_SHADER_SPECIFIC_CONST_1,
					VERTEX_SHADER_SHADER_SPECIFIC_CONST_2
					);
			}
			break;
		}

#ifdef SHADOWMAPPING_USE_COLOR
		CommitViewVertexShader( pShaderAPI, VERTEX_SHADER_SHADER_SPECIFIC_CONST_7 );
#endif
	}

	pShader->Draw();
}