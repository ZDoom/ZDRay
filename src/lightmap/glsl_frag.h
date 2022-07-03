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

	fragcolor = vec4(incoming, 1.0);
}

)glsl";
