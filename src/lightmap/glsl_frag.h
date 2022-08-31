static const char* glsl_frag = R"glsl(

#if defined(USE_RAYQUERY)
layout(set = 0, binding = 0) uniform accelerationStructureEXT acc;
#else
struct CollisionNode
{
	vec3 center;
	float padding1;
	vec3 extents;
	float padding2;
	int left;
	int right;
	int element_index;
	int padding3;
};
layout(set = 1, binding = 0) buffer NodeBuffer { CollisionNode nodes[]; };
layout(set = 1, binding = 1) buffer VertexBuffer { vec3 vertices[]; };
layout(set = 1, binding = 2) buffer ElementBuffer { vec3 elements[]; };
#endif

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
	float Sky;
	float SamplingDistance;
	float Padding1, Padding2, Padding3;
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
	vec3 LightmapOrigin;
	float PushPadding2;
	vec3 LightmapStepX;
	float PushPadding3;
	vec3 LightmapStepY;
	float PushPadding4;
};

layout(location = 0) centroid in vec3 worldpos;
layout(location = 0) out vec4 fragcolor;

vec3 TraceSunLight(vec3 origin);
vec3 TraceLight(vec3 origin, vec3 normal, LightInfo light);
float TraceAmbientOcclusion(vec3 origin, vec3 normal);
vec2 Hammersley(uint i, uint N);
float RadicalInverse_VdC(uint bits);

bool TraceAnyHit(vec3 origin, float tmin, vec3 dir, float tmax);
int TraceFirstHitTriangle(vec3 origin, float tmin, vec3 dir, float tmax);
int TraceFirstHitTriangleT(vec3 origin, float tmin, vec3 dir, float tmax, out float t);

void main()
{
	vec3 normal = surfaces[SurfaceIndex].Normal;
	vec3 origin = worldpos + normal * 0.1;

	vec3 incoming = TraceSunLight(origin);

	for (uint j = LightStart; j < LightEnd; j++)
	{
		incoming += TraceLight(origin, normal, lights[j]);
	}

	incoming.rgb *= TraceAmbientOcclusion(origin, normal);

	fragcolor = vec4(incoming, 1.0);
}

vec3 TraceLight(vec3 origin, vec3 normal, LightInfo light)
{
	const float minDistance = 0.01;
	vec3 incoming = vec3(0.0);
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
			if (TraceAnyHit(origin, minDistance, dir, dist))
			{
				incoming.rgb += light.Color * (attenuation * light.Intensity);
			}
		}
	}
	return incoming;
}

vec3 TraceSunLight(vec3 origin)
{
	const float minDistance = 0.01;
	vec3 incoming = vec3(0.0);
	const float dist = 32768.0;

	int primitiveID = TraceFirstHitTriangle(origin, minDistance, SunDir, dist);
	if (primitiveID != -1)
	{
		SurfaceInfo surface = surfaces[surfaceIndices[primitiveID]];
		incoming.rgb += SunColor * SunIntensity * surface.Sky;
	}
	return incoming;
}

float TraceAmbientOcclusion(vec3 origin, vec3 normal)
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

		float hitDistance;
		int primitiveID = TraceFirstHitTriangleT(origin, minDistance, L, aoDistance, hitDistance);
		if (primitiveID != -1)
		{
			SurfaceInfo surface = surfaces[surfaceIndices[primitiveID]];
			if (surface.Sky == 0.0)
			{
				ambience += clamp(hitDistance / aoDistance, 0.0, 1.0);
			}
		}
		else
		{
			ambience += 1.0;
		}
	}
	return ambience / float(SampleCount);
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

#if defined(USE_RAYQUERY)

bool TraceAnyHit(vec3 origin, float tmin, vec3 dir, float tmax)
{
	rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery, acc, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, tmin, dir, tmax);
	while(rayQueryProceedEXT(rayQuery)) { }
	return rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT;
}

int TraceFirstHitTriangle(vec3 origin, float tmin, vec3 dir, float tmax)
{
	rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery, acc, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, tmin, dir, tmax);

	while(rayQueryProceedEXT(rayQuery))
	{
		if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCommittedIntersectionTriangleEXT)
		{
			rayQueryConfirmIntersectionEXT(rayQuery);
		}
	}

	if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
	{
		return rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
	}
	else
	{
		return -1;
	}
}

int TraceFirstHitTriangleT(vec3 origin, float tmin, vec3 dir, float tmax, out float t)
{
	rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery, acc, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, tmin, dir, tmax);

	while(rayQueryProceedEXT(rayQuery))
	{
		if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCommittedIntersectionTriangleEXT)
		{
			rayQueryConfirmIntersectionEXT(rayQuery);
		}
	}

	if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
	{
		t = rayQueryGetIntersectionTEXT(rayQuery, true);
		return rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
	}
	else
	{
		return -1;
	}
}

#else

bool TraceAnyHit(vec3 origin, float tmin, vec3 dir, float tmax)
{
	// To do: port TriangleMeshShape::find_any_hit(TriangleMeshShape *shape, const vec3 &ray_start, const vec3 &ray_end) to glsl
	return false;
}

int TraceFirstHitTriangle(vec3 origin, float tmin, vec3 dir, float tmax)
{
	float t;
	return TraceFirstHitTriangleT(origin, tmin, dir, tmax, t);
}

int TraceFirstHitTriangleT(vec3 origin, float tmin, vec3 dir, float tmax, out float t)
{
	// To do: port TriangleMeshShape::find_first_hit(TriangleMeshShape *shape, const vec3 &ray_start, const vec3 &ray_end) to glsl
	return -1;
}

#endif


)glsl";
