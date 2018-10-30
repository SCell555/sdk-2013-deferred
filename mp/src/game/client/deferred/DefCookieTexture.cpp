
#include "cbase.h"
#include "deferred/deferred_shared_common.h"

#include "tier0/memdbgon.h"

CDefCookieTexture::CDefCookieTexture( ITexture *pTex )
{
	Assert( pTex != NULL );
	m_pTexture = pTex;
}

ITexture *CDefCookieTexture::GetCookieTarget( const int iTargetIndex )
{
	return m_pTexture;
}