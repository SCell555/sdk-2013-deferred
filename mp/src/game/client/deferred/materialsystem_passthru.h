//====== Copyright © Sandern Corporation, All rights reserved. ===========//
//
// Purpose: Implementation of IMaterialSystem interface which "passes tru" all
//			function calls to the real interface. Can be used to override
//			IMaterialSystem function calls (combined with engine->Mat_Stub).
//
//=============================================================================//

#ifndef WARS_MATERIALSYSTEM_PASSTHRU_H
#define WARS_MATERIALSYSTEM_PASSTHRU_H
#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/imaterialsystem.h"

class CPassThruMaterialSystem : public IMaterialSystem
{
public:
	CPassThruMaterialSystem()
	{
		m_pBaseMaterialsPassThru = NULL;
	}

	void InitPassThru( IMaterialSystem* pBaseMaterialsPassThru )
	{
		m_pBaseMaterialsPassThru = pBaseMaterialsPassThru;
	}

public:
	bool Connect( CreateInterfaceFn factory ) OVERRIDE { return m_pBaseMaterialsPassThru->Connect( factory ); }
	void Disconnect() OVERRIDE { return m_pBaseMaterialsPassThru->Disconnect(); }
	void *QueryInterface( const char *pInterfaceName ) OVERRIDE { return m_pBaseMaterialsPassThru->QueryInterface( pInterfaceName ); }
	InitReturnVal_t Init() OVERRIDE { return m_pBaseMaterialsPassThru->Init(); }
	void Shutdown() OVERRIDE { m_pBaseMaterialsPassThru->Shutdown(); }

	CreateInterfaceFn	Init( char const* pShaderAPIDLL,
									  IMaterialProxyFactory *pMaterialProxyFactory,
									  CreateInterfaceFn fileSystemFactory,
									  CreateInterfaceFn cvarFactory = NULL ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->Init( pShaderAPIDLL, pMaterialProxyFactory, fileSystemFactory, cvarFactory );
	}

	void				SetShaderAPI( char const *pShaderAPIDLL ) OVERRIDE { m_pBaseMaterialsPassThru->SetShaderAPI( pShaderAPIDLL ); }

	void				SetAdapter( int nAdapter, int nFlags ) OVERRIDE { m_pBaseMaterialsPassThru->SetAdapter( nAdapter, nFlags ); }

	void				ModInit() OVERRIDE { m_pBaseMaterialsPassThru->ModInit(); }
	void				ModShutdown() OVERRIDE { m_pBaseMaterialsPassThru->ModShutdown(); }

	void				SetThreadMode( MaterialThreadMode_t mode, int nServiceThread = -1 ) OVERRIDE { m_pBaseMaterialsPassThru->SetThreadMode( mode, nServiceThread ); }
	MaterialThreadMode_t GetThreadMode() OVERRIDE { return m_pBaseMaterialsPassThru->GetThreadMode(); }
	void				ExecuteQueued() OVERRIDE { m_pBaseMaterialsPassThru->ExecuteQueued(); }

	IMaterialSystemHardwareConfig *GetHardwareConfig( const char *pVersion, int *returnCode ) OVERRIDE { return m_pBaseMaterialsPassThru->GetHardwareConfig( pVersion, returnCode ); }

	bool				UpdateConfig( bool bForceUpdate ) OVERRIDE { return m_pBaseMaterialsPassThru->UpdateConfig( bForceUpdate ); }

	bool				OverrideConfig( const MaterialSystem_Config_t &config, bool bForceUpdate ) OVERRIDE { return m_pBaseMaterialsPassThru->OverrideConfig( config, bForceUpdate ); }

	const MaterialSystem_Config_t &GetCurrentConfigForVideoCard() const OVERRIDE { return m_pBaseMaterialsPassThru->GetCurrentConfigForVideoCard(); }

	bool				GetRecommendedConfigurationInfo( int nDXLevel, KeyValues * pKeyValues ) OVERRIDE { return m_pBaseMaterialsPassThru->GetRecommendedConfigurationInfo( nDXLevel, pKeyValues ); }

	int					GetDisplayAdapterCount() const OVERRIDE { return m_pBaseMaterialsPassThru->GetDisplayAdapterCount(); }

	int					GetCurrentAdapter() const OVERRIDE { return m_pBaseMaterialsPassThru->GetCurrentAdapter(); }

	void				GetDisplayAdapterInfo( int adapter, MaterialAdapterInfo_t& info ) const OVERRIDE { m_pBaseMaterialsPassThru->GetDisplayAdapterInfo( adapter, info ); }

	int					GetModeCount( int adapter ) const OVERRIDE { return m_pBaseMaterialsPassThru->GetModeCount( adapter ); }

	void				GetModeInfo( int adapter, int mode, MaterialVideoMode_t& info ) const OVERRIDE { m_pBaseMaterialsPassThru->GetModeInfo( adapter, mode, info ); }

	void				AddModeChangeCallBack( ModeChangeCallbackFunc_t func ) OVERRIDE { m_pBaseMaterialsPassThru->AddModeChangeCallBack( func ); }

	void				GetDisplayMode( MaterialVideoMode_t& mode ) const OVERRIDE { m_pBaseMaterialsPassThru->GetDisplayMode( mode ); }

	bool				SetMode( void* hwnd, const MaterialSystem_Config_t &config ) OVERRIDE { return m_pBaseMaterialsPassThru->SetMode( hwnd, config ); }

	bool				SupportsMSAAMode( int nMSAAMode ) OVERRIDE { return m_pBaseMaterialsPassThru->SupportsMSAAMode( nMSAAMode ); }

	const MaterialSystemHardwareIdentifier_t &GetVideoCardIdentifier( void ) const OVERRIDE { return m_pBaseMaterialsPassThru->GetVideoCardIdentifier(); }

	void				SpewDriverInfo() const OVERRIDE { m_pBaseMaterialsPassThru->SpewDriverInfo(); }

	void				GetBackBufferDimensions( int &width, int &height ) const OVERRIDE { m_pBaseMaterialsPassThru->GetBackBufferDimensions( width, height ); }
	ImageFormat			GetBackBufferFormat() const OVERRIDE { return m_pBaseMaterialsPassThru->GetBackBufferFormat(); }

	bool				SupportsHDRMode( HDRType_t nHDRModede ) OVERRIDE { return m_pBaseMaterialsPassThru->SupportsHDRMode( nHDRModede ); }

	bool				AddView( void* hwnd ) OVERRIDE { return m_pBaseMaterialsPassThru->AddView( hwnd ); }
	void				RemoveView( void* hwnd ) OVERRIDE { m_pBaseMaterialsPassThru->RemoveView( hwnd ); }

	void				SetView( void* hwnd ) OVERRIDE { m_pBaseMaterialsPassThru->SetView( hwnd ); }

	void				BeginFrame( float frameTime ) OVERRIDE { m_pBaseMaterialsPassThru->BeginFrame( frameTime ); }
	void				EndFrame() OVERRIDE { m_pBaseMaterialsPassThru->EndFrame(); }
	void				Flush( bool flushHardware = false ) OVERRIDE { m_pBaseMaterialsPassThru->Flush( flushHardware ); }

	void				SwapBuffers() OVERRIDE { m_pBaseMaterialsPassThru->SwapBuffers(); }

	void				EvictManagedResources() OVERRIDE { m_pBaseMaterialsPassThru->EvictManagedResources(); }

	void				ReleaseResources( void ) OVERRIDE { m_pBaseMaterialsPassThru->ReleaseResources(); }
	void				ReacquireResources( void ) OVERRIDE { m_pBaseMaterialsPassThru->ReacquireResources(); }

	void				AddReleaseFunc( MaterialBufferReleaseFunc_t func ) OVERRIDE { m_pBaseMaterialsPassThru->AddReleaseFunc( func ); }
	void				RemoveReleaseFunc( MaterialBufferReleaseFunc_t func ) OVERRIDE { m_pBaseMaterialsPassThru->RemoveReleaseFunc( func ); }

	void				AddRestoreFunc( MaterialBufferRestoreFunc_t func ) OVERRIDE { m_pBaseMaterialsPassThru->AddRestoreFunc( func ); }
	void				RemoveRestoreFunc( MaterialBufferRestoreFunc_t func ) OVERRIDE { m_pBaseMaterialsPassThru->RemoveRestoreFunc( func ); }

	void				ResetTempHWMemory( bool bExitingLevel = false ) OVERRIDE { m_pBaseMaterialsPassThru->ResetTempHWMemory( bExitingLevel ); }

	void				HandleDeviceLost() OVERRIDE { m_pBaseMaterialsPassThru->HandleDeviceLost(); }

	int					ShaderCount() const OVERRIDE { return m_pBaseMaterialsPassThru->ShaderCount(); }
	int					GetShaders( int nFirstShader, int nMaxCount, IShader **ppShaderList ) const OVERRIDE { return m_pBaseMaterialsPassThru->GetShaders( nFirstShader, nMaxCount, ppShaderList ); }

	int					ShaderFlagCount() const OVERRIDE { return m_pBaseMaterialsPassThru->ShaderFlagCount(); }
	const char *		ShaderFlagName( int nIndex ) const OVERRIDE { return m_pBaseMaterialsPassThru->ShaderFlagName( nIndex ); }

	void				GetShaderFallback( const char *pShaderName, char *pFallbackShader, int nFallbackLength ) OVERRIDE { m_pBaseMaterialsPassThru->GetShaderFallback( pShaderName, pFallbackShader, nFallbackLength ); }

	IMaterialProxyFactory *GetMaterialProxyFactory() OVERRIDE { return m_pBaseMaterialsPassThru->GetMaterialProxyFactory(); }

	void				SetMaterialProxyFactory( IMaterialProxyFactory* pFactory ) OVERRIDE { m_pBaseMaterialsPassThru->SetMaterialProxyFactory( pFactory ); }

	void				EnableEditorMaterials() OVERRIDE { m_pBaseMaterialsPassThru->EnableEditorMaterials(); }

	void				SetInStubMode( bool bInStubMode ) OVERRIDE { m_pBaseMaterialsPassThru->SetInStubMode( bInStubMode ); }

	void				DebugPrintUsedMaterials( const char *pSearchSubString, bool bVerbose ) OVERRIDE { m_pBaseMaterialsPassThru->DebugPrintUsedMaterials( pSearchSubString, bVerbose ); }
	void				DebugPrintUsedTextures( void ) OVERRIDE { m_pBaseMaterialsPassThru->DebugPrintUsedTextures(); }

	void				ToggleSuppressMaterial( char const* pMaterialName ) OVERRIDE { m_pBaseMaterialsPassThru->ToggleSuppressMaterial( pMaterialName ); }
	void				ToggleDebugMaterial( char const* pMaterialName ) OVERRIDE { m_pBaseMaterialsPassThru->ToggleDebugMaterial( pMaterialName ); }

	bool				UsingFastClipping( void ) OVERRIDE { return m_pBaseMaterialsPassThru->UsingFastClipping(); }

	int					StencilBufferBits( void ) OVERRIDE { return m_pBaseMaterialsPassThru->StencilBufferBits(); }

	void				UncacheAllMaterials() OVERRIDE { m_pBaseMaterialsPassThru->UncacheAllMaterials(); }

	void				UncacheUnusedMaterials( bool bRecomputeStateSnapshots = false ) OVERRIDE { m_pBaseMaterialsPassThru->UncacheUnusedMaterials( bRecomputeStateSnapshots ); }


	void				CacheUsedMaterials() OVERRIDE { m_pBaseMaterialsPassThru->CacheUsedMaterials(); }

	void				ReloadTextures() OVERRIDE { m_pBaseMaterialsPassThru->ReloadTextures(); }

	void				ReloadMaterials( const char *pSubString = NULL ) OVERRIDE { m_pBaseMaterialsPassThru->ReloadMaterials( pSubString ); }

	IMaterial *			CreateMaterial( const char *pMaterialName, KeyValues *pVMTKeyValues ) OVERRIDE { return m_pBaseMaterialsPassThru->CreateMaterial( pMaterialName, pVMTKeyValues ); }

	IMaterial *			FindMaterial( char const* pMaterialName, const char *pTextureGroupName, bool complain = true, const char *pComplainPrefix = NULL ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->FindMaterial( pMaterialName, pTextureGroupName, complain, pComplainPrefix );
	}

	MaterialHandle_t	FirstMaterial() const OVERRIDE { return m_pBaseMaterialsPassThru->FirstMaterial(); }

	MaterialHandle_t	NextMaterial( MaterialHandle_t h ) const OVERRIDE { return m_pBaseMaterialsPassThru->NextMaterial( h ); }

	MaterialHandle_t	InvalidMaterial() const OVERRIDE { return m_pBaseMaterialsPassThru->InvalidMaterial(); }

	IMaterial*			GetMaterial( MaterialHandle_t h ) const OVERRIDE { return m_pBaseMaterialsPassThru->GetMaterial( h ); }

	int					GetNumMaterials() const OVERRIDE { return m_pBaseMaterialsPassThru->GetNumMaterials(); }

	ITexture*			FindTexture( char const* pTextureName, const char *pTextureGroupName, bool complain = true, int nAdditionalCreationFlags = 0) OVERRIDE { return m_pBaseMaterialsPassThru->FindTexture( pTextureName, pTextureGroupName, complain, nAdditionalCreationFlags ); }

	bool				IsTextureLoaded( char const* pTextureName ) const OVERRIDE { return m_pBaseMaterialsPassThru->IsTextureLoaded( pTextureName ); }

	ITexture*			CreateProceduralTexture( const char	*pTextureName,
														 const char *pTextureGroupName,
														 int w,
														 int h,
														 ImageFormat fmt,
														 int nFlags ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->CreateProceduralTexture( pTextureName, pTextureGroupName, w, h, fmt, nFlags );
	}

	void				BeginRenderTargetAllocation() OVERRIDE { return m_pBaseMaterialsPassThru->BeginRenderTargetAllocation(); }
	void				EndRenderTargetAllocation() OVERRIDE { return m_pBaseMaterialsPassThru->EndRenderTargetAllocation(); }

	ITexture*			CreateRenderTargetTexture( int w,
														   int h,
														   RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
														   ImageFormat	format,
														   MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->CreateRenderTargetTexture( w, h, sizeMode, format, depth );
	}

	ITexture*			CreateNamedRenderTargetTextureEx( const char *pRTName,				// Pass in NULL here for an unnamed render target.
																  int w,
																  int h,
																  RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
																  ImageFormat format,
																  MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED,
																  unsigned int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
																  unsigned int renderTargetFlags = 0 ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->CreateNamedRenderTargetTextureEx( pRTName, w, h, sizeMode, format, depth, textureFlags, renderTargetFlags );
	}

	ITexture*			CreateNamedRenderTargetTexture( const char *pRTName,
																int w,
																int h,
																RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
																ImageFormat format,
																MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED,
																bool bClampTexCoords = true,
																bool bAutoMipMap = false ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->CreateNamedRenderTargetTexture( pRTName, w, h, sizeMode, format, depth, bClampTexCoords, bAutoMipMap );
	}

	ITexture*			CreateNamedRenderTargetTextureEx2( const char *pRTName,				// Pass in NULL here for an unnamed render target.
																   int w,
																   int h,
																   RenderTargetSizeMode_t sizeMode,	// Controls how size is generated (and regenerated on video mode change).
																   ImageFormat format,
																   MaterialRenderTargetDepth_t depth = MATERIAL_RT_DEPTH_SHARED,
																   unsigned int textureFlags = TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT,
																   unsigned int renderTargetFlags = 0 ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->CreateNamedRenderTargetTextureEx2( pRTName, w, h, sizeMode, format, depth, textureFlags, renderTargetFlags );
	}

	void				BeginLightmapAllocation() OVERRIDE { m_pBaseMaterialsPassThru->BeginLightmapAllocation(); }
	void				EndLightmapAllocation() OVERRIDE { m_pBaseMaterialsPassThru->EndLightmapAllocation(); }

	int 				AllocateLightmap( int width, int height,
												  int offsetIntoLightmapPage[2],
												  IMaterial *pMaterial ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->AllocateLightmap( width, height, offsetIntoLightmapPage, pMaterial );
	}
	int					AllocateWhiteLightmap( IMaterial *pMaterial ) OVERRIDE { return m_pBaseMaterialsPassThru->AllocateWhiteLightmap( pMaterial ); }

	void				UpdateLightmap( int lightmapPageID, int lightmapSize[2],
												int offsetIntoLightmapPage[2],
												float *pFloatImage, float *pFloatImageBump1,
												float *pFloatImageBump2, float *pFloatImageBump3 ) OVERRIDE
	{
		m_pBaseMaterialsPassThru->UpdateLightmap( lightmapPageID, lightmapSize, offsetIntoLightmapPage, pFloatImage, pFloatImageBump1, pFloatImageBump2, pFloatImageBump3 );
	}

	int					GetNumSortIDs() OVERRIDE { return m_pBaseMaterialsPassThru->GetNumSortIDs(); }
	void				GetSortInfo( MaterialSystem_SortInfo_t *sortInfoArray ) OVERRIDE { m_pBaseMaterialsPassThru->GetSortInfo( sortInfoArray ); }

	void				GetLightmapPageSize( int lightmap, int *width, int *height ) const OVERRIDE { m_pBaseMaterialsPassThru->GetLightmapPageSize( lightmap, width, height ); }

	void				ResetMaterialLightmapPageInfo() OVERRIDE { m_pBaseMaterialsPassThru->ResetMaterialLightmapPageInfo(); }

	void				ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil = false ) OVERRIDE { m_pBaseMaterialsPassThru->ClearBuffers( bClearColor, bClearDepth, bClearStencil ); }

	IMatRenderContext*	GetRenderContext() OVERRIDE { return m_pBaseMaterialsPassThru->GetRenderContext(); }

	void				BeginUpdateLightmaps( void ) OVERRIDE { m_pBaseMaterialsPassThru->BeginUpdateLightmaps(); }
	void				EndUpdateLightmaps( void ) OVERRIDE { m_pBaseMaterialsPassThru->EndUpdateLightmaps(); }

	MaterialLock_t		Lock() OVERRIDE { return m_pBaseMaterialsPassThru->Lock(); }
	void				Unlock( MaterialLock_t l ) OVERRIDE { return m_pBaseMaterialsPassThru->Unlock( l ); }

	IMatRenderContext *CreateRenderContext( MaterialContextType_t type ) OVERRIDE { return m_pBaseMaterialsPassThru->CreateRenderContext( type ); }

	IMatRenderContext *SetRenderContext( IMatRenderContext *c ) OVERRIDE { return m_pBaseMaterialsPassThru->SetRenderContext( c ); }

	bool				SupportsCSAAMode( int nNumSamples, int nQualityLevel ) OVERRIDE { return m_pBaseMaterialsPassThru->SupportsCSAAMode( nNumSamples, nQualityLevel ); }

	void				RemoveModeChangeCallBack( ModeChangeCallbackFunc_t func ) OVERRIDE { m_pBaseMaterialsPassThru->RemoveModeChangeCallBack( func ); }

	IMaterial*			FindProceduralMaterial( const char *pMaterialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->FindProceduralMaterial( pMaterialName, pTextureGroupName, pVMTKeyValues );
	}

	void				AddTextureAlias( const char *pAlias, const char *pRealName ) OVERRIDE { m_pBaseMaterialsPassThru->AddTextureAlias( pAlias, pRealName ); }
	void				RemoveTextureAlias( const char *pAlias ) OVERRIDE { m_pBaseMaterialsPassThru->RemoveTextureAlias( pAlias ); }

	int					AllocateDynamicLightmap( int lightmapSize[2], int *pOutOffsetIntoPage, int frameID ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->AllocateDynamicLightmap( lightmapSize, pOutOffsetIntoPage, frameID );
	}

	void				SetExcludedTextures( const char *pScriptName ) OVERRIDE { m_pBaseMaterialsPassThru->SetExcludedTextures( pScriptName ); }
	void				UpdateExcludedTextures( void ) OVERRIDE { m_pBaseMaterialsPassThru->UpdateExcludedTextures(); }

	bool				IsInFrame() const OVERRIDE { return m_pBaseMaterialsPassThru->IsInFrame(); }

	void				CompactMemory() OVERRIDE { m_pBaseMaterialsPassThru->CompactMemory(); }

	void				ReloadFilesInList( IFileList *pFilesToReload ) OVERRIDE { m_pBaseMaterialsPassThru->ReloadFilesInList( pFilesToReload ); }

	bool				AllowThreading( bool bAllow, int nServiceThread ) OVERRIDE { return m_pBaseMaterialsPassThru->AllowThreading( bAllow, nServiceThread ); }

	bool				IsRenderThreadSafe() OVERRIDE { return m_pBaseMaterialsPassThru->IsRenderThreadSafe(); }

	void				GetDXLevelDefaults( uint &max_dxlevel, uint &recomended ) OVERRIDE { m_pBaseMaterialsPassThru->GetDXLevelDefaults( max_dxlevel, recomended ); }

	bool				IsMaterialLoaded( const char* name ) OVERRIDE { return m_pBaseMaterialsPassThru->IsMaterialLoaded( name ); }

	void				SetAsyncTextureLoadCache( void* data ) OVERRIDE { m_pBaseMaterialsPassThru->SetAsyncTextureLoadCache( data ); }

	bool				SupportsShadowDepthTextures() OVERRIDE { return m_pBaseMaterialsPassThru->SupportsShadowDepthTextures(); }

	ImageFormat			GetShadowDepthTextureFormat() OVERRIDE { return m_pBaseMaterialsPassThru->GetShadowDepthTextureFormat(); }

	bool				SupportsFetch4() OVERRIDE { return m_pBaseMaterialsPassThru->SupportsFetch4(); }

	ImageFormat			GetNullTextureFormat() OVERRIDE { return m_pBaseMaterialsPassThru->GetNullTextureFormat(); }

	IMaterial*			FindMaterialEx( char const* pMaterialName, const char *pTextureGroupName, int nContext, bool complain = true, const char *pComplainPrefix = NULL ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->FindMaterialEx( pMaterialName, pTextureGroupName, nContext, complain, pComplainPrefix );
	}

#ifdef DX_TO_GL_ABSTRACTION
	void				DoStartupShaderPreloading() OVERRIDE { m_pBaseMaterialsPassThru->DoStartupShaderPreloading(); }
#endif

	void				SetRenderTargetFrameBufferSizeOverrides( int nWidth, int nHeight ) OVERRIDE { m_pBaseMaterialsPassThru->SetRenderTargetFrameBufferSizeOverrides( nWidth, nHeight ); }

	void				GetRenderTargetFrameBufferDimensions( int & nWidth, int & nHeight ) OVERRIDE { m_pBaseMaterialsPassThru->GetRenderTargetFrameBufferDimensions( nWidth, nHeight ); }

	char*				GetDisplayDeviceName() const OVERRIDE { return m_pBaseMaterialsPassThru->GetDisplayDeviceName(); }

	ITexture*			CreateTextureFromBits( int w, int h, int mips, ImageFormat fmt, int srcBufferSize, byte* srcBits ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->CreateTextureFromBits( w, h, mips, fmt, srcBufferSize, srcBits );
	}

	void				OverrideRenderTargetAllocation( bool rtAlloc ) OVERRIDE { m_pBaseMaterialsPassThru->OverrideRenderTargetAllocation( rtAlloc ); }

	ITextureCompositor*	NewTextureCompositor( int w, int h, const char* pCompositeName, int nTeamNum, uint64 randomSeed, KeyValues* stageDesc, uint32 texCompositeCreateFlags = 0 ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->NewTextureCompositor( w, h, pCompositeName, nTeamNum, randomSeed, stageDesc, texCompositeCreateFlags );
	}

	void				AsyncFindTexture( const char* pFilename, const char *pTextureGroupName, IAsyncTextureOperationReceiver* pRecipient, void* pExtraArgs, bool bComplain = true, int nAdditionalCreationFlags = 0 ) OVERRIDE
	{
		m_pBaseMaterialsPassThru->AsyncFindTexture( pFilename, pTextureGroupName, pRecipient, pExtraArgs, bComplain, nAdditionalCreationFlags );
	}

	ITexture*			CreateNamedTextureFromBitsEx( const char* pName, const char *pTextureGroupName, int w, int h, int mips, ImageFormat fmt, int srcBufferSize, byte* srcBits, int nFlags ) OVERRIDE
	{
		return m_pBaseMaterialsPassThru->CreateNamedTextureFromBitsEx( pName, pTextureGroupName, w, h, mips, fmt, srcBufferSize, srcBits, nFlags );
	}

protected:
	IMaterialSystem* m_pBaseMaterialsPassThru;
};

class CDeferredMaterialSystem : public CPassThruMaterialSystem
{
	typedef CPassThruMaterialSystem BaseClass;

public:
	IMaterial* FindMaterialEx( char const* pMaterialName, const char* pTextureGroupName, int nContext, bool complain = true, const char* pComplainPrefix = NULL ) OVERRIDE
	{
		return ReplaceMaterialInternal( BaseClass::FindMaterialEx( pMaterialName, pTextureGroupName, nContext, complain, pComplainPrefix ) );
	}

	IMaterial* FindMaterial( char const* pMaterialName, const char* pTextureGroupName, bool complain = true, const char* pComplainPrefix = NULL ) OVERRIDE
	{
		return ReplaceMaterialInternal( BaseClass::FindMaterial( pMaterialName, pTextureGroupName, complain, pComplainPrefix ) );
	}

	IMaterial* FindProceduralMaterial( const char* pMaterialName, const char* pTextureGroupName, KeyValues* pVMTKeyValues ) OVERRIDE;

	IMaterial* CreateMaterial( const char* pMaterialName, KeyValues* pVMTKeyValues ) OVERRIDE;
private:
	IMaterial * ReplaceMaterialInternal( IMaterial* pMat ) const;
};

#endif // WARS_MATERIALSYSTEM_PASSTHRU_H