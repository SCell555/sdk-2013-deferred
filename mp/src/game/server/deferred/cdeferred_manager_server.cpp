
#include "cbase.h"
#include "deferred/deferred_shared_common.h"
#include "mapentities.h"
#include "filesystem.h"
#include "bspfile.h"
#include "utlbuffer.h"
#include "lzmaDecoder.h"

#include "tier0/memdbgon.h"

static CDeferredManagerServer __g_defmanager;

IGameSystem* DeferredManagerSystem()
{
	return &__g_defmanager;
}

CDeferredManagerServer *GetDeferredManager()
{
	return &__g_defmanager;
}

CDeferredManagerServer::CDeferredManagerServer()
{
}

CDeferredManagerServer::~CDeferredManagerServer()
{
}

bool CDeferredManagerServer::Init()
{
	return true;
}

void CDeferredManagerServer::Shutdown()
{
}

#define LIGHT_MIN_LIGHT_VALUE 0.03f
float ComputeLightRadius( float radius, float intensity, float constant_attn, float linear_attn, float quadratic_attn )
{
	float flLightRadius = radius;
	if (flLightRadius == 0.0f)
	{
		// Compute the light range based on attenuation factors
		if (quadratic_attn == 0.0f)
		{
			if (linear_attn == 0.0f)
			{
				// Infinite, but we're not going to draw it as such
				flLightRadius = 2000;
			}
			else
			{
				flLightRadius = (intensity / LIGHT_MIN_LIGHT_VALUE - constant_attn) / linear_attn;
			}
		}
		else
		{
			const float a = quadratic_attn;
			const float b = linear_attn;
			const float c = constant_attn - intensity / LIGHT_MIN_LIGHT_VALUE;
			const float discrim = b * b - 4 * a * c;
			if (discrim < 0.0f)
			{
				// Infinite, but we're not going to draw it as such
				flLightRadius = 2000;
			}
			else
			{
				flLightRadius = (-b + sqrtf(discrim)) / (2.0f * a);
				if (flLightRadius < 0)
					flLightRadius = 0;
			}
		}
	}
	return flLightRadius;
}

float ComputeLightRadius( const dworldlight_t &light )
{
	float flLightRadius = light.radius;
	if (flLightRadius == 0.0f)
	{
		// Compute the light range based on attenuation factors
		const float flIntensity = sqrtf( DotProduct( light.intensity, light.intensity ) );
		if ( light.quadratic_attn == 0.0f )
		{
			if ( light.linear_attn == 0.0f )
			{
				// Infinite, but we're not going to draw it as such
				flLightRadius = 2000;
			}
			else
			{
				flLightRadius = (flIntensity / LIGHT_MIN_LIGHT_VALUE - light.constant_attn) / light.linear_attn;
			}
		}
		else
		{
			const float a = light.quadratic_attn;
			const float b = light.linear_attn;
			const float c = light.constant_attn - flIntensity / LIGHT_MIN_LIGHT_VALUE;
			const float discrim = b * b - 4 * a * c;
			if (discrim < 0.0f)
			{
				// Infinite, but we're not going to draw it as such
				flLightRadius = 2000;
			}
			else
			{
				flLightRadius = (-b + sqrtf(discrim)) / (2.0f * a);
				if (flLightRadius < 0)
					flLightRadius = 0;
			}
		}
	}

	return flLightRadius;
}

static ConVar deferred_autoenvlight_ambient_intensity_low("deferred_autoenvlight_ambient_intensity_low", "0.15");
static ConVar deferred_autoenvlight_ambient_intensity_high("deferred_autoenvlight_ambient_intensity_high", "0.45");
static ConVar deferred_autoenvlight_diffuse_intensity("deferred_autoenvlight_diffuse_intensity", "1");

void CDeferredManagerServer::LevelInitPreEntity()
{
	if ( gpGlobals->eLoadType == MapLoad_LoadGame )
		return;
	const char* entStr = engine->GetMapEntitiesString();

	if ( V_stristr( entStr, "light_deferred_global" ) )
		return;

	const FileHandle_t hFile = g_pFullFileSystem->Open(MapName(), "rb");
	if ( !hFile )
		return;

	dheader_t header;
	g_pFullFileSystem->Read( &header, sizeof(dheader_t), hFile );

	const lump_t &lightLump = header.lumps[LUMP_WORLDLIGHTS];
	dworldlight_t* lights;
	size_t lightCount;

	if ( lightLump.uncompressedSize )
	{
		byte* compressed = new byte[lightLump.filelen];
		g_pFullFileSystem->Seek( hFile, lightLump.fileofs, FILESYSTEM_SEEK_HEAD );
		g_pFullFileSystem->Read( compressed, lightLump.filelen, hFile );

		if ( CLZMA::IsCompressed( compressed ) )
		{
			if ( lightLump.uncompressedSize % sizeof( dworldlight_t ) )
				return;

			lightCount = lightLump.uncompressedSize / sizeof( dworldlight_t );
			lights = new dworldlight_t[lightCount];
			CLZMA::Uncompress( compressed, reinterpret_cast<byte*>( lights ) );
		}
		else
			Error( "Compressed lump that isn't compressed?" );

		delete[] compressed;
	}
	else
	{
		if ( lightLump.filelen % sizeof( dworldlight_t ) )
			return;

		g_pFullFileSystem->Seek( hFile, lightLump.fileofs, FILESYSTEM_SEEK_HEAD );

		lightCount = lightLump.filelen / sizeof( dworldlight_t );

		lights = new dworldlight_t[lightCount];
		g_pFullFileSystem->Read( lights, lightLump.filelen, hFile );
	}

	g_pFullFileSystem->Close( hFile );

	KeyValuesAD vmfFile( "vmf" );

	char szTokenBuffer[MAPKEY_MAXLENGTH];
	for ( ; true; entStr = MapEntity_SkipToNextEntity( entStr, szTokenBuffer ) )
	{
		char token[MAPKEY_MAXLENGTH];
		entStr = MapEntity_ParseToken( entStr, token );

		if ( !entStr )
			break;

		if ( token[0] != '{' )
		{
			Error( "MapEntity_ParseAllEntities: found %s when expecting {", token );
			continue;
		}
		
		CEntityMapData entData( (char*)entStr );
		
		char className[MAPKEY_MAXLENGTH];
		entData.ExtractValue( "classname", className );
		int iType;
		if ( FStrEq( className, "light" ) )
			iType = 0;
		else if ( FStrEq( className, "light_spot" ) )
			iType = 1;
		else if ( FStrEq( className, "point_spotlight" ) )
			iType = 2;
		else if ( FStrEq( className, "light_environment" ) )
			iType = 3;
		else
			continue;
		
		char keyName[MAPKEY_MAXLENGTH];
		char value[MAPKEY_MAXLENGTH];
		KeyValues* pSubKey = new KeyValues( className );
		if ( entData.GetFirstKey( keyName, value ) )
		{
			do 
			{
				pSubKey->SetString( keyName, value );
			} 
			while ( entData.GetNextKey( keyName, value ) );
		}
		pSubKey->SetInt( "light_type", iType );
		vmfFile->AddSubKey( pSubKey );
	}

	KeyValuesDumpAsDevMsg( vmfFile, 0, 4 );

	struct SpotLightPair_t
	{
		KeyValues* spotlight;
		KeyValues* light;
	};

	CUtlVector<SpotLightPair_t> spotLightPairs;

	// Find light_spot and point_spotlight entities at same place
	FOR_EACH_TRUE_SUBKEY( vmfFile, entity1 )
	{
		const int type1 = entity1->GetInt( "light_type" );
		if ( type1 != 1 && type1 != 2 )
			continue;
		bool bSkip = false;
		const int numPairs = spotLightPairs.Count();
		for ( int i = 0; i < numPairs; ++i )
		{
			const SpotLightPair_t& pair = spotLightPairs[i];
			if (pair.light == entity1 || pair.spotlight == entity1)
			{
				bSkip = true;
				break;
			}
		}
		if ( bSkip )
			continue;

		Vector pos1;
		QAngle rot1;
		UTIL_StringToVector( pos1.Base(), entity1->GetString( "origin" ) );
		UTIL_StringToVector( rot1.Base(), entity1->GetString( "angles" ) );
		FOR_EACH_TRUE_SUBKEY( vmfFile, entity2 )
		{
			bool bSkip = false;
			const int numPairs = spotLightPairs.Count();
			for ( int i = 0; i < numPairs; ++i )
			{
				const SpotLightPair_t& pair = spotLightPairs[i];
				if (pair.light == entity2 || pair.spotlight == entity2)
				{
					bSkip = true;
					break;
				}
			}
			if ( bSkip )
				continue;
			const int type2 = entity2->GetInt( "light_type" );
			if ( entity1 == entity2 || ( type2 != 1 && type2 != 2 && type1 != type2 ) )
				continue;

			Vector pos2;
			QAngle rot2;
			UTIL_StringToVector( pos2.Base(), entity1->GetString( "origin" ) );
			UTIL_StringToVector( rot2.Base(), entity1->GetString( "angles" ) );

			if ( CloseEnough( pos1, pos2, 0.2f ) && CloseEnough( rot1.x, rot2.x, 2.f ) && CloseEnough( rot1.y, rot2.y, 2.f ) && CloseEnough( rot1.z, rot2.z, 2.f ) )
			{
				SpotLightPair_t& pair = spotLightPairs[spotLightPairs.AddToTail()];
				pair.light = type1 == 1 ? entity1 : entity2;
				pair.spotlight = type1 != 1 ? entity1 : entity2;
				break;
			}
		}
	}

#define COPY_LIGHT_DATA( name ) pair.spotlight->SetString( name, pair.light->GetString( name ) )

	const int numPairs = spotLightPairs.Count();
	for ( int i = 0; i < numPairs; ++i )
	{
		SpotLightPair_t& pair = spotLightPairs[i];
		vmfFile->RemoveSubKey( pair.light );
		COPY_LIGHT_DATA( "_light" );
		COPY_LIGHT_DATA( "_exponent" );
		COPY_LIGHT_DATA( "_cone" );
		COPY_LIGHT_DATA( "_inner_cone" );
		COPY_LIGHT_DATA( "_constant_attn" );
		COPY_LIGHT_DATA( "_linear_attn" );
		COPY_LIGHT_DATA( "_quadratic_attn" );
		pair.spotlight->SetInt( "light_type", 5 );
		pair.light->deleteThis();
	}

	const char* szParamDiffuse = GetLightParamName( LPARAM_DIFFUSE );
	const char* szParamLightType = GetLightParamName( LPARAM_LIGHTTYPE );
	const char* szParamSpotConeInner = GetLightParamName( LPARAM_SPOTCONE_INNER );
	const char* szParamSpotConeOuter = GetLightParamName( LPARAM_SPOTCONE_OUTER );
	const char* szParamPower = GetLightParamName( LPARAM_POWER );
	const char* szParamRadius = GetLightParamName( LPARAM_RADIUS );
	const char* szParamVisDist = GetLightParamName( LPARAM_VIS_DIST );
	const char* szParamVisRange = GetLightParamName( LPARAM_VIS_RANGE );
	const char* szParamShadowDist = GetLightParamName( LPARAM_SHADOW_DIST );
	const char* szParamShadowRange = GetLightParamName( LPARAM_SHADOW_RANGE );
	const char* szParamVolumeSamples = GetLightParamName( LPARAM_VOLUME_SAMPLES );

	bool bCreatedGlobalLight = false;
	CUtlVector<const dworldlight_t*> unspawnedLights;
	FOR_EACH_TRUE_SUBKEY( vmfFile, entity )
	{
		const int type = entity->GetInt( "light_type" );
		if ( type == -1 || ( type == 3 && bCreatedGlobalLight ) || type == 2 )
			continue;

		Vector pos;
		QAngle rot;
		UTIL_StringToVector( pos.Base(), entity->GetString( "origin" ) );
		if ( KeyValues* angle = entity->FindKey( "angles" ) )
			UTIL_StringToVector( rot.Base(), angle->GetString() );
		else
			rot = vec3_angle;

		if ( type == 3 )
		{
			rot.x = -entity->GetInt( "pitch" );
			CDeferredLightGlobal* lightEntity = static_cast<CDeferredLightGlobal*>( CBaseEntity::CreateNoSpawn( "light_deferred_global", pos, rot ) );
			if ( !lightEntity )
				break;

			int color[4], ambient[4];
			UTIL_StringToIntArray( color, 4, entity->GetString( "_light" ) );
			UTIL_StringToIntArray( ambient, 4, entity->GetString( "_ambient" ) );

			const float ds = deferred_autoenvlight_diffuse_intensity.GetFloat();
			const float asl = deferred_autoenvlight_ambient_intensity_low.GetFloat();
			const float ash = deferred_autoenvlight_ambient_intensity_high.GetFloat();

			lightEntity->KeyValue( "diffuse", UTIL_VarArgs("%d %d %d %f", color[0], color[1], color[2], color[3] * ds ) );
			lightEntity->KeyValue( "ambient_high", UTIL_VarArgs("%d %d %d %f", ambient[0], ambient[1], ambient[2], ambient[3] * ash ) );
			lightEntity->KeyValue( "ambient_low", UTIL_VarArgs("%d %d %d %f", ambient[0], ambient[1], ambient[2], ambient[3] * asl ) );
			lightEntity->KeyValue( "spawnflags", "3" );

			DispatchSpawn( lightEntity );

			bCreatedGlobalLight = true;
			continue;
		}

		for ( uint i = 0; i < lightCount; ++i )
		{
			const dworldlight_t& light = lights[i];
			if ( light.type != emit_spotlight && light.type != emit_point )
				continue;

			if ( CloseEnough( light.origin, pos, 1.f ) )
			{
				if ( light.type == emit_point )
				{
					entity->SetFloat( "_distance", ComputeLightRadius( light ) );
					break;
				}

				QAngle ang;
				VectorAngles( light.normal, ang );
				if ( CloseEnough( -rot.x, ang.x, 2.f ) && CloseEnough( rot.y, ang.y, 2.f ) )
				{
					rot = ang;
					entity->SetFloat( "_inner_cone", acos( light.stopdot ) * 180.f / M_PI_F );
					entity->SetFloat( "_cone", acos( light.stopdot2 ) * 180.f / M_PI_F );
					entity->SetFloat( "_distance", ComputeLightRadius( light ) );
					break;
				}
			}
			else
			{
				const int numUnspawnedLights = unspawnedLights.Count();
				for ( int i = 0; i < numUnspawnedLights; ++i )
				{
					if (unspawnedLights[i] == &light)
						goto out;
				}
				unspawnedLights.AddToTail( &light );
			}
		}

		out:
		CDeferredLight* lightEntity = static_cast<CDeferredLight*>( CBaseEntity::CreateNoSpawn( "light_deferred", pos, rot ) );
		if ( !lightEntity )
			break;

		int color[4];
		UTIL_StringToIntArray( color, 4, entity->GetString( type == 2 ? "rendercolor" : "_light" ) );
		if ( type == 5 )
			color[3] = 255;

		char string[256];
		V_sprintf_safe( string, "%d %d %d %d", color[0], color[1], color[2], color[3] );
		lightEntity->KeyValue( szParamDiffuse, string );

		lightEntity->KeyValue( "spawnflags", type == 2 || type == 5 ? "11" : "3" );
		if ( type == 1 || type == 5 )
		{
			lightEntity->KeyValue( szParamLightType, "1" );
			lightEntity->KeyValue( szParamSpotConeInner, entity->GetFloat( "_inner_cone" ) );
			lightEntity->KeyValue( szParamSpotConeOuter, entity->GetFloat( "_cone" ) );
			lightEntity->KeyValue( szParamPower, entity->GetFloat( "_exponent", 1.f ) );
			if ( type == 5 )
				lightEntity->KeyValue( szParamVolumeSamples, 50 );
		}
		/*else if ( type == 2 )
		{
			lightEntity->KeyValue( szParamLightType, "1" );
			const float width = entity->GetFloat( "spotlightwidth" );
			lightEntity->KeyValue( szParamSpotConeInner, width );
			lightEntity->KeyValue( szParamSpotConeOuter, width * 1.5f );
			lightEntity->KeyValue( szParamPower, 1 );
			lightEntity->KeyValue( szParamVolumeSamples, 50 );
		}*/
		else
		{
			lightEntity->KeyValue( szParamLightType, "0" );
			lightEntity->KeyValue( szParamPower, "1" );
		}

		const float radius = /*type == 2 ? entity->GetFloat( "spotlightlength" ) : */
		ComputeLightRadius( entity->GetFloat( "_distance" ), color[3], entity->GetFloat( "_constant_attn" ), entity->GetFloat( "_linear_attn" ), entity->GetFloat( "_quadratic_attn" ) );
		lightEntity->KeyValue( szParamRadius, radius );
		lightEntity->KeyValue( szParamVisDist, radius * 2 );
		lightEntity->KeyValue( szParamVisRange, radius * 1.25f );
		lightEntity->KeyValue( szParamShadowDist, radius * ( 5.f / 6.f ) );
		lightEntity->KeyValue( szParamShadowRange, radius * ( 2.f / 3.f ) );

		if ( KeyValues* targetname = entity->FindKey( "targetname" ) )
			lightEntity->KeyValue( "targetname", targetname->GetString() );

		if ( KeyValues* parent = entity->FindKey( "parentname" ) )
			lightEntity->KeyValue( "parentname", parent->GetString() );

		DispatchSpawn( lightEntity );
	}

	const int numUnspawnedLights = unspawnedLights.Count();
	for ( int i = 0; i < numUnspawnedLights; ++i )
	{
		const dworldlight_t* light = unspawnedLights[i];
		const float radius = ComputeLightRadius( *light );

		if ( radius == 0 )
			continue;

		CDeferredLight* lightEntity = static_cast<CDeferredLight*>( CBaseEntity::CreateNoSpawn( "light_deferred", light->origin, vec3_angle ) );
		if ( !lightEntity )
			break;

		Vector intensity = light->intensity;
		const float ratio = light->constant_attn + 100 * light->linear_attn + 100 * 100 * light->quadratic_attn;
		if ( ratio > 0 )
			VectorScale( light->intensity, 1.f / ratio, intensity );
		intensity *= 255.f;

		char string[256];
		V_sprintf_safe( string, "%f %f %f 255", intensity.x, intensity.y, intensity.z );
		lightEntity->KeyValue( szParamDiffuse, string );

		lightEntity->KeyValue( "spawnflags", "3" );
		if ( light->type == emit_spotlight )
		{
			QAngle angle;
			VectorAngles( light->normal, angle );
			lightEntity->SetAbsAngles( angle );
			lightEntity->KeyValue( szParamLightType, "1" );
			lightEntity->KeyValue( szParamSpotConeInner, acos( light->stopdot ) * 180.f / M_PI_F );
			lightEntity->KeyValue( szParamSpotConeOuter, acos( light->stopdot2 ) * 180.f / M_PI_F );
			lightEntity->KeyValue( szParamPower, light->exponent );
		}
		else
		{
			lightEntity->KeyValue( szParamLightType, "0" );
			lightEntity->KeyValue( szParamPower, "1" );
		}

		lightEntity->KeyValue( szParamRadius, radius );
		lightEntity->KeyValue( szParamVisDist, radius * 2 );
		lightEntity->KeyValue( szParamVisRange, radius * 1.25f );
		lightEntity->KeyValue( szParamShadowDist, radius * ( 5.f / 6.f ) );
		lightEntity->KeyValue( szParamShadowRange, radius * ( 2.f / 3.f ) );

		DispatchSpawn( lightEntity );
	}

	delete[] lights;
}

int CDeferredManagerServer::AddCookieTexture( const char *pszCookie )
{
	Assert( g_pStringTable_LightCookies != NULL );

	return  g_pStringTable_LightCookies->AddString( true, pszCookie );
}

void CDeferredManagerServer::AddWorldLight( CDeferredLight *l )
{
	CDeferredLightContainer *pC = FindAvailableContainer();

	if ( !pC )
		pC = assert_cast< CDeferredLightContainer* >( CreateEntityByName( "deferred_light_container" ) );

	pC->AddWorldLight( l );
}
