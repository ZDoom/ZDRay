static const char* glsl_rgen_ambient = R"glsl(

#version 460
#extension GL_EXT_ray_tracing : require

struct hitPayload
{
	vec3 hitPosition;
	float hitAttenuation;
	int hitSurfaceIndex;
};

layout(location = 0) rayPayloadEXT hitPayload payload;

layout(set = 0, binding = 0) uniform accelerationStructureEXT acc;
layout(set = 0, binding = 1, rgba32f) uniform image2D startpositions;
layout(set = 0, binding = 2, rgba32f) uniform image2D positions;
layout(set = 0, binding = 3, rgba32f) uniform image2D outputs;

layout(set = 0, binding = 4) uniform Uniforms
{
	uint SampleIndex;
	uint SampleCount;
	uint PassType;
	uint Padding0;
	vec3 SunDir;
	float SampleDistance;
	vec3 SunColor;
	float SunIntensity;
	vec3 HemisphereVec;
	float Padding1;
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

layout(set = 0, binding = 6) buffer SurfaceBuffer { SurfaceInfo surfaces[]; };

layout(push_constant) uniform PushConstants
{
	uint LightStart;
	uint LightEnd;
	ivec2 pushPadding;
};

vec2 Hammersley(uint i, uint N);
float RadicalInverse_VdC(uint bits);

void main()
{
	ivec2 texelPos = ivec2(gl_LaunchIDEXT.xy);

	vec4 data0 = imageLoad(startpositions, texelPos);
	vec4 incoming = imageLoad(outputs, texelPos);

	if (PassType == 1)
		incoming.rgb = vec3(1.0); // For debugging

	vec3 origin = data0.xyz;
	int surfaceIndex = int(data0.w);

	if (surfaceIndex >= 0)
	{
		const float minDistance = 0.05;
		const float aoDistance = 100;

		vec3 N = surfaces[surfaceIndex].Normal;
		vec3 up = abs(N.x) < abs(N.y) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
		vec3 tangent = normalize(cross(up, N));
		vec3 bitangent = cross(N, tangent);

		float ambience = 0.0f;
		for (uint i = 0; i < SampleCount; i++)
		{
			vec2 Xi = Hammersley(i, SampleCount);
			vec3 H = normalize(vec3(Xi.x * 2.0f - 1.0f, Xi.y * 2.0f - 1.0f, RadicalInverse_VdC(i) + 0.01f));
			vec3 L = H.x * tangent + H.y * bitangent + H.z * N;
			traceRayEXT(acc, gl_RayFlagsOpaqueEXT, 0xff, 3, 0, 3, origin, minDistance, L, 32768, 0);
			ambience += clamp(payload.hitAttenuation / aoDistance, 0.0, 1.0);
		}
		ambience /= float(SampleCount);

		incoming.rgb = incoming.rgb * ambience;
	}

	imageStore(outputs, texelPos, incoming);
}

vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

)glsl";
