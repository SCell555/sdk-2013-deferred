#ifndef DEF_LIGHT_LUMP_T
#define DEF_LIGHT_LUMP_T

#pragma once

#include "mathlib/vector.h"
#include "datamap.h"

#ifndef GAMELUMP_MAKE_CODE
#define GAMELUMP_MAKE_CODE(a, b, c, d) ((a) << 24 | (b) << 16 | (c) << 8 | (d) << 0)
#endif

enum
{
	GAMELUMP_DEFERRED_LIGHTS = GAMELUMP_MAKE_CODE('d', 'l', 's', 'l'),
	GAMELUMP_DEFERRED_LIGHTS_VERSION = 0
};

struct color_t
{
	DECLARE_BYTESWAP_DATADESC();
	unsigned int r, g, b, a;
};

struct def_lump_light_global_t
{
	DECLARE_BYTESWAP_DATADESC();
	QAngle	angles;
	color_t	diffuse;
	color_t	ambientLow;
	color_t	ambientHigh;
};

enum
{
	DEFLIGHT_IS_SPOTLIGHT = 1 << 0,
	DEFLIGHT_ENABLED_VOLUMETRICS = 1 << 1,
	DEFLIGHT_ENABLED_COOKIE = 1 << 2,
	DEFLIGHT_ENABLED_LIGHTSTYLE = 1 << 3
};

struct def_lump_light_t
{
	DECLARE_BYTESWAP_DATADESC();
	Vector	origin;
	QAngle	angles;
	byte	flags;
	color_t	diffuse;
	color_t	ambient;
	float	radius;
	float	power;
	float	vis_dist;
	float	vis_range;
	float	shadow_dist;
	float	shadow_range;
	float	spot_cone_inner;
	float	spot_cone_outer;
	int		style_seed;
	float	style_amt;
	float	style_speed;
	float	style_smooth;
	float	style_random;
	char	cookietex[128];
};

#endif	// DEF_LIGHT_LUMP_T