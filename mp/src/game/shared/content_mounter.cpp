#include "cbase.h"
#include "SteamCommon.h"
#ifdef CLIENT_DLL
#include "clientsteamcontext.h"
#endif
#include "filesystem.h"
#include "fmtstr.h"

#include "tier3/tier3.h"
#include "vgui/ILocalize.h"

void MountExtraContent()
{
	KeyValuesAD gameinfo("GameInfo");
	gameinfo->LoadFromFile(filesystem, "gameinfo.txt");

	char path[MAX_PATH];
	ISteamApps* const steamApps = steamapicontext->SteamApps();
	if ( KeyValues* pMounts = gameinfo->FindKey( "mount" ) )
	{
		FOR_EACH_TRUE_SUBKEY( pMounts, pMount )
		{
			const int appId = V_atoi( pMount->GetName() );
			if ( !steamApps->BIsAppInstalled( appId ) )
				continue;
			steamApps->GetAppInstallDir( appId, path, sizeof( path ) );
			const SearchPathAdd_t head = pMount->GetBool( "head" ) ? PATH_ADD_TO_HEAD : PATH_ADD_TO_TAIL;
			FOR_EACH_TRUE_SUBKEY( pMount, pModDir )
			{
				const char* const modDir = pModDir->GetName();
				const CFmtStr mod( "%s" CORRECT_PATH_SEPARATOR_S "%s", path, modDir );
				filesystem->AddSearchPath( mod, "GAME", head );
				filesystem->AddSearchPath( mod, "MOD", head );
				FOR_EACH_VALUE( pModDir, pPath )
				{
					const char* const keyName = pPath->GetName();
					if ( FStrEq( keyName, "vpk" ) )
					{
						const CFmtStr file( "%s" CORRECT_PATH_SEPARATOR_S "%s.vpk", mod.Get(), pPath->GetString() );
						filesystem->AddSearchPath( file, "GAME", head );
						filesystem->AddSearchPath( file, "MOD", head );
					}
					else if ( FStrEq( keyName, "dir" ) )
					{
						const CFmtStr folder( "%s" CORRECT_PATH_SEPARATOR_S "%s", mod.Get(), pPath->GetString() );
						filesystem->AddSearchPath( folder, "GAME", head );
						filesystem->AddSearchPath( folder, "MOD", head );
					}
					else
						Warning( "Unknown key \"%s\" in mounts\n", keyName );
				}
				CFmtStr localization( "resource/%s", modDir );
				localization.Append( "_%language%.txt" );
				g_pVGuiLocalize->AddFile( localization );
			}
		}
	}
}