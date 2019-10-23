#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 3) uniform sampler2D shadowMap;

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_color;
layout(location = 3) in vec3 v_light;
layout(location = 4) in vec4 v_shadowCoord;
layout(location = 5) in vec3 v_view;

layout(location = 0) out vec4 outColor;

#define ambient 0.1

float textureProj(vec4 shadowCoord)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowMap, shadowCoord.st).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = ambient;
		}
	}
	return shadow;
}


void main()
{
	float shadow = textureProj(v_shadowCoord);

	vec3 N = normalize(v_normal);
	vec3 L = normalize(v_light);
	vec3 V = normalize(v_view);
	vec3 R = normalize(-reflect(L, N));
	vec3 diffuse = max(dot(N, L), ambient) * v_color;
//	vec3 specular = pow(max(dot(R, V), 0.0), 50.0) * vec3(0.75);

	outColor = vec4(diffuse * shadow, 1.0);
}