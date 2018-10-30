#include "shaderlib/cshader.h"
class radiosity_propagate_ps30_Static_Index
{
private:
	int m_nBLUR;
#ifdef _DEBUG
	bool m_bBLUR;
#endif
public:
	void SetBLUR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLUR = i;
#ifdef _DEBUG
		m_bBLUR = true;
#endif
	}
	void SetBLUR( bool i )
	{
		m_nBLUR = i ? 1 : 0;
#ifdef _DEBUG
		m_bBLUR = true;
#endif
	}
public:
	radiosity_propagate_ps30_Static_Index( )
	{
#ifdef _DEBUG
		m_bBLUR = false;
#endif // _DEBUG
		m_nBLUR = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bBLUR;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nBLUR ) + 0;
	}
};
#define shaderStaticTest_radiosity_propagate_ps30 psh_forgot_to_set_static_BLUR + 0
class radiosity_propagate_ps30_Dynamic_Index
{
public:
	radiosity_propagate_ps30_Dynamic_Index()
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
#define shaderDynamicTest_radiosity_propagate_ps30 0
