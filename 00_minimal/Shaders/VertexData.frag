#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec3 v_eyePosition;
layout(location = 3) in vec2 v_uv;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

// SHADOW

layout(set = 0, binding = 3) uniform sampler2D shadowMap;

layout(location = 4) in vec3 v_light;
layout(location = 5) in vec4 v_shadowCoord;

#define ambient 0.1

//

// MATERIAU
//const vec3 albedoRGB = vec3(0.6011, 0.10383, 0.7924);	// albedo = Cdiff, ici magenta
//const vec3 Cdiff = vec3(255, 255, 0) * 1.0 / 255.0; 
//vec3 albedoRGB = pow(Cdiff, vec3(2.2)); // gamma->linear

const float perceptual_roughness = 0.5;
const float roughness = perceptual_roughness * perceptual_roughness;
const float metallic = 0.0;					// surface metallique ou pas ?
const float reflectance = 1.0;

vec3 GetF0(vec3 albedo, float metallic)
{
	return mix(vec3(0.16 * (reflectance * reflectance)), albedo, metallic);
}

float GetShininess()
{
	const float maxRoughness = max(roughness, 0.001);
	return ( 2.0 / (maxRoughness*maxRoughness) ) - 2.0;
}

vec3 FresnelSchlick(vec3 f0, float cosTheta)
{
	return f0 + (vec3(1.0) - f0) * pow(1.0 - cosTheta, 5.0);
}

vec3 CalculeDiffuse(vec3 albedo)
{
	return mix(albedo, vec3(0.0),metallic);
}

float Distribution_GGX(float NdotH)
{
	float alpha = roughness * roughness;
	float F = (NdotH * alpha - NdotH) * NdotH + 1.0;
	return alpha / (F * F);
}

float Visibility_SmithGGXCorrelated(float NdotL, float NdotV)
{
	float alpha = roughness * roughness;
	float Lambda_GGIV = NdotL * sqrt((-NdotV * alpha + NdotV) * NdotV + alpha);
	float Lambda_GGXL = NdotV * sqrt((-NdotL * alpha + NdotL) * NdotL + alpha);

	return 0.5 / (Lambda_GGIV + Lambda_GGXL);
}

vec3 CookTorranceGGX(vec3 f0, float NdotL, float NdotV, float NdotH, float VdotH)
{
	vec3 fresnel = FresnelSchlick(f0, VdotH); 
	float Vis = Visibility_SmithGGXCorrelated(NdotL, NdotV);
	float D = Distribution_GGX(NdotH);
	
	return D * fresnel * Vis;
}

// remapping de Karis, K = @ / 2, @ = roughness^2
// de plus, en se basant sur Burley (Disney) il remappe 
// roughness = (roughness + 1.0) / 2.0
// du coup, comme @ = roughness^2 -> (roughness + 1)^2 / 2^2
// et k = @ /2
// identite (a/b) / (c/d) -> a*d / b*c
// ce qui donne (roughness + 1)^2 / 8
// mais attention, ca ne marche en IBL, trop sombre

// SHADOW MAP 

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowMap, shadowCoord.st + off).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = ambient;
		}
	}
	return shadow;
}

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadowMap, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
}


void main() 
{
	// LUMIERE : vecteur VERS la lumiere en repere main droite OpenGL (+Z vers nous)
	//const vec3 L = normalize(vec3(0.0, 1.0, 0.0));
	const vec3 L = v_light;

	vec3 albedo = texture(texSampler, v_uv).rgb;

	float shadow = filterPCF(v_shadowCoord / v_shadowCoord.w);

	const vec3 f0 = GetF0(albedo,metallic);
	
	const float shininess = GetShininess();				// brillance speculaire (intensite & rugosite)

	// rappel : le rasterizer interpole lineairement
	// il faut donc normaliser sinon la norme des vecteurs va changer de magnitude
	vec3 N = normalize(v_normal);
	vec3 V = normalize(v_eyePosition - v_position);
	vec3 H = normalize(L + V);

	// on max a 0.001 afin d'eviter les problemes a 90°
	float NdotL = max(dot(N, L), 0.001);
	float NdotH = max(dot(N, H), 0.001);
	float VdotH = max(dot(V, H), 0.001);
	float NdotV = max(dot(N, V), 0.001);

	//
	// diffuse = Lambert BRDF * cos0
	//

	vec3 diffuse = CalculeDiffuse(albedo) * NdotL * shadow;
	
	//
	// specular = Gotanda BRDF * cos0 
	//
	// Gotanda utilise VdotH plutot que NdotV
	// car Blinn-Phong est inspire du modele "micro-facette" base sur le vecteur H
	//float normalisation = (shininess + 2.0) / ( 4.0 * ( 2.0 - exp2(-shininess/2.0) ) );
	//float BlinnPhong = pow(NdotH, shininess); // D = NDF ou blinn-phong
	//float G = 1.0 / max(NdotL, NdotV);			// NEUMANN
	//vec3 specular = vec3(normalisation * BlinnPhong * G) * NdotL;

	//
	// GGX
	//
	vec3 fresnel = FresnelSchlick(f0, VdotH); 
	float Vis = Visibility_SmithGGXCorrelated(NdotL, NdotV);
	float D = Distribution_GGX(NdotH);
	vec3 specular = CookTorranceGGX(f0, NdotL, NdotV, NdotH, VdotH) * NdotL;

	// 
	// couleur finale
	//
	vec3 Ks = fresnel;
	// Kd implicite
	// vec3 Kd = vec3(1.0) - Ks;
	// Gotanda utilise la formulation suivante pour Kd :
	vec3 Kd = vec3(1.0) - FresnelSchlick(f0, NdotL);

	vec3 finalColor = Kd * diffuse + specular ;
    outColor = vec4(finalColor, 1.0);

	//float depth = texture(shadowMap, v_uv).r;
	// = vec4(1.0 - (1.0 - depth) * 100.0);
	
}