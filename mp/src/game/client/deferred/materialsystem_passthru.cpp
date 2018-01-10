//====== Copyright © Sandern Corporation, All rights reserved. ===========//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "filesystem.h"
#include "utlbuffer.h"
#include "igamesystem.h"
#include "materialsystem_passthru.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static int matCount = 0;
CON_COMMAND(print_num_replaced_mats, "")
{
	ConColorMsg( COLOR_GREEN, "%d replaced materials\n", matCount );
}

//-----------------------------------------------------------------------------
// List of materials that should be replaced
//-----------------------------------------------------------------------------
static const char * const pszShaderReplaceDict[][2] = {
	{ "VertexLitGeneric",			"DEFERRED_MODEL" },
	{ "LightmappedGeneric",			"DEFERRED_BRUSH" },
	{ "WorldVertexTransition",		"DEFERRED_BRUSH" },
	{ "DecalModulate",				"DEFERRED_DECALMODULATE" }
};

// Copied from cdeferred_manager_client.cpp
static void ShaderReplaceReplMat( const char *szNewShadername, IMaterial *pMat )
{
	const char *pszOldShadername = pMat->GetShaderName();
	const char *pszMatname = pMat->GetName();

	KeyValuesAD msg( szNewShadername );

	const int nParams = pMat->ShaderParamCount();
	IMaterialVar **pParams = pMat->GetShaderParams();

	char str[ 512 ];

	for ( int i = 0; i < nParams; ++i )
	{
		IMaterialVar *pVar = pParams[ i ];
		const char *pVarName = pVar->GetName();

		if (!V_stricmp("$flags", pVarName) ||
			!V_stricmp("$flags_defined", pVarName) ||
			!V_stricmp("$flags2", pVarName) ||
			!V_stricmp("$flags_defined2", pVarName) )
			continue;

		switch ( pVar->GetType() )
		{
			case MATERIAL_VAR_TYPE_FLOAT:
				msg->SetFloat( pVarName, pVar->GetFloatValue() );
				break;

			case MATERIAL_VAR_TYPE_INT:
				msg->SetInt( pVarName, pVar->GetIntValue() );
				break;

			case MATERIAL_VAR_TYPE_STRING:
				msg->SetString( pVarName, pVar->GetStringValue() );
				break;

			case MATERIAL_VAR_TYPE_FOURCC:
				//Assert( 0 ); // JDTODO
				break;

			case MATERIAL_VAR_TYPE_VECTOR:
			{
				const float *pVal = pVar->GetVecValue();
				int dim = pVar->VectorSize();
				switch ( dim )
				{
				case 1:
					V_sprintf_safe( str, "[%f]", pVal[ 0 ] );
					break;
				case 2:
					V_sprintf_safe( str, "[%f %f]", pVal[ 0 ], pVal[ 1 ] );
					break;
				case 3:
					V_sprintf_safe( str, "[%f %f %f]", pVal[ 0 ], pVal[ 1 ], pVal[ 2 ] );
					break;
				case 4:
					V_sprintf_safe( str, "[%f %f %f %f]", pVal[ 0 ], pVal[ 1 ], pVal[ 2 ], pVal[ 3 ] );
					break;
				default:
					Assert( 0 );
					*str = 0;
				}
				msg->SetString( pVarName, str );
			}
				break;

			case MATERIAL_VAR_TYPE_MATRIX:
			{
				const float *pVal = pVar->GetMatrixValue().Base();
				V_sprintf_safe( str,
					"[%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f]",
					pVal[ 0 ],  pVal[ 1 ],  pVal[ 2 ],  pVal[ 3 ],
					pVal[ 4 ],  pVal[ 5 ],  pVal[ 6 ],  pVal[ 7 ],
					pVal[ 8 ],  pVal[ 9 ],  pVal[ 10 ], pVal[ 11 ],
					pVal[ 12 ], pVal[ 13 ], pVal[ 14 ], pVal[ 15 ] );
				msg->SetString( pVarName, str );
			}
			break;

			case MATERIAL_VAR_TYPE_TEXTURE:
				msg->SetString( pVarName, pVar->GetTextureValue()->GetName() );
				break;

			case MATERIAL_VAR_TYPE_MATERIAL:
				msg->SetString( pVarName, pVar->GetMaterialValue()->GetName() );
				break;
		}
	}

	const bool alphaBlending = pMat->IsTranslucent() || pMat->GetMaterialVarFlag( MATERIAL_VAR_TRANSLUCENT );
	const bool translucentOverride = pMat->IsAlphaTested() || pMat->GetMaterialVarFlag( MATERIAL_VAR_ALPHATEST ) || alphaBlending;
	const bool bSelfillum = pMat->GetMaterialVarFlag( MATERIAL_VAR_SELFILLUM );

	const bool bDecal = ( pszOldShadername != NULL && Q_stristr( pszOldShadername, "decal" ) != NULL ) ||
		( pszMatname != NULL && Q_stristr( pszMatname, "decal" ) != NULL ) ||
		pMat->GetMaterialVarFlag( MATERIAL_VAR_DECAL );

	if ( bDecal )
	{
		msg->SetInt( "$decal", 1 );

		if ( alphaBlending )
			msg->SetInt( "$translucent", 1 );
	}
	else if ( translucentOverride )
	{
		msg->SetInt( "$alphatest", 1 );
	}

	// Find a better solution to this...
	if (true) //( pMat->IsTwoSided() )
	{
		msg->SetInt( "$nocull", 1 );
	}

	if ( bSelfillum )
	{
		msg->SetInt( "$selfillum", 1 );
	}

	pMat->SetShaderAndParams( msg );

	pMat->RefreshPreservingMaterialVars();
}

IMaterial* CDeferredMaterialSystem::FindProceduralMaterial( const char* pMaterialName, const char* pTextureGroupName,
                                                        KeyValues* pVMTKeyValues )
{
	const char* pShaderName = pVMTKeyValues->GetName();
	for ( const char* const* row : pszShaderReplaceDict )
	{
		if ( Q_stristr( pShaderName, row[0] ) == pShaderName )
		{
			pVMTKeyValues->SetName( row[1] );
			matCount++;
			break;
		}
	}
	return BaseClass::FindProceduralMaterial( pMaterialName, pTextureGroupName, pVMTKeyValues );
}

IMaterial* CDeferredMaterialSystem::CreateMaterial( const char* pMaterialName, KeyValues* pVMTKeyValues )
{
	const char* pShaderName = pVMTKeyValues->GetName();
	for ( const char* const* row : pszShaderReplaceDict )
	{
		if ( Q_stristr( pShaderName, row[0] ) == pShaderName )
		{
			pVMTKeyValues->SetName( row[1] );
			matCount++;
			break;
		}
	}
	return BaseClass::CreateMaterial( pMaterialName, pVMTKeyValues );
}

IMaterial* CDeferredMaterialSystem::ReplaceMaterialInternal( IMaterial* pMat ) const
{
	if ( !pMat || pMat->IsErrorMaterial() )
		return pMat;

	const char* pShaderName = pMat->GetShaderName();
	if ( pShaderName )
	{
		for ( const char* const* row : pszShaderReplaceDict )
		{
			if ( Q_stristr( pShaderName, row[0] ) == pShaderName )
			{
				ShaderReplaceReplMat( row[1], pMat );
				matCount++;
				break;
			}
		}
	}
	return pMat;
}
