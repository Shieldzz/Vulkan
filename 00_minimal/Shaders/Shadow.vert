#version 450
#extension GL_ARB_seperate_shader_objects : enable

layout(set = 0, binding = 0) uniform UBO 
{
	mat4 view;
	mat4 proj;
	mat4 lightSpace;
	vec3 lightPos;
} ubo;

layout(set = 0, binding = 2) uniform Model
{
	mat4 world;
} model;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec3 a_normal;


layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec3 v_eyePosition;
layout(location = 3) out vec2 v_uv;
layout(location = 4) out vec3 v_light;
layout(location = 5) out vec4 v_shadowCoord;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );


void main()
{
	vec4 positionWS = model.world * vec4(a_position, 1.0);
	// todo: UBO
	mat3 normalMatrix = transpose(inverse(mat3(model.world))); 
	vec3 normalWS = normalMatrix * a_normal;
	v_position = vec3(positionWS);
	v_normal = normalWS;
	v_eyePosition = -vec3(ubo.view[3]); 
	v_uv = a_uv;
	v_light = normalize(ubo.lightPos - a_position);
	v_shadowCoord = (biasMat * ubo.lightSpace * model.world) * positionWS;

	gl_Position = ubo.proj * ubo.view * positionWS;

}