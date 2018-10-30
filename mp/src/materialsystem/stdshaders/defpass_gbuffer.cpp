
#include "deferred_includes.h"

#include "gbuffer_vs30.inc"
#include "gbuffer_ps30.inc"

#include "tier0/memdbgon.h"

static CCommandBufferBuilder< CFixedCommandStorageBuffer< 512 > > tmpBuf;

void InitParmsGBuffer( const defParms_gBuffer &info, CBaseVSShader *pShader, IMaterialVar **params )
{
	if ( PARM_NO_DEFAULT( info.iAlphatestRef ) ||
		( PARM_VALID( info.iAlphatestRef ) && PARM_FLOAT( info.iAlphatestRef ) == 0.0f ) )
		params[ info.iAlphatestRef ]->SetFloatValue( DEFAULT_ALPHATESTREF );

	PARM_INIT_FLOAT( info.iPhongExp, DEFAULT_PHONG_EXP );
	PARM_INIT_FLOAT( info.iPhongExp2, DEFAULT_PHONG_EXP );

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

void InitPassGBuffer( const defParms_gBuffer &info, CBaseVSShader *pShader, IMaterialVar **params )
{
	if ( PARM_DEFINED( info.iBumpmap ) )
		pShader->LoadBumpMap( info.iBumpmap );

	if ( PARM_DEFINED( info.iBumpmap2 ) )
		pShader->LoadBumpMap( info.iBumpmap2 );

	if ( PARM_DEFINED( info.iBlendmodulate ) )
		pShader->LoadTexture( info.iBlendmodulate );

	if ( PARM_DEFINED( info.iAlbedo ) )
		pShader->LoadTexture( info.iAlbedo );

#if DEFCFG_DEFERRED_SHADING == 1
	if ( PARM_DEFINED( info.iAlbedo2 ) )
		pShader->LoadTexture( info.iAlbedo2 );
#endif

	if ( PARM_DEFINED( info.iPhongmap ) )
		pShader->LoadTexture( info.iPhongmap );
}

void DrawPassGBuffer( const defParms_gBuffer &info, CBaseVSShader *pShader, IMaterialVar **params,
	IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI,
	VertexCompressionType_t vertexCompression, CDeferredPerMaterialContextData *pDeferredContext )
{
	const bool bDeferredShading = false;

	const bool bModel = info.bModel;
	const bool bIsDecal = IS_FLAG_SET( MATERIAL_VAR_DECAL );
	const bool bFastVTex = g_pHardwareConfig->HasFastVertexTextures();
	const bool bNoCull = IS_FLAG_SET( MATERIAL_VAR_NOCULL );

	const bool bAlbedo = PARM_TEX( info.iAlbedo );
	const bool bAlbedo2 = bDeferredShading && PARM_TEX( info.iAlbedo2 );
	const bool bBumpmap = PARM_TEX( info.iBumpmap );
	const bool bBumpmap2 = bBumpmap && PARM_TEX( info.iBumpmap2 );
	const bool bPhongmap = PARM_TEX( info.iPhongmap );

	const bool bBlendmodulate = ( bAlbedo2 || bBumpmap2 ) && PARM_TEX( info.iBlendmodulate );

	const int nTreeSwayMode = clamp( GetIntParam( info.iTreeSway, params, 0 ), 0, 2 );
	const bool bTreeSway = nTreeSwayMode != 0;

	const bool bAlphatest = IS_FLAG_SET( MATERIAL_VAR_ALPHATEST ) && bAlbedo;
	const bool bTranslucent = IS_FLAG_SET( MATERIAL_VAR_TRANSLUCENT ) && bAlbedo && bIsDecal;
	const bool bSSBump = bBumpmap && PARM_SET( info.iSSBump );

	Assert( !bIsDecal || bDeferredShading );
	Assert( !bTranslucent || bDeferredShading );

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
		else
		{
			if ( bBumpmap2 || bAlbedo2 )
				iVFmtFlags |= VERTEX_COLOR;
		}

		if ( bAlphatest || bDeferredShading )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, false );
		}

		if ( bBumpmap )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, false );

			if ( bModel )
				iUserDataSize = 4;
			else
			{
				iVFmtFlags |= VERTEX_TANGENT_SPACE;
			}
		}

		if ( bPhongmap )
		{
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, false );
		}

		if ( bAlbedo2 || bBumpmap2 )
			{
			pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
			if ( bAlbedo2 )
				pShaderShadow->EnableTexture( SHADER_SAMPLER9, true );

			if ( bBlendmodulate )
				pShaderShadow->EnableTexture( SHADER_SAMPLER4, true );
		}

		pShaderShadow->EnableAlphaWrites( true );

		pShaderShadow->VertexShaderVertexFormat( iVFmtFlags, iTexCoordNum, pTexCoordDim, iUserDataSize );

		if ( bTranslucent )
		{
			pShader->EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
		}

		DECLARE_STATIC_VERTEX_SHADER( gbuffer_vs30 );
		SET_STATIC_VERTEX_SHADER_COMBO( MODEL, bModel );
		SET_STATIC_VERTEX_SHADER_COMBO( MORPHING_VTEX, bModel && bFastVTex );
		SET_STATIC_VERTEX_SHADER_COMBO( TANGENTSPACE, bBumpmap );
		SET_STATIC_VERTEX_SHADER_COMBO( BUMPMAP2, bBumpmap2 );
		SET_STATIC_VERTEX_SHADER_COMBO( BLENDMODULATE, bBlendmodulate );
		SET_STATIC_VERTEX_SHADER_COMBO( TREESWAY, nTreeSwayMode );
		SET_STATIC_VERTEX_SHADER( gbuffer_vs30 );

#if DEFCFG_DEFERRED_SHADING == 1
		DECLARE_STATIC_PIXEL_SHADER( gbuffer_defshading_ps30 );
#else
		DECLARE_STATIC_PIXEL_SHADER( gbuffer_ps30 );
		SET_STATIC_PIXEL_SHADER_COMBO( BUMPMAP2, bBumpmap2 );
#endif
		SET_STATIC_PIXEL_SHADER_COMBO( ALPHATEST, bAlphatest );
		SET_STATIC_PIXEL_SHADER_COMBO( BUMPMAP, bBumpmap ? bSSBump ? 2 : 1 : 0 );
		SET_STATIC_PIXEL_SHADER_COMBO( NOCULL, bNoCull );
		SET_STATIC_PIXEL_SHADER_COMBO( PHONGMAP, bPhongmap );
		SET_STATIC_PIXEL_SHADER_COMBO( BLENDMODULATE, bBlendmodulate );
#if DEFCFG_DEFERRED_SHADING == 1
		SET_STATIC_PIXEL_SHADER_COMBO( TWOTEXTURE, (bAlbedo2 || bBumpmap2) );
		SET_STATIC_PIXEL_SHADER_COMBO( DECAL, bIsDecal );
		SET_STATIC_PIXEL_SHADER( gbuffer_defshading_ps30 );
#else
		SET_STATIC_PIXEL_SHADER( gbuffer_ps30 );
#endif
	}
	DYNAMIC_STATE
	{
		Assert( pDeferredContext != NULL );

		if ( pDeferredContext->m_bMaterialVarsChanged || !pDeferredContext->HasCommands( CDeferredPerMaterialContextData::DEFSTAGE_GBUFFER ) )
		{
			tmpBuf.Reset();

			if ( bAlphatest )
			{
				PARM_VALIDATE( info.iAlphatestRef );

				tmpBuf.SetPixelShaderConstant1( 0, PARM_FLOAT( info.iAlphatestRef ) );
			}

			if ( bAlphatest || bDeferredShading )
			{
				if ( bAlbedo )
					tmpBuf.BindTexture( pShader, SHADER_SAMPLER0, info.iAlbedo );
				else
					tmpBuf.BindStandardTexture( SHADER_SAMPLER0, TEXTURE_GREY );
			}

			if ( bBumpmap )
				tmpBuf.BindTexture( pShader, SHADER_SAMPLER1, info.iBumpmap );

			if ( bPhongmap )
				tmpBuf.BindTexture( pShader, SHADER_SAMPLER2, info.iPhongmap );
			else
			{
				float flPhongExp[2] = { 0 };
				flPhongExp[0] = clamp( PARM_FLOAT( info.iPhongExp ), 0, 1 ) * 63.0f;

				if ( bBumpmap2 || bAlbedo2 )
				{
					PARM_VALIDATE( info.iPhongExp2 );

					flPhongExp[1] = clamp( PARM_FLOAT( info.iPhongExp2 ), 0, 1 ) * 63.0f;
					tmpBuf.SetPixelShaderConstant2( 2, flPhongExp[0], flPhongExp[1] );
				}
				else
					tmpBuf.SetPixelShaderConstant1( 2, flPhongExp[0] );
			}

			if ( bAlbedo2 || bBumpmap2 )
			{
				if ( bBumpmap2 )
					tmpBuf.BindTexture( pShader, SHADER_SAMPLER3, info.iBumpmap2 );
				else
					tmpBuf.BindStandardTexture( SHADER_SAMPLER3, TEXTURE_NORMALMAP_FLAT );

				if ( bAlbedo2 )
					tmpBuf.BindTexture( pShader, SHADER_SAMPLER9, info.iAlbedo2 );
				else
					tmpBuf.BindStandardTexture( SHADER_SAMPLER9, TEXTURE_GREY );

				if ( bBlendmodulate )
				{
					tmpBuf.SetVertexShaderTextureTransform( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3, info.iBlendmodulateTransform );
					tmpBuf.BindTexture( pShader, SHADER_SAMPLER4, info.iBlendmodulate );
				}
			}

			if ( bTreeSway )
			{
				float flParams[4];
				flParams[0] = GetFloatParam( info.iTreeSwaySpeedHighWindMultiplier, params, 2.0f );
				flParams[1] = GetFloatParam( info.iTreeSwayScrumbleFalloffExp, params, 1.0f );
				flParams[2] = GetFloatParam( info.iTreeSwayFalloffExp, params, 1.0f );
				flParams[3] = GetFloatParam( info.iTreeSwayScrumbleSpeed, params, 3.0f );
				tmpBuf.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_5, flParams );

				flParams[0] = GetFloatParam( info.iTreeSwayHeight, params, 1000.0f );
				flParams[1] = GetFloatParam( info.iTreeSwayStartHeight, params, 0.1f );
				flParams[2] = GetFloatParam( info.iTreeSwayRadius, params, 300.0f );
				flParams[3] = GetFloatParam( info.iTreeSwayStartRadius, params, 0.2f );
				tmpBuf.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, flParams );

				flParams[0] = GetFloatParam( info.iTreeSwaySpeed, params, 1.0f );
				flParams[1] = GetFloatParam( info.iTreeSwayStrength, params, 10.0f );
				flParams[2] = GetFloatParam( info.iTreeSwayScrumbleFrequency, params, 12.0f );
				flParams[3] = GetFloatParam( info.iTreeSwayScrumbleStrength, params, 10.0f );
				tmpBuf.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, flParams );

				flParams[0] = GetFloatParam( info.iTreeSwaySpeedLerpStart, params, 3.0f );
				flParams[1] = GetFloatParam( info.iTreeSwaySpeedLerpEnd, params, 6.0f );
				tmpBuf.SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_9, flParams );
			}

			tmpBuf.SetPixelShaderConstant2( 1,
				IS_FLAG_SET( MATERIAL_VAR_HALFLAMBERT ) ? 1.0f : 0.0f,
				PARM_SET( info.iLitface ) ? 1.0f : 0.0f );

			tmpBuf.End();

			pDeferredContext->SetCommands( CDeferredPerMaterialContextData::DEFSTAGE_GBUFFER, tmpBuf.Copy() );
		}

		pShaderAPI->SetDefaultState();

		if ( bModel && bFastVTex )
			pShader->SetHWMorphVertexShaderState( VERTEX_SHADER_SHADER_SPECIFIC_CONST_10, VERTEX_SHADER_SHADER_SPECIFIC_CONST_11, SHADER_VERTEXTEXTURE_SAMPLER0 );

		DECLARE_DYNAMIC_VERTEX_SHADER( gbuffer_vs30 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( COMPRESSED_VERTS, (bModel && (int)vertexCompression) ? 1 : 0 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( SKINNING, (bModel && pShaderAPI->GetCurrentNumBones() > 0) ? 1 : 0 );
		SET_DYNAMIC_VERTEX_SHADER_COMBO( MORPHING, (bModel && pShaderAPI->IsHWMorphingEnabled()) ? 1 : 0 );
		SET_DYNAMIC_VERTEX_SHADER( gbuffer_vs30 );

#if DEFCFG_DEFERRED_SHADING == 1
		DECLARE_DYNAMIC_PIXEL_SHADER( gbuffer_defshading_ps30 );
		SET_DYNAMIC_PIXEL_SHADER( gbuffer_defshading_ps30 );
#else
		DECLARE_DYNAMIC_PIXEL_SHADER( gbuffer_ps30 );
		SET_DYNAMIC_PIXEL_SHADER( gbuffer_ps30 );
#endif

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
			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, fTempConst );
		}

		float vPos[4] = {0,0,0,0};
		pShaderAPI->GetWorldSpaceCameraPosition( vPos );
		float zScale[4] = {GetDeferredExt()->GetZScale(),0,0,0};
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, vPos );
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_1, GetDeferredExt()->GetForwardBase() );
		pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_2, zScale );

		pShader->LoadViewMatrixIntoVertexShaderConstant( VERTEX_SHADER_AMBIENT_LIGHT );

		pShaderAPI->ExecuteCommandBuffer( pDeferredContext->GetCommands( CDeferredPerMaterialContextData::DEFSTAGE_GBUFFER ) );
	}

	pShader->Draw();
}