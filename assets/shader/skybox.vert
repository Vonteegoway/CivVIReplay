#version 450

layout(push_constant) uniform ModelView {
	mat4 projModelViewMatrix;
	uvec2 imageExtent;
};

const vec3 CubeVertices[] = {
	vec3(-1.0f,  1.0f, -1.0f),
	vec3(-1.0f, -1.0f, -1.0f),
	vec3( 1.0f, -1.0f, -1.0f),
	vec3( 1.0f, -1.0f, -1.0f),
	vec3( 1.0f,  1.0f, -1.0f),
	vec3(-1.0f,  1.0f, -1.0f),
	vec3(-1.0f, -1.0f,  1.0f),
	vec3(-1.0f, -1.0f, -1.0f),
	vec3(-1.0f,  1.0f, -1.0f),
	vec3(-1.0f,  1.0f, -1.0f),
	vec3(-1.0f,  1.0f,  1.0f),
	vec3(-1.0f, -1.0f,  1.0f),
	vec3( 1.0f, -1.0f, -1.0f),
	vec3( 1.0f, -1.0f,  1.0f),
	vec3( 1.0f,  1.0f,  1.0f),
	vec3( 1.0f,  1.0f,  1.0f),
	vec3( 1.0f,  1.0f, -1.0f),
	vec3( 1.0f, -1.0f, -1.0f),
	vec3(-1.0f, -1.0f,  1.0f),
	vec3(-1.0f,  1.0f,  1.0f),
	vec3( 1.0f,  1.0f,  1.0f),
	vec3( 1.0f,  1.0f,  1.0f),
	vec3( 1.0f, -1.0f,  1.0f),
	vec3(-1.0f, -1.0f,  1.0f),
	vec3(-1.0f,  1.0f, -1.0f),
	vec3( 1.0f,  1.0f, -1.0f),
	vec3( 1.0f,  1.0f,  1.0f),
	vec3( 1.0f,  1.0f,  1.0f),
	vec3(-1.0f,  1.0f,  1.0f),
	vec3(-1.0f,  1.0f, -1.0f),
	vec3(-1.0f, -1.0f, -1.0f),
	vec3(-1.0f, -1.0f,  1.0f),
	vec3( 1.0f, -1.0f, -1.0f),
	vec3( 1.0f, -1.0f, -1.0f),
	vec3(-1.0f, -1.0f,  1.0f),
	vec3( 1.0f, -1.0f,  1.0f)
};

layout(location = 0) out vec3 outPosition;

void main() {
	outPosition = CubeVertices[gl_VertexIndex] * 1000.f;
	gl_Position = (projModelViewMatrix * vec4(outPosition, 1.)).xyww;
}