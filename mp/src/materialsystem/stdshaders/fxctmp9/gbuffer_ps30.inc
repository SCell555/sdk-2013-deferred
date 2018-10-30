#include "shaderlib/cshader.h"
class gbuffer_ps30_Static_Index
{
private:
	int m_nALPHATEST;
#ifdef _DEBUG
	bool m_bALPHATEST;
#endif
public:
	void SetALPHATEST( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nALPHATEST = i;
#ifdef _DEBUG
		m_bALPHATEST = true;
#endif
	}
	void SetALPHATEST( bool i )
	{
		m_nALPHATEST = i ? 1 : 0;
#ifdef _DEBUG
		m_bALPHATEST = true;
#endif
	}
private:
	int m_nBUMPMAP;
#ifdef _DEBUG
	bool m_bBUMPMAP;
#endif
public:
	void SetBUMPMAP( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nBUMPMAP = i;
#ifdef _DEBUG
		m_bBUMPMAP = true;
#endif
	}
	void SetBUMPMAP( bool i )
	{
		m_nBUMPMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bBUMPMAP = true;
#endif
	}
private:
	int m_nNOCULL;
#ifdef _DEBUG
	bool m_bNOCULL;
#endif
public:
	void SetNOCULL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nNOCULL = i;
#ifdef _DEBUG
		m_bNOCULL = true;
#endif
	}
	void SetNOCULL( bool i )
	{
		m_nNOCULL = i ? 1 : 0;
#ifdef _DEBUG
		m_bNOCULL = true;
#endif
	}
private:
	int m_nPHONGMAP;
#ifdef _DEBUG
	bool m_bPHONGMAP;
#endif
public:
	void SetPHONGMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPHONGMAP = i;
#ifdef _DEBUG
		m_bPHONGMAP = true;
#endif
	}
	void SetPHONGMAP( bool i )
	{
		m_nPHONGMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bPHONGMAP = true;
#endif
	}
private:
	int m_nBUMPMAP2;
#ifdef _DEBUG
	bool m_bBUMPMAP2;
#endif
public:
	void SetBUMPMAP2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBUMPMAP2 = i;
#ifdef _DEBUG
		m_bBUMPMAP2 = true;
#endif
	}
	void SetBUMPMAP2( bool i )
	{
		m_nBUMPMAP2 = i ? 1 : 0;
#ifdef _DEBUG
		m_bBUMPMAP2 = true;
#endif
	}
private:
	int m_nBLENDMODULATE;
#ifdef _DEBUG
	bool m_bBLENDMODULATE;
#endif
public:
	void SetBLENDMODULATE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLENDMODULATE = i;
#ifdef _DEBUG
		m_bBLENDMODULATE = true;
#endif
	}
	void SetBLENDMODULATE( bool i )
	{
		m_nBLENDMODULATE = i ? 1 : 0;
#ifdef _DEBUG
		m_bBLENDMODULATE = true;
#endif
	}
public:
	gbuffer_ps30_Static_Index( )
	{
#ifdef _DEBUG
		m_bALPHATEST = false;
#endif // _DEBUG
		m_nALPHATEST = 0;
#ifdef _DEBUG
		m_bBUMPMAP = false;
#endif // _DEBUG
		m_nBUMPMAP = 0;
#ifdef _DEBUG
		m_bNOCULL = false;
#endif // _DEBUG
		m_nNOCULL = 0;
#ifdef _DEBUG
		m_bPHONGMAP = false;
#endif // _DEBUG
		m_nPHONGMAP = 0;
#ifdef _DEBUG
		m_bBUMPMAP2 = false;
#endif // _DEBUG
		m_nBUMPMAP2 = 0;
#ifdef _DEBUG
		m_bBLENDMODULATE = false;
#endif // _DEBUG
		m_nBLENDMODULATE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bALPHATEST && m_bBUMPMAP && m_bNOCULL && m_bPHONGMAP && m_bBUMPMAP2 && m_bBLENDMODULATE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nALPHATEST ) + ( 2 * m_nBUMPMAP ) + ( 6 * m_nNOCULL ) + ( 12 * m_nPHONGMAP ) + ( 24 * m_nBUMPMAP2 ) + ( 48 * m_nBLENDMODULATE ) + 0;
	}
};
#define shaderStaticTest_gbuffer_ps30 psh_forgot_to_set_static_ALPHATEST + psh_forgot_to_set_static_BUMPMAP + psh_forgot_to_set_static_NOCULL + psh_forgot_to_set_static_PHONGMAP + psh_forgot_to_set_static_BUMPMAP2 + psh_forgot_to_set_static_BLENDMODULATE + 0
class gbuffer_ps30_Dynamic_Index
{
public:
	gbuffer_ps30_Dynamic_Index()
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderDynamicTest_gbuffer_ps30 0
