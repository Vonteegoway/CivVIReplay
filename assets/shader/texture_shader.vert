#version 450

layout(push_constant) uniform ModelView {
	mat4 projModelViewMatrix;
	uvec2 imageExtent;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;
layout(location = 1) out vec2 outUV;

void main() {
	gl_Position = vec4(inPosition.x * 2.f / imageExtent.x - 1.f, inPosition.y * 2.f / imageExtent.y - 1.f, 0.0, 1.0);
	outFragColor = inColor;
	outUV = inUV;
}