static const char* glsl_closesthit = R"glsl(

#version 460
#extension GL_EXT_ray_tracing : require

struct hitPayload
{
	float hitAttenuation;
	bool isSkyRay;
};

struct SurfaceInfo
{
	vec3 Normal;
	float EmissiveDistance;
	vec3 EmissiveColor;
	float EmissiveIntensity;
	float Sky;
	float Padding0, Padding1, Padding2;
};

layout(location = 0) rayPayloadInEXT hitPayload payload;

layout(set = 0, binding = 5) buffer SurfaceIndexBuffer { int surfaceIndices[]; };
layout(set = 0, binding = 6) buffer SurfaceBuffer { SurfaceInfo surfaces[]; };

void main()
{
	SurfaceInfo surface = surfaces[surfaceIndices[gl_PrimitiveID]];

	if (!payload.isSkyRay)
	{
		payload.hitAttenuation = 0.0;
	}
	else
	{
		payload.hitAttenuation = surface.Sky;
	}
}

)glsl";
