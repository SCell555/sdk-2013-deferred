#ifndef DEFPASS_GBUFFER_H
#define DEFPASS_GBUFFER_H

class CDeferredPerMaterialContextData;

struct defParms_gBuffer
{
	defParms_gBuffer()
	{
		Q_memset( this, 0xFF, sizeof( defParms_gBuffer ) );

		bModel = false;
	};

	// textures
	int iAlbedo;
	int iAlbedo2;

	int iBumpmap;
	int iBumpmap2;
	int iPhongmap;
	int iBlendmodulate;

	// control
	int iAlphatestRef;
	int iLitface;
	int iPhongExp;
	int iPhongExp2;
	int iSSBump;

	// blending
	int iBlendmodulateTransform;

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


void InitParmsGBuffer( const defParms_gBuffer &info, CBaseVSShader *pShader, IMaterialVar **params );
void InitPassGBuffer( const defParms_gBuffer &info, CBaseVSShader *pShader, IMaterialVar **params );
void DrawPassGBuffer( const defParms_gBuffer &info, CBaseVSShader *pShader, IMaterialVar **params,
	IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI,
	VertexCompressionType_t vertexCompression, CDeferredPerMaterialContextData *pDeferredContext );


#endif