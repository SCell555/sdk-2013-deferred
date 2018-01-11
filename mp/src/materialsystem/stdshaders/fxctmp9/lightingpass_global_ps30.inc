#include "shaderlib/cshader.h"
class lightingpass_global_ps30_Static_Index
{
private:
	int m_nHARDWARE_FILTER;
#ifdef _DEBUG
	bool m_bHARDWARE_FILTER;
#endif
public:
	void SetHARDWARE_FILTER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHARDWARE_FILTER = i;
#ifdef _DEBUG
		m_bHARDWARE_FILTER = true;
#endif
	}
	void SetHARDWARE_FILTER( bool i )
	{
		m_nHARDWARE_FILTER = i ? 1 : 0;
#ifdef _DEBUG
		m_bHARDWARE_FILTER = true;
#endif
	}
public:
	lightingpass_global_ps30_Static_Index( )
	{
#ifdef _DEBUG
		m_bHARDWARE_FILTER = false;
#endif // _DEBUG
		m_nHARDWARE_FILTER = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bHARDWARE_FILTER;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nHARDWARE_FILTER ) + 0;
	}
};
#define shaderStaticTest_lightingpass_global_ps30 psh_forgot_to_set_static_HARDWARE_FILTER + 0
class lightingpass_global_ps30_Dynamic_Index
{
private:
	int m_nHAS_SHADOW;
#ifdef _DEBUG
	bool m_bHAS_SHADOW;
#endif
public:
	void SetHAS_SHADOW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHAS_SHADOW = i;
#ifdef _DEBUG
		m_bHAS_SHADOW = true;
#endif
	}
	void SetHAS_SHADOW( bool i )
	{
		m_nHAS_SHADOW = i ? 1 : 0;
#ifdef _DEBUG
		m_bHAS_SHADOW = true;
#endif
	}
public:
	lightingpass_global_ps30_Dynamic_Index()
	{
#ifdef _DEBUG
		m_bHAS_SHADOW = false;
#endif // _DEBUG
		m_nHAS_SHADOW = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bHAS_SHADOW;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nHAS_SHADOW ) + 0;
	}
};
#define shaderDynamicTest_lightingpass_global_ps30 psh_forgot_to_set_dynamic_HAS_SHADOW + 0
