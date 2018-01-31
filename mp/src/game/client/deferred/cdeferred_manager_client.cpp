#include "cbase.h"

#include "tier0/icommandline.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialvar.h"
#include "filesystem.h"
#include "deferred/deferred_shared_common.h"
#include "utlbuffer.h"
#include "deferred_light_lump.h"

#include "vgui_controls/MessageBox.h"

#include "tier0/memdbgon.h"

static CDeferredManagerClient __g_defmanager;
CDeferredManagerClient *GetDeferredManager()
{
	return &__g_defmanager;
}

static IViewRender *g_pCurrentViewRender = NULL;

static CDeferredMaterialSystem g_DeferredMaterialSystem;
static IMaterialSystem *g_pOldMatSystem;


CDeferredManagerClient::CDeferredManagerClient() : BaseClass( "DeferredManagerClient" )
{
	m_bDefRenderingEnabled = false;

	Q_memset( m_pMat_Def, 0, sizeof(IMaterial*) * DEF_MAT_COUNT );
	Q_memset( m_pKV_Def, 0, sizeof(KeyValues*) * DEF_MAT_COUNT );
}

CDeferredManagerClient::~CDeferredManagerClient()
{
	for ( KeyValues* &values : m_pKV_Def )
	{
		values->deleteThis();
		values = NULL;
	}
}

bool CDeferredManagerClient::Init()
{
	AssertMsg( g_pCurrentViewRender == NULL, "viewrender already allocated?!" );

	if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 95 )
	{
		Warning( "The engine doesn't recognize your GPU to support SM3.0, running deferred anyway...\n" );
	}

	{
		const bool bGotDefShaderDll = ConnectDeferredExt();

		if ( bGotDefShaderDll )
		{
#define VENDOR_NVIDIA 0x10DE
#define VENDOR_INTEL 0x8086
#define VENDOR_ATI 0x1002
#define VENDOR_AMD 0x1022

			MaterialAdapterInfo_t info;
			materials->GetDisplayAdapterInfo(materials->GetCurrentAdapter(), info);
			m_bHardwareFiltering = info.m_VendorID == VENDOR_NVIDIA || info.m_VendorID == VENDOR_INTEL; // remove intel?
			GetDeferredExt()->SetUsingHardwareFiltering( m_bHardwareFiltering && !CommandLine()->FindParm( "-software" ) );
			static const Color k( 0, 255, 128, 255 );
			ConColorMsg( k, "Running deferred with %s filtering\n", m_bHardwareFiltering ? "HARDWARE" : "SOFTWARE" );

			g_pOldMatSystem = materials;

			g_DeferredMaterialSystem.InitPassThru( materials );
			materials = &g_DeferredMaterialSystem;
			engine->Mat_Stub( &g_DeferredMaterialSystem );

			m_bDefRenderingEnabled = true;
			GetDeferredExt()->EnableDeferredLighting();

			g_pCurrentViewRender = new CDeferredViewRender();

			ConVarRef r_shadows( "r_shadows" );
			r_shadows.SetValue( "0" );

			InitDeferredRTs( true );

			materials->AddModeChangeCallBack( &DefRTsOnModeChanged );

			InitializeDeferredMaterials();
		}
	}

	if ( !m_bDefRenderingEnabled )
	{
		Assert( g_pCurrentViewRender == NULL );

		Warning( "Your hardware does not seem to support shader model 3.0. If you think that this is an error (hybrid GPUs), add -forcedeferred as start parameter.\n" );
		g_pCurrentViewRender = new CViewRender();
	}

	view = g_pCurrentViewRender;

	return true;
}

void CDeferredManagerClient::Shutdown()
{
	def_light_t::ShutdownSharedMeshes();

	ShutdownDeferredMaterials();
	ShutdownDeferredExt();

	if ( IsDeferredRenderingEnabled() )
	{
		materials->RemoveModeChangeCallBack( &DefRTsOnModeChanged );

		materials = g_pOldMatSystem;
		engine->Mat_Stub( g_pOldMatSystem );
	}

	if ( m_bDefRenderingEnabled )
		delete static_cast<CDeferredViewRender*>( g_pCurrentViewRender );
	else
		delete static_cast<CViewRender*>( g_pCurrentViewRender );
	g_pCurrentViewRender = NULL;
	view = NULL;
}

ImageFormat CDeferredManagerClient::GetShadowDepthFormat()
{
	ImageFormat f = materials->GetShadowDepthTextureFormat();

	// hack for hybrid stuff
	if ( f == IMAGE_FORMAT_UNKNOWN )
		f = IMAGE_FORMAT_NV_DST16;

	return f;
}

ImageFormat CDeferredManagerClient::GetNullFormat()
{
	return materials->GetNullTextureFormat();
}

//#define DEF_WRITE_VMT

void CDeferredManagerClient::InitializeDeferredMaterials()
{
	// BUG!!! Creating the materials directly is causing some weird performance bug at map start (high cpu load)
	// Instead write each material to file and then find them.
#ifdef DEF_WRITE_VMT

	// Make sure the directory exists
	if( !filesystem->FileExists("materials/deferred", "MOD") )
	{
		filesystem->CreateDirHierarchy("materials/deferred", "MOD");
	}

#if DEBUG
	m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ] = new KeyValues( "wireframe" );
	if ( m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ]->SetString( "$color", "[1 0.5 0.1]" );
		m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ]->SaveToFile( filesystem, "materials/deferred/lightworld_wirefram.vmt", "MOD" );
	}
	m_pMat_Def[ DEF_MAT_WIREFRAME_DEBUG ] = materials->FindMaterial( "deferred/lightworld_wirefram", NULL );
#endif

	// Create Materials
	m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ] = new KeyValues( "LIGHTING_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ]->SaveToFile( filesystem, "materials/deferred/lightpass_global.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ]->SaveToFile( filesystem, "materials/deferred/lightpass_point_fs.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ]->SaveToFile( filesystem, "materials/deferred/lightpass_point_w.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ]->SaveToFile( filesystem, "materials/deferred/lightpass_spot_fs.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ]->SaveToFile( filesystem, "materials/deferred/lightpass_spot_w.vmt", "MOD" );
	}

	// Find the materials
	m_pMat_Def[ DEF_MAT_LIGHT_GLOBAL ] = materials->FindMaterial( "deferred/lightpass_global", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] = materials->FindMaterial( "deferred/lightpass_point_fs", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_POINT_WORLD ] = materials->FindMaterial( "deferred/lightpass_point_w", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] = materials->FindMaterial( "deferred/lightpass_spot_fs", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] = materials->FindMaterial( "deferred/lightpass_spot_w", NULL );

	/*

	lighting volumes

	*/

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ]->SaveToFile( filesystem, "materials/deferred/lightpass_point_vfs.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ]->SaveToFile( filesystem, "materials/deferred/lightpass_point_v.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ]->SaveToFile( filesystem, "materials/deferred/lightpass_spot_vfs.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ]->SaveToFile( filesystem, "materials/deferred/lightpass_spot_v.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] = new KeyValues( "VOLUME_PREPASS" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ]->SaveToFile( filesystem, "materials/deferred/volume_prepass.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] = new KeyValues( "VOLUME_BLEND" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ]->SaveToFile( filesystem, "materials/deferred/volume_blend.vmt", "MOD" );
	}

	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] = materials->FindMaterial( "deferred/lightpass_point_vfs", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] = materials->FindMaterial( "deferred/lightpass_point_v", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] = materials->FindMaterial( "deferred/lightpass_spot_vfs", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] = materials->FindMaterial( "deferred/lightpass_spot_v", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] = materials->FindMaterial( "deferred/volume_prepass", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] = materials->FindMaterial( "deferred/volume_blend", NULL );

#if DEFCFG_ENABLE_RADIOSITY == 1
	/*

	radiosity

	*/

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ]->SaveToFile( filesystem, "materials/deferred/radpass_global.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_DEBUG ] = new KeyValues( "DEBUG_RADIOSITY_GRID" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_DEBUG ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_DEBUG ]->SaveToFile( filesystem, "materials/deferred/radpass_dbg_grid.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$BASETEXTURE", GetDefRT_RadiosityBuffer( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$NORMALMAP", GetDefRT_RadiosityNormal( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SaveToFile( filesystem, "materials/deferred/radpass_prop_0.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$BASETEXTURE", GetDefRT_RadiosityBuffer( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$NORMALMAP", GetDefRT_RadiosityNormal( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SaveToFile( filesystem, "materials/deferred/radpass_prop_1.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ]->SetInt( "$BLUR", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ]->SetString( "$BASETEXTURE", GetDefRT_RadiosityBuffer( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ]->SetString( "$NORMALMAP", GetDefRT_RadiosityNormal( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ]->SaveToFile( filesystem, "materials/deferred/radpass_blur_0.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ]->SetInt( "$BLUR", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ]->SetString( "$BASETEXTURE", GetDefRT_RadiosityBuffer( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ]->SetString( "$NORMALMAP", GetDefRT_RadiosityNormal( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ]->SaveToFile( filesystem, "materials/deferred/radpass_blur_1.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] = new KeyValues( "RADIOSITY_BLEND" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] != NULL )
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ]->SaveToFile( filesystem, "materials/deferred/radpass_blend.vmt", "MOD" );

	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] = materials->FindMaterial( "deferred/radpass_global", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_DEBUG ] = materials->FindMaterial( "deferred/radpass_dbg_grid", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] = materials->FindMaterial( "deferred/radpass_prop_0", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] = materials->FindMaterial( "deferred/radpass_prop_1", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ] = materials->FindMaterial( "deferred/radpass_blur_0", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ] = materials->FindMaterial( "deferred/radpass_blur_1", NULL );
	m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] = materials->FindMaterial( "deferred/radpass_blend", NULL );
#endif // DEFCFG_ENABLE_RADIOSITY

#if DEFCFG_DEFERRED_SHADING == 1
	/*

	deferred shading

	*/

	m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ] = new KeyValues( "SCREENSPACE_SHADING" );
	if ( m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ] != NULL )
		m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ]->SaveToFile( filesystem, "materials/deferred/screenspace_shading.vmt", "MOD" );

	m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ] = new KeyValues( "SCREENSPACE_COMBINE" );
	if ( m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ] != NULL )
		m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ]->SaveToFile( filesystem, "materials/deferred/screenspace_combine.vmt", "MOD" );

	m_pMat_Def[ DEF_MAT_SCREENSPACE_SHADING ] = materials->FindMaterial( "deferred/screenspace_shading", NULL );
	m_pMat_Def[ DEF_MAT_SCREENSPACE_COMBINE ] = materials->FindMaterial( "deferred/screenspace_combine", NULL );
#endif

	/*

	blur

	*/

	m_pKV_Def[ DEF_MAT_BLUR_G6_X ] = new KeyValues( "GAUSSIAN_BLUR_6" );
	if ( m_pKV_Def[ DEF_MAT_BLUR_G6_X ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_BLUR_G6_X ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_BLUR_G6_X ]->SaveToFile( filesystem, "materials/deferred/blurpass_vbuf_x.vmt", "MOD" );
	}

	m_pKV_Def[ DEF_MAT_BLUR_G6_Y ] = new KeyValues( "GAUSSIAN_BLUR_6" );
	if ( m_pKV_Def[ DEF_MAT_BLUR_G6_Y ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_BLUR_G6_Y ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_BLUR_G6_Y ]->SetInt( "$ISVERTICAL", 1 );
		m_pKV_Def[ DEF_MAT_BLUR_G6_Y ]->SaveToFile( filesystem, "materials/deferred/blurpass_vbuf_y.vmt", "MOD" );
	}

	m_pMat_Def[ DEF_MAT_BLUR_G6_X ] = materials->FindMaterial( "deferred/blurpass_vbuf_x", NULL );
	m_pMat_Def[ DEF_MAT_BLUR_G6_Y ] = materials->FindMaterial( "deferred/blurpass_vbuf_y", NULL );

#else // Create materials directly (caused performance bug)

#if DEBUG
	m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ] = new KeyValues( "wireframe" );
	if ( m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ]->SetString( "$color", "[1 0.5 0.1]" );
		m_pMat_Def[ DEF_MAT_WIREFRAME_DEBUG ] = materials->CreateMaterial( "__lightworld_wireframe", m_pKV_Def[ DEF_MAT_WIREFRAME_DEBUG ] );
	}
#endif

	m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ] = new KeyValues( "LIGHTING_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_GLOBAL ] = materials->CreateMaterial( "__lightpass_global", m_pKV_Def[ DEF_MAT_LIGHT_GLOBAL ] );

	m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] = materials->CreateMaterial( "__lightpass_point_fs", m_pKV_Def[ DEF_MAT_LIGHT_POINT_FULLSCREEN ] );

	m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_POINT_WORLD ] = materials->CreateMaterial( "__lightpass_point_w", m_pKV_Def[ DEF_MAT_LIGHT_POINT_WORLD ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pMat_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] = materials->CreateMaterial( "__lightpass_spot_fs", m_pKV_Def[ DEF_MAT_LIGHT_SPOT_FULLSCREEN ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] = new KeyValues( "LIGHTING_WORLD" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] = materials->CreateMaterial( "__lightpass_spot_w", m_pKV_Def[ DEF_MAT_LIGHT_SPOT_WORLD ] );
	}


	/*

	lighting volumes

	*/

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] = materials->CreateMaterial( "__lightpass_point_vfs", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN ] );

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] = materials->CreateMaterial( "__lightpass_point_v", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_POINT_WORLD ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] = materials->CreateMaterial( "__lightpass_spot_vfs", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] = new KeyValues( "LIGHTING_VOLUME" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ]->SetInt( "$WORLDPROJECTION", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ]->SetInt( "$LIGHTTYPE", DEFLIGHTTYPE_SPOT );
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] = materials->CreateMaterial( "__lightpass_spot_v", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_SPOT_WORLD ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] = new KeyValues( "VOLUME_PREPASS" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] = materials->CreateMaterial( "__volume_prepass", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_PREPASS ] );

	m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] = new KeyValues( "VOLUME_BLEND" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 0 )->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] = materials->CreateMaterial( "__volume_blend", m_pKV_Def[ DEF_MAT_LIGHT_VOLUME_BLEND ] );
	}

	/*

	radiosity

	*/

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] = new KeyValues( "RADIOSITY_GLOBAL" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] = materials->CreateMaterial( "__radpass_global", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_GLOBAL ] );

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_DEBUG ] = new KeyValues( "DEBUG_RADIOSITY_GRID" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_DEBUG ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_DEBUG ] = materials->CreateMaterial( "__radpass_dbg_grid", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_DEBUG ] );

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$BASETEXTURE", GetDefRT_RadiosityBuffer( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ]->SetString( "$NORMALMAP", GetDefRT_RadiosityNormal( 0 )->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] = materials->CreateMaterial( "__radpass_prop_0", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$BASETEXTURE", GetDefRT_RadiosityBuffer( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ]->SetString( "$NORMALMAP", GetDefRT_RadiosityNormal( 1 )->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] = materials->CreateMaterial( "__radpass_prop_1", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ]->SetInt( "$BLUR", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ]->SetString( "$BASETEXTURE", GetDefRT_RadiosityBuffer( 0 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ]->SetString( "$NORMALMAP", GetDefRT_RadiosityNormal( 0 )->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ] = materials->CreateMaterial( "__radpass_blur_0", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_0 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ] = new KeyValues( "RADIOSITY_PROPAGATE" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ]->SetInt( "$BLUR", 1 );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ]->SetString( "$BASETEXTURE", GetDefRT_RadiosityBuffer( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ]->SetString( "$NORMALMAP", GetDefRT_RadiosityNormal( 1 )->GetName() );
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ] = materials->CreateMaterial( "__radpass_blur_1", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLUR_1 ] );
	}

	m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] = new KeyValues( "RADIOSITY_BLEND" );
	if ( m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] != NULL )
		m_pMat_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] = materials->CreateMaterial( "__radpass_blend", m_pKV_Def[ DEF_MAT_LIGHT_RADIOSITY_BLEND ] );

#if DEFCFG_DEFERRED_SHADING == 1
	/*

	deferred shading

	*/

	m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ] = new KeyValues( "SCREENSPACE_SHADING" );
	if ( m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ] != NULL )
		m_pMat_Def[ DEF_MAT_SCREENSPACE_SHADING ] = materials->CreateMaterial( "__screenspace_shading", m_pKV_Def[ DEF_MAT_SCREENSPACE_SHADING ] );

	m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ] = new KeyValues( "SCREENSPACE_COMBINE" );
	if ( m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ] != NULL )
		m_pMat_Def[ DEF_MAT_SCREENSPACE_COMBINE ] = materials->CreateMaterial( "__screenspace_combine", m_pKV_Def[ DEF_MAT_SCREENSPACE_COMBINE ] );
#endif

	/*

	blur

	*/

	m_pKV_Def[ DEF_MAT_BLUR_G6_X ] = new KeyValues( "GAUSSIAN_BLUR_6" );
	if ( m_pKV_Def[ DEF_MAT_BLUR_G6_X ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_BLUR_G6_X ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 0 )->GetName() );
		m_pMat_Def[ DEF_MAT_BLUR_G6_X ] = materials->CreateMaterial( "__blurpass_vbuf_x", m_pKV_Def[ DEF_MAT_BLUR_G6_X ] );
	}

	m_pKV_Def[ DEF_MAT_BLUR_G6_Y ] = new KeyValues( "GAUSSIAN_BLUR_6" );
	if ( m_pKV_Def[ DEF_MAT_BLUR_G6_Y ] != NULL )
	{
		m_pKV_Def[ DEF_MAT_BLUR_G6_Y ]->SetString( "$BASETEXTURE", GetDefRT_VolumetricsBuffer( 1 )->GetName() );
		m_pKV_Def[ DEF_MAT_BLUR_G6_Y ]->SetInt( "$ISVERTICAL", 1 );
		m_pMat_Def[ DEF_MAT_BLUR_G6_Y ] = materials->CreateMaterial( "__blurpass_vbuf_y", m_pKV_Def[ DEF_MAT_BLUR_G6_Y ] );
	}

#endif // DEF_WRITE_VMT

#if DEBUG
	for ( int i = 0; i < DEF_MAT_COUNT; i++ )
	{
		Assert( m_pKV_Def[ i ] != NULL );
		Assert( m_pMat_Def[ i ] != NULL );
	}
#endif

	for ( IMaterial* material : m_pMat_Def )
	{
		material->Refresh();
	}
}

void CDeferredManagerClient::ShutdownDeferredMaterials()
{
	// not deleted on purpose!!!!!
	for ( int i = 0; i < DEF_MAT_COUNT; i++ )
	{
		if ( m_pKV_Def[ i ] != NULL )
			m_pKV_Def[ i ]->Clear();
		m_pKV_Def[ i ] = NULL;
	}
}

void CDeferredManagerClient::LevelInitPreEntity()
{
	const int lumpSize = engine->GameLumpSize( GAMELUMP_DEFERRED_LIGHTS );
	if ( !lumpSize || engine->GameLumpVersion( GAMELUMP_DEFERRED_LIGHTS ) != GAMELUMP_DEFERRED_LIGHTS_VERSION )
		return;
	CUtlBuffer buffer( 0, lumpSize, CUtlBuffer::READ_ONLY );
	const bool success = engine->LoadGameLump( GAMELUMP_DEFERRED_LIGHTS, buffer.Base(), lumpSize );
	if ( !success )
		return;
	const bool bHasGlobalLight = buffer.GetUnsignedChar();
	def_lump_light_global_t globalLight;
	buffer.GetObjects( &globalLight );
	const int numLights = buffer.GetInt();
	CUtlVector<def_lump_light_t> lumpLights;
	lumpLights.EnsureCount( numLights );
	buffer.GetObjects( lumpLights.Base(), numLights );
	Warning( "Global light: %d | lights: %d\n", bHasGlobalLight, numLights );
}

void CDeferredManagerClient::LevelShutdownPostEntity()
{

}
