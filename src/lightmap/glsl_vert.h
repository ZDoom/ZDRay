static const char* glsl_vert = R"glsl(

#version 460

layout(push_constant) uniform PushConstants
{
	uint LightStart;
	uint LightEnd;
	int SurfaceIndex;
	int PushPadding1;
	vec2 TileTL;
	vec2 TileBR;
	vec3 LightmapOrigin;
	float PushPadding2;
	vec3 LightmapStepX;
	float PushPadding3;
	vec3 LightmapStepY;
	float PushPadding4;
};

layout(location = 0) out vec3 worldpos;

vec2 positions[4] = vec2[](
	vec2(0.0, 0.0),
	vec2(1.0, 0.0),
	vec2(0.0, 1.0),
	vec2(1.0, 1.0)
);

void main()
{
	vec2 tilepos = positions[gl_VertexIndex];
	worldpos = LightmapOrigin + LightmapStepX * tilepos.x + LightmapStepY * tilepos.y;
	gl_Position = vec4(mix(TileTL, TileBR, tilepos) * 2.0 - 1.0, 0.0, 1.0);
}

)glsl";
