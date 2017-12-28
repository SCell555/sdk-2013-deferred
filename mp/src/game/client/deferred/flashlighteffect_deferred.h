#ifndef FLASHLIGHTEFFECT_DEFERRED_H
#define FLASHLIGHTEFFECT_DEFERRED_H


#include "flashlighteffect.h"

struct def_light_t;

class CFlashlightEffectDeferred : public CFlashlightEffect
{
public:

	CFlashlightEffectDeferred( int nEntIndex = 0 );
	~CFlashlightEffectDeferred();

protected:

	void UpdateLightProjection( FlashlightState_t &state );
	void LightOff();

private:

	def_light_t *m_pDefLight;
};


#endif