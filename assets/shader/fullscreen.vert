#version 450 
#extension GL_ARB_shader_viewport_layer_array : enable

layout(push_constant) uniform FullScreenData {
	mat4 projModelViewMatrix;
	int TargetLayer;
};

layout (location = 0) out vec3 outPosition;

void main() {
	vec2 outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	outPosition = vec3(outUV * 2.0f + -1.0f, 0.0f);
	gl_Layer = TargetLayer;
	gl_Position = vec4(outPosition, 1.0f);
	if (TargetLayer == 2 || TargetLayer == 3)
		outPosition *= -1;
	outPosition = (projModelViewMatrix * vec4(outPosition, 1.f)).xyz;
}