#ifndef DEFPASS_SHADOW_H
#define DEFPASS_SHADOW_H

class CDeferredPerMaterialContextData;

struct defParms_shadow
{
	defParms_shadow()
	{
		Q_memset( this, 0xFF, sizeof( defParms_shadow ) );

		bModel = false;
	};

	// textures
	int iAlbedo;
	int iAlbedo2;

	// control
	int iAlphatestRef;

	// Tree Sway
	int iTreeSway;
	int iTreeSwayHeight;
	int iTreeSwayStartHeight;
	int iTreeSwayRadius;
	int iTreeSwayStartRadius;
	int iTreeSwaySpeed;
	int iTreeSwaySpeedHighWindMultiplier;
	int iTreeSwayStrength;
	int iTreeSwayScrumbleSpeed;
	int iTreeSwayScrumbleStrength;
	int iTreeSwayScrumbleFrequency;
	int iTreeSwayFalloffExp;
	int iTreeSwayScrumbleFalloffExp;
	int iTreeSwaySpeedLerpStart;
	int iTreeSwaySpeedLerpEnd;

	// config
	bool bModel;
};


void InitParmsShadowPass( const defParms_shadow &info, CBaseVSShader *pShader, IMaterialVar **params );
void InitPassShadowPass( const defParms_shadow &info, CBaseVSShader *pShader, IMaterialVar **params );
void DrawPassShadowPass( const defParms_shadow &info, CBaseVSShader *pShader, IMaterialVar **params,
	IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI,
	VertexCompressionType_t vertexCompression, CDeferredPerMaterialContextData *pDeferredContext );


#endif