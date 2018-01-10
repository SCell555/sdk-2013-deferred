//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "BaseVSShader.h"

#include "defpass_shadow.h"
#include "deferred_context.h"

#include "tier0/memdbgon.h"

BEGIN_VS_SHADER_FLAGS( DepthWrite, "Help for Depth Write", SHADER_NOT_EDITABLE )

	BEGIN_SHADER_PARAMS
	SHADER_PARAM( ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "", "Alpha reference value" )
	SHADER_PARAM( COLOR_DEPTH, SHADER_PARAM_TYPE_BOOL, "0", "Write depth as color")
	END_SHADER_PARAMS

		SHADER_INIT_PARAMS()
		{
			SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );

			defParms_shadow info;
			SetupParamsShadow( info );
			InitParmsShadowPass( info, this, params );
		}

		SHADER_FALLBACK
		{
			return 0;
		}

		SHADER_INIT
		{
			defParms_shadow info;
			SetupParamsShadow( info );
			InitPassShadowPass( info, this, params );
		}

		void SetupParamsShadow(defParms_shadow& params)
		{
			params.iAlphatestRef = ALPHATESTREFERENCE;
		}

		SHADER_DRAW
		{
			if ( pShaderAPI != NULL && *pContextDataPtr == NULL )
				*pContextDataPtr = new CDeferredPerMaterialContextData();

			CDeferredPerMaterialContextData *pDefContext = reinterpret_cast<CDeferredPerMaterialContextData*>(*pContextDataPtr);

			defParms_shadow info;
			SetupParamsShadow( info );
			DrawPassShadowPass( info, this, params, pShaderShadow, pShaderAPI, vertexCompression, pDefContext );
		}
END_SHADER
