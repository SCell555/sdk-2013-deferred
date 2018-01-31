#include "cbase.h"
#include "viewrender_deferred_helper.h"

#include <vector>
#include "winlite.h"
#include "Psapi.h"
#undef GetObject
#include "../thirdparty/minhook/include/MinHook.h"

#include "tier0/memdbgon.h"

namespace Memory
{
	struct BytePattern
	{
		struct Entry
		{
			uint8 Value;
			bool Unknown;
		};

		std::vector<Entry> Bytes;
	};

	BytePattern GetPatternFromString( const char* input )
	{
		BytePattern ret;

		while ( *input )
		{
			if ( isspace( *input ) )
			{
				++input;
			}

			BytePattern::Entry entry;

			if ( isxdigit( *input ) )
			{
				entry.Unknown = false;
				entry.Value = strtol( input, NULL, 16 );

				input += 2;
			}
			else
			{
				entry.Unknown = true;
				input += 2;
			}

			ret.Bytes.emplace_back( entry );
		}

		return ret;
	}

	/*
	Not accessing the STL iterators in debug mode makes this run >10x faster, less sitting around waiting for nothing.
	*/
	inline bool DataCompare( const uint8* data, const BytePattern::Entry* pattern, size_t patternlength )
	{
		uint index = 0;

		for ( size_t i = 0; i < patternlength; i++ )
		{
			const BytePattern::Entry& byte = *pattern;

			if ( !byte.Unknown && *data != byte.Value )
			{
				return false;
			}

			++data;
			++pattern;
			++index;
		}

		return index == patternlength;
	}

	void* FindPattern( void* start, size_t searchlength, const BytePattern& pattern )
	{
		const BytePattern::Entry* patternstart = pattern.Bytes.data();
		const size_t length = pattern.Bytes.size();

		for ( size_t i = 0; i <= searchlength - length; ++i )
		{
			const uint8* addr = static_cast< const uint8* >( start ) + i;

			if ( DataCompare( addr, patternstart, length ) )
			{
				return static_cast<void*>( const_cast<uint8*>( addr ) );
			}
		}

		return NULL;
	}
}

namespace helper
{
	bool bDisableDecalRendering = false;
}
CON_COMMAND( test_render, "" )
{
	using namespace helper;
	bDisableDecalRendering = !bDisableDecalRendering;
}

void( *DecalSurfaceDrawOriginal )( IMatRenderContext* pRenderContext, int renderGroup, float a3 );
void DecalSurfaceDraw( IMatRenderContext *pRenderContext, int renderGroup, float a3 )
{
	if ( helper::bDisableDecalRendering )
		return;
	return DecalSurfaceDrawOriginal( pRenderContext, renderGroup, a3 );
}

void (*DispInfo_DrawDecalsGroupOriginal)( int iGroup, int iTreeType );
void DispInfo_DrawDecalsGroup( int iGroup, int iTreeType )
{
	if ( helper::bDisableDecalRendering )
		return;
	return DispInfo_DrawDecalsGroupOriginal( iGroup, iTreeType );
}


static class MinHookTest : public CAutoGameSystem
{
public:
	bool Init() OVERRIDE
	{
		using namespace Memory;
		CSysModule* engineDll = Sys_LoadModule( "engine.dll" );
		MODULEINFO info;
		GetModuleInformation( GetCurrentProcess(), reinterpret_cast< HMODULE >( engineDll ), &info, sizeof( info ) );

		const BytePattern& decalSurfaceDrawPattern			= GetPatternFromString( "55 8B EC A1 ?? ?? ?? ?? 83 EC 24" );
		const BytePattern& dispInfo_DrawDecalsGroupPattern	= GetPatternFromString( "55 8B EC 81 EC ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 56 8B 01 FF 90 ?? ?? ?? ?? 8B F0 89 75 E0" );

#define FUNC_FROM_PATTERN( name )	void* name##Func = FindPattern( info.lpBaseOfDll, info.SizeOfImage, name##Pattern )

		FUNC_FROM_PATTERN( decalSurfaceDraw );
		FUNC_FROM_PATTERN( dispInfo_DrawDecalsGroup );

#undef FUNC_FROM_PATTERN

		MH_STATUS result = MH_Initialize();

#define HOOK_FUNC( funcAddr, funcRepl )		result = MH_CreateHook( funcAddr##Func, reinterpret_cast<void*>( &funcRepl ), reinterpret_cast<void**>( &funcRepl##Original ) )

		HOOK_FUNC( decalSurfaceDraw, DecalSurfaceDraw );
		HOOK_FUNC( dispInfo_DrawDecalsGroup, DispInfo_DrawDecalsGroup );

#undef HOOK_FUNC

		result = MH_EnableHook( MH_ALL_HOOKS );

		//g_WorldStaticMeshes = reinterpret_cast<CUtlVector<IMesh*>*>(reinterpret_cast<byte*>( engineDll ) + 0x5AD2A8);

		return true;
	}

	void Shutdown() OVERRIDE
	{
		MH_Uninitialize();
	}
} mh_test;