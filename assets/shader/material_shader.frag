#version 450
#extension GL_EXT_nonuniform_qualifier : enable
//---------------------------------------
struct LIGHT{
	vec3 lightPos;
	int lightType;
	vec3 lightDir;
	float intensity;
	vec3 lightColor;
	uint iDataOffset;
	float near;
	float far;
	uint LIGHT0_Padding[2];
};
//---------------------------------------
#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT 1
#define LIGHT_SPOT 2
//---------------------------------------
struct DATA_LIGHT{
	uint renderIndex;
	uint renderSubIndex;
	uint renderSubSubIndex;
	uint DATA_LIGHT0_Padding;
	mat4 projModelViewMatrix;
};
//---------------------------------------
layout(set = 0, binding = 0) uniform sampler2D sTextures[];
layout(set = 1, binding = 0) uniform sampler2D sNormals[];
layout(set = 2, binding = 0) uniform sampler2D sEmissives[];
//layout(set = 3, binding = 0) uniform sampler2D sMetalnessRoughnesses[];
//---------------------------------------
#if defined(SHADOWS)
layout(set = 3, binding = 0) uniform sampler2DShadow sShadowMap;
//---------------------------------------
layout(set = 4, binding = 0) buffer bLights {
	uint lightsCount;
	uint shadowMapTileSize;
	uint bLights0_Padding[2];
	LIGHT lights[];
};
//---------------------------------------
layout(set = 5, binding = 0) buffer bDataLights {
	uint dataLightsCount;
	uint bDataLights0_Padding[3];
	DATA_LIGHT dataLights[];
};
#endif
//---------------------------------------
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inFragPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in flat int inMaterialIndex;
//---------------------------------------
layout(location = 0) out vec4 outColor;
//---------------------------------------
#define SHADOWS_TEX_DIMENSION_X 10
#define SHADOWS_TEX_DIMENSION_Y 10
#define SHADOWS_VIEW_WIDTH 1024
#define SHADOWS_VIEW_HEIGHT 1024
//---------------------------------------
#if defined(SHADOWS)
vec2 GetDepth(uint iLight, uint iDataLight, vec2 projCoords) {
	LIGHT light = lights[iLight];
	DATA_LIGHT dataLight = dataLights[iDataLight];
	//--
	float x = float(dataLight.renderIndex % SHADOWS_TEX_DIMENSION_X) * float(SHADOWS_VIEW_WIDTH);
	float y = float(dataLight.renderIndex / SHADOWS_TEX_DIMENSION_X) * float(SHADOWS_VIEW_HEIGHT);
	//--
	float width = float(SHADOWS_VIEW_WIDTH);
	float height = float(SHADOWS_VIEW_HEIGHT);
	//--
	if (dataLight.renderSubIndex != -1) {
		x += float(dataLight.renderSubIndex % 2) * float(SHADOWS_VIEW_WIDTH / 2);
		y += float(dataLight.renderSubIndex / 2) * float(SHADOWS_VIEW_HEIGHT / 2);
		//--
		width /= 2.f;
		height /= 2.f;
	}
	//--
	if (dataLight.renderSubSubIndex != -1) {
		x += float(dataLight.renderSubSubIndex % 2) * float(SHADOWS_VIEW_WIDTH / 4);
		y += float(dataLight.renderSubSubIndex / 2) * float(SHADOWS_VIEW_HEIGHT / 4);
		//--
		width /= 2.f;
		height /= 2.f;
	}
	//--
	vec2 texCoords;
	texCoords.x = (projCoords.x * width + x) / (SHADOWS_TEX_DIMENSION_X * SHADOWS_VIEW_WIDTH);
	texCoords.y = (projCoords.y * height + y) / (SHADOWS_TEX_DIMENSION_Y * SHADOWS_VIEW_HEIGHT);
	//--
	return texCoords;
}
//---------------------------------------
float ShadowCalculation(uint iLight, uint iDataLight, vec3 lightDir) {
	LIGHT light = lights[iLight];
	DATA_LIGHT dataLight = dataLights[iDataLight];
	//--
	vec4 fragPosLightSpace = dataLight.projModelViewMatrix * vec4(inFragPos, 1.0);
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	//--
	projCoords.xy = projCoords.xy * 0.5 + 0.5;
	vec2 texCoords = GetDepth(iLight, iDataLight, projCoords.xy);
	//--
	vec3 normal = texture(sNormals[inMaterialIndex], inUV).xyz * inNormal;
	//--
	float currentDepth = projCoords.z;
	//--
	float shadow = 0.;
	vec2 texelSize = 1.0 / textureSize(sShadowMap, 0);
	float bias = 0.;
	//--
	for(int x = -1; x <= 1; ++x) {
		for(int y = -1; y <= 1; ++y) {
			shadow += texture(sShadowMap, vec3(texCoords + vec2(x, y) * texelSize, currentDepth)).r; 
		}
	}
	shadow /= 9.0;
	//--
	return shadow;
}
//---------------------------------------
//vec3 ComputeSpecular() {
//	vec3 viewDir = normalize(viewPos - fs_in.FragPos);
//	vec3 reflectDir = reflect(-lightDir, normal);
//	float spec = 0.0;
//	vec3 halfwayDir = normalize(lightDir + viewDir);  
//	spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
//	vec3 specular = spec * lightColor;   
//}
//---------------------------------------
struct FRAGMENT_DATA {
	vec3 diffuse;
//	vec3 specular;
	float shadow;
};
//---------------------------------------
const vec3 pointLightDirections[] = {
	{ 1., 0., 0. },
	{ -1., 0., 0. },
	{ 0., 0., 1. },
	{ 0., 0., -1. },
	{ 0., 1., 0. },
	{ 0., -1., 0. },
};
//---------------------------------------
const int componentLightDirection[] = {
	0, 0, 2, 2, 1, 1
};
//---------------------------------------
FRAGMENT_DATA ProcessLights(){
	FRAGMENT_DATA fragData;
	//--
	if (lightsCount == 0){
		fragData.diffuse = vec3(1.);
		//fragData.specular = vec3(0.);
		fragData.shadow = 0.5;
		return fragData;
	}
	//--
	vec3 normal = texture(sNormals[inMaterialIndex], inUV).xyz * inNormal;
	//--
	{
		fragData.diffuse = vec3(0.);
		//fragData.specular = vec3(0.);
		fragData.shadow = 1.0;
	}
	//--
	for (uint iLight = 0; iLight < lightsCount; ++iLight){
		LIGHT light = lights[iLight];
		//--
		if (light.intensity < 0.01){
			continue;
		}
		//--
		uint iDataLight = light.iDataOffset;
		//--
		switch (light.lightType){
			case LIGHT_DIRECTIONAL:{
				fragData.shadow *= ShadowCalculation(iLight, iDataLight, light.lightDir);
				fragData.diffuse += max(dot(-light.lightDir, normal), 0.0) * light.lightColor;
				//--
				++iDataLight;
				break;
			}
			case LIGHT_POINT:{
				float lightDist = length(light.lightPos - inFragPos);
				if (lightDist < 15.){
					vec3 lightDir = (light.lightPos - inFragPos) / lightDist;
					float attenuation = max(min(1.0 - pow((lightDist / 15.), 4.), 1.), 0.) / lightDist;
					//--
					//vec3 reflectDir = reflect(-lightDir, normal);
					//float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
					//vec3 specular = light.specular * spec * vec3(texture(material.specular, TexCoords));
					float shadow;
					//--
					{
						int iDirection = 0;
						//--
						for (int iOtherDirection = 1; iOtherDirection < 6; ++iOtherDirection){
							int iComponent = componentLightDirection[iDirection];
							int iOtherComponent = componentLightDirection[iOtherDirection];
							if (pointLightDirections[iOtherDirection][iOtherComponent] * -lightDir[iOtherComponent] < 0.)
								continue;
							if (abs(-lightDir[iOtherComponent]) >= abs(-lightDir[iComponent]))
								iDirection = iOtherDirection;
						}
						//--
						shadow = ShadowCalculation(iLight, iDataLight + iDirection, lightDir);
					}
					//--
					fragData.diffuse += (1. - shadow) * max(dot(lightDir, normal), 0.) * light.lightColor * vec3(attenuation);
					fragData.shadow *= (1. - (1. - shadow) * attenuation);
				}
				break;
			}
			case LIGHT_SPOT:{
				float shadow = ShadowCalculation(iLight, iDataLight, light.lightDir);  
				fragData.shadow *= shadow;
				++iDataLight;
				break;
			}
		}
	}
	return fragData;
}
#endif
//---------------------------------------
void main() {
	vec3 color = texture(sTextures[inMaterialIndex], inUV).rgb;
	vec3 emissive = texture(sEmissives[inMaterialIndex], inUV).rgb;
	vec3 ambient = vec3(0.1);
	//--
#if defined(SHADOWS)
	FRAGMENT_DATA fragData = ProcessLights();
	outColor = vec4((ambient + emissive + (1.0 - fragData.shadow)) * (fragData.diffuse) * color, 1); 
#else
	outColor = vec4(color, 1);
#endif
}
//---------------------------------------
