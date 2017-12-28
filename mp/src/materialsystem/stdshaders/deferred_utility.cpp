
#include "deferred_includes.h"

#include "tier0/memdbgon.h"

ConVar deferred_radiosity_multiplier( "deferred_radiosity_multiplier", "0.4" );

void GetTexcoordSettings( const bool bDecal, int &iNumTexcoords, int **iTexcoordDim )
{
	static int iDimDefault[] = {
		2, 0, 3,
	};

	if ( bDecal )
	{
		*iTexcoordDim = iDimDefault;
		iNumTexcoords = 3;
	}
	else
	{
		*iTexcoordDim = iDimDefault;
		iNumTexcoords = 1;
	}
}