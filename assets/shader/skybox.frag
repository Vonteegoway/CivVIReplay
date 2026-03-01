#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(set = 0, binding = 0) uniform samplerCube sSkyTexture;


layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

void main() {
	outColor.rgba = texture(sSkyTexture, inPosition);
}