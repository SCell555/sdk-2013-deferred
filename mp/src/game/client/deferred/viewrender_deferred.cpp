#include "cbase.h"
#include "viewrender_deferred.h"
#include "vprof.h"
#include "c_rope.h"
#include "rendertexture.h"
#include "renderparm.h"
#include "viewdebug.h"
#include "glow_overlay.h"
#include "c_effects.h"
#include "clientsideeffects.h"
#include "view_scene.h"
#include "toolframework_client.h"
#include "model_types.h"
#include "view.h"
#include "viewpostprocess.h"
#include "ivieweffects.h"
#include "smoke_fog_overlay.h"
#include "clientmode_shared.h"

#include "ienginevgui.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "client_virtualreality.h"
#include "sourcevr/isourcevirtualreality.h"
#include "ShaderEditor/IVShaderEditor.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialvar.h"
#include "tier1/callqueue.h"

#include "vgui_int.h"
#include "vgui/IPanel.h"
#include "vgui_controls/Controls.h"
#include "debugoverlay_shared.h"

#include "tier0/memdbgon.h"

extern ConVar r_drawopaquerenderables;
extern ConVar r_entityclips;
extern ConVar r_eyewaterepsilon;
extern ConVar mat_clipz;
extern ConVar r_visocclusion;
extern ConVar fog_enableskybox;
extern ConVar r_3dsky;
extern ConVar r_skybox;
extern ConVar r_ForceWaterLeaf;
extern ConVar mat_viewportupscale;
extern ConVar mat_viewportscale;
extern ConVar mat_motion_blur_enabled;

extern int g_CurrentViewID;

extern void MaybeInvalidateLocalPlayerAnimation();
extern bool DoesViewPlaneIntersectWater( float waterZ, int leafWaterDataID );
extern void SetupCurrentView( const Vector &vecOrigin, const QAngle &angles, view_id_t viewID );
extern void SetClearColorToFogColor();
extern void FinishCurrentView();
extern void GetSkyboxFogColor( float *pColor );
extern float GetSkyboxFogStart( void );
extern float GetSkyboxFogEnd( void );
extern float GetSkyboxFogMaxDensity( void );
extern void FlushWorldLists();

void DrawCube( CMeshBuilder &meshBuilder, Vector vecPos, float flRadius, float *pfl2Texcoords )
{
	const float flOffsets[24][3] = {
		-flRadius, -flRadius, -flRadius,
		flRadius, -flRadius, -flRadius,
		flRadius, flRadius, -flRadius,
		-flRadius, flRadius, -flRadius,

		flRadius, flRadius, flRadius,
		flRadius, flRadius, -flRadius,
		flRadius, -flRadius, -flRadius,
		flRadius, -flRadius, flRadius,

		-flRadius, flRadius, flRadius,
		-flRadius, -flRadius, flRadius,
		-flRadius, -flRadius, -flRadius,
		-flRadius, flRadius, -flRadius,

		flRadius, flRadius, flRadius,
		-flRadius, flRadius, flRadius,
		-flRadius, flRadius, -flRadius,
		flRadius, flRadius, -flRadius,

		flRadius, -flRadius, flRadius,
		flRadius, -flRadius, -flRadius,
		-flRadius, -flRadius, -flRadius,
		-flRadius, -flRadius, flRadius,

		flRadius, flRadius, flRadius,
		flRadius, -flRadius, flRadius,
		-flRadius, -flRadius, flRadius,
		-flRadius, flRadius, flRadius,
	};

	for ( int i = 0; i < 24; i++ )
	{
		meshBuilder.Position3f( vecPos.x + flOffsets[i][0],
			vecPos.y + flOffsets[i][1],
			vecPos.z + flOffsets[i][2] );
		meshBuilder.TexCoord2fv( 0, pfl2Texcoords );
		meshBuilder.AdvanceVertex();
	}
}


class CBaseWorldViewDeferred : public CRendering3dView
{
	DECLARE_CLASS( CBaseWorldViewDeferred, CRendering3dView );
protected:
	CBaseWorldViewDeferred(CViewRender *pMainView) : CRendering3dView( pMainView ) {}

	virtual bool	AdjustView( float waterHeight );

	void			DrawSetup( float waterHeight, int nSetupFlags, float waterZAdjust, int iForceViewLeaf = -1, bool bShadowDepth = false );
	void			DrawExecute( float waterHeight, view_id_t viewID, float waterZAdjust, bool bShadowDepth = false );

	virtual void	PushView( float waterHeight );
	virtual void	PopView();

	// BUGBUG this causes all sorts of problems
	virtual bool	ShouldCacheLists(){ return false; };

	virtual void	DrawWorldDeferred( float waterZAdjust );
	virtual void	DrawOpaqueRenderablesDeferred(bool);

protected:

	static void PushComposite();
	static void PopComposite();
};

//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
class CSimpleWorldViewDeferred : public CBaseWorldViewDeferred
{
	DECLARE_CLASS( CSimpleWorldViewDeferred, CBaseWorldViewDeferred );
public:
	CSimpleWorldViewDeferred(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView ) {}

	void			Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& info, ViewCustomVisibility_t *pCustomVisibility = NULL );
	void			Draw();

	virtual bool	ShouldCacheLists() { return true; }

private:
	VisibleFogVolumeInfo_t m_fogInfo;
};

class CGBufferView : public CBaseWorldViewDeferred
{
	DECLARE_CLASS( CGBufferView, CBaseWorldViewDeferred );
public:
	CGBufferView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView )
	{
	}

	void			Setup( const CViewSetup &view, bool bDrewSkybox );
	void			Draw();

	virtual void	PushView( float waterHeight );
	virtual void	PopView();

	static void PushGBuffer( bool bInitial, float zScale = 1.0f, bool bClearDepth = true );
	static void PopGBuffer();

private:
	VisibleFogVolumeInfo_t m_fogInfo;
	bool m_bDrewSkybox;
};

class CSkyboxViewDeferred : public CGBufferView
{
	DECLARE_CLASS( CSkyboxViewDeferred, CRendering3dView );
public:
	CSkyboxViewDeferred(CViewRender *pMainView) :
		CGBufferView( pMainView ),
		m_pSky3dParams( NULL )
	  {
	  }

	bool			Setup( const CViewSetup &view, bool bGBuffer, SkyboxVisibility_t *pSkyboxVisible );
	void			Draw();

protected:

	virtual SkyboxVisibility_t	ComputeSkyboxVisibility();
	bool			GetSkyboxFogEnable();

	void			Enable3dSkyboxFog( void );
	void			DrawInternal( view_id_t iSkyBoxViewID = VIEW_3DSKY, ITexture *pRenderTarget = NULL, ITexture *pDepthTarget = NULL );

	sky3dparams_t *	PreRender3dSkyboxWorld( SkyboxVisibility_t nSkyboxVisible );
	sky3dparams_t *m_pSky3dParams;

	bool		m_bGBufferPass;
};

class CPostLightingView : public CBaseWorldViewDeferred
{
	DECLARE_CLASS( CPostLightingView, CBaseWorldViewDeferred );
public:
	CPostLightingView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView )
	{
	}

	void			Setup( const CViewSetup &view );
	void			Draw();

	virtual void	PushView( float waterHeight );
	virtual void	PopView();

	virtual void	DrawWorldDeferred( float waterZAdjust );
	virtual void	DrawOpaqueRenderablesDeferred(bool);

	static void		PushDeferredShadingFrameBuffer();
	static void		PopDeferredShadingFrameBuffer();

private:
	VisibleFogVolumeInfo_t m_fogInfo;
};

class CBaseShadowView : public CBaseWorldViewDeferred
{
	DECLARE_CLASS( CBaseShadowView, CBaseWorldViewDeferred );
public:
	CBaseShadowView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView )
	{
		m_bOutputRadiosity = false;
	};

	void			Setup( const CViewSetup &view,
						ITexture *pDepthTexture,
						ITexture *pDummyTexture );
	void			SetupRadiosityTargets(
						ITexture *pAlbedoTexture,
						ITexture *pNormalTexture );

	void SetRadiosityOutputEnabled( bool bEnabled );

	void			Draw();
	virtual bool	AdjustView( float waterHeight );
	virtual void	PushView( float waterHeight );
	virtual void	PopView();

	virtual void	CalcShadowView() = 0;
	virtual void	CommitData(){};

	virtual int		GetShadowMode() = 0;

private:

	ITexture *m_pDepthTexture;
	ITexture *m_pDummyTexture;
	ITexture *m_pRadAlbedoTexture;
	ITexture *m_pRadNormalTexture;
	ViewCustomVisibility_t shadowVis;

	bool m_bOutputRadiosity;
};

class COrthoShadowView : public CBaseShadowView
{
	DECLARE_CLASS( COrthoShadowView, CBaseShadowView );
public:
	COrthoShadowView(CViewRender *pMainView, const int &index)
		: CBaseShadowView( pMainView )
	{
			iCascadeIndex = index;
	}

	virtual void	CalcShadowView();
	virtual void	CommitData();

	virtual int		GetShadowMode(){
		return DEFERRED_SHADOW_MODE_ORTHO;
	};

private:
	int iCascadeIndex;
};

class CDualParaboloidShadowView : public CBaseShadowView
{
	DECLARE_CLASS( CDualParaboloidShadowView, CBaseShadowView );
public:
	CDualParaboloidShadowView(CViewRender *pMainView,
		def_light_t *pLight,
		const bool &bSecondary)
		: CBaseShadowView( pMainView )
	{
			m_pLight = pLight;
			m_bSecondary = bSecondary;
	}
	virtual bool	AdjustView( float waterHeight );
	virtual void	PushView( float waterHeight );
	virtual void	PopView();

	virtual void	CalcShadowView();

	virtual int		GetShadowMode(){
		return DEFERRED_SHADOW_MODE_DPSM;
	};

private:
	bool m_bSecondary;
	def_light_t *m_pLight;
};

class CSpotLightShadowView : public CBaseShadowView
{
	DECLARE_CLASS( CSpotLightShadowView, CBaseShadowView );
public:
	CSpotLightShadowView(CViewRender *pMainView,
		def_light_t *pLight, int index )
		: CBaseShadowView( pMainView )
	{
			m_pLight = pLight;
			m_iIndex = index;
	}

	virtual void	CalcShadowView();
	virtual void	CommitData();

	virtual int		GetShadowMode(){
		return DEFERRED_SHADOW_MODE_PROJECTED;
	};

private:
	def_light_t *m_pLight;
	int m_iIndex;
};

//-----------------------------------------------------------------------------
// Base class for scenes with water
//-----------------------------------------------------------------------------
class CBaseWaterViewDeferred : public CBaseWorldViewDeferred
{
	DECLARE_CLASS( CBaseWaterViewDeferred, CBaseWorldViewDeferred );
public:
	CBaseWaterViewDeferred(CViewRender *pMainView) :
		CBaseWorldViewDeferred( pMainView ),
		m_SoftwareIntersectionView( pMainView )
	{}

	//	void Setup( const CViewSetup &, const WaterRenderInfo_t& info );

protected:
	void			CalcWaterEyeAdjustments( const VisibleFogVolumeInfo_t &fogInfo, float &newWaterHeight, float &waterZAdjust, bool bSoftwareUserClipPlane );

	class CSoftwareIntersectionView : public CBaseWorldViewDeferred
	{
		DECLARE_CLASS( CSoftwareIntersectionView, CBaseWorldViewDeferred );
	public:
		CSoftwareIntersectionView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView ) {}

		void Setup( bool bAboveWater );
		void Draw();

	private:
		CBaseWaterViewDeferred *GetOuter() { return GET_OUTER( CBaseWaterViewDeferred, m_SoftwareIntersectionView ); }
	};

	friend class CSoftwareIntersectionView;

	CSoftwareIntersectionView m_SoftwareIntersectionView;

	WaterRenderInfo_t m_waterInfo;
	float m_waterHeight;
	float m_waterZAdjust;
	bool m_bSoftwareUserClipPlane;
	VisibleFogVolumeInfo_t m_fogInfo;
};


//-----------------------------------------------------------------------------
// Scenes above water
//-----------------------------------------------------------------------------
class CAboveWaterViewDeferred : public CBaseWaterViewDeferred
{
	DECLARE_CLASS( CAboveWaterViewDeferred, CBaseWaterViewDeferred );
public:
	CAboveWaterViewDeferred(CViewRender *pMainView) :
		CBaseWaterViewDeferred( pMainView ),
		m_ReflectionView( pMainView ),
		m_RefractionView( pMainView ),
		m_IntersectionView( pMainView )
	{}

	void Setup(  const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& waterInfo );
	void			Draw();

	class CReflectionView : public CBaseWorldViewDeferred
	{
		DECLARE_CLASS( CReflectionView, CBaseWorldViewDeferred );
	public:
		CReflectionView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView ) {}

		void Setup( bool bReflectEntities );
		void Draw();

	private:
		CAboveWaterViewDeferred *GetOuter() { return GET_OUTER( CAboveWaterViewDeferred, m_ReflectionView ); }
	};

	class CRefractionView : public CBaseWorldViewDeferred
	{
		DECLARE_CLASS( CRefractionView, CBaseWorldViewDeferred );
	public:
		CRefractionView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView ) {}

		void Setup();
		void Draw();

	private:
		CAboveWaterViewDeferred *GetOuter() { return GET_OUTER( CAboveWaterViewDeferred, m_RefractionView ); }
	};

	class CIntersectionView : public CBaseWorldViewDeferred
	{
		DECLARE_CLASS( CIntersectionView, CBaseWorldViewDeferred );
	public:
		CIntersectionView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView ) {}

		void Setup();
		void Draw();

	private:
		CAboveWaterViewDeferred *GetOuter() { return GET_OUTER( CAboveWaterViewDeferred, m_IntersectionView ); }
	};


	friend class CRefractionView;
	friend class CReflectionView;
	friend class CIntersectionView;

	bool m_bViewIntersectsWater;

	CReflectionView m_ReflectionView;
	CRefractionView m_RefractionView;
	CIntersectionView m_IntersectionView;
};


//-----------------------------------------------------------------------------
// Scenes below water
//-----------------------------------------------------------------------------
class CUnderWaterViewDeferred : public CBaseWaterViewDeferred
{
	DECLARE_CLASS( CUnderWaterViewDeferred, CBaseWaterViewDeferred );
public:
	CUnderWaterViewDeferred(CViewRender *pMainView) :
		CBaseWaterViewDeferred( pMainView ),
		m_RefractionView( pMainView )
	{}

	void			Setup( const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& info );
	void			Draw();

	class CRefractionView : public CBaseWorldViewDeferred
	{
		DECLARE_CLASS( CRefractionView, CBaseWorldViewDeferred );
	public:
		CRefractionView(CViewRender *pMainView) : CBaseWorldViewDeferred( pMainView ) {}

		void Setup();
		void Draw();

	private:
		CUnderWaterViewDeferred *GetOuter() { return GET_OUTER( CUnderWaterViewDeferred, m_RefractionView ); }
	};

	friend class CRefractionView;

	bool m_bDrawSkybox; // @MULTICORE (toml 8/17/2006): remove after setup hoisted

	CRefractionView m_RefractionView;
};

bool CBaseWorldViewDeferred::AdjustView( float waterHeight )
{
	if( m_DrawFlags & DF_RENDER_REFRACTION )
	{
		ITexture *pTexture = GetWaterRefractionTexture();

		// Use the aspect ratio of the main view! So, don't recompute it here
		x = y = 0;
		width = pTexture->GetActualWidth();
		height = pTexture->GetActualHeight();

		return true;
	}

	if( m_DrawFlags & DF_RENDER_REFLECTION )
	{
		ITexture *pTexture = GetWaterReflectionTexture();

		// Use the aspect ratio of the main view! So, don't recompute it here
		x = y = 0;
		width = pTexture->GetActualWidth();
		height = pTexture->GetActualHeight();
		angles[0] = -angles[0];
		angles[2] = -angles[2];
		origin[2] -= 2.0f * ( origin[2] - (waterHeight));
		return true;
	}

	return false;
}

void CBaseWorldViewDeferred::DrawSetup( float waterHeight, int nSetupFlags, float waterZAdjust, int iForceViewLeaf,
	bool bShadowDepth )
{
	const int savedViewID = g_CurrentViewID;
	g_CurrentViewID = bShadowDepth ? VIEW_DEFERRED_SHADOW : VIEW_MAIN;

	const bool bViewChanged = AdjustView( waterHeight );

	if ( bViewChanged )
	{
		render->Push3DView( *this, 0, NULL, GetFrustum() );
	}

	const bool bDrawEntities = ( nSetupFlags & DF_DRAW_ENTITITES ) != 0;
	const bool bDrawReflection = ( nSetupFlags & DF_RENDER_REFLECTION ) != 0;
	BuildWorldRenderLists( bDrawEntities, iForceViewLeaf, ShouldCacheLists(), false, bDrawReflection ? &waterHeight : NULL );

	PruneWorldListInfo();

	if ( bDrawEntities )
	{
		const bool bOptimized = bShadowDepth || savedViewID == VIEW_DEFERRED_GBUFFER;
		BuildRenderableRenderLists( bOptimized ? VIEW_SHADOW_DEPTH_TEXTURE : savedViewID );
	}

	if ( bViewChanged )
	{
		render->PopView( GetFrustum() );
	}

	g_CurrentViewID = savedViewID;
}

void CBaseWorldViewDeferred::DrawExecute( float waterHeight, view_id_t viewID, float waterZAdjust, bool bShadowDepth )
{
	const int savedViewID = g_CurrentViewID;

	// @MULTICORE (toml 8/16/2006): rethink how, where, and when this is done...
	g_CurrentViewID = VIEW_SHADOW_DEPTH_TEXTURE;
	//MaybeInvalidateLocalPlayerAnimation();
	//g_pClientShadowMgr->ComputeShadowTextures( *this, m_pWorldListInfo->m_LeafCount, m_pWorldListInfo->m_pLeafList );
	MaybeInvalidateLocalPlayerAnimation();

	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	g_CurrentViewID = viewID;

	// Update our render view flags.
	const int iDrawFlagsBackup = m_DrawFlags;
	m_DrawFlags |= m_pMainView->GetBaseDrawFlags();

	PushView( waterHeight );

	CMatRenderContextPtr pRenderContext( materials );

	ITexture *pSaveFrameBufferCopyTexture = pRenderContext->GetFrameBufferCopyTexture( 0 );
	if ( engine->GetDXSupportLevel() >= 80 )
	{
		pRenderContext->SetFrameBufferCopyTexture( GetPowerOfTwoFrameBufferTexture() );
	}

	pRenderContext.SafeRelease();

	DrawWorldDeferred( waterZAdjust );

	//if ( m_DrawFlags & DF_DRAW_ENTITITES )
	DrawOpaqueRenderablesDeferred( false );

	//if (!m_bDrawWorldNormal)
	{
		if ( m_DrawFlags & DF_DRAW_ENTITITES )
		{
			DrawTranslucentRenderables( false, false );
			if (!bShadowDepth)
				DrawNoZBufferTranslucentRenderables();
		}
		else
		{
			// Draw translucent world brushes only, no entities
			DrawTranslucentWorldInLeaves( false );
		}
	}

	pRenderContext.GetFrom( materials );
	pRenderContext->SetFrameBufferCopyTexture( pSaveFrameBufferCopyTexture );
	PopView();

	m_DrawFlags = iDrawFlagsBackup;

	g_CurrentViewID = savedViewID;
}

void CBaseWorldViewDeferred::PushView( float waterHeight )
{
	float spread = 2.0f;
	if( m_DrawFlags & DF_FUDGE_UP )
	{
		waterHeight += spread;
	}
	else
	{
		waterHeight -= spread;
	}

	MaterialHeightClipMode_t clipMode = MATERIAL_HEIGHTCLIPMODE_DISABLE;
	if ( ( m_DrawFlags & DF_CLIP_Z ) && mat_clipz.GetBool() )
	{
		if( m_DrawFlags & DF_CLIP_BELOW )
		{
			clipMode = MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT;
		}
		else
		{
			clipMode = MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT;
		}
	}

	CMatRenderContextPtr pRenderContext( materials );

	if( m_DrawFlags & DF_RENDER_REFRACTION )
	{
		pRenderContext->SetFogZ( waterHeight );
		pRenderContext->SetHeightClipZ( waterHeight );
		pRenderContext->SetHeightClipMode( clipMode );

		// Have to re-set up the view since we reset the size
		render->Push3DView( *this, m_ClearFlags, GetWaterRefractionTexture(), GetFrustum() );

		return;
	}

	if( m_DrawFlags & DF_RENDER_REFLECTION )
	{
		ITexture *pTexture = GetWaterReflectionTexture();

		pRenderContext->SetFogZ( waterHeight );

		bool bSoftwareUserClipPlane = g_pMaterialSystemHardwareConfig->UseFastClipping();
		if( bSoftwareUserClipPlane && ( origin[2] > waterHeight - r_eyewaterepsilon.GetFloat() ) )
		{
			waterHeight = origin[2] + r_eyewaterepsilon.GetFloat();
		}

		pRenderContext->SetHeightClipZ( waterHeight );
		pRenderContext->SetHeightClipMode( clipMode );

		render->Push3DView( *this, m_ClearFlags, pTexture, GetFrustum() );
		return;
	}

	if ( m_ClearFlags & ( VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR | VIEW_CLEAR_STENCIL ) )
	{
		if ( m_ClearFlags & VIEW_CLEAR_OBEY_STENCIL )
		{
			pRenderContext->ClearBuffersObeyStencil( m_ClearFlags & VIEW_CLEAR_COLOR, m_ClearFlags & VIEW_CLEAR_DEPTH );
		}
		else
		{
			pRenderContext->ClearBuffers( m_ClearFlags & VIEW_CLEAR_COLOR, m_ClearFlags & VIEW_CLEAR_DEPTH, m_ClearFlags & VIEW_CLEAR_STENCIL );
		}
	}

	pRenderContext->SetHeightClipMode( clipMode );
	if ( clipMode != MATERIAL_HEIGHTCLIPMODE_DISABLE )
	{
		pRenderContext->SetHeightClipZ( waterHeight );
	}
}

void CBaseWorldViewDeferred::PopView()
{
	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
	if( m_DrawFlags & (DF_RENDER_REFRACTION | DF_RENDER_REFLECTION) )
	{
		if ( IsX360() )
		{
			// these renders paths used their surfaces, so blit their results
			if ( m_DrawFlags & DF_RENDER_REFRACTION )
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetWaterRefractionTexture(), NULL, NULL );
			}
			if ( m_DrawFlags & DF_RENDER_REFLECTION )
			{
				pRenderContext->CopyRenderTargetToTextureEx( GetWaterReflectionTexture(), NULL, NULL );
			}
		}

		render->PopView( GetFrustum() );
	}
}

void CBaseWorldViewDeferred::DrawWorldDeferred( float waterZAdjust )
{
	DrawWorld( waterZAdjust );
}

void CBaseWorldViewDeferred::DrawOpaqueRenderablesDeferred(bool k)
{
	DrawOpaqueRenderables(k ? DEPTH_MODE_SHADOW : DEPTH_MODE_NORMAL);
}

void CBaseWorldViewDeferred::PushComposite()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_COMPOSITION );
}

void CBaseWorldViewDeferred::PopComposite()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_INVALID );
}

//-----------------------------------------------------------------------------
// Draws the scene when there's no water or only cheap water
//-----------------------------------------------------------------------------
void CSimpleWorldViewDeferred::Setup( const CViewSetup &view, int nClearFlags, bool bDrawSkybox,
	const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t &waterInfo, ViewCustomVisibility_t *pCustomVisibility )
{
	BaseClass::Setup( view );

	m_ClearFlags = nClearFlags;
	m_DrawFlags = DF_DRAW_ENTITITES;

	if ( !waterInfo.m_bOpaqueWater )
	{
		m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
	}
	else
	{
		const bool bViewIntersectsWater = DoesViewPlaneIntersectWater( fogInfo.m_flWaterHeight, fogInfo.m_nVisibleFogVolume );
		if( bViewIntersectsWater )
		{
			// have to draw both sides if we can see both.
			m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
		}
		else if ( fogInfo.m_bEyeInFogVolume )
		{
			m_DrawFlags |= DF_RENDER_UNDERWATER;
		}
		else
		{
			m_DrawFlags |= DF_RENDER_ABOVEWATER;
		}
	}
	if ( waterInfo.m_bDrawWaterSurface )
	{
		m_DrawFlags |= DF_RENDER_WATER;
	}

	if ( !fogInfo.m_bEyeInFogVolume && bDrawSkybox )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

	m_pCustomVisibility = pCustomVisibility;
	m_fogInfo = fogInfo;
}

void CSimpleWorldViewDeferred::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_NoWater" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CSimpleWorldView::Draw" );

	pRenderContext.SafeRelease();

	PushComposite();

	DrawSetup( 0, m_DrawFlags, 0 );

	if ( !m_fogInfo.m_bEyeInFogVolume )
	{
		EnableWorldFog();
	}
	else
	{
		m_ClearFlags |= VIEW_CLEAR_COLOR;

		SetFogVolumeState( m_fogInfo, false );

		pRenderContext.GetFrom( materials );

		unsigned char ucFogColor[3];
		pRenderContext->GetFogColor( ucFogColor );
		pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
	}

	pRenderContext.SafeRelease();

	DrawExecute( 0, CurrentViewID(), 0 );

	PopComposite();

	pRenderContext.GetFrom( materials );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
}

void CGBufferView::Setup( const CViewSetup &view, bool bDrewSkybox )
{
	m_fogInfo.m_bEyeInFogVolume = false;
	m_bDrewSkybox = bDrewSkybox;

	BaseClass::Setup( view );
	m_bDrawWorldNormal = true;

	m_ClearFlags = 0;
	m_DrawFlags = DF_DRAW_ENTITITES;

	m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;

#if DEFCFG_DEFERRED_SHADING
	if ( !bDrewSkybox )
		m_DrawFlags |= DF_DRAWSKYBOX;
#endif
}

void CGBufferView::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_NoWater" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CSimpleWorldViewDeferred::Draw" );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 ); //lean toward pixel shader threads
#endif

	SetupCurrentView( origin, angles, VIEW_DEFERRED_GBUFFER );

	DrawSetup( 0, m_DrawFlags, 0 );

	const bool bOptimizedGbuffer = DEFCFG_DEFERRED_SHADING == 0;
	DrawExecute( 0, CurrentViewID(), 0, bOptimizedGbuffer );

	pRenderContext.GetFrom( materials );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

void CGBufferView::PushView( float waterHeight )
{
	PushGBuffer( !m_bDrewSkybox );
}

void CGBufferView::PopView()
{
	PopGBuffer();
}

void CGBufferView::PushGBuffer( bool bInitial, float zScale, bool bClearDepth )
{
	ITexture *pNormals = GetDefRT_Normals();
	ITexture *pDepth = GetDefRT_Depth();

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->ClearColor4ub( 0, 0, 0, 0 );

	if ( bInitial )
	{
		pRenderContext->PushRenderTargetAndViewport( pDepth );
		pRenderContext->ClearBuffers( true, false );
		pRenderContext->PopRenderTargetAndViewport();
	}

#if DEFCFG_DEFERRED_SHADING == 1
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_Albedo() );
#else
	pRenderContext->PushRenderTargetAndViewport( pNormals );
#endif

	if ( bClearDepth )
		pRenderContext->ClearBuffers( false, true );

	pRenderContext->SetRenderTargetEx( 1, pDepth );

#if DEFCFG_DEFERRED_SHADING == 1
	pRenderContext->SetRenderTargetEx( 2, pNormals );
	pRenderContext->SetRenderTargetEx( 3, GetDefRT_Specular() );
#endif

	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_GBUFFER );

	struct defData_setZScale
	{
	public:
		float zScale;

		static void Fire( defData_setZScale d )
		{
			GetDeferredExt()->CommitZScale( d.zScale );
		};
	};

	defData_setZScale data;
	data.zScale = zScale;
	QUEUE_FIRE( defData_setZScale, Fire, data );
}

void CGBufferView::PopGBuffer()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_INVALID );

	pRenderContext->PopRenderTargetAndViewport();
}

SkyboxVisibility_t CSkyboxViewDeferred::ComputeSkyboxVisibility()
{
	if ( ( enginetrace->GetPointContents( origin ) & CONTENTS_SOLID ) != 0 )
		return SKYBOX_NOT_VISIBLE;

	return engine->IsSkyboxVisibleFromPoint( origin );
}

bool CSkyboxViewDeferred::GetSkyboxFogEnable()
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return false;
	}
	CPlayerLocalData	*local		= &pbp->m_Local;

	static ConVarRef fog_override( "fog_override" );
	if( fog_override.GetInt() )
	{
		return fog_enableskybox.GetBool();
	}
	else
	{
		return !!local->m_skybox3d.fog.enable;
	}
}

void CSkyboxViewDeferred::Enable3dSkyboxFog( void )
{
	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();
	if( !pbp )
	{
		return;
	}
	CPlayerLocalData *local = &pbp->m_Local;

	CMatRenderContextPtr pRenderContext( materials );

	if( GetSkyboxFogEnable() )
	{
		float fogColor[3];
		GetSkyboxFogColor( fogColor );
		float scale = 1.0f;
		if ( local->m_skybox3d.scale > 0.0f )
		{
			scale = 1.0f / local->m_skybox3d.scale;
		}
		pRenderContext->FogMode( MATERIAL_FOG_LINEAR );
		pRenderContext->FogColor3fv( fogColor );
		pRenderContext->FogStart( GetSkyboxFogStart() * scale );
		pRenderContext->FogEnd( GetSkyboxFogEnd() * scale );
		pRenderContext->FogMaxDensity( GetSkyboxFogMaxDensity() );
	}
	else
	{
		pRenderContext->FogMode( MATERIAL_FOG_NONE );
	}
}

sky3dparams_t *CSkyboxViewDeferred::PreRender3dSkyboxWorld( SkyboxVisibility_t nSkyboxVisible )
{
	if ( ( nSkyboxVisible != SKYBOX_3DSKYBOX_VISIBLE ) && r_3dsky.GetInt() != 2 )
		return NULL;

	// render the 3D skybox
	if ( !r_3dsky.GetInt() )
		return NULL;

	C_BasePlayer *pbp = C_BasePlayer::GetLocalPlayer();

	// No local player object yet...
	if ( !pbp )
		return NULL;

	CPlayerLocalData* local = &pbp->m_Local;
	if ( local->m_skybox3d.area == 255 )
		return NULL;

	return &local->m_skybox3d;
}

void CSkyboxViewDeferred::DrawInternal( view_id_t iSkyBoxViewID, ITexture *pRenderTarget, ITexture *pDepthTarget )
{
	const bool bInvokePreAndPostRender = !m_bGBufferPass;

	if ( m_bGBufferPass )
	{
#if DEFCFG_DEFERRED_SHADING
		m_DrawFlags |= DF_DRAWSKYBOX;
#endif
	}

	unsigned char **areabits = render->GetAreaBits();
	unsigned char *savebits;
	unsigned char tmpbits[ 32 ];
	savebits = *areabits;
	memset( tmpbits, 0, sizeof(tmpbits) );

	// set the sky area bit
	tmpbits[m_pSky3dParams->area>>3] |= 1 << (m_pSky3dParams->area&7);

	*areabits = tmpbits;

	// if you can get really close to the skybox geometry it's possible that you'll be able to clip into it
	// with this near plane.  If so, move it in a bit.  It's at 2.0 to give us more precision.  That means you
	// need to keep the eye position at least 2 * scale away from the geometry in the skybox
	zNear = 2.0;
	zFar = 10000.0f; //MAX_TRACE_LENGTH;

	float skyScale = 1.0f;
	// scale origin by sky scale and translate to sky origin
	{
		skyScale = (m_pSky3dParams->scale > 0) ? m_pSky3dParams->scale : 1.0f;
		const float scale = 1.0f / skyScale;

		const Vector& vSkyOrigin = m_pSky3dParams->origin;
		VectorScale( origin, scale, origin );
		VectorAdd( origin, vSkyOrigin, origin );
	}

	if ( !m_bGBufferPass )
		Enable3dSkyboxFog();

	// BUGBUG: Fix this!!!  We shouldn't need to call setup vis for the sky if we're connecting
	// the areas.  We'd have to mark all the clusters in the skybox area in the PVS of any
	// cluster with sky.  Then we could just connect the areas to do our vis.
	//m_bOverrideVisOrigin could hose us here, so call direct
	render->ViewSetupVis( false, 1, &m_pSky3dParams->origin.Get() );
	render->Push3DView( (*this), m_ClearFlags, pRenderTarget, GetFrustum(), pDepthTarget );

	if ( m_bGBufferPass )
		PushGBuffer( true, skyScale );
	else
		PushComposite();

	// Store off view origin and angles
	SetupCurrentView( origin, angles, iSkyBoxViewID );

#if defined( _X360 )
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushVertexShaderGPRAllocation( 32 );
	pRenderContext.SafeRelease();
#endif

	// Invoke pre-render methods
	if ( bInvokePreAndPostRender )
	{
		IGameSystem::PreRenderAllSystems();
	}

	BuildWorldRenderLists( true, -1, ShouldCacheLists() );

	BuildRenderableRenderLists( m_bGBufferPass ? VIEW_SHADOW_DEPTH_TEXTURE : iSkyBoxViewID );

	DrawWorldDeferred( 0.0f );

	// Iterate over all leaves and render objects in those leaves
	DrawOpaqueRenderablesDeferred( false );

	if ( !m_bGBufferPass )
	{
		// Iterate over all leaves and render objects in those leaves
		DrawTranslucentRenderables( true, false );
		DrawNoZBufferTranslucentRenderables();
	}

	if ( !m_bGBufferPass )
	{
		m_pMainView->DisableFog();

		CGlowOverlay::UpdateSkyOverlays( zFar, m_bCacheFullSceneState );

		PixelVisibility_EndCurrentView();
	}

	// restore old area bits
	*areabits = savebits;

	// Invoke post-render methods
	if( bInvokePreAndPostRender )
	{
		IGameSystem::PostRenderAllSystems();
		FinishCurrentView();
	}

	if ( m_bGBufferPass )
		PopGBuffer();
	else
		PopComposite();

	render->PopView( GetFrustum() );

#if defined( _X360 )
	pRenderContext.GetFrom( materials );
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

bool CSkyboxViewDeferred::Setup( const CViewSetup &view, bool bGBuffer, SkyboxVisibility_t *pSkyboxVisible )
{
	BaseClass::Setup( view );

	// The skybox might not be visible from here
	*pSkyboxVisible = ComputeSkyboxVisibility();
	m_pSky3dParams = PreRender3dSkyboxWorld( *pSkyboxVisible );

	if ( !m_pSky3dParams )
	{
		return false;
	}

	m_bGBufferPass = bGBuffer;
	// At this point, we've cleared everything we need to clear
	// The next path will need to clear depth, though.
	m_ClearFlags = VIEW_CLEAR_DEPTH; //*pClearFlags;
	//*pClearFlags &= ~( VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH | VIEW_CLEAR_STENCIL | VIEW_CLEAR_FULL_TARGET );
	//*pClearFlags |= VIEW_CLEAR_DEPTH; // Need to clear depth after rednering the skybox

	m_DrawFlags = DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER | DF_RENDER_WATER;
	if( !m_bGBufferPass && r_skybox.GetBool() )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

	return true;
}

void CSkyboxViewDeferred::Draw()
{
	VPROF_BUDGET( "CViewRender::Draw3dSkyboxworld", "3D Skybox" );

	ITexture *pRTColor = NULL;
	ITexture *pRTDepth = NULL;
	if( m_eStereoEye != STEREO_EYE_MONO )
	{
		pRTColor = g_pSourceVR->GetRenderTarget( (ISourceVirtualReality::VREye)(m_eStereoEye-1), ISourceVirtualReality::RT_Color );
		pRTDepth = g_pSourceVR->GetRenderTarget( (ISourceVirtualReality::VREye)(m_eStereoEye-1), ISourceVirtualReality::RT_Depth );
	}

	DrawInternal(VIEW_3DSKY, pRTColor, pRTDepth);
}

void CPostLightingView::Setup( const CViewSetup &view )
{
	m_fogInfo.m_bEyeInFogVolume = false;

	BaseClass::Setup( view );

	m_ClearFlags = 0;

	m_DrawFlags = DF_DRAW_ENTITITES;
	m_DrawFlags |= DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
}

void CPostLightingView::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_NoWater" );

	CMatRenderContextPtr pRenderContext( materials );
	PIXEVENT( pRenderContext, "CSimpleWorldViewDeferred::Draw" );

#if defined( _X360 )
	pRenderContext->PushVertexShaderGPRAllocation( 32 ); //lean toward pixel shader threads
#endif

	ITexture *pTexAlbedo = GetDefRT_Albedo();
	pRenderContext->CopyRenderTargetToTexture( pTexAlbedo );
	pRenderContext->PushRenderTargetAndViewport( pTexAlbedo );
	pRenderContext.SafeRelease();
	PushComposite();

	SetupCurrentView( origin, angles, VIEW_MAIN );

	DrawSetup( 0, m_DrawFlags, 0 );

	DrawExecute( 0, CurrentViewID(), 0, false );

	PopComposite();

	pRenderContext.GetFrom( materials );
	pRenderContext->PopRenderTargetAndViewport();

#if defined( _X360 )
	pRenderContext->PopVertexShaderGPRAllocation();
#endif
}

void CPostLightingView::PushView( float waterHeight )
{
	//PushGBuffer( !m_bDrewSkybox );
	BaseClass::PushView( waterHeight );
}

void CPostLightingView::PopView()
{
	BaseClass::PopView();
}

void CPostLightingView::DrawWorldDeferred( float waterZAdjust )
{
#if 0
	int iOldDrawFlags = m_DrawFlags;

	m_DrawFlags &= ~DF_DRAW_ENTITITES;
	m_DrawFlags &= ~DF_RENDER_UNDERWATER;
	m_DrawFlags &= ~DF_RENDER_ABOVEWATER;
	m_DrawFlags &= ~DF_DRAW_ENTITITES;

	BaseClass::DrawWorldDeferred( waterZAdjust );

	m_DrawFlags = iOldDrawFlags;
#endif
}

void CPostLightingView::DrawOpaqueRenderablesDeferred(bool)
{
}

void CPostLightingView::PushDeferredShadingFrameBuffer()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_Albedo() );
}

void CPostLightingView::PopDeferredShadingFrameBuffer()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PopRenderTargetAndViewport();
}

void CBaseShadowView::Setup( const CViewSetup &view, ITexture *pDepthTexture, ITexture *pDummyTexture )
{
	m_pDepthTexture = pDepthTexture;
	m_pDummyTexture = pDummyTexture;

	BaseClass::Setup( view );

	m_bDrawWorldNormal = true;

	m_DrawFlags = DF_DRAW_ENTITITES | DF_RENDER_UNDERWATER | DF_RENDER_ABOVEWATER;
	m_ClearFlags = 0;

	CalcShadowView();

	m_pCustomVisibility = &shadowVis;
	shadowVis.AddVisOrigin( origin );
}

void CBaseShadowView::SetupRadiosityTargets( ITexture *pAlbedoTexture, ITexture *pNormalTexture )
{
	m_pRadAlbedoTexture = pAlbedoTexture;
	m_pRadNormalTexture = pNormalTexture;
}

void CBaseShadowView::Draw()
{
	const int oldViewID = g_CurrentViewID;
	SetupCurrentView( origin, angles, VIEW_DEFERRED_SHADOW );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_SHADOWPASS );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_MODE,
		GetShadowMode() );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_RADIOSITY,
		m_bOutputRadiosity ? 1 : 0 );
	pRenderContext.SafeRelease();

	DrawSetup( 0, m_DrawFlags, 0, -1, true );

	DrawExecute( 0, CurrentViewID(), 0, true );

	pRenderContext.GetFrom( materials );
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_RADIOSITY,
		0 );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
		DEFERRED_RENDER_STAGE_INVALID );

	g_CurrentViewID = oldViewID;
}

bool CBaseShadowView::AdjustView( float waterHeight )
{
	CommitData();

	return true;
}

void CBaseShadowView::PushView( float waterHeight )
{
	render->Push3DView( *this, 0, m_pDummyTexture, GetFrustum(), m_pDepthTexture );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushRenderTargetAndViewport( m_pDummyTexture, m_pDepthTexture, x, y, width, height );

#if defined( DEBUG )
	pRenderContext->ClearColor4ub( 255, 255, 255, 255 );
	pRenderContext->ClearBuffers( true, true );
#else
	if ( GetDeferredManager()->UsingHardwareFiltering() )
		pRenderContext->ClearBuffers( false, true );
	else
	{
		pRenderContext->ClearColor4ub( 255, 255, 255, 255 );
		pRenderContext->ClearBuffers( true, true );
	}
#endif

	if ( m_bOutputRadiosity )
	{
		Assert( !IsErrorTexture( m_pRadAlbedoTexture ) );
		Assert( !IsErrorTexture( m_pRadNormalTexture ) );

		pRenderContext->SetRenderTargetEx( 1, m_pRadAlbedoTexture );
		pRenderContext->SetRenderTargetEx( 2, m_pRadNormalTexture );
	}
}

void CBaseShadowView::PopView()
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PopRenderTargetAndViewport();

	render->PopView( GetFrustum() );
}

void CBaseShadowView::SetRadiosityOutputEnabled( bool bEnabled )
{
	m_bOutputRadiosity = bEnabled;
}

static lightData_Global_t& GetActiveGlobalLightState()
{
	static lightData_Global_t data;
	CLightingEditor *pEditor = GetLightingEditor();

	if ( pEditor->IsEditorLightingActive() && pEditor->GetKVGlobalLight() != NULL )
	{
		data = pEditor->GetGlobalState();
	}
	else if ( GetGlobalLight() != NULL )
	{
		data = GetGlobalLight()->GetState();
	}

	return data;
}

void COrthoShadowView::CalcShadowView()
{
	const cascade_t &m_data = GetCascadeInfo( iCascadeIndex );
	Vector mainFwd;
	AngleVectors( angles, &mainFwd );

	const lightData_Global_t& state = GetActiveGlobalLightState();
	QAngle lightAng;
	VectorAngles( -state.vecLight.AsVector3D(), lightAng );

	Vector viewFwd, viewRight, viewUp;
	AngleVectors( lightAng, &viewFwd, &viewRight, &viewUp );

	const float halfOrthoSize = m_data.flProjectionSize * 0.5f;

	origin += -viewFwd * m_data.flOriginOffset +
		viewUp * halfOrthoSize -
		viewRight * halfOrthoSize +
		mainFwd * halfOrthoSize;

	angles = lightAng;

	x = 0;
	y = 0;
	height = m_data.iResolution;
	width = m_data.iResolution;

	m_bOrtho = true;
	m_OrthoLeft = 0;
	m_OrthoTop = -m_data.flProjectionSize;
	m_OrthoRight = m_data.flProjectionSize;
	m_OrthoBottom = 0;

	zNear = zNearViewmodel = 0;
	zFar = zFarViewmodel = m_data.flFarZ;
	m_flAspectRatio = 0;

	float mapping_world = m_data.flProjectionSize / m_data.iResolution;
	origin -= fmod( DotProduct( viewRight, origin ), mapping_world ) * viewRight;
	origin -= fmod( DotProduct( viewUp, origin ), mapping_world ) * viewUp;

	origin -= fmod( DotProduct( viewFwd, origin ), GetDepthMapDepthResolution( zFar - zNear ) ) * viewFwd;

#if CSM_USE_COMPOSITED_TARGET
	x = m_data.iViewport_x;
	y = m_data.iViewport_y;
#endif
}

void COrthoShadowView::CommitData()
{
	struct sendShadowDataOrtho
	{
		shadowData_ortho_t data;
		int index;
		static void Fire( sendShadowDataOrtho d )
		{
			GetDeferredExt()->CommitShadowData_Ortho( d.index, d.data );
		};
	};

	const cascade_t &data = GetCascadeInfo( iCascadeIndex );

	Vector fwd, right, down;
	AngleVectors( angles, &fwd, &right, &down );
	down *= -1.0f;

	sendShadowDataOrtho shadowData;
	shadowData.index = iCascadeIndex;

#if CSM_USE_COMPOSITED_TARGET
	shadowData.data.iRes_x = CSM_COMP_RES_X;
	shadowData.data.iRes_y = CSM_COMP_RES_Y;
#else
	shadowData.data.iRes_x = width;
	shadowData.data.iRes_y = height;
#endif

	shadowData.data.vecSlopeSettings.Init(
		data.flSlopeScaleMin, data.flSlopeScaleMax, data.flNormalScaleMax, 1.0f / zFar
		);
	shadowData.data.vecOrigin.Init( origin );

	Vector4D matrix_scale_offset( 0.5f, -0.5f, 0.5f, 0.5f );

#if CSM_USE_COMPOSITED_TARGET
	shadowData.data.vecUVTransform.Init( x / (float)CSM_COMP_RES_X,
		y / (float)CSM_COMP_RES_Y,
		width / (float) CSM_COMP_RES_X,
		height / (float) CSM_COMP_RES_Y );
#endif

	VMatrix a,b,c,d,screenToTexture;
	render->GetMatricesForView( *this, &a, &b, &c, &d );
	MatrixBuildScale( screenToTexture, matrix_scale_offset.x,
		matrix_scale_offset.y,
		1.0f );

	screenToTexture[0][3] = matrix_scale_offset.z;
	screenToTexture[1][3] = matrix_scale_offset.w;

	MatrixMultiply( screenToTexture, c, shadowData.data.matWorldToTexture );

	QUEUE_FIRE( sendShadowDataOrtho, Fire, shadowData );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_INDEX, iCascadeIndex );
}

bool CDualParaboloidShadowView::AdjustView( float waterHeight )
{
	BaseClass::AdjustView( waterHeight );

	// HACK: when pushing our actual view the renderer fails building the worldlist right!
	// So we can't. Shit.
	return false;
}

void CDualParaboloidShadowView::PushView( float waterHeight )
{
	BaseClass::PushView( waterHeight );
}

void CDualParaboloidShadowView::PopView()
{
	BaseClass::PopView();
}

void CDualParaboloidShadowView::CalcShadowView()
{
	float flRadius = m_pLight->flRadius;

	m_bOrtho = true;
	m_OrthoTop = m_OrthoLeft = -flRadius;
	m_OrthoBottom = m_OrthoRight = flRadius;

	const int dpsmRes = GetShadowResolution_Point();

	width = dpsmRes;
	height = dpsmRes;

	zNear = zNearViewmodel = 0;
	zFar = zFarViewmodel = flRadius;

	if ( m_bSecondary )
	{
		y = dpsmRes;

		Vector fwd, up;
		AngleVectors( angles, &fwd, NULL, &up );
		VectorAngles( -fwd, up, angles );
	}
}

void CSpotLightShadowView::CalcShadowView()
{
	float flRadius = m_pLight->flRadius;

	int spotRes = GetShadowResolution_Spot();

	width = spotRes;
	height = spotRes;

	zNear = zNearViewmodel = DEFLIGHT_SPOT_ZNEAR;
	zFar = zFarViewmodel = flRadius;

	fov = fovViewmodel = m_pLight->GetFOV();
}

void CSpotLightShadowView::CommitData()
{
	struct sendShadowDataProj
	{
		shadowData_proj_t data;
		int index;
		static void Fire( sendShadowDataProj d )
		{
			GetDeferredExt()->CommitShadowData_Proj( d.index, d.data );
		};
	};

	Vector fwd;
	AngleVectors( angles, &fwd );

	sendShadowDataProj data;
	data.index = m_iIndex;
	data.data.vecForward.Init( fwd );
	data.data.vecOrigin.Init( origin );
	// slope min, slope max, normal max, depth
	//data.data.vecSlopeSettings.Init( 0.005f, 0.02f, 3, zFar );
	data.data.vecSlopeSettings.Init( 0.001f, 0.005f, 3, 0 );

	QUEUE_FIRE( sendShadowDataProj, Fire, data );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_SHADOW_INDEX, m_iIndex );
}

CDeferredViewRender::CDeferredViewRender()
{
	m_pMesh_RadiosityScreenGrid[0] = NULL;
	m_pMesh_RadiosityScreenGrid[1] = NULL;
}

void CDeferredViewRender::Shutdown()
{
	CMatRenderContextPtr pRenderContext( materials );
	for ( IMesh* &mesh : m_pMesh_RadiosityScreenGrid)
	{
		if ( mesh != NULL )
			pRenderContext->DestroyStaticMesh( mesh );

		mesh = NULL;
	}

	for ( CUtlVector<IMesh*>& list : m_hRadiosityDebugMeshList )
	{
        const int count = list.Count();
		for ( int i = 0; i < count; i++ )
		{
            IMesh* mesh = list[i];
			Assert( mesh != NULL );

			pRenderContext->DestroyStaticMesh( mesh );
		}
	}

	BaseClass::Shutdown();
}

void CDeferredViewRender::LevelInit()
{
	BaseClass::LevelInit();
}

void CDeferredViewRender::ViewDrawSceneDeferred( const CViewSetup &view, int nClearFlags, view_id_t viewID, bool bDrawViewModel )
{
	VPROF( "CViewRender::ViewDrawScene" );

	bool bDrew3dSkybox = false;
	SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;

	ViewDrawGBuffer( view, bDrew3dSkybox, nSkyboxVisible, bDrawViewModel );

	PerformLighting( view );

#if DEFCFG_DEFERRED_SHADING
	ViewCombineDeferredShading( view, viewID );
#else
	ViewDrawComposite( view, bDrew3dSkybox, nSkyboxVisible, nClearFlags, viewID, bDrawViewModel );
#endif

#if DEFCFG_ENABLE_RADIOSITY
	if ( deferred_radiosity_debug.GetBool() )
		DebugRadiosity( view );
#endif

#if DEFCFG_DEFERRED_SHADING == 1
	CPostLightingView::PushDeferredShadingFrameBuffer();
#endif

	g_ShaderEditorSystem->UpdateSkymask( bDrew3dSkybox, view.x, view.y, view.width, view.height );

	GetLightingManager()->RenderVolumetrics( view );

	// Disable fog for the rest of the stuff
	DisableFog();

	// UNDONE: Don't do this with masked brush models, they should probably be in a separate list
	// render->DrawMaskEntities()

	// Here are the overlays...
	CGlowOverlay::DrawOverlays( view.m_bCacheFullSceneState );

	// issue the pixel visibility tests
	PixelVisibility_EndCurrentView();

	// Draw rain..
	DrawPrecipitation();

	// Make sure sound doesn't stutter
	engine->Sound_ExtraUpdate();

	// Debugging info goes over the top
	CDebugViewRender::Draw3DDebuggingInfo( view );

	// Draw client side effects
	// NOTE: These are not sorted against the rest of the frame
	clienteffects->DrawEffects( gpGlobals->frametime );

	// Mark the frame as locked down for client fx additions
	SetFXCreationAllowed( false );

	// Invoke post-render methods
	IGameSystem::PostRenderAllSystems();

#if DEFCFG_DEFERRED_SHADING == 1
	CPostLightingView::PopDeferredShadingFrameBuffer();

	ViewOutputDeferredShading( view );
#endif

	FinishCurrentView();

	// Set int rendering parameters back to defaults
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING, 0 );
}

void CDeferredViewRender::ViewDrawGBuffer( const CViewSetup &view, bool &bDrew3dSkybox, SkyboxVisibility_t &nSkyboxVisible,
	bool bDrawViewModel )
{
	MDLCACHE_CRITICAL_SECTION();

	int oldViewID = g_CurrentViewID;
	g_CurrentViewID = VIEW_DEFERRED_GBUFFER;

	CSkyboxViewDeferred *pSkyView = new CSkyboxViewDeferred( this );
	if ( ( bDrew3dSkybox = pSkyView->Setup( view, true, &nSkyboxVisible ) ) != false )
		AddViewToScene( pSkyView );

	SafeRelease( pSkyView );

	// Start view
	unsigned int visFlags;
	SetupVis( view, visFlags, NULL );

	CRefPtr<CGBufferView> pGBufferView = new CGBufferView( this );
	pGBufferView->Setup( view, bDrew3dSkybox );
	AddViewToScene( pGBufferView );

	DrawViewModels( view, bDrawViewModel, true );

	g_CurrentViewID = oldViewID;
}

void CDeferredViewRender::ViewDrawComposite( const CViewSetup &view, bool &bDrew3dSkybox, SkyboxVisibility_t &nSkyboxVisible,
		int nClearFlags, view_id_t viewID, bool bDrawViewModel )
{
	DrawSkyboxComposite( view, bDrew3dSkybox );

	// this allows the refract texture to be updated once per *scene* on 360
	// (e.g. once for a monitor scene and once for the main scene)
	g_viewscene_refractUpdateFrame = gpGlobals->framecount - 1;

	m_BaseDrawFlags = 0;

	SetupCurrentView( view.origin, view.angles, viewID );

	// Invoke pre-render methods
	IGameSystem::PreRenderAllSystems();

	// Start view
	unsigned int visFlags;
	SetupVis( view, visFlags, NULL );

	if ( !bDrew3dSkybox &&
		( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) && ( visFlags & IVRenderView::VIEW_SETUP_VIS_EX_RETURN_FLAGS_USES_RADIAL_VIS ) )
	{
		// This covers the case where we don't see a 3dskybox, yet radial vis is clipping
		// the far plane.  Need to clear to fog color in this case.
		nClearFlags |= VIEW_CLEAR_COLOR;
		SetClearColorToFogColor( );
	}
	else
		nClearFlags |= VIEW_CLEAR_DEPTH;

	bool drawSkybox = r_skybox.GetBool();
	if ( bDrew3dSkybox || ( nSkyboxVisible == SKYBOX_NOT_VISIBLE ) )
		drawSkybox = false;

	ParticleMgr()->IncrementFrameCode();

	DrawWorldComposite( view, nClearFlags, drawSkybox );

	DrawViewModels( view, bDrawViewModel, false );
}

void CDeferredViewRender::ViewCombineDeferredShading( const CViewSetup &view, view_id_t viewID )
{
#if DEFCFG_DEFERRED_SHADING == 1

	DrawLightPassFullscreen( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_SCREENSPACE_SHADING ),
		view.width, view.height );

	g_viewscene_refractUpdateFrame = gpGlobals->framecount - 1;

	m_BaseDrawFlags = 0;

	SetupCurrentView( view.origin, view.angles, viewID );

	IGameSystem::PreRenderAllSystems();

	ParticleMgr()->IncrementFrameCode();

	MDLCACHE_CRITICAL_SECTION();

	CRefPtr<CPostLightingView> pPostLightingView = new CPostLightingView( this );
	pPostLightingView->Setup( view );
	AddViewToScene( pPostLightingView );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ClearBuffers( false, true );

#else

#endif
}

void CDeferredViewRender::ViewOutputDeferredShading( const CViewSetup &view )
{
#if DEFCFG_DEFERRED_SHADING
	DrawLightPassFullscreen( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_SCREENSPACE_COMBINE ),
		view.width, view.height );
#endif
}

void CDeferredViewRender::DrawSkyboxComposite( const CViewSetup &view, const bool &bDrew3dSkybox )
{
	if ( !bDrew3dSkybox )
		return;

	CSkyboxViewDeferred *pSkyView = new CSkyboxViewDeferred( this );
	SkyboxVisibility_t nSkyboxVisible = SKYBOX_NOT_VISIBLE;
	if ( pSkyView->Setup( view, false, &nSkyboxVisible ) )
	{
		AddViewToScene( pSkyView );
		g_ShaderEditorSystem->UpdateSkymask(false, view.x, view.y, view.width, view.height);
	}

	SafeRelease( pSkyView );
	Assert( nSkyboxVisible == SKYBOX_3DSKYBOX_VISIBLE );
}

void CDeferredViewRender::DrawWorldComposite( const CViewSetup &view, int nClearFlags, bool bDrawSkybox )
{
#if 1
	MDLCACHE_CRITICAL_SECTION();

	VisibleFogVolumeInfo_t fogVolumeInfo;

	render->GetVisibleFogVolume( view.origin, &fogVolumeInfo );

	WaterRenderInfo_t info;
	DetermineWaterRenderInfo( fogVolumeInfo, info );

	if ( info.m_bCheapWater )
	{
#if 0 // TODO
		cplane_t glassReflectionPlane;
		if ( IsReflectiveGlassInView( view, glassReflectionPlane ) )
		{
			CRefPtr<CReflectiveGlassViewDeferred> pGlassReflectionView = new CReflectiveGlassViewDeferred( this );
			pGlassReflectionView->Setup( view, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, bDrawSkybox, fogVolumeInfo, info, glassReflectionPlane );
			AddViewToScene( pGlassReflectionView );

			CRefPtr<CRefractiveGlassViewDeferred> pGlassRefractionView = new CRefractiveGlassViewDeferred( this );
			pGlassRefractionView->Setup( view, VIEW_CLEAR_DEPTH | VIEW_CLEAR_COLOR, bDrawSkybox, fogVolumeInfo, info, glassReflectionPlane );
			AddViewToScene( pGlassRefractionView );
		}
#endif // 0

		CRefPtr<CSimpleWorldViewDeferred> pNoWaterView = new CSimpleWorldViewDeferred( this );
		pNoWaterView->Setup( view, nClearFlags, bDrawSkybox, fogVolumeInfo, info );
		AddViewToScene( pNoWaterView );
		return;
	}

	// Blat out the visible fog leaf if we're not going to use it
	if ( !r_ForceWaterLeaf.GetBool() )
	{
		fogVolumeInfo.m_nVisibleFogVolumeLeaf = -1;
	}

	// We can see water of some sort
	if ( !fogVolumeInfo.m_bEyeInFogVolume )
	{
		CRefPtr<CAboveWaterViewDeferred> pAboveWaterView = new CAboveWaterViewDeferred( this );
		pAboveWaterView->Setup( view, bDrawSkybox, fogVolumeInfo, info );
		AddViewToScene( pAboveWaterView );
	}
	else
	{
		CRefPtr<CUnderWaterViewDeferred> pUnderWaterView = new CUnderWaterViewDeferred( this );
		pUnderWaterView->Setup( view, bDrawSkybox, fogVolumeInfo, info );
		AddViewToScene( pUnderWaterView );
	}
#else
	VisibleFogVolumeInfo_t fogVolumeInfo;
	render->GetVisibleFogVolume( view.origin, &fogVolumeInfo );

	WaterRenderInfo_t info;
	DetermineWaterRenderInfo( fogVolumeInfo, info );

	CRefPtr<CSimpleWorldViewDeferred> pNoWaterView = new CSimpleWorldViewDeferred( this );
	pNoWaterView->Setup( view, nClearFlags, bDrawSkybox, fogVolumeInfo, info );
	AddViewToScene( pNoWaterView );
#endif
}

void CDeferredViewRender::PerformLighting( const CViewSetup &view )
{
	bool bResetLightAccum = false;
	const bool bRadiosityEnabled = DEFCFG_ENABLE_RADIOSITY != 0 && deferred_radiosity_enable.GetBool();

	if ( bRadiosityEnabled )
		BeginRadiosity( view );

	if ( GetGlobalLight() != NULL )
	{
		struct defData_setGlobalLightState
		{
		public:
			lightData_Global_t state;

			static void Fire( defData_setGlobalLightState d )
			{
				GetDeferredExt()->CommitLightData_Global( d.state );
			};
		};

		defData_setGlobalLightState lightDataState;
		lightDataState.state = GetActiveGlobalLightState();

		if ( !GetLightingEditor()->IsEditorLightingActive() &&
			deferred_override_globalLight_enable.GetBool() )
		{
			lightDataState.state.bShadow = deferred_override_globalLight_shadow_enable.GetBool();
			UTIL_StringToVector( lightDataState.state.diff.AsVector3D().Base(), deferred_override_globalLight_diffuse.GetString() );
			UTIL_StringToVector( lightDataState.state.ambh.AsVector3D().Base(), deferred_override_globalLight_ambient_high.GetString() );
			UTIL_StringToVector( lightDataState.state.ambl.AsVector3D().Base(), deferred_override_globalLight_ambient_low.GetString() );

			lightDataState.state.bEnabled = ( lightDataState.state.diff.LengthSqr() > 0.01f ||
				lightDataState.state.ambh.LengthSqr() > 0.01f ||
				lightDataState.state.ambl.LengthSqr() > 0.01f );
		}

		QUEUE_FIRE( defData_setGlobalLightState, Fire, lightDataState );

		if ( lightDataState.state.bEnabled )
		{
			bool bShadowedGlobal = lightDataState.state.bShadow;

			if ( bShadowedGlobal )
			{
				Vector origins[2] = { view.origin, view.origin + lightDataState.state.vecLight.AsVector3D() * 1024 };
				render->ViewSetupVis( false, 2, origins );

				RenderCascadedShadows( view, bRadiosityEnabled );
			}
		}
		else
			bResetLightAccum = true;
	}
	else
		bResetLightAccum = true;

	CViewSetup lightingView = view;

	if ( building_cubemaps.GetBool() )
		engine->GetScreenSize( lightingView.width, lightingView.height );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->PushRenderTargetAndViewport( GetDefRT_Lightaccum() );

	if ( bResetLightAccum )
	{
		pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false );
	}
	else
		DrawLightPassFullscreen( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_GLOBAL ), lightingView.width, lightingView.height );

	pRenderContext.SafeRelease();

	GetLightingManager()->RenderLights( lightingView, this );

	if ( bRadiosityEnabled )
		EndRadiosity( view );

	pRenderContext.GetFrom( materials );
	pRenderContext->PopRenderTargetAndViewport();
}

static int GetSourceRadBufferIndex( const int index )
{
	Assert( index == 0 || index == 1 );

	const bool bFar = index == 1;
	const int iNumSteps = (bFar ? deferred_radiosity_propagate_count_far.GetInt() : deferred_radiosity_propagate_count.GetInt())
		+ (bFar ? deferred_radiosity_blur_count_far.GetInt() : deferred_radiosity_blur_count.GetInt());
	return ( iNumSteps % 2 == 0 ) ? 0 : 1;
}

void CDeferredViewRender::BeginRadiosity( const CViewSetup &view )
{
	Vector fwd;
	AngleVectors( view.angles, &fwd );

	float flAmtVertical = abs( DotProduct( fwd, Vector( 0, 0, 1 ) ) );
	flAmtVertical = RemapValClamped( flAmtVertical, 0, 1, 1, 0.5f );

	for ( int iCascade = 0; iCascade < 2; iCascade++ )
	{
		const bool bFar = iCascade == 1;
		const Vector gridSize( RADIOSITY_BUFFER_SAMPLES_XY, RADIOSITY_BUFFER_SAMPLES_XY,
								RADIOSITY_BUFFER_SAMPLES_Z );
		const Vector gridSizeHalf = gridSize / 2;
		const float gridStepSize = bFar ? RADIOSITY_BUFFER_GRID_STEP_SIZE_FAR
			: RADIOSITY_BUFFER_GRID_STEP_SIZE_CLOSE;
		const float flGridDistance = bFar ? RADIOSITY_BUFFER_GRID_STEP_DISTANCEMULT_FAR
			: RADIOSITY_BUFFER_GRID_STEP_DISTANCEMULT_CLOSE;

		Vector vecFwd;
		AngleVectors( view.angles, &vecFwd );

		m_vecRadiosityOrigin[iCascade] = view.origin
			+ vecFwd * gridStepSize * RADIOSITY_BUFFER_SAMPLES_XY * flGridDistance * flAmtVertical;

		for ( int i = 0; i < 3; i++ )
			m_vecRadiosityOrigin[iCascade][i] -= fmod( m_vecRadiosityOrigin[iCascade][i], gridStepSize );

		m_vecRadiosityOrigin[iCascade] -= gridSizeHalf * gridStepSize;

		const int iSourceBuffer = GetSourceRadBufferIndex( iCascade );
		static int iLastSourceBuffer[2] = { iSourceBuffer, GetSourceRadBufferIndex( 1 ) };

		const int clearSizeY = RADIOSITY_BUFFER_RES_Y / 2;
		const int clearOffset = (iCascade == 1) ? clearSizeY : 0;

		CMatRenderContextPtr pRenderContext( materials );

		pRenderContext->PushRenderTargetAndViewport( GetDefRT_RadiosityBuffer( iSourceBuffer ), NULL,
			0, clearOffset, RADIOSITY_BUFFER_RES_X, clearSizeY );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, false );
		pRenderContext->PopRenderTargetAndViewport();

		if ( iLastSourceBuffer[iCascade] != iSourceBuffer )
		{
			iLastSourceBuffer[iCascade] = iSourceBuffer;

			pRenderContext->PushRenderTargetAndViewport( GetDefRT_RadiosityBuffer( 1 - iSourceBuffer ), NULL,
				0, clearOffset, RADIOSITY_BUFFER_RES_X, clearSizeY );
			pRenderContext->ClearColor3ub( 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
			pRenderContext->PopRenderTargetAndViewport();

			pRenderContext->PushRenderTargetAndViewport( GetDefRT_RadiosityNormal( 1 - iSourceBuffer ), NULL,
				0, clearOffset, RADIOSITY_BUFFER_RES_X, clearSizeY );
			pRenderContext->ClearColor3ub( 127, 127, 127 );
			pRenderContext->ClearBuffers( true, false );
			pRenderContext->PopRenderTargetAndViewport();
		}

		pRenderContext->PushRenderTargetAndViewport( GetDefRT_RadiosityNormal( iSourceBuffer ), NULL,
			0, clearOffset, RADIOSITY_BUFFER_RES_X, clearSizeY );
		pRenderContext->ClearColor3ub( 127, 127, 127 );
		pRenderContext->ClearBuffers( true, false );
		pRenderContext->PopRenderTargetAndViewport();
	}

	UpdateRadiosityPosition();
}

void CDeferredViewRender::UpdateRadiosityPosition()
{
	struct defData_setupRadiosity
	{
	public:
		radiosityData_t data;

		static void Fire( defData_setupRadiosity d )
		{
			GetDeferredExt()->CommitRadiosityData( d.data );
		};
	};

	defData_setupRadiosity radSetup;
	radSetup.data.vecOrigin[0] = m_vecRadiosityOrigin[0];
	radSetup.data.vecOrigin[1] = m_vecRadiosityOrigin[1];

	QUEUE_FIRE( defData_setupRadiosity, Fire, radSetup );
}

void CDeferredViewRender::PerformRadiosityGlobal( const int iRadiosityCascade, const CViewSetup &view )
{
	const int iSourceBuffer = GetSourceRadBufferIndex( iRadiosityCascade );
	const int iOffsetY = (iRadiosityCascade == 1) ? RADIOSITY_BUFFER_RES_Y/2 : 0;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RADIOSITY_CASCADE, iRadiosityCascade );

	pRenderContext->PushRenderTargetAndViewport( GetDefRT_RadiosityBuffer( iSourceBuffer ), NULL,
		0, iOffsetY, RADIOSITY_BUFFER_VIEWPORT_SX, RADIOSITY_BUFFER_VIEWPORT_SY );
	pRenderContext->SetRenderTargetEx( 1, GetDefRT_RadiosityNormal( iSourceBuffer ) );

	pRenderContext->Bind( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_GLOBAL ) );
	GetRadiosityScreenGrid( iRadiosityCascade )->Draw();

	pRenderContext->PopRenderTargetAndViewport();
}

void CDeferredViewRender::EndRadiosity( const CViewSetup &view )
{
	const int iNumPropagateSteps[2] = { deferred_radiosity_propagate_count.GetInt(),
		deferred_radiosity_propagate_count_far.GetInt() };
	const int iNumBlurSteps[2] = { deferred_radiosity_blur_count.GetInt(),
		deferred_radiosity_blur_count_far.GetInt() };

	IMaterial *pPropagateMat[2] = {
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ),
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ),
	};

	IMaterial *pBlurMat[2] = {
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ),
		GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ),
	};

	for ( int iCascade = 0; iCascade < 2; iCascade++ )
	{
		bool bSecondDestBuffer = GetSourceRadBufferIndex( iCascade ) == 0;
		const int iOffsetY = (iCascade==1) ? RADIOSITY_BUFFER_RES_Y / 2 : 0;

		for ( int i = 0; i < iNumPropagateSteps[iCascade]; i++ )
		{
			const int index = bSecondDestBuffer ? 1 : 0;
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PushRenderTargetAndViewport( GetDefRT_RadiosityBuffer( index ), NULL,
				0, iOffsetY, RADIOSITY_BUFFER_VIEWPORT_SX, RADIOSITY_BUFFER_VIEWPORT_SY );
			pRenderContext->SetRenderTargetEx( 1, GetDefRT_RadiosityNormal( index ) );

			pRenderContext->Bind( pPropagateMat[ 1 - index ] );

			GetRadiosityScreenGrid( iCascade )->Draw();

			pRenderContext->PopRenderTargetAndViewport();
			bSecondDestBuffer = !bSecondDestBuffer;
		}

		for ( int i = 0; i < iNumBlurSteps[iCascade]; i++ )
		{
			const int index = bSecondDestBuffer ? 1 : 0;
			CMatRenderContextPtr pRenderContext( materials );
			pRenderContext->PushRenderTargetAndViewport( GetDefRT_RadiosityBuffer( index ), NULL,
				0, iOffsetY, RADIOSITY_BUFFER_VIEWPORT_SX, RADIOSITY_BUFFER_VIEWPORT_SY );
			pRenderContext->SetRenderTargetEx( 1, GetDefRT_RadiosityNormal( index ) );

			pRenderContext->Bind( pBlurMat[ 1 - index ] );

			GetRadiosityScreenGrid( iCascade )->Draw();

			pRenderContext->PopRenderTargetAndViewport();
			bSecondDestBuffer = !bSecondDestBuffer;
		}
	}

#if ( DEFCFG_DEFERRED_SHADING == 0 )
	DrawLightPassFullscreen( GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_BLEND ),
		view.width, view.height );
#endif
}

void CDeferredViewRender::DebugRadiosity( const CViewSetup &view )
{
#if 0
	Vector tmp[3] = { m_vecRadiosityOrigin[1],
		m_vecRadiosityOrigin[1],
		m_vecRadiosityOrigin[1] };

	const int directions[3][2] = {
		1, 2,
		0, 2,
		0, 1,
	};

	const Vector vecCross[3] = {
		Vector( 1, 0, 0 ),
		Vector( 0, 1, 0 ),
		Vector( 0, 0, 1 ),
	};

	const int iColors[3][3] = {
		255, 0, 0,
		0, 255, 0,
		0, 0, 255,
	};

	for ( int i = 0; i < 3; i++ )
	{
		for ( int x = 0; x < RADIOSITY_BUFFER_GRID_STEP_SIZE_FAR; x++ )
		{
			Vector tmp2 = tmp[i];

			for ( int y = 0; y < RADIOSITY_BUFFER_GRID_STEP_SIZE_FAR; y++ )
			{
				NDebugOverlay::Line( tmp2, tmp2 + vecCross[i] * RADIOSITY_BUFFER_GRID_STEP_DISTANCEMULT_FAR * RADIOSITY_BUFFER_GRID_STEP_SIZE_FAR,
					iColors[i][0], iColors[i][1], iColors[i][2], true, -1 );

				tmp2[ directions[i][1] ] += RADIOSITY_BUFFER_GRID_STEP_SIZE_FAR;
			}

			tmp[i][ directions[i][0] ] += RADIOSITY_BUFFER_GRID_STEP_SIZE_FAR;
		}
	}
#endif

	IMaterial *pMatDbgRadGrid = GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_DEBUG );

	if ( m_hRadiosityDebugMeshList[0].Count() == 0 )
	{
		for ( int iCascade = 0; iCascade < 2; iCascade++ )
		{
			const bool bFar = iCascade == 1;
			const float flGridSize = bFar ? RADIOSITY_BUFFER_GRID_STEP_SIZE_FAR : RADIOSITY_BUFFER_GRID_STEP_SIZE_CLOSE;
			const float flCubesize = flGridSize * 0.15f;
			const Vector directions[3] = {
				Vector( flGridSize, 0, 0 ),
				Vector( 0, flGridSize, 0 ),
				Vector( 0, 0, flGridSize ),
			};

			const float flUVOffsetY = bFar ? 0.5f : 0.0f;

			int nMaxVerts, nMaxIndices;
			CMatRenderContextPtr pRenderContext( materials );
			CMeshBuilder meshBuilder;

			IMesh *pMesh = pRenderContext->CreateStaticMesh( VERTEX_POSITION | VERTEX_TEXCOORD_SIZE( 0, 2 ),
				TEXTURE_GROUP_OTHER,
				pMatDbgRadGrid );
			m_hRadiosityDebugMeshList[iCascade].AddToTail( pMesh );

			IMesh *pMeshDummy = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMatDbgRadGrid );
			pRenderContext->GetMaxToRender( pMeshDummy, false, &nMaxVerts, &nMaxIndices );
			pMeshDummy->Draw();

			int nMaxCubes = nMaxIndices / 36;
			if ( nMaxCubes > nMaxVerts / 24 )
				nMaxCubes = nMaxVerts / 24;

			int nRenderRemaining = nMaxCubes;
			meshBuilder.Begin( pMesh, MATERIAL_QUADS, nMaxCubes * 6 );

			const Vector2D flUVTexelSize( 1.0f / RADIOSITY_BUFFER_RES_X,
				1.0f / RADIOSITY_BUFFER_RES_Y );
			const Vector2D flUVTexelSizeHalf = flUVTexelSize * 0.5f;
			const Vector2D flUVGridSize =
				Vector2D( RADIOSITY_UVRATIO_X, RADIOSITY_UVRATIO_Y )
				* 1.0f / RADIOSITY_BUFFER_GRIDS_PER_AXIS;

			for ( int x = 0; x < RADIOSITY_BUFFER_SAMPLES_XY; x++ )
			for ( int y = 0; y < RADIOSITY_BUFFER_SAMPLES_XY; y++ )
			for ( int z = 0; z < RADIOSITY_BUFFER_SAMPLES_Z; z++ )
			{
				if ( nRenderRemaining <= 0 )
				{
					nRenderRemaining = nMaxCubes;
					meshBuilder.End();
					pMesh = pRenderContext->CreateStaticMesh( VERTEX_POSITION | VERTEX_TEXCOORD_SIZE( 0, 2 ),
								TEXTURE_GROUP_OTHER,
								GetDeferredManager()->GetDeferredMaterial( DEF_MAT_LIGHT_RADIOSITY_DEBUG ) );
					m_hRadiosityDebugMeshList[iCascade].AddToTail( pMesh );
					meshBuilder.Begin( pMesh, MATERIAL_QUADS, nMaxCubes * 6 );
				}

				int grid_x = z % RADIOSITY_BUFFER_GRIDS_PER_AXIS;
				int grid_y = z / RADIOSITY_BUFFER_GRIDS_PER_AXIS;

				float flUV[2] = {
					grid_x * flUVGridSize.x + x * flUVTexelSize.x + flUVTexelSizeHalf.x,
					grid_y * flUVGridSize.y + y * flUVTexelSize.y + flUVTexelSizeHalf.y + flUVOffsetY,
				};

				DrawCube( meshBuilder, directions[ 0 ] * x
					+ directions[ 1 ] * y
					+ directions[ 2 ] * z,
					flCubesize,
					flUV );

				nRenderRemaining--;
			}

			if ( nRenderRemaining != nMaxCubes )
				meshBuilder.End();
		}
	}

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMatDbgRadGrid );

	for ( int iCascade = 0; iCascade < 2; iCascade++ )
	{
		VMatrix pos;
		pos.SetupMatrixOrgAngles( m_vecRadiosityOrigin[iCascade], vec3_angle );

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		pRenderContext->LoadMatrix( pos );

		for ( int i = 0; i < m_hRadiosityDebugMeshList[iCascade].Count(); i++ )
			m_hRadiosityDebugMeshList[iCascade][ i ]->Draw();

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();
	}
}

void CDeferredViewRender::RenderCascadedShadows( const CViewSetup &view, const bool bEnableRadiosity )
{
	for ( int i = 0; i < SHADOW_NUM_CASCADES; i++ )
	{
		const cascade_t &cascade = GetCascadeInfo(i);
		const bool bDoRadiosity = bEnableRadiosity && cascade.bOutputRadiosityData;
		const int iRadTarget = cascade.iRadiosityCascadeTarget;

#if CSM_USE_COMPOSITED_TARGET == 0
		int textureIndex = i;
#else
		int textureIndex = 0;
#endif

		CRefPtr<COrthoShadowView> pOrthoDepth = new COrthoShadowView( this, i );
		pOrthoDepth->Setup( view, GetShadowDepthRT_Ortho( textureIndex ), GetShadowColorRT_Ortho( textureIndex ) );
		if ( bDoRadiosity )
		{
			pOrthoDepth->SetRadiosityOutputEnabled( true );
			pOrthoDepth->SetupRadiosityTargets( GetRadiosityAlbedoRT_Ortho( textureIndex ),
				GetRadiosityNormalRT_Ortho( textureIndex ) );
		}
		AddViewToScene( pOrthoDepth );

		if ( bDoRadiosity )
			PerformRadiosityGlobal( iRadTarget, view );
	}
}

void CDeferredViewRender::DrawLightShadowView( const CViewSetup &view, int iDesiredShadowmap, def_light_t *l )
{
	CViewSetup setup;
	setup.origin = l->pos;
	setup.angles = l->ang;
	setup.m_bOrtho = false;
	setup.m_flAspectRatio = 1;
	setup.x = setup.y = 0;

	Vector origins[2] = { view.origin, l->pos };
	render->ViewSetupVis( false, 2, origins );

	switch ( l->iLighttype )
	{
	default:
		Assert( 0 );
		break;
	case DEFLIGHTTYPE_POINT:
		{
			CRefPtr<CDualParaboloidShadowView> pDPView0 = new CDualParaboloidShadowView( this, l, false );
			pDPView0->Setup( setup, GetShadowDepthRT_DP( iDesiredShadowmap ), GetShadowColorRT_DP( iDesiredShadowmap ) );
			AddViewToScene( pDPView0 );

			CRefPtr<CDualParaboloidShadowView> pDPView1 = new CDualParaboloidShadowView( this, l, true );
			pDPView1->Setup( setup, GetShadowDepthRT_DP( iDesiredShadowmap ), GetShadowColorRT_DP( iDesiredShadowmap ) );
			AddViewToScene( pDPView1 );
		}
		break;
	case DEFLIGHTTYPE_SPOT:
		{
			CRefPtr<CSpotLightShadowView> pProjView = new CSpotLightShadowView( this, l, iDesiredShadowmap );

			pProjView->Setup( setup, GetShadowDepthRT_Proj( iDesiredShadowmap ), GetShadowColorRT_Proj( iDesiredShadowmap ) );
			AddViewToScene( pProjView );
		}
		break;
	}
}

void CDeferredViewRender::DrawViewModels( const CViewSetup &view, bool drawViewmodel, bool bGBuffer )
{
	VPROF( "CViewRender::DrawViewModel" );
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

#ifdef PORTAL //in portal, we'd like a copy of the front buffer without the gun in it for use with the depth doubler
	g_pPortalRender->UpdateDepthDoublerTexture( view );
#endif

	bool bShouldDrawPlayerViewModel = ShouldDrawViewModel( drawViewmodel );
	bool bShouldDrawToolViewModels = ToolsEnabled();

	CMatRenderContextPtr pRenderContext( materials );

	PIXEVENT( pRenderContext, "DrawViewModels" );

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();

	CViewSetup viewModelSetup( view );
	viewModelSetup.zNear = view.zNearViewmodel;
	viewModelSetup.zFar = view.zFarViewmodel;
	viewModelSetup.fov = view.fovViewmodel;
	viewModelSetup.m_flAspectRatio = engine->GetScreenAspectRatio();

	ITexture *pRTColor = NULL;
	ITexture *pRTDepth = NULL;
	if( view.m_eStereoEye != STEREO_EYE_MONO )
	{
		pRTColor = g_pSourceVR->GetRenderTarget( (ISourceVirtualReality::VREye)(view.m_eStereoEye-1), ISourceVirtualReality::RT_Color );
		pRTDepth = g_pSourceVR->GetRenderTarget( (ISourceVirtualReality::VREye)(view.m_eStereoEye-1), ISourceVirtualReality::RT_Depth );
	}

	render->Push3DView( viewModelSetup, 0, pRTColor, GetFrustum(), pRTDepth );

	if ( bGBuffer )
	{
		const float flViewmodelScale = view.zFarViewmodel / view.zFar;
		CGBufferView::PushGBuffer( false, flViewmodelScale, false );
	}
	else
	{
		pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
			DEFERRED_RENDER_STAGE_COMPOSITION );
	}

#ifdef PORTAL //the depth range hack doesn't work well enough for the portal mod (and messing with the depth hack values makes some models draw incorrectly)
				//step up to a full depth clear if we're extremely close to a portal (in a portal environment)
	extern bool LocalPlayerIsCloseToPortal( void ); //defined in C_Portal_Player.cpp, abstracting to a single bool function to remove explicit dependence on c_portal_player.h/cpp, you can define the function as a "return true" in other build configurations at the cost of some perf
	bool bUseDepthHack = !LocalPlayerIsCloseToPortal();
	if( !bUseDepthHack )
		pRenderContext->ClearBuffers( false, true, false );
#else
	const bool bUseDepthHack = true;
#endif

	// FIXME: Add code to read the current depth range
	float depthmin = 0.0f;
	float depthmax = 1.0f;

	// HACK HACK:  Munge the depth range to prevent view model from poking into walls, etc.
	// Force clipped down range
	if( bUseDepthHack )
		pRenderContext->DepthRange( 0.0f, 0.1f );

	if ( bShouldDrawPlayerViewModel || bShouldDrawToolViewModels )
	{
		CUtlVector< IClientRenderable * > opaqueViewModelList( 32 );
		CUtlVector< IClientRenderable * > translucentViewModelList( 32 );

		ClientLeafSystem()->CollateViewModelRenderables( opaqueViewModelList, translucentViewModelList );

		if ( ToolsEnabled() && ( !bShouldDrawPlayerViewModel || !bShouldDrawToolViewModels ) )
		{
			int nOpaque = opaqueViewModelList.Count();
			for ( int i = nOpaque-1; i >= 0; --i )
			{
				IClientRenderable *pRenderable = opaqueViewModelList[ i ];
				bool bEntity = pRenderable->GetIClientUnknown()->GetBaseEntity();
				if ( ( bEntity && !bShouldDrawPlayerViewModel ) || ( !bEntity && !bShouldDrawToolViewModels ) )
				{
					opaqueViewModelList.FastRemove( i );
				}
			}

			int nTranslucent = translucentViewModelList.Count();
			for ( int i = nTranslucent-1; i >= 0; --i )
			{
				IClientRenderable *pRenderable = translucentViewModelList[ i ];
				bool bEntity = pRenderable->GetIClientUnknown()->GetBaseEntity();
				if ( ( bEntity && !bShouldDrawPlayerViewModel ) || ( !bEntity && !bShouldDrawToolViewModels ) )
				{
					translucentViewModelList.FastRemove( i );
				}
			}
		}

		if ( !UpdateRefractIfNeededByList( opaqueViewModelList ) && !bGBuffer )
		{
			UpdateRefractIfNeededByList( translucentViewModelList );
		}

		DrawRenderablesInList( opaqueViewModelList );
		if (!bGBuffer)
			DrawRenderablesInList( translucentViewModelList, STUDIO_TRANSPARENCY );
		else
			pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_DEFERRED_RENDER_STAGE,
			                                          DEFERRED_RENDER_STAGE_INVALID );
	}

	// Reset the depth range to the original values
	if( bUseDepthHack )
		pRenderContext->DepthRange( depthmin, depthmax );

	if ( bGBuffer )
	{
		CGBufferView::PopGBuffer();
	}

	render->PopView( GetFrustum() );

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}

void CDeferredViewRender::RenderView( const CViewSetup &view, int nClearFlags, int whatToDraw )
{
	m_UnderWaterOverlayMaterial.Shutdown();					// underwater view will set

	CViewSetup worldView = view;

	CLightingEditor *pLightEditor = GetLightingEditor();

	if ( pLightEditor->IsEditorActive() && !building_cubemaps.GetBool() )
		pLightEditor->GetEditorView( &worldView.origin, &worldView.angles );
	else
		pLightEditor->SetEditorView( &worldView.origin, &worldView.angles );

	m_CurrentView = worldView;

	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, true );
	VPROF( "CViewRender::RenderView" );
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	CMatRenderContextPtr pRenderContext( materials );
	ITexture *saveRenderTarget = pRenderContext->GetRenderTarget();
	pRenderContext.SafeRelease(); // don't want to hold for long periods in case in a locking active share thread mode

	g_pClientShadowMgr->AdvanceFrame();

	// Must be first
	render->SceneBegin();

	pRenderContext.GetFrom( materials );
	pRenderContext->TurnOnToneMapping();
	pRenderContext.SafeRelease();

	// clear happens here probably
	SetupMain3DView( worldView, nClearFlags );

	ProcessDeferredGlobals( worldView );
	GetLightingManager()->LightSetup( worldView );

	// Force it to clear the framebuffer if they're in solid space.
	if ( ( nClearFlags & VIEW_CLEAR_COLOR ) == 0 )
	{
		if ( enginetrace->GetPointContents( worldView.origin ) == CONTENTS_SOLID )
		{
			nClearFlags |= VIEW_CLEAR_COLOR;
		}
	}

	// Render world and all entities, particles, etc.
	ViewDrawSceneDeferred( worldView, nClearFlags, VIEW_MAIN, whatToDraw & RENDERVIEW_DRAWVIEWMODEL );

	// We can still use the 'current view' stuff set up in ViewDrawScene
	AllowCurrentViewAccess( true );

	// must happen before teardown
	pLightEditor->OnRender();

	GetLightingManager()->LightTearDown();

	engine->DrawPortals();

	DisableFog();

	// Finish scene
	render->SceneEnd();

	// Draw lightsources if enabled
	//render->DrawLights();

	RenderPlayerSprites();

	// Image-space motion blur
	if ( !building_cubemaps.GetBool() && worldView.m_bDoBloomAndToneMapping ) // We probably should use a different view. variable here
	{
		if ( ( mat_motion_blur_enabled.GetInt() ) && ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() >= 90 ) )
		{
			pRenderContext.GetFrom( materials );
			{
				PIXEVENT( pRenderContext, "DoImageSpaceMotionBlur" );
				DoImageSpaceMotionBlur( worldView, worldView.x, worldView.y, worldView.width, worldView.height );
			}
			pRenderContext.SafeRelease();
		}
	}

	GetClientModeNormal()->DoPostScreenSpaceEffects( &worldView );

	DrawUnderwaterOverlay();

	PixelVisibility_EndScene();

	// Draw fade over entire screen if needed
	byte color[4];
	bool blend;
	vieweffects->GetFadeParams( &color[0], &color[1], &color[2], &color[3], &blend );

	// Draw an overlay to make it even harder to see inside smoke particle systems.
	DrawSmokeFogOverlay();

	// Overlay screen fade on entire screen
	IMaterial* pMaterial = blend ? m_ModulateSingleColor : m_TranslucentSingleColor;
	render->ViewDrawFade( color, pMaterial );
	PerformScreenOverlay( worldView.x, worldView.y, worldView.width, worldView.height );

	// Prevent sound stutter if going slow
	engine->Sound_ExtraUpdate();

	if ( !building_cubemaps.GetBool() && worldView.m_bDoBloomAndToneMapping )
	{
		pRenderContext.GetFrom( materials );
		{
			PIXEVENT( pRenderContext, "DoEnginePostProcessing" );

			bool bFlashlightIsOn = false;
			C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
			if ( pLocal )
			{
				bFlashlightIsOn = pLocal->IsEffectActive( EF_DIMLIGHT );
			}
			DoEnginePostProcessing( worldView.x, worldView.y, worldView.width, worldView.height, bFlashlightIsOn );
		}
		pRenderContext.SafeRelease();
	}

	//SSE
	g_ShaderEditorSystem->CustomPostRender();

	// And here are the screen-space effects

	if ( IsPC() )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "GrabPreColorCorrectedFrame" );

		// Grab the pre-color corrected frame for editing purposes
		engine->GrabPreColorCorrectedFrame( worldView.x, worldView.y, worldView.width, worldView.height );
	}

	PerformScreenSpaceEffects( 0, 0, worldView.width, worldView.height );

	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER )
	{
		pRenderContext.GetFrom( materials );
		pRenderContext->SetToneMappingScaleLinear(Vector(1,1,1));
		pRenderContext.SafeRelease();
	}

	CleanupMain3DView( worldView );

	pRenderContext = materials->GetRenderContext();
	pRenderContext->SetRenderTarget( saveRenderTarget );
	pRenderContext.SafeRelease();

	// Draw the overlay
	if ( m_bDrawOverlay )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "DrawOverlay" );

		// This allows us to be ok if there are nested overlay views
		const CViewSetup currentView = m_CurrentView;
		CViewSetup tempView = m_OverlayViewSetup;
		tempView.fov = ScaleFOVByWidthRatio( tempView.fov, tempView.m_flAspectRatio / ( 4.0f / 3.0f ) );
		tempView.m_bDoBloomAndToneMapping = false;	// FIXME: Hack to get Mark up and running
		m_bDrawOverlay = false;
		RenderView( tempView, m_OverlayClearFlags, m_OverlayDrawFlags );
		m_CurrentView = currentView;
	}

	if ( mat_viewportupscale.GetBool() && mat_viewportscale.GetFloat() < 1.0f )
	{
		CMatRenderContextPtr pRenderContext( materials );

		ITexture	*pFullFrameFB1 = materials->FindTexture( "_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET );
		IMaterial	*pCopyMaterial = materials->FindMaterial( "dev/upscale", TEXTURE_GROUP_OTHER );
		pCopyMaterial->IncrementReferenceCount();

		Rect_t	DownscaleRect, UpscaleRect;

		DownscaleRect.x = worldView.x;
		DownscaleRect.y = worldView.y;
		DownscaleRect.width = worldView.width;
		DownscaleRect.height = worldView.height;

		UpscaleRect.x = worldView.m_nUnscaledX;
		UpscaleRect.y = worldView.m_nUnscaledY;
		UpscaleRect.width = worldView.m_nUnscaledWidth;
		UpscaleRect.height = worldView.m_nUnscaledHeight;

		pRenderContext->CopyRenderTargetToTextureEx( pFullFrameFB1, 0, &DownscaleRect, &DownscaleRect );
		pRenderContext->DrawScreenSpaceRectangle( pCopyMaterial, UpscaleRect.x, UpscaleRect.y, UpscaleRect.width, UpscaleRect.height,
			DownscaleRect.x, DownscaleRect.y, DownscaleRect.x+DownscaleRect.width-1, DownscaleRect.y+DownscaleRect.height-1,
			pFullFrameFB1->GetActualWidth(), pFullFrameFB1->GetActualHeight() );

		pCopyMaterial->DecrementReferenceCount();
	}

	// if we're in VR mode we might need to override the render target
	if( UseVR() )
	{
		saveRenderTarget = g_pSourceVR->GetRenderTarget( (ISourceVirtualReality::VREye)(worldView.m_eStereoEye - 1), ISourceVirtualReality::RT_Color );
	}

	// Draw the 2D graphics
	render->Push2DView( worldView, 0, saveRenderTarget, GetFrustum() );

	Render2DEffectsPreHUD( worldView );

	if ( whatToDraw & RENDERVIEW_DRAWHUD )
	{
		VPROF_BUDGET( "VGui_DrawHud", VPROF_BUDGETGROUP_OTHER_VGUI );
		int viewWidth = worldView.m_nUnscaledWidth;
		int viewHeight = worldView.m_nUnscaledHeight;
		int viewActualWidth = worldView.m_nUnscaledWidth;
		int viewActualHeight = worldView.m_nUnscaledHeight;
		int viewX = worldView.m_nUnscaledX;
		int viewY = worldView.m_nUnscaledY;
		int viewFramebufferX = 0;
		int viewFramebufferY = 0;
		int viewFramebufferWidth = viewWidth;
		int viewFramebufferHeight = viewHeight;
		bool bClear = false;
		bool bPaintMainMenu = false;
		ITexture *pTexture = NULL;
		if( UseVR() )
		{
			if( g_ClientVirtualReality.ShouldRenderHUDInWorld() )
			{
				pTexture = materials->FindTexture( "_rt_gui", NULL, false );
				if( pTexture )
				{
					bPaintMainMenu = true;
					bClear = true;
					viewX = 0;
					viewY = 0;
					viewActualWidth = pTexture->GetActualWidth();
					viewActualHeight = pTexture->GetActualHeight();

					vgui::surface()->GetScreenSize( viewWidth, viewHeight );

					viewFramebufferX = 0;
					if( worldView.m_eStereoEye == STEREO_EYE_RIGHT && !saveRenderTarget )
						viewFramebufferX = viewFramebufferWidth;
					viewFramebufferY = 0;
				}
			}
			else
			{
				viewFramebufferX = worldView.m_eStereoEye == STEREO_EYE_RIGHT ? viewWidth : 0;
				viewFramebufferY = 0;
			}
		}

		// Get the render context out of materials to avoid some debug stuff.
		// WARNING THIS REQUIRES THE .SafeRelease below or it'll never release the ref
		pRenderContext = materials->GetRenderContext();

		// clear depth in the backbuffer before we push the render target
		if( bClear )
		{
			pRenderContext->ClearBuffers( false, true, true );
		}

		// constrain where VGUI can render to the view
		pRenderContext->PushRenderTargetAndViewport( pTexture, NULL, viewX, viewY, viewActualWidth, viewActualHeight );
		// If drawing off-screen, force alpha for that pass
		if (pTexture)
		{
			pRenderContext->OverrideAlphaWriteEnable( true, true );
		}

		// let vgui know where to render stuff for the forced-to-framebuffer panels
		if( UseVR() )
		{
			g_pMatSystemSurface->SetFullscreenViewportAndRenderTarget( viewFramebufferX, viewFramebufferY, viewFramebufferWidth, viewFramebufferHeight, saveRenderTarget );
		}

		// clear the render target if we need to
		if( bClear )
		{
			pRenderContext->ClearColor4ub( 0, 0, 0, 0 );
			pRenderContext->ClearBuffers( true, false );
		}
		pRenderContext.SafeRelease();

		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "VGui_DrawHud", __FUNCTION__ );

		// paint the vgui screen
		VGui_PreRender();

		// Make sure the client .dll root panel is at the proper point before doing the "SolveTraverse" calls
		vgui::VPANEL root = enginevgui->GetPanel( PANEL_CLIENTDLL );
		if ( root != 0 )
		{
			vgui::ipanel()->SetSize( root, viewWidth, viewHeight );
		}
		// Same for client .dll tools
		root = enginevgui->GetPanel( PANEL_CLIENTDLL_TOOLS );
		if ( root != 0 )
		{
			vgui::ipanel()->SetSize( root, viewWidth, viewHeight );
		}

		// The crosshair, etc. needs to get at the current setup stuff
		AllowCurrentViewAccess( true );

		// Draw the in-game stuff based on the actual viewport being used
		render->VGui_Paint( PAINT_INGAMEPANELS );

		// maybe paint the main menu and cursor too if we're in stereo hud mode
		if( bPaintMainMenu )
			render->VGui_Paint( PAINT_UIPANELS | PAINT_CURSOR );

		AllowCurrentViewAccess( false );

		VGui_PostRender();

		g_pClientMode->PostRenderVGui();
		pRenderContext = materials->GetRenderContext();
		if (pTexture)
		{
			pRenderContext->OverrideAlphaWriteEnable( false, true );
		}
		pRenderContext->PopRenderTargetAndViewport();

		if ( UseVR() )
		{
			// figure out if we really want to draw the HUD based on freeze cam
			C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
			bool bInFreezeCam = ( pPlayer && pPlayer->GetObserverMode() == OBS_MODE_FREEZECAM );

			// draw the HUD after the view model so its "I'm closer" depth queues work right.
			if( !bInFreezeCam && g_ClientVirtualReality.ShouldRenderHUDInWorld() )
			{
				// Now we've rendered the HUD to its texture, actually get it on the screen.
				// Since we're drawing it as a 3D object, we need correctly set up frustum, etc.
				int ClearFlags = 0;
				SetupMain3DView( worldView, ClearFlags );

				// TODO - a bit of a shonky test - basically trying to catch the main menu, the briefing screen, the loadout screen, etc.
				bool bTranslucent = !g_pMatSystemSurface->IsCursorVisible();
				g_ClientVirtualReality.RenderHUDQuad( g_pClientMode->ShouldBlackoutAroundHUD(), bTranslucent );
				CleanupMain3DView( worldView );
			}
		}

		pRenderContext->Flush();
		pRenderContext.SafeRelease();
	}

	CDebugViewRender::Draw2DDebuggingInfo( worldView );

	Render2DEffectsPostHUD( worldView );

	// We can no longer use the 'current view' stuff set up in ViewDrawScene
	AllowCurrentViewAccess( false );

	if ( IsPC() )
	{
		CDebugViewRender::GenerateOverdrawForTesting();
	}

	render->PopView( GetFrustum() );
	FlushWorldLists();

	m_CurrentView = worldView;
}

struct defData_setGlobals
{
public:
	Vector orig, fwd;
	float zDists[2];
	VMatrix frustumDeltas;
#if DEFCFG_BILATERAL_DEPTH_TEST
	VMatrix worldCameraDepthTex;
#endif

	static void Fire( defData_setGlobals d )
	{
		IDeferredExtension *pDef = GetDeferredExt();
		pDef->CommitOrigin( d.orig );
		pDef->CommitViewForward( d.fwd );
		pDef->CommitZDists( d.zDists[0], d.zDists[1] );
		pDef->CommitFrustumDeltas( d.frustumDeltas );
#if DEFCFG_BILATERAL_DEPTH_TEST
		pDef->CommitWorldToCameraDepthTex( d.worldCameraDepthTex );
#endif
	};
};

void CDeferredViewRender::ProcessDeferredGlobals( const CViewSetup &view )
{
	VMatrix matPerspective, matView, matViewProj, screen2world;
	matView.Identity();
	matView.SetupMatrixOrgAngles( vec3_origin, view.angles );

	MatrixSourceToDeviceSpace( matView );
	g_ShaderEditorSystem->SetMainViewMatrix( matView );

	matView = matView.Transpose3x3();
	Vector viewPosition;

	Vector3DMultiply( matView, view.origin, viewPosition );
	matView.SetTranslation( -viewPosition );
	MatrixBuildPerspectiveX( matPerspective, view.fov, view.m_flAspectRatio,
		view.zNear, view.zFar );
	MatrixMultiply( matPerspective, matView, matViewProj );

	MatrixInverseGeneral( matViewProj, screen2world );

	GetLightingManager()->SetRenderConstants( screen2world, view );

	Vector frustum_c0, frustum_cc, frustum_1c;
	float projDistance = 1.0f;
	Vector3DMultiplyPositionProjective( screen2world, Vector(0,projDistance,projDistance), frustum_c0 );
	Vector3DMultiplyPositionProjective( screen2world, Vector(0,0,projDistance), frustum_cc );
	Vector3DMultiplyPositionProjective( screen2world, Vector(projDistance,0,projDistance), frustum_1c );

	frustum_c0 -= view.origin;
	frustum_cc -= view.origin;
	frustum_1c -= view.origin;

	Vector frustum_up = frustum_c0 - frustum_cc;
	Vector frustum_right = frustum_1c - frustum_cc;

	frustum_cc /= view.zFar;
	frustum_right /= view.zFar;
	frustum_up /= view.zFar;

	defData_setGlobals data;
	data.orig = view.origin;
	AngleVectors( view.angles, &data.fwd );
	data.zDists[0] = view.zNear;
	data.zDists[1] = view.zFar;

	data.frustumDeltas.Identity();
	data.frustumDeltas.SetBasisVectors( frustum_cc, frustum_right, frustum_up );
	data.frustumDeltas = data.frustumDeltas.Transpose3x3();

#if DEFCFG_BILATERAL_DEPTH_TEST
	VMatrix matWorldToCameraDepthTex;
	MatrixBuildScale( matWorldToCameraDepthTex, 0.5f, -0.5f, 1.0f );
	matWorldToCameraDepthTex[0][3] = matWorldToCameraDepthTex[1][3] = 0.5f;
	MatrixMultiply( matWorldToCameraDepthTex, matViewProj, matWorldToCameraDepthTex );

	data.worldCameraDepthTex = matWorldToCameraDepthTex.Transpose();
#endif

	QUEUE_FIRE( defData_setGlobals, Fire, data );
}

IMesh *CDeferredViewRender::GetRadiosityScreenGrid( const int iCascade )
{
	if ( m_pMesh_RadiosityScreenGrid[iCascade] == NULL )
	{
		Assert( m_pMesh_RadiosityScreenGrid[iCascade] == NULL );

		const bool bFar = iCascade == 1;

		m_pMesh_RadiosityScreenGrid[iCascade] = CreateRadiosityScreenGrid(
			Vector2D( 0, (bFar?0.5f:0) ),
			bFar ? RADIOSITY_BUFFER_GRID_STEP_SIZE_FAR : RADIOSITY_BUFFER_GRID_STEP_SIZE_CLOSE );
	}

	Assert( m_pMesh_RadiosityScreenGrid[iCascade] != NULL );

	return m_pMesh_RadiosityScreenGrid[iCascade];
}

IMesh *CDeferredViewRender::CreateRadiosityScreenGrid( const Vector2D &vecViewportBase,
	const float flWorldStepSize )
{
	VertexFormat_t format = VERTEX_POSITION
		| VERTEX_TEXCOORD_SIZE( 0, 4 )
		| VERTEX_TEXCOORD_SIZE( 1, 4 )
		| VERTEX_TANGENT_S;

	const float flTexelGridMargin = 1.5f / RADIOSITY_BUFFER_SAMPLES_XY;
	const float flTexelHalf[2] = { 0.5f / RADIOSITY_BUFFER_VIEWPORT_SX,
		0.5f / RADIOSITY_BUFFER_VIEWPORT_SY };

	const float flLocalCoordSingle = 1.0f / RADIOSITY_BUFFER_GRIDS_PER_AXIS;
	const float flLocalCoords[4][2] = {
		0, 0,
		flLocalCoordSingle, 0,
		flLocalCoordSingle, flLocalCoordSingle,
		0, flLocalCoordSingle,
	};

	CMatRenderContextPtr pRenderContext( materials );
	IMesh *pRet = pRenderContext->CreateStaticMesh(
		format, TEXTURE_GROUP_OTHER );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pRet, MATERIAL_QUADS,
		RADIOSITY_BUFFER_GRIDS_PER_AXIS * RADIOSITY_BUFFER_GRIDS_PER_AXIS );

	float flGridOrigins[RADIOSITY_BUFFER_SAMPLES_Z][2];
	for ( int i = 0; i < RADIOSITY_BUFFER_SAMPLES_Z; i++ )
	{
		int x = i % RADIOSITY_BUFFER_GRIDS_PER_AXIS;
		int y = i / RADIOSITY_BUFFER_GRIDS_PER_AXIS;

		flGridOrigins[i][0] = x * flLocalCoordSingle + flTexelHalf[0];
		flGridOrigins[i][1] = y * flLocalCoordSingle + flTexelHalf[1];
	}

	const float flGridSize = flWorldStepSize * RADIOSITY_BUFFER_SAMPLES_XY;
	const float flLocalGridSize[4][2] = {
		0, 0,
		flGridSize, 0,
		flGridSize, flGridSize,
		0, flGridSize,
	};
	const float flLocalGridLimits[4][2] = {
		-flTexelGridMargin, -flTexelGridMargin,
		1 + flTexelGridMargin, -flTexelGridMargin,
		1 + flTexelGridMargin, 1 + flTexelGridMargin,
		-flTexelGridMargin, 1 + flTexelGridMargin,
	};

	for ( int x = 0; x < RADIOSITY_BUFFER_GRIDS_PER_AXIS; x++ )
	{
		for ( int y = 0; y < RADIOSITY_BUFFER_GRIDS_PER_AXIS; y++ )
		{
			const int iIndexLocal = x + y * RADIOSITY_BUFFER_GRIDS_PER_AXIS;
			const int iIndicesOne[2] = { Min( RADIOSITY_BUFFER_SAMPLES_Z - 1, iIndexLocal + 1 ), Max( 0, iIndexLocal - 1 ) };

			for ( int q = 0; q < 4; q++ )
			{
				meshBuilder.Position3f(
					(x * flLocalCoordSingle + flLocalCoords[q][0]) * 2 - flLocalCoordSingle * RADIOSITY_BUFFER_GRIDS_PER_AXIS,
					flLocalCoordSingle * RADIOSITY_BUFFER_GRIDS_PER_AXIS - (y * flLocalCoordSingle + flLocalCoords[q][1]) * 2,
					0 );

				meshBuilder.TexCoord4f( 0,
					(flGridOrigins[iIndexLocal][0] + flLocalCoords[q][0]) * RADIOSITY_UVRATIO_X + vecViewportBase.x,
					(flGridOrigins[iIndexLocal][1] + flLocalCoords[q][1]) * RADIOSITY_UVRATIO_Y + vecViewportBase.y,
					flLocalGridLimits[q][0],
					flLocalGridLimits[q][1] );

				meshBuilder.TexCoord4f( 1,
					(flGridOrigins[iIndicesOne[0]][0] + flLocalCoords[q][0]) * RADIOSITY_UVRATIO_X + vecViewportBase.x,
					(flGridOrigins[iIndicesOne[0]][1] + flLocalCoords[q][1]) * RADIOSITY_UVRATIO_Y + vecViewportBase.y,
					(flGridOrigins[iIndicesOne[1]][0] + flLocalCoords[q][0]) * RADIOSITY_UVRATIO_X + vecViewportBase.x,
					(flGridOrigins[iIndicesOne[1]][1] + flLocalCoords[q][1]) * RADIOSITY_UVRATIO_Y + vecViewportBase.y );

				meshBuilder.TangentS3f( flLocalGridSize[q][0],
					flLocalGridSize[q][1],
					iIndexLocal * flWorldStepSize );

				meshBuilder.AdvanceVertex();
			}
		}
	}

	meshBuilder.End();

	return pRet;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CBaseWaterViewDeferred::CalcWaterEyeAdjustments( const VisibleFogVolumeInfo_t &fogInfo,
											 float &newWaterHeight, float &waterZAdjust, bool bSoftwareUserClipPlane )
{
	if( !bSoftwareUserClipPlane )
	{
		newWaterHeight = fogInfo.m_flWaterHeight;
		waterZAdjust = 0.0f;
		return;
	}

	newWaterHeight = fogInfo.m_flWaterHeight;
	float eyeToWaterZDelta = origin[2] - fogInfo.m_flWaterHeight;
	float epsilon = r_eyewaterepsilon.GetFloat();
	waterZAdjust = 0.0f;
	if( fabs( eyeToWaterZDelta ) < epsilon )
	{
		if( eyeToWaterZDelta > 0 )
		{
			newWaterHeight = origin[2] - epsilon;
		}
		else
		{
			newWaterHeight = origin[2] + epsilon;
		}
		waterZAdjust = newWaterHeight - fogInfo.m_flWaterHeight;
	}

	//	Warning( "view.origin[2]: %f newWaterHeight: %f fogInfo.m_flWaterHeight: %f waterZAdjust: %f\n",
	//		( float )view.origin[2], newWaterHeight, fogInfo.m_flWaterHeight, waterZAdjust );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CBaseWaterViewDeferred::CSoftwareIntersectionView::Setup( bool bAboveWater )
{
	BaseClass::Setup( *GetOuter() );

	m_DrawFlags = 0;
	m_DrawFlags = ( bAboveWater ) ? DF_RENDER_UNDERWATER : DF_RENDER_ABOVEWATER;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CBaseWaterViewDeferred::CSoftwareIntersectionView::Draw()
{
	PushComposite();
	DrawSetup( GetOuter()->m_waterHeight, m_DrawFlags, GetOuter()->m_waterZAdjust );
	DrawExecute( GetOuter()->m_waterHeight, CurrentViewID(), GetOuter()->m_waterZAdjust );
	PopComposite();
}

//-----------------------------------------------------------------------------
// Draws the scene when the view point is above the level of the water
//-----------------------------------------------------------------------------
void CAboveWaterViewDeferred::Setup( const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& waterInfo )
{
	BaseClass::Setup( view );

	m_bSoftwareUserClipPlane = g_pMaterialSystemHardwareConfig->UseFastClipping();

	CalcWaterEyeAdjustments( fogInfo, m_waterHeight, m_waterZAdjust, m_bSoftwareUserClipPlane );

	// BROKEN STUFF!
	if ( m_waterZAdjust == 0.0f )
	{
		m_bSoftwareUserClipPlane = false;
	}

	m_DrawFlags = DF_RENDER_ABOVEWATER | DF_DRAW_ENTITITES;
	m_ClearFlags = VIEW_CLEAR_DEPTH;

#ifdef PORTAL
	if( g_pPortalRender->ShouldObeyStencilForClears() )
		m_ClearFlags |= VIEW_CLEAR_OBEY_STENCIL;
#endif

	if ( bDrawSkybox )
	{
		m_DrawFlags |= DF_DRAWSKYBOX;
	}

	if ( waterInfo.m_bDrawWaterSurface )
	{
		m_DrawFlags |= DF_RENDER_WATER;
	}
	if ( !waterInfo.m_bRefract && !waterInfo.m_bOpaqueWater )
	{
		m_DrawFlags |= DF_RENDER_UNDERWATER;
	}

	m_fogInfo = fogInfo;
	m_waterInfo = waterInfo;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CAboveWaterViewDeferred::Draw()
{
	VPROF( "CViewRender::ViewDrawScene_EyeAboveWater" );

	// eye is outside of water

	CMatRenderContextPtr pRenderContext( materials );

	// render the reflection
	if( m_waterInfo.m_bReflect )
	{
		m_ReflectionView.Setup( m_waterInfo.m_bReflectEntities );
		m_pMainView->AddViewToScene( &m_ReflectionView );
	}

	bool bViewIntersectsWater = false;

	// render refraction
	if ( m_waterInfo.m_bRefract )
	{
		m_RefractionView.Setup();
		m_pMainView->AddViewToScene( &m_RefractionView );

		if( !m_bSoftwareUserClipPlane )
		{
			bViewIntersectsWater = DoesViewPlaneIntersectWater( m_fogInfo.m_flWaterHeight, m_fogInfo.m_nVisibleFogVolume );
		}
	}
	else if ( !( m_DrawFlags & DF_DRAWSKYBOX ) )
	{
		m_ClearFlags |= VIEW_CLEAR_COLOR;
	}

#ifdef PORTAL
	if( g_pPortalRender->ShouldObeyStencilForClears() )
		m_ClearFlags |= VIEW_CLEAR_OBEY_STENCIL;
#endif

	// NOTE!!!!!  YOU CAN ONLY DO THIS IF YOU HAVE HARDWARE USER CLIP PLANES!!!!!!
	bool bHardwareUserClipPlanes = !g_pMaterialSystemHardwareConfig->UseFastClipping();
	if( bViewIntersectsWater && bHardwareUserClipPlanes )
	{
		// This is necessary to keep the non-water fogged world from drawing underwater in
		// the case where we want to partially see into the water.
		m_DrawFlags |= DF_CLIP_Z | DF_CLIP_BELOW;
	}

	// render the world
	PushComposite();
	DrawSetup( m_waterHeight, m_DrawFlags, m_waterZAdjust );
	EnableWorldFog();
	DrawExecute( m_waterHeight, CurrentViewID(), m_waterZAdjust );
	PopComposite();

	if ( m_waterInfo.m_bRefract )
	{
		if ( m_bSoftwareUserClipPlane )
		{
			m_SoftwareIntersectionView.Setup( true );
			m_SoftwareIntersectionView.Draw( );
		}
		else if ( bViewIntersectsWater )
		{
			m_IntersectionView.Setup();
			m_pMainView->AddViewToScene( &m_IntersectionView );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CAboveWaterViewDeferred::CReflectionView::Setup( bool bReflectEntities )
{
	BaseClass::Setup( *GetOuter() );

	m_ClearFlags = VIEW_CLEAR_DEPTH;

	// NOTE: Clearing the color is unnecessary since we're drawing the skybox
	// and dest-alpha is never used in the reflection
	m_DrawFlags = DF_RENDER_REFLECTION | DF_CLIP_Z | DF_CLIP_BELOW |
		DF_RENDER_ABOVEWATER;

	// NOTE: This will cause us to draw the 2d skybox in the reflection
	// (which we want to do instead of drawing the 3d skybox)
	m_DrawFlags |= DF_DRAWSKYBOX;

	if( bReflectEntities )
	{
		m_DrawFlags |= DF_DRAW_ENTITITES;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CAboveWaterViewDeferred::CReflectionView::Draw()
{
#ifdef PORTAL
	g_pPortalRender->WaterRenderingHandler_PreReflection();
#endif

	// Store off view origin and angles and set the new view
	int nSaveViewID = CurrentViewID();
	SetupCurrentView( origin, angles, VIEW_REFLECTION );

	// Disable occlusion visualization in reflection
	bool bVisOcclusion = r_visocclusion.GetInt();
	r_visocclusion.SetValue( 0 );

	PushComposite();

	DrawSetup( GetOuter()->m_fogInfo.m_flWaterHeight, m_DrawFlags, 0.0f, GetOuter()->m_fogInfo.m_nVisibleFogVolumeLeaf );

	EnableWorldFog();
	DrawExecute( GetOuter()->m_fogInfo.m_flWaterHeight, VIEW_REFLECTION, 0.0f );

	PopComposite();

	r_visocclusion.SetValue( bVisOcclusion );

#ifdef PORTAL
	// deal with stencil
	g_pPortalRender->WaterRenderingHandler_PostReflection();
#endif

	// finish off the view and restore the previous view.
	SetupCurrentView( origin, angles, ( view_id_t )nSaveViewID );

	// This is here for multithreading
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Flush();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CAboveWaterViewDeferred::CRefractionView::Setup()
{
	BaseClass::Setup( *GetOuter() );

	m_ClearFlags = VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH;

	m_DrawFlags = DF_RENDER_REFRACTION | DF_CLIP_Z |
		DF_RENDER_UNDERWATER | DF_FUDGE_UP |
		DF_DRAW_ENTITITES ;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CAboveWaterViewDeferred::CRefractionView::Draw()
{
#ifdef PORTAL
	g_pPortalRender->WaterRenderingHandler_PreRefraction();
#endif

	// Store off view origin and angles and set the new view
	int nSaveViewID = CurrentViewID();
	SetupCurrentView( origin, angles, VIEW_REFRACTION );

	PushComposite();

	DrawSetup( GetOuter()->m_waterHeight, m_DrawFlags, GetOuter()->m_waterZAdjust );

	SetFogVolumeState( GetOuter()->m_fogInfo, true );
	SetClearColorToFogColor();
	DrawExecute( GetOuter()->m_waterHeight, VIEW_REFRACTION, GetOuter()->m_waterZAdjust );

	PopComposite();

#ifdef PORTAL
	// deal with stencil
	g_pPortalRender->WaterRenderingHandler_PostRefraction();
#endif

	// finish off the view.  restore the previous view.
	SetupCurrentView( origin, angles, ( view_id_t )nSaveViewID );

	// This is here for multithreading
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	pRenderContext->Flush();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CAboveWaterViewDeferred::CIntersectionView::Setup()
{
	BaseClass::Setup( *GetOuter() );
	m_DrawFlags = DF_RENDER_UNDERWATER | DF_CLIP_Z | DF_DRAW_ENTITITES;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CAboveWaterViewDeferred::CIntersectionView::Draw()
{
	PushComposite();

	DrawSetup( GetOuter()->m_fogInfo.m_flWaterHeight, m_DrawFlags, 0 );

	SetFogVolumeState( GetOuter()->m_fogInfo, true );
	SetClearColorToFogColor( );
	DrawExecute( GetOuter()->m_fogInfo.m_flWaterHeight, VIEW_NONE, 0 );
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );

	PopComposite();
}


//-----------------------------------------------------------------------------
// Draws the scene when the view point is under the level of the water
//-----------------------------------------------------------------------------
void CUnderWaterViewDeferred::Setup( const CViewSetup &view, bool bDrawSkybox, const VisibleFogVolumeInfo_t &fogInfo, const WaterRenderInfo_t& waterInfo )
{
	BaseClass::Setup( view );

	m_bSoftwareUserClipPlane = g_pMaterialSystemHardwareConfig->UseFastClipping();

	CalcWaterEyeAdjustments( fogInfo, m_waterHeight, m_waterZAdjust, m_bSoftwareUserClipPlane );

	IMaterial *pWaterMaterial = fogInfo.m_pFogVolumeMaterial;
	if (engine->GetDXSupportLevel() >= 90 )					// screen overlays underwater are a dx9 feature
	{
		IMaterialVar *pScreenOverlayVar = pWaterMaterial->FindVar( "$underwateroverlay", NULL, false );
		if ( pScreenOverlayVar && ( pScreenOverlayVar->IsDefined() ) )
		{
			char const *pOverlayName = pScreenOverlayVar->GetStringValue();
			if ( pOverlayName[0] != '0' )						// fixme!!!
			{
				IMaterial *pOverlayMaterial = materials->FindMaterial( pOverlayName,  TEXTURE_GROUP_OTHER );
				m_pMainView->SetWaterOverlayMaterial( pOverlayMaterial );
			}
		}
	}
	// NOTE: We're not drawing the 2d skybox under water since it's assumed to not be visible.

	// render the world underwater
	// Clear the color to get the appropriate underwater fog color
	m_DrawFlags = DF_FUDGE_UP | DF_RENDER_UNDERWATER | DF_DRAW_ENTITITES;
	m_ClearFlags = VIEW_CLEAR_DEPTH;

	if( !m_bSoftwareUserClipPlane )
	{
		m_DrawFlags |= DF_CLIP_Z;
	}
	if ( waterInfo.m_bDrawWaterSurface )
	{
		m_DrawFlags |= DF_RENDER_WATER;
	}
	if ( !waterInfo.m_bRefract && !waterInfo.m_bOpaqueWater )
	{
		m_DrawFlags |= DF_RENDER_ABOVEWATER;
	}

	m_fogInfo = fogInfo;
	m_waterInfo = waterInfo;
	m_bDrawSkybox = bDrawSkybox;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CUnderWaterViewDeferred::Draw()
{
	// FIXME: The 3d skybox shouldn't be drawn when the eye is under water

	VPROF( "CViewRender::ViewDrawScene_EyeUnderWater" );

	CMatRenderContextPtr pRenderContext( materials );

	// render refraction (out of water)
	if ( m_waterInfo.m_bRefract )
	{
		m_RefractionView.Setup( );
		m_pMainView->AddViewToScene( &m_RefractionView );
	}

	if ( !m_waterInfo.m_bRefract )
	{
		SetFogVolumeState( m_fogInfo, true );
		unsigned char ucFogColor[3];
		pRenderContext->GetFogColor( ucFogColor );
		pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );
	}

	PushComposite();

	DrawSetup( m_waterHeight, m_DrawFlags, m_waterZAdjust );
	SetFogVolumeState( m_fogInfo, false );
	DrawExecute( m_waterHeight, CurrentViewID(), m_waterZAdjust );
	m_ClearFlags = 0;

	PopComposite();

	if( m_waterZAdjust != 0.0f && m_bSoftwareUserClipPlane && m_waterInfo.m_bRefract )
	{
		m_SoftwareIntersectionView.Setup( false );
		m_SoftwareIntersectionView.Draw( );
	}
	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );

}



//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CUnderWaterViewDeferred::CRefractionView::Setup()
{
	BaseClass::Setup( *GetOuter() );
	// NOTE: Refraction renders into the back buffer, over the top of the 3D skybox
	// It is then blitted out into the refraction target. This is so that
	// we only have to set up 3d sky vis once, and only render it once also!
	m_DrawFlags = DF_CLIP_Z |
		DF_CLIP_BELOW | DF_RENDER_ABOVEWATER |
		DF_DRAW_ENTITITES;

	m_ClearFlags = VIEW_CLEAR_DEPTH;
	if ( GetOuter()->m_bDrawSkybox )
	{
		m_ClearFlags |= VIEW_CLEAR_COLOR;
		m_DrawFlags |= DF_DRAWSKYBOX | DF_CLIP_SKYBOX;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CUnderWaterViewDeferred::CRefractionView::Draw()
{
	CMatRenderContextPtr pRenderContext( materials );
	SetFogVolumeState( GetOuter()->m_fogInfo, true );
	unsigned char ucFogColor[3];
	pRenderContext->GetFogColor( ucFogColor );
	pRenderContext->ClearColor4ub( ucFogColor[0], ucFogColor[1], ucFogColor[2], 255 );

	PushComposite();

	DrawSetup( GetOuter()->m_waterHeight, m_DrawFlags, GetOuter()->m_waterZAdjust );

	EnableWorldFog();
	DrawExecute( GetOuter()->m_waterHeight, VIEW_REFRACTION, GetOuter()->m_waterZAdjust );

	PopComposite();

	Rect_t srcRect;
	srcRect.x = x;
	srcRect.y = y;
	srcRect.width = width;
	srcRect.height = height;

	ITexture *pTexture = GetWaterRefractionTexture();
	pRenderContext->CopyRenderTargetToTextureEx( pTexture, 0, &srcRect, NULL );
}
