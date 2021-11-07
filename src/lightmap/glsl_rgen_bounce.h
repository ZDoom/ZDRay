static const char* glsl_rgen_bounce = R"glsl(

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
	uint Padding2;
	vec3 LightOrigin;
	float Padding0;
	float LightRadius;
	float LightIntensity;
	float LightInnerAngleCos;
	float LightOuterAngleCos;
	vec3 LightDir;
	float SampleDistance;
	vec3 LightColor;
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

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness);
vec2 Hammersley(uint i, uint N);
float RadicalInverse_VdC(uint bits);

void main()
{
	ivec2 texelPos = ivec2(gl_LaunchIDEXT.xy);

	vec4 data0;
	if (PassType == 2)
		data0 = imageLoad(positions, texelPos);
	else
		data0 = imageLoad(startpositions, texelPos);

	vec4 incoming = vec4(0.0, 0.0, 0.0, 1.0);

	int surfaceIndex = int(data0.w);
	if (surfaceIndex >= 0)
	{
		SurfaceInfo surface = surfaces[surfaceIndex];

		vec3 origin = data0.xyz;
		vec3 normal = surface.Normal;
	
		if (PassType == 0)
		{
			incoming.rgb = surface.EmissiveColor * surface.EmissiveIntensity;
		}
		else
		{
			incoming = imageLoad(outputs, texelPos);

			if (PassType == 1)
				incoming.w = 1.0f / float(SampleCount);

			vec2 Xi = Hammersley(SampleIndex, SampleCount);
			vec3 H = ImportanceSampleGGX(Xi, normal, 1.0f);
			vec3 L = normalize(H * (2.0f * dot(normal, H)) - normal);

			float NdotL = max(dot(normal, L), 0.0);

			const float p = 1 / (2 * 3.14159265359);
			incoming.w *= NdotL / p;

			if (NdotL > 0.0f)
			{
				const float minDistance = 0.1;

				traceRayEXT(acc, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, origin, minDistance, L, 2000, 0);
				if (payload.hitAttenuation == 1.0)
				{
					float hitDistance = distance(origin, payload.hitPosition);
					surfaceIndex = payload.hitSurfaceIndex;
					surface = surfaces[surfaceIndex];
					origin = payload.hitPosition;

					if (surface.EmissiveDistance > 0.0)
					{
						float attenuation = max(1.0 - (hitDistance / surface.EmissiveDistance), 0.0f);
						incoming.rgb += surface.EmissiveColor * (surface.EmissiveIntensity * attenuation * incoming.w);
					}
				}
			}

			incoming.w *= 0.25; // the amount of incoming light the surfaces emit
		}

		data0.xyz = origin;
		data0.w = float(surfaceIndex);
	}

	imageStore(positions, texelPos, data0);
	imageStore(outputs, texelPos, incoming);
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0f * 3.14159265359 * Xi.x;
	float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates
	vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

	// from tangent-space vector to world-space sample vector
	vec3 up = abs(N.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
	return normalize(sampleVec);
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

vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

)glsl";
