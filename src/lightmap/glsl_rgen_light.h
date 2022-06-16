static const char* glsl_rgen_light = R"glsl(

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
	float Padding1;
	vec3 SunColor;
	float SunIntensity;
	vec3 HemisphereVec;
	float Padding2;
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

struct LightInfo
{
	vec3 Origin;
	float Padding0;
	float Radius;
	float Intensity;
	float InnerAngleCos;
	float OuterAngleCos;
	vec3 SpotDir;
	float Padding1;
	vec3 Color;
	float Padding2;
};

layout(set = 0, binding = 6) buffer SurfaceBuffer { SurfaceInfo surfaces[]; };
layout(set = 0, binding = 7) buffer LightBuffer { LightInfo lights[]; };

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
	vec4 incoming = imageLoad(outputs, texelPos);
	vec4 data0 = imageLoad(positions, texelPos);
	int surfaceIndex = int(data0.w);
	if (surfaceIndex == -1 || incoming.w <= 0.0)
		return;

	vec3 origin = data0.xyz;
	vec3 normal;
	if (surfaceIndex >= 0)
	{
		normal = surfaces[surfaceIndex].Normal;
		origin += normal * 0.1;
	}

	const float minDistance = 0.01;

	if (LightStart == 0) // Sun light
	{
		const float dist = 32768.0;

		float attenuation = 0.0;
		if (PassType == 0 && surfaceIndex >= 0)
		{
			vec3 e0 = normalize(cross(normal, abs(normal.x) < abs(normal.y) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0)));
			vec3 e1 = cross(normal, e0);
			e0 = cross(normal, e1);

			for (uint i = 0; i < SampleCount; i++)
			{
				vec2 offset = (Hammersley(i, SampleCount) - 0.5) * surfaces[surfaceIndex].SamplingDistance;
				vec3 origin2 = origin + offset.x * e0 + offset.y * e1;

				traceRayEXT(acc, gl_RayFlagsOpaqueEXT, 0xff, 2, 0, 2, origin2, minDistance, SunDir, dist, 0);
				attenuation += payload.hitAttenuation;
			}
			attenuation *= 1.0 / float(SampleCount);
		}
		else
		{
			traceRayEXT(acc, gl_RayFlagsOpaqueEXT, 0xff, 2, 0, 2, origin, minDistance, SunDir, dist, 0);
			attenuation = payload.hitAttenuation;
		}
		incoming.rgb += SunColor * (attenuation * SunIntensity * incoming.w);
	}

	for (uint j = LightStart; j < LightEnd; j++)
	{
		LightInfo light = lights[j];

		float dist = distance(light.Origin, origin);
		if (dist > minDistance && dist < light.Radius)
		{
			vec3 dir = normalize(light.Origin - origin);

			float distAttenuation = max(1.0 - (dist / light.Radius), 0.0);
			float angleAttenuation = 1.0f;
			if (surfaceIndex >= 0)
			{
				angleAttenuation = max(dot(normal, dir), 0.0);
			}
			float spotAttenuation = 1.0;
			if (light.OuterAngleCos > -1.0)
			{
				float cosDir = dot(dir, light.SpotDir);
				spotAttenuation = smoothstep(light.OuterAngleCos, light.InnerAngleCos, cosDir);
				spotAttenuation = max(spotAttenuation, 0.0);
			}

			float attenuation = distAttenuation * angleAttenuation * spotAttenuation;
			if (attenuation > 0.0)
			{
				float shadowAttenuation = 0.0;

				if (PassType == 0 && surfaceIndex >= 0)
				{
					vec3 e0 = normalize(cross(normal, abs(normal.x) < abs(normal.y) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0)));
					vec3 e1 = cross(normal, e0);
					e0 = cross(normal, e1);
					for (uint i = 0; i < SampleCount; i++)
					{
						vec2 offset = (Hammersley(i, SampleCount) - 0.5) * surfaces[surfaceIndex].SamplingDistance;
						vec3 origin2 = origin + offset.x * e0 + offset.y * e1;

						float dist2 = distance(light.Origin, origin2);
						vec3 dir2 = normalize(light.Origin - origin2);

						traceRayEXT(acc, gl_RayFlagsOpaqueEXT, 0xff, 1, 0, 1, origin2, minDistance, dir2, dist2, 0);
						shadowAttenuation += payload.hitAttenuation;
					}
					shadowAttenuation *= 1.0 / float(SampleCount);
				}
				else
				{
					traceRayEXT(acc, gl_RayFlagsOpaqueEXT, 0xff, 1, 0, 1, origin, minDistance, dir, dist, 0);
					shadowAttenuation = payload.hitAttenuation;
				}

				attenuation *= shadowAttenuation;

				incoming.rgb += light.Color * (attenuation * light.Intensity) * incoming.w;
			}
		}
	}

	imageStore(outputs, texelPos, incoming);
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
