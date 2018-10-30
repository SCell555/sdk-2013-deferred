#ifndef CDEFERRED_MANAGER_CLIENT_H
#define CDEFERRED_MANAGER_CLIENT_H

enum DEF_MATERIALS
{
	DEF_MAT_LIGHT_GLOBAL = 0,

	DEF_MAT_LIGHT_POINT_WORLD,
	DEF_MAT_LIGHT_POINT_FULLSCREEN,
	DEF_MAT_LIGHT_SPOT_WORLD,
	DEF_MAT_LIGHT_SPOT_FULLSCREEN,

	DEF_MAT_LIGHT_VOLUME_PREPASS,
	DEF_MAT_LIGHT_VOLUME_BLEND,
	DEF_MAT_LIGHT_VOLUME_POINT_WORLD,
	DEF_MAT_LIGHT_VOLUME_POINT_FULLSCREEN,
	DEF_MAT_LIGHT_VOLUME_SPOT_WORLD,
	DEF_MAT_LIGHT_VOLUME_SPOT_FULLSCREEN,

	DEF_MAT_LIGHT_RADIOSITY_GLOBAL,
	DEF_MAT_LIGHT_RADIOSITY_DEBUG,
	DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_0,
	DEF_MAT_LIGHT_RADIOSITY_PROPAGATE_1,
	DEF_MAT_LIGHT_RADIOSITY_BLUR_0,
	DEF_MAT_LIGHT_RADIOSITY_BLUR_1,
	DEF_MAT_LIGHT_RADIOSITY_BLEND,

	DEF_MAT_BLUR_G6_X,
	DEF_MAT_BLUR_G6_Y,

#ifdef DEBUG
	DEF_MAT_WIREFRAME_DEBUG,
#endif

	DEF_MAT_COUNT
};

class CDeferredManagerClient : public CAutoGameSystem
{
	typedef CAutoGameSystem BaseClass;
public:

	CDeferredManagerClient();
	~CDeferredManagerClient();

	virtual bool Init();
	virtual void Shutdown();

	inline bool IsDeferredRenderingEnabled() const;

	ImageFormat GetShadowDepthFormat();
	ImageFormat GetNullFormat();

	inline IMaterial *GetDeferredMaterial( DEF_MATERIALS mat ) const;

	void LevelInitPreEntity();
	void LevelShutdownPostEntity();

private:

	bool m_bDefRenderingEnabled;

	void InitializeDeferredMaterials();
	void ShutdownDeferredMaterials();

	IMaterial *m_pMat_Def[ DEF_MAT_COUNT ];
	KeyValues *m_pKV_Def[ DEF_MAT_COUNT ];

	CUtlVector<def_light_t*> m_lights;
};

bool CDeferredManagerClient::IsDeferredRenderingEnabled() const
{
	return m_bDefRenderingEnabled;
}

IMaterial *CDeferredManagerClient::GetDeferredMaterial( DEF_MATERIALS mat ) const
{
	Assert( mat >= 0 && mat < DEF_MAT_COUNT );

	return m_pMat_Def[ mat ];
}

extern CDeferredManagerClient *GetDeferredManager();

#endif