#version 450

layout(location = 0) in vec4 inFragColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = inFragColor;
}