static const char* glsl_rchit_light = R"glsl(

#version 460
#extension GL_EXT_ray_tracing : require

struct hitPayload
{
	vec3 hitPosition;
	float hitAttenuation;
	int hitSurfaceIndex;
};

struct SurfaceInfo
{
	vec3 Normal;
	float EmissiveDistance;
	vec3 EmissiveColor;
	float EmissiveIntensity;
	float Sky;
	float SamplingDistance;
	float Padding1, Padding2;
};

layout(location = 0) rayPayloadInEXT hitPayload payload;

layout(set = 0, binding = 5) buffer SurfaceIndexBuffer { uint surfaceIndices[]; };
layout(set = 0, binding = 6) buffer SurfaceBuffer { SurfaceInfo surfaces[]; };

void main()
{
	//SurfaceInfo surface = surfaces[surfaceIndices[gl_PrimitiveID]];

	payload.hitAttenuation = 0.0;
}

)glsl";
