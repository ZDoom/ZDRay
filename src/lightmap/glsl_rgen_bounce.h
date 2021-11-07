static const char* glsl_rgen_bounce = R"glsl(

#version 460
#extension GL_EXT_ray_tracing : require

struct hitPayload
{
	float hitAttenuation;
};

layout(location = 0) rayPayloadEXT hitPayload payload;

layout(set = 0, binding = 0) uniform accelerationStructureEXT acc;
layout(set = 0, binding = 1, rgba32f) uniform image2D positions;
layout(set = 0, binding = 2, rgba32f) uniform image2D normals;
layout(set = 0, binding = 3, rgba32f) uniform image2D outputs;

layout(set = 0, binding = 4) uniform Uniforms
{
	vec3 LightOrigin;
	float PassType;
	float LightRadius;
	float LightIntensity;
	float LightInnerAngleCos;
	float LightOuterAngleCos;
	vec3 LightSpotDir;
	float SampleDistance;
	vec3 LightColor;
	float Padding;
};

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

void main()
{
	ivec2 texelPos = ivec2(gl_LaunchIDEXT.xy);
	vec4 data0 = imageLoad(positions, texelPos);
	vec4 data1 = imageLoad(normals, texelPos);
	if (data1 == vec4(0))
		return;

	vec3 origin = data0.xyz;
	vec3 normal = data1.xyz;

	vec4 emittance = vec4(0.0);
	if (PassType == 1.0)
		emittance = imageLoad(outputs, texelPos);

	const float minDistance = 0.01;
	const uint sample_count = 1024;

	float dist = distance(LightOrigin, origin);
	if (dist > minDistance && dist < LightRadius)
	{
		vec3 dir = normalize(LightOrigin - origin);

		float distAttenuation = max(1.0 - (dist / LightRadius), 0.0);
		float angleAttenuation = max(dot(normal, dir), 0.0);
		float spotAttenuation = 1.0;
		if (LightOuterAngleCos > -1.0)
		{
			float cosDir = dot(dir, LightSpotDir);
			spotAttenuation = smoothstep(LightOuterAngleCos, LightInnerAngleCos, cosDir);
			spotAttenuation = max(spotAttenuation, 0.0);
		}

		float attenuation = distAttenuation * angleAttenuation * spotAttenuation;
		if (attenuation > 0.0)
		{
			float shadowAttenuation = 0.0;
			vec3 e0 = cross(normal, abs(normal.x) < abs(normal.y) ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0));
			vec3 e1 = cross(normal, e0);
			e0 = cross(normal, e1);
			for (uint i = 0; i < sample_count; i++)
			{
				vec2 offset = (Hammersley(i, sample_count) - 0.5) * SampleDistance;
				vec3 origin2 = origin + offset.x * e0 + offset.y * e1;

				float dist2 = distance(LightOrigin, origin2);
				vec3 dir2 = normalize(LightOrigin - origin2);

				traceRayEXT(acc, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, origin2, minDistance, dir2, dist2, 0);
				shadowAttenuation += payload.hitAttenuation;
			}
			shadowAttenuation *= 1.0 / float(sample_count);

			attenuation *= shadowAttenuation;

			emittance.rgb += LightColor * (attenuation * LightIntensity);
		}
	}

	emittance.w += 1.0;
	imageStore(outputs, texelPos, emittance);
}

)glsl";
