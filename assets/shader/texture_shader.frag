#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform sampler2D sTextures[];


layout(location = 0) in vec4 inFragColor;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = inFragColor;
	vec4 texColor = texture(sTextures[0], inUV).xyzw; 
	outColor.rbga *= texColor.rbga;
}