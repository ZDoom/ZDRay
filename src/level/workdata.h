
#pragma once

#include "framework/zdray.h"

struct vertex_t
{
	fixed_t x, y;
};

struct node_t
{
	fixed_t x, y, dx, dy;
	fixed_t bbox[2][4];
	unsigned int intchildren[2];
};

struct subsector_t
{
	uint32_t numlines;
	uint32_t firstline;
};
