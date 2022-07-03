static const char* glsl_frag = R"glsl(

#version 460
#extension GL_EXT_ray_query : require

layout(set = 0, binding = 0) uniform accelerationStructureEXT acc;

layout(set = 0, binding = 1) uniform Uniforms
{
	vec3 SunDir;
	float Padding1;
	vec3 SunColor;
	float SunIntensity;
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

layout(set = 0, binding = 2) buffer SurfaceIndexBuffer { uint surfaceIndices[]; };
layout(set = 0, binding = 3) buffer SurfaceBuffer { SurfaceInfo surfaces[]; };
layout(set = 0, binding = 4) buffer LightBuffer { LightInfo lights[]; };

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

layout(location = 0) in vec3 worldpos;
layout(location = 0) out vec4 fragcolor;

vec2 Hammersley(uint i, uint N);
float RadicalInverse_VdC(uint bits);

void main()
{
	const float minDistance = 0.01;

	vec3 origin = worldpos;
	vec3 normal;
	if (SurfaceIndex >= 0)
	{
		normal = surfaces[SurfaceIndex].Normal;
		origin += normal * 0.1;
	}

	vec3 incoming = vec3(0.0);

	// Sun light
	{
		const float dist = 32768.0;

		rayQueryEXT rayQuery;
		rayQueryInitializeEXT(rayQuery, acc, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, minDistance, SunDir, dist);

		while(rayQueryProceedEXT(rayQuery))
		{
			if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCommittedIntersectionTriangleEXT)
			{
				rayQueryConfirmIntersectionEXT(rayQuery);
			}
		}

		if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
		{
			int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
			SurfaceInfo surface = surfaces[surfaceIndices[primitiveID]];
			incoming.rgb += SunColor * SunIntensity * surface.Sky;
		}
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
			if (SurfaceIndex >= 0)
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
				rayQueryEXT rayQuery;
				rayQueryInitializeEXT(rayQuery, acc, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, minDistance, dir, dist);

				while(rayQueryProceedEXT(rayQuery)) { }

				if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT)
				{
					incoming.rgb += light.Color * (attenuation * light.Intensity);
				}
			}
		}
	}

	// Ambient occlusion
	if (SurfaceIndex >= 0)
	{
		const float minDistance = 0.05;
		const float aoDistance = 100;
		const int SampleCount = 2048;

		vec3 N = normal;
		vec3 up = abs(N.x) < abs(N.y) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
		vec3 tangent = normalize(cross(up, N));
		vec3 bitangent = cross(N, tangent);

		float ambience = 0.0f;
		for (uint i = 0; i < SampleCount; i++)
		{
			vec2 Xi = Hammersley(i, SampleCount);
			vec3 H = normalize(vec3(Xi.x * 2.0f - 1.0f, Xi.y * 2.0f - 1.0f, 1.5 - length(Xi)));
			vec3 L = H.x * tangent + H.y * bitangent + H.z * N;

			rayQueryEXT rayQuery;
			rayQueryInitializeEXT(rayQuery, acc, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, minDistance, L, aoDistance);

			while(rayQueryProceedEXT(rayQuery))
			{
				if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCommittedIntersectionTriangleEXT)
				{
					rayQueryConfirmIntersectionEXT(rayQuery);
				}
			}

			if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
			{
				int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
				SurfaceInfo surface = surfaces[surfaceIndices[primitiveID]];
				if (surface.Sky == 0.0)
				{
					float hitDistance = rayQueryGetIntersectionTEXT(rayQuery, true);
					ambience += clamp(hitDistance / aoDistance, 0.0, 1.0);
				}
			}
			else
			{
				ambience += 1.0;
			}
		}
		ambience /= float(SampleCount);

		incoming.rgb = incoming.rgb * ambience;
	}

	fragcolor = vec4(incoming, 1.0);
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
