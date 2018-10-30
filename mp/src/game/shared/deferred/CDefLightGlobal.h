#ifndef CDEF_LIGHT_GLOBAL_H
#define CDEF_LIGHT_GLOBAL_H

#ifndef GAME_DLL
#include "../../../materialsystem/stdshaders/IDeferredExt.h"
#endif

class CDeferredLightGlobal : public CBaseEntity
{
	DECLARE_CLASS( CDeferredLightGlobal, CBaseEntity );
	DECLARE_NETWORKCLASS();
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

public:

	CDeferredLightGlobal();
	~CDeferredLightGlobal();

#ifdef GAME_DLL
	virtual void Activate();

	virtual int UpdateTransmitState();
#else
	const lightData_Global_t& GetState() const;
	void OnDataChanged(DataUpdateType_t type) override;
#endif

	inline const Vector& GetColor_Diffuse() const;
	inline const Vector& GetColor_Ambient_High() const;
	inline const Vector& GetColor_Ambient_Low() const;

	inline float GetFadeTime() const;

	inline bool IsEnabled() const;
	inline bool HasShadow() const;
	inline bool ShouldFade() const;

private:

#ifdef GAME_DLL
	string_t m_str_Diff;
	string_t m_str_Ambient_High;
	string_t m_str_Ambient_Low;
#endif

	CNetworkVector( m_vecColor_Diff );
	CNetworkVector( m_vecColor_Ambient_High );
	CNetworkVector( m_vecColor_Ambient_Low );

	float m_flFadeTime;
	CNetworkVar( int, m_iDefFlags );

#ifndef GAME_DLL
	mutable lightData_Global_t lightData;
	mutable bool bDirty;
#endif
};

extern CDeferredLightGlobal *GetGlobalLight();

const Vector& CDeferredLightGlobal::GetColor_Diffuse() const
{
	return m_vecColor_Diff;
}

const Vector& CDeferredLightGlobal::GetColor_Ambient_High() const
{
	return m_vecColor_Ambient_High;
}

const Vector& CDeferredLightGlobal::GetColor_Ambient_Low() const
{
	return m_vecColor_Ambient_Low;
}

float CDeferredLightGlobal::GetFadeTime() const
{
	return m_flFadeTime;
}

bool CDeferredLightGlobal::IsEnabled() const
{
	return ( m_iDefFlags & DEFLIGHTGLOBAL_ENABLED ) != 0;
}

bool CDeferredLightGlobal::HasShadow() const
{
	return ( m_iDefFlags & DEFLIGHTGLOBAL_SHADOW_ENABLED ) != 0;
}

bool CDeferredLightGlobal::ShouldFade() const
{
	return ( m_iDefFlags & DEFLIGHTGLOBAL_TRANSITION_FADE ) != 0;
}

#endif