#ifndef SHEDITSYSTEM_H
#define SHEDITSYSTEM_H

#include "cbase.h"

#include "datacache/imdlcache.h"

#include "iviewrender.h"
#include "view_shared.h"
#include "viewrender.h"


class ShaderEditorHandler : public CAutoGameSystemPerFrame
{
public:
	ShaderEditorHandler( char const *name );
	~ShaderEditorHandler();

	virtual bool Init();
	virtual void Shutdown();

	virtual void Update( float frametime );
	virtual void InitialPreRender();
	virtual void PostRender();
	virtual void LevelShutdownPreEntity();

	void SetMainViewMatrix( const VMatrix &view );

	void CustomViewRender( int *viewId, const VisibleFogVolumeInfo_t &fogVolumeInfo, const WaterRenderInfo_t &waterRenderInfo );
	void CustomPostRender();
	void UpdateSkymask( bool bCombineMode, int x, int y, int w, int h );

	bool IsReady() const;
	int &GetViewIdForModify();
	const VisibleFogVolumeInfo_t &GetFogVolumeInfo();
	const WaterRenderInfo_t &GetWaterRenderInfo();

private:
	bool m_bReady;

	void RegisterCallbacks();
	void PrepareCallbackData();

	void RegisterViewRenderCallbacks();

	int *m_piCurrentViewId;
	VisibleFogVolumeInfo_t m_tFogVolumeInfo;
	WaterRenderInfo_t m_tWaterRenderInfo;
};

extern ShaderEditorHandler *g_ShaderEditorSystem;


#endif