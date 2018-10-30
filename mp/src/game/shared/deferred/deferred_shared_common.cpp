
#include "cbase.h"
#include "deferred/deferred_shared_common.h"

#include "tier0/memdbgon.h"


INetworkStringTable *g_pStringTable_LightCookies = NULL;


static constexpr const char* const g_pszLightParamNames[ LPARAM_COUNT ] =
{
	"diffuse",
	"ambient",
	"radius",
	"power",
	"spot_cone_inner",
	"spot_cone_outer",
	"vis_dist",
	"vis_range",
	"shadow_dist",
	"shadow_range",
	"light_type",
	"cookietex",
	"style_amt",
	"style_speed",
	"style_smooth",
	"style_random",
	"style_seed",

	"spawnFlags",
	"angles",
	"origin",

	"ambient_low",
	"ambient_high",

#if DEFCFG_ADAPTIVE_VOLUMETRIC_LOD
	"volume_lod0_dist",
	"volume_lod1_dist",
	"volume_lod2_dist",
	"volume_lod3_dist",
#endif
#if DEFCFG_CONFIGURABLE_VOLUMETRIC_LOD
	"volume_samples",
#endif
};

const char *GetLightParamName( LIGHT_PARAM_ID id )
{
	Assert( id >= 0 && id < LPARAM_COUNT );

	return g_pszLightParamNames[ id ];
}