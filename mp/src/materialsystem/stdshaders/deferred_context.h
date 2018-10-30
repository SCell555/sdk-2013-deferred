#ifndef DEFERRED_CONTEXT_H
#define DEFERRED_CONTEXT_H

#include "BaseVSShader.h"
#include "tier0/memdbgon.h"

#pragma warning(push)
#pragma warning(disable:4351)

class CDeferredPerMaterialContextData : public CBasePerMaterialContextData
{
public:

	enum DefStage_t
	{
		DEFSTAGE_GBUFFER = 0,
		DEFSTAGE_SHADOW,
		DEFSTAGE_COMPOSITE,

		DEFSTAGE_COUNT
	};

	CDeferredPerMaterialContextData()
	{
		V_memset( m_piCommandArray, 0, sizeof( m_piCommandArray ) );
	};

	~CDeferredPerMaterialContextData()
	{
		for ( int i = 0; i < DEFSTAGE_COUNT; i++ )
			if ( m_piCommandArray[i] )
				delete[] m_piCommandArray[i];
	};

	void SetCommands( DefStage_t p_iStage, uint8 *p_iCmd )
	{
		m_piCommandArray[p_iStage] = p_iCmd;
	};

	uint8 *GetCommands( DefStage_t p_iStage )
	{
		return m_piCommandArray[p_iStage];
	}

	bool HasCommands( DefStage_t p_iStage )
	{
		return m_piCommandArray[p_iStage] != NULL;
	}

private:
	uint8 *m_piCommandArray[DEFSTAGE_COUNT];
};

#include "tier0/memdbgoff.h"

#pragma warning(pop)

#endif