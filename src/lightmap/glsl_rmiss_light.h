static const char* glsl_rmiss_light = R"glsl(

#version 460
#extension GL_EXT_ray_tracing : require

struct hitPayload
{
	float hitAttenuation;
};

layout(location = 0) rayPayloadInEXT hitPayload payload;

void main()
{
	payload.hitAttenuation = 1.0;
}

)glsl";
