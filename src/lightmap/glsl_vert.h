static const char* glsl_vert = R"glsl(

#version 460

layout(set = 0, binding = 1) uniform Uniforms
{
	vec3 SunDir;
	float Padding1;
	vec3 SunColor;
	float SunIntensity;
};

layout(push_constant) uniform PushConstants
{
	uint LightStart;
	uint LightEnd;
	int surfaceIndex;
	int pushPadding;
};

layout(location = 0) out vec3 worldpos;

void main()
{
	worldpos = vec3(0.0);
	gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}

)glsl";
