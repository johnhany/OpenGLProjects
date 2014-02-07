#version 130

uniform sampler2D textures[2];

uniform vec4 materAmbient, materSpecular;
uniform vec4 lightDiffuse, lightAmbient, lightSpecular;
uniform vec4 envirAmbient;

varying vec2 texture_coord;

varying vec3 normalVect;
varying vec3 lightVect;
varying vec3 eyeVect;
varying vec3 halfWayVect;
varying vec3 reflectVect;

void main()
{
	vec4 diffuse, ambient, globalAmt;
	vec4 specular;
	vec3 eyeDir, lightDir, normalDir, halfWayDir, reflectDir;
	float NdotL, NdotH, NdotR, S, temp, delta;
	float alpha = 0.4;
	
	eyeDir = normalize(eyeVect);
	lightDir = normalize(lightVect);
	normalDir = normalize(normalVect);
	halfWayDir = normalize(halfWayVect);
	reflectDir = normalize(reflectVect);
	
	NdotL = max(dot(normalDir, lightDir), 0.0);
	NdotH = max(dot(normalDir, halfWayDir), 0.0);
	NdotR = max(dot(normalDir, reflectDir), 0.0);
	
	delta = acos(NdotH);
	temp = -1.0 * tan(delta) * tan(delta) / alpha / alpha;
	S = pow(2.71828, temp) / 4.0 / 3.14159 / alpha / alpha / pow(NdotL*NdotR, 0.5);
	
	diffuse = texture2D(textures[0], texture_coord) * lightDiffuse;
	globalAmt = envirAmbient * materAmbient;
	ambient = envirAmbient * lightAmbient;
	specular = materSpecular * lightSpecular;
	
	gl_FragColor = NdotL * (diffuse + specular * S) + globalAmt;
}