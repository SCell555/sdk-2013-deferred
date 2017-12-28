
#include "deferred_light_lump.h"
#ifndef CLIENT_DLL
#include "utlvector.h"
#include "utlbuffer.h"
#include "bsplib.h"
#endif

#include "tier0/memdbgon.h"

BEGIN_BYTESWAP_DATADESC( color_t )
	DEFINE_FIELD( r, FIELD_INTEGER ),
	DEFINE_FIELD( g, FIELD_INTEGER ),
	DEFINE_FIELD( b, FIELD_INTEGER ),
	DEFINE_FIELD( a, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( def_lump_light_global_t )
	DEFINE_FIELD( angles, FIELD_VECTOR ),
	DEFINE_EMBEDDED( diffuse ),
	DEFINE_EMBEDDED( ambientLow ),
	DEFINE_EMBEDDED( ambientHigh ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( def_lump_light_t )
	DEFINE_FIELD( origin,	FIELD_VECTOR ),
	DEFINE_FIELD( angles,	FIELD_VECTOR ),
	DEFINE_FIELD( flags,	FIELD_CHARACTER ),
	DEFINE_EMBEDDED( diffuse ),
	DEFINE_EMBEDDED( ambient ),
	DEFINE_FIELD( radius,	FIELD_FLOAT ),
	DEFINE_FIELD( power,	FIELD_FLOAT ),
	DEFINE_FIELD( vis_dist,	FIELD_FLOAT ),
	DEFINE_FIELD( vis_range, FIELD_FLOAT ),
	DEFINE_FIELD( shadow_dist, FIELD_FLOAT ),
	DEFINE_FIELD( shadow_range, FIELD_FLOAT ),
	DEFINE_FIELD( spot_cone_inner, FIELD_FLOAT ),
	DEFINE_FIELD( spot_cone_outer, FIELD_FLOAT ),
	DEFINE_FIELD( style_seed, FIELD_INTEGER ),
	DEFINE_FIELD( style_amt, FIELD_FLOAT ),
	DEFINE_FIELD( style_speed, FIELD_FLOAT ),
	DEFINE_FIELD( style_smooth, FIELD_FLOAT ),
	DEFINE_FIELD( style_random, FIELD_FLOAT ),
	DEFINE_ARRAY( cookietex, FIELD_CHARACTER, 128 ),
END_BYTESWAP_DATADESC()

#ifndef CLIENT_DLL
static bool s_ParsedGlobalLight = false;
static def_lump_light_global_t s_GlobalLight;
static CUtlVector<def_lump_light_t> s_Lights;

void WriteDeferredLights()
{
	GameLumpHandle_t handle = g_GameLumps.GetGameLumpHandle( GAMELUMP_DEFERRED_LIGHTS );
	if ( handle != g_GameLumps.InvalidGameLump() )
		g_GameLumps.DestroyGameLump( handle );

	const int nSize = sizeof( bool ) + sizeof( def_lump_light_global_t ) + sizeof( int ) + s_Lights.Count() * sizeof( def_lump_light_t );

	handle = g_GameLumps.CreateGameLump( GAMELUMP_DEFERRED_LIGHTS, nSize, 0, GAMELUMP_DEFERRED_LIGHTS_VERSION );

	CUtlBuffer buf( g_GameLumps.GetGameLump( handle ), nSize );
	buf.PutUnsignedChar( s_ParsedGlobalLight );
	buf.PutObjects( &s_GlobalLight );
	buf.PutInt( s_Lights.Count() );
	if ( s_Lights.Count() > 0 )
		buf.PutObjects( s_Lights.Base(), s_Lights.Count() );
}

static void GetColorForKey( entity_t *ent, const char *key, color_t& out )
{
	const char* k = ValueForKey (ent, key);
	sscanf( k, "%d %d %d %d", &out.r, &out.g, &out.b, &out.a );
}

bool bSkipDeferredLights = false;
void ProcessDeferredLight( entity_t* mapent )
{
	if ( bSkipDeferredLights )
		return;

	const char* value = ValueForKey( mapent, "targetname" );
	if (value[0])
		return;
	value = ValueForKey( mapent, "parentname" );
	if (value[0])
		return;

	value = ValueForKey( mapent, "classname" );
	if ( !stricmp( value, "light_deferred_global" ) )
	{
		if (!s_ParsedGlobalLight)
		{
			s_ParsedGlobalLight = true;
			GetColorForKey( mapent, "diffuse", s_GlobalLight.diffuse );
			GetColorForKey( mapent, "ambient_low", s_GlobalLight.ambientLow );
			GetColorForKey( mapent, "ambient_high", s_GlobalLight.ambientHigh );
			GetAnglesForKey( mapent, "angles", s_GlobalLight.angles );
		}
		else
			Warning( "Multiple light_deferred_global entites, ignoring.\n" );

		mapent->epairs = NULL;
		return;
	}

	def_lump_light_t& light = s_Lights[s_Lights.AddToTail()];

	const int spawnflags = IntForKeyWithDefault( mapent, "spawnflags", 0 );
	const bool spotLight = IntForKeyWithDefault( mapent, "light_type", 0 ) != 0;
	light.flags = (spotLight ? DEFLIGHT_IS_SPOTLIGHT : 0) | (spawnflags & (1 << 2) ? DEFLIGHT_ENABLED_COOKIE : 0) |
		(spawnflags & (1 << 3) ? DEFLIGHT_ENABLED_VOLUMETRICS : 0) | (spawnflags & (1 << 4) ? DEFLIGHT_ENABLED_LIGHTSTYLE : 0);

	GetVectorForKey( mapent, "origin", light.origin );
	GetAnglesForKey( mapent, "angles", light.angles );
	GetColorForKey( mapent, "diffuse", light.diffuse );
	GetColorForKey( mapent, "ambient", light.ambient );
	light.radius = FloatForKey( mapent, "radius" );
	light.power = FloatForKey( mapent, "power" );

	light.vis_dist = FloatForKey( mapent, "vis_dist" );
	light.vis_range = FloatForKey( mapent, "vis_range" );
	light.shadow_dist = FloatForKey( mapent, "shadow_dist" );
	light.shadow_range = FloatForKey( mapent, "shadow_range" );

	light.style_seed = IntForKey( mapent, "style_seed" );
	light.style_amt = FloatForKey( mapent, "style_amt" );
	light.style_speed = FloatForKey( mapent, "style_speed" );
	light.style_smooth = FloatForKey( mapent, "style_smooth" );
	light.style_random = FloatForKey( mapent, "style_random" );

	light.spot_cone_inner = FloatForKey( mapent, "spot_cone_inner" );
	light.spot_cone_outer = FloatForKey( mapent, "spot_cone_outer" );

	value = ValueForKey( mapent, "cookietex" );
	if (value[0])
		strcpy( light.cookietex, value );
	else
		memset( light.cookietex, 0, 128 );

	mapent->epairs = NULL;
}
#endif