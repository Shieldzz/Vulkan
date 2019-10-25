#version 450
#extension GL_ARB_seperate_shader_objects : enable

#define SHADOWMAP_CASCADE 4

layout(set = 0, binding = 0) uniform UBO 
{
	mat4 view;
	mat4 proj;
	mat4 lightSpace[SHADOWMAP_CASCADE];
	vec4 cascadeSplits;
	vec3 lightPos;
} ubo;

layout(set = 0, binding = 2) uniform Model
{
	mat4 world;
	vec4 cascadeIdx;
} model;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec3 a_normal;

void main()
{
	vec4 positionWS = model.world * vec4(a_position, 1.0);
	int idx = int(model.cascadeIdx.x);
	gl_Position = ubo.lightSpace[idx] * positionWS;
}