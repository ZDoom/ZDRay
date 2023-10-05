static const char* binding_raytrace_glsl = R"glsl(

#if defined(USE_RAYQUERY)

layout(set = 1, binding = 0) uniform accelerationStructureEXT acc;

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

layout(std430, set = 1, binding = 0) buffer NodeBuffer
{
	int nodesRoot;
	int nodebufferPadding1;
	int nodebufferPadding2;
	int nodebufferPadding3;
	CollisionNode nodes[];
};

#endif

struct SurfaceVertex
{
	vec4 pos;
	vec2 uv;
	float Padding1, Padding2;
};

layout(std430, set = 1, binding = 1) buffer VertexBuffer { SurfaceVertex vertices[]; };
layout(std430, set = 1, binding = 2) buffer ElementBuffer { int elements[]; };

)glsl";
