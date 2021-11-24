static const char* glsl_rmiss_ambient = R"glsl(

#version 460
#extension GL_EXT_ray_tracing : require

struct hitPayload
{
	vec3 hitPosition;
	float hitAttenuation;
	int hitSurfaceIndex;
};

layout(location = 0) rayPayloadInEXT hitPayload payload;

void main()
{
	payload.hitAttenuation = 100000.0;
}

)glsl";
