#version 450

layout(push_constant) uniform ModelView {
	mat4 projModelViewMatrix;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in int inMaterialIndex;

#if defined(INSTANCING)
layout(location = 5) in vec3 inInstancePosition;
layout(location = 6) in vec3 inInstanceRotation;
layout(location = 7) in vec3 inInstanceScale;
#endif

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outFragPos;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out int outMaterialIndex;

void main() {
#if defined(INSTANCING)
	vec3 position = inPosition * inInstanceScale + inInstancePosition;
#else
	vec3 position = inPosition;
#endif
	gl_Position = projModelViewMatrix * vec4(position, 1.0);
	outUV = inUV;
	outNormal = inNormal;
	outFragPos = position;
	outMaterialIndex = inMaterialIndex;
}