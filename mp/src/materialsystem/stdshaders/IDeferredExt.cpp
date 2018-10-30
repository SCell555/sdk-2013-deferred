
#include "deferred_includes.h"

CDeferredExtension __g_defExt;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CDeferredExtension, IDeferredExtension, DEFERRED_EXTENSION_VERSION, __g_defExt );

CDeferredExtension::CDeferredExtension()
{
	m_bDefLightingEnabled = false;

	m_vecOrigin.Init();
	m_vecForward.Init();
	m_flZDists[0] = m_flZDists[1] = m_flZDists[2] = 0;
	m_matTFrustumD.Identity();

	m_pTexNormals = NULL;
	m_pTexDepth = NULL;
	m_pTexLightAccum = NULL;
	m_pTexLightAccum2 = NULL;
	m_pTexLightCtrl = NULL;

	Q_memset( m_pTexShadowDepth_Ortho, 0, sizeof( ITexture* ) * MAX_SHADOW_ORTHO );
	Q_memset( m_pTexShadowDepth_DP, 0, sizeof( ITexture* ) * MAX_SHADOW_DP );
	Q_memset( m_pTexShadowDepth_Proj, 0, sizeof( ITexture* ) * MAX_SHADOW_PROJ );
	Q_memset( m_pTexCookie, 0, sizeof( ITexture* ) * NUM_COOKIE_SLOTS );
	m_pTexVolumePrePass = NULL;
	Q_memset( m_pTexShadowRad_Ortho, 0, sizeof( ITexture* ) * 2 );
	Q_memset( m_pTexRadBuffer, 0, sizeof( ITexture* ) * 2 );
	Q_memset( m_pTexRadNormal, 0, sizeof( ITexture* ) * 2 );

	m_pflCommonLightData = NULL;
	m_iCommon_NumRows = 0;
	m_iNumCommon_ShadowedCookied = 0;
	m_iNumCommon_Shadowed = 0;
	m_iNumCommon_Cookied = 0;
	m_iNumCommon_Simple = 0;
}

CDeferredExtension::~CDeferredExtension()
{
}


void CDeferredExtension::EnableDeferredLighting()
{
	m_bDefLightingEnabled = true;
}

bool CDeferredExtension::IsDeferredLightingEnabled() const
{
	return m_bDefLightingEnabled;
}

bool CDeferredExtension::IsRadiosityEnabled() const
{
	static ConVarRef refRadiosity( "deferred_radiosity_enable" );

	Assert( refRadiosity.IsValid() );

	return refRadiosity.GetBool();
}

void CDeferredExtension::CommitCommonData( const Vector& origin,
										   const Vector& fwd,
										   const float& zNear, const float& zFar,
										   const VMatrix& matTFrustum
#if DEFCFG_BILATERAL_DEPTH_TEST
										   , const VMatrix &matWorldCameraDepthTex
#endif
																							)
{
	m_vecOrigin.Init( origin );
	m_vecForward.Init( fwd );
	m_flZDists[0] = zNear;
	m_flZDists[1] = zFar;
	m_matTFrustumD = matTFrustum;
#if DEFCFG_BILATERAL_DEPTH_TEST
	m_matWorldCameraDepthTex = matWorldCameraDepthTex;
#endif
}

void CDeferredExtension::CommitZScale( const float &zFar )
{
	m_flZDists[2] = zFar;
}

void CDeferredExtension::CommitShadowData_Ortho( const int &index, const shadowData_ortho_t &data )
{
	Assert( index >= 0 && index < SHADOW_NUM_CASCADES );
	m_dataOrtho[ index ] = data;
}

void CDeferredExtension::CommitShadowData_Proj( const int &index, const shadowData_proj_t &data )
{
	Assert( index >= 0 && index < MAX_SHADOW_PROJ );
	m_dataProj[ index ] = data;
}

void CDeferredExtension::CommitShadowData_General( const shadowData_general_t &data )
{
	m_dataGeneral = data;
}

void CDeferredExtension::CommitVolumeData( const volumeData_t &data )
{
	m_dataVolume = data;
}

void CDeferredExtension::CommitRadiosityData( const radiosityData_t &data )
{
	m_dataRadiosity = data;
}

void CDeferredExtension::CommitLightData_Global( const lightData_Global_t &data )
{
	m_globalLight = data;
}

void CDeferredExtension::CommitLightData_Common( lightDataCommon_t* pData )
{
	if ( m_pflCommonLightData )
		m_pflCommonLightData->DeleteThis();

	m_pflCommonLightData = pData;
	if ( pData )
	{
		m_iCommon_NumRows = pData->numRows;
		m_iNumCommon_ShadowedCookied = pData->numShadowedCookied;
		m_iNumCommon_Shadowed = pData->numShadowed;
		m_iNumCommon_Cookied = pData->numCookied;
		m_iNumCommon_Simple = pData->numSimple;
	}
	else
	{
		m_iCommon_NumRows = 0;
		m_iNumCommon_ShadowedCookied = 0;
		m_iNumCommon_Shadowed = 0;
		m_iNumCommon_Cookied = 0;
		m_iNumCommon_Simple = 0;
	}
}

void CDeferredExtension::CommitTexture_General( ITexture *pTexNormals, ITexture *pTexDepth,
		ITexture *pTexLightingCtrl,
		ITexture *pTexWorldPos,
		ITexture *pTexLightAccum,
		ITexture *pTexLightAccum2 )
{
	m_pTexNormals = pTexNormals;
	m_pTexDepth = pTexDepth;
	m_pTexLightAccum = pTexLightAccum;
	m_pTexLightAccum2 = pTexLightAccum2;
	m_pTexLightCtrl = pTexLightingCtrl;
	m_pTexWorldPos = pTexWorldPos;
}
void CDeferredExtension::CommitTexture_CascadedDepth( const int &index, ITexture *pTexShadowDepth )
{
	Assert( index >= 0 && index < MAX_SHADOW_ORTHO );
	m_pTexShadowDepth_Ortho[ index ] = pTexShadowDepth;
}

void CDeferredExtension::CommitTexture_DualParaboloidDepth( const int &index, ITexture *pTexShadowDepth )
{
	Assert( index >= 0 && index < MAX_SHADOW_DP );
	m_pTexShadowDepth_DP[ index ] = pTexShadowDepth;
}

void CDeferredExtension::CommitTexture_ProjectedDepth( const int &index, ITexture *pTexShadowDepth )
{
	Assert( index >= 0 && index < MAX_SHADOW_PROJ );
	m_pTexShadowDepth_Proj[ index ] = pTexShadowDepth;
}

void CDeferredExtension::CommitTexture_Cookie( const int &index, ITexture *pTexCookie )
{
	Assert( index >= 0 && index < NUM_COOKIE_SLOTS );
	m_pTexCookie[ index ] = pTexCookie;
}

void CDeferredExtension::CommitTexture_VolumePrePass( ITexture *pTexVolumePrePass )
{
	m_pTexVolumePrePass = pTexVolumePrePass;
}

void CDeferredExtension::CommitTexture_ShadowRadOutput_Ortho( ITexture *pAlbedo, ITexture *pNormal )
{
	m_pTexShadowRad_Ortho[0] = pAlbedo;
	m_pTexShadowRad_Ortho[1] = pNormal;
}

void CDeferredExtension::CommitTexture_Radiosity( ITexture *pTexRadBuffer0, ITexture *pTexRadBuffer1,
		ITexture *pTexRadNormal0, ITexture *pTexRadNormal1 )
{
	m_pTexRadBuffer[0] = pTexRadBuffer0;
	m_pTexRadBuffer[1] = pTexRadBuffer1;
	m_pTexRadNormal[0] = pTexRadNormal0;
	m_pTexRadNormal[1] = pTexRadNormal1;
}