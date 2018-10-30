//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef FLASHLIGHTEFFECT_H
#define FLASHLIGHTEFFECT_H
#ifdef _WIN32
#pragma once
#endif


class CFlashlightEffect
{
public:

	CFlashlightEffect(int nEntIndex = 0);
	virtual ~CFlashlightEffect();

	virtual void UpdateLight(const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, int nDistance);
	void TurnOn();
	void TurnOff();
	bool IsOn( void ) { return m_bIsOn;	}

	ClientShadowHandle_t GetFlashlightHandle( void ) { return m_FlashlightHandle; }
	void SetFlashlightHandle( ClientShadowHandle_t Handle ) { m_FlashlightHandle = Handle;	}
	
protected:

	virtual void UpdateLightProjection( FlashlightState_t &state );

	virtual void LightOff();
	void LightOffNew();

	void UpdateLightNew(const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp);

	bool m_bIsOn;
	int m_nEntIndex;
	ClientShadowHandle_t m_FlashlightHandle;

	float m_flDistMod;

	// Texture for flashlight
	CTextureReference m_FlashlightTexture;
};

template <typename T>
class CHeadlightEffect : public T
{
public:
	
	CHeadlightEffect() {}
	~CHeadlightEffect() {}

	virtual void UpdateLight(const Vector &vecPos, const Vector &vecDir, const Vector &vecRight, const Vector &vecUp, int nDistance)
	{
		if ( this->IsOn() == false )
			return;

		FlashlightState_t state;
		Vector basisX = vecDir;
		Vector basisY = vecRight;
		Vector basisZ = vecUp;
		VectorNormalize( basisX );
		VectorNormalize( basisY );
		VectorNormalize( basisZ );

		BasisToQuaternion( basisX, basisY, basisZ, state.m_quatOrientation );

		state.m_vecLightOrigin = vecPos;

		static ConVarRef r_flashlightquadratic( "r_flashlightquadratic" );
		static ConVarRef r_flashlightlinear( "r_flashlightlinear" );
		static ConVarRef r_flashlightconstant( "r_flashlightconstant" );
		static ConVarRef r_flashlightambient( "r_flashlightambient" );
		static ConVarRef r_flashlightnear( "r_flashlightnear" );
		static ConVarRef r_flashlightfar( "r_flashlightfar" );

		state.m_fHorizontalFOVDegrees = 45.0f;
		state.m_fVerticalFOVDegrees = 30.0f;
		state.m_fQuadraticAtten = r_flashlightquadratic.GetFloat();
		state.m_fLinearAtten = r_flashlightlinear.GetFloat();
		state.m_fConstantAtten = r_flashlightconstant.GetFloat();
		state.m_Color[0] = 1.0f;
		state.m_Color[1] = 1.0f;
		state.m_Color[2] = 1.0f;
		state.m_Color[3] = r_flashlightambient.GetFloat();
		state.m_NearZ = r_flashlightnear.GetFloat();
		state.m_FarZ = r_flashlightfar.GetFloat();
		state.m_bEnableShadows = true;
		state.m_pSpotlightTexture = this->m_FlashlightTexture;
		state.m_nSpotlightTextureFrame = 0;

		this->UpdateLightProjection( state );
	}
};



#endif // FLASHLIGHTEFFECT_H
