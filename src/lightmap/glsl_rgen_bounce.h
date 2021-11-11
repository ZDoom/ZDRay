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
	uint LightCount;
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

vec3 ImportanceSample(vec3 N);

void main()
{
	ivec2 texelPos = ivec2(gl_LaunchIDEXT.xy);

	vec4 data0;
	if (PassType == 2)
		data0 = imageLoad(positions, texelPos);
	else
		data0 = imageLoad(startpositions, texelPos);

	vec4 incoming = vec4(0.0, 0.0, 0.0, 1.0);
	if (PassType != 0)
		incoming = imageLoad(outputs, texelPos);

	vec3 origin = data0.xyz;
	int surfaceIndex = int(data0.w);
	if (surfaceIndex != -1)
	{
		if (PassType == 0)
		{
			if (surfaceIndex >= 0)
			{
				SurfaceInfo surface = surfaces[surfaceIndex];
				incoming.rgb = surface.EmissiveColor * surface.EmissiveIntensity;
			}
		}
		else
		{
			if (PassType == 1)
				incoming.w = 1.0f / float(SampleCount);

			vec3 normal;
			if (surfaceIndex >= 0)
			{
				normal = surfaces[surfaceIndex].Normal;
			}
			else
			{
				switch (SampleIndex % 6)
				{
				case 0: normal = vec3( 1.0f,  0.0f,  0.0f); break;
				case 1: normal = vec3(-1.0f,  0.0f,  0.0f); break;
				case 2: normal = vec3( 0.0f,  1.0f,  0.0f); break;
				case 3: normal = vec3( 0.0f, -1.0f,  0.0f); break;
				case 4: normal = vec3( 0.0f,  0.0f,  1.0f); break;
				case 5: normal = vec3( 0.0f,  0.0f, -1.0f); break;
				}
			}

			vec3 H = ImportanceSample(normal);
			vec3 L = normalize(H * (2.0f * dot(normal, H)) - normal);

			float NdotL = max(dot(normal, L), 0.0);

			const float p = 1 / (2 * 3.14159265359);
			incoming.w *= NdotL / p;

			surfaceIndex = -1;
			if (NdotL > 0.0f)
			{
				const float minDistance = 0.1;

				traceRayEXT(acc, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, origin + normal * 0.1, minDistance, L, 32768, 0);
				if (payload.hitAttenuation == 1.0)
				{
					surfaceIndex = payload.hitSurfaceIndex;
					SurfaceInfo surface = surfaces[surfaceIndex];
					if (surface.EmissiveDistance > 0.0)
					{
						float hitDistance = distance(origin, payload.hitPosition);
						float attenuation = max(1.0 - (hitDistance / surface.EmissiveDistance), 0.0f);
						incoming.rgb += surface.EmissiveColor * (surface.EmissiveIntensity * attenuation * incoming.w);
					}
					origin = payload.hitPosition;
				}
			}

			incoming.w *= 0.25; // the amount of incoming light the surfaces emit
		}
	}

	data0.xyz = origin;
	data0.w = float(surfaceIndex);

	imageStore(positions, texelPos, data0);
	imageStore(outputs, texelPos, incoming);
}

vec3 ImportanceSample(vec3 N)
{
	// from tangent-space vector to world-space sample vector
	vec3 up = abs(N.x) < abs(N.y) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);

	vec3 sampleVec = tangent * HemisphereVec.x + bitangent * HemisphereVec.y + N * HemisphereVec.z;
	return normalize(sampleVec);
}

)glsl";
