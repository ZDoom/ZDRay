static const char* glsl_miss = R"glsl(

#version 460
#extension GL_EXT_ray_tracing : require

struct hitPayload
{
	float hitAttenuation;
	bool isSkyRay;
};

layout(location = 0) rayPayloadInEXT hitPayload payload;

void main()
{
	if (!payload.isSkyRay)
	{
		payload.hitAttenuation = 1.0;
	}
	else
	{
		payload.hitAttenuation = 0.0;
	}
}

)glsl";
