#version 330 core
out vec4 FragColor;
//   ----------SconceLight----------------
uniform bool sconceLightOn;
uniform vec3 sconceLightPos;
uniform vec3 sconceLightColor;
uniform float sconceConstant;
uniform float sconceLinear;
uniform float sconceQuadratic;

// -----------the lamp would emit light------------- 
uniform bool isLamp;
uniform vec3 lampEmissionColor;
uniform float lampEmissionStrength;

// --------------fog-----------
uniform vec3 fogColor;
uniform float fogDensity;

//-------------Weather code------------------
uniform vec3 globalLightColor;
uniform float ambientStrength;
uniform float exposure;
uniform bool isNight;


struct Material {
    sampler2D texture_diffuse1; //only diffuse map, the ambient usually has the same color as the diffuse 
    sampler2D texture_specular1;    
    float shininess;
}; 

struct Light {
    vec3 position;
    vec3 diffuse; //the color of the light
};

in vec3 FragPos;  
in vec3 Normal;  
in vec2 TexCoords;
  
uniform vec3 viewPos;
uniform Material material;
uniform Light light;

void main()
{
    // ambient
    vec3 ambient = ambientStrength  * texture(material.texture_diffuse1, TexCoords).rgb;
  	  
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0); //cos to light direction
    vec3 diffuse = light.diffuse * diff * texture(material.texture_diffuse1, TexCoords).rgb;
	//vec3 diffuse = texture(material.texture_diffuse1, TexCoords).rgb;

    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  

	//cos to the power of shininess of angle between light reflected ray and view direction
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess); 
    vec3 specular = spec * texture(material.texture_specular1, TexCoords).rgb;  
        
    vec3 result = ambient + diffuse + specular;
	//vec3 result = diffuse;

        // --- sconce (point) light contribution (toggleable) ---
    if (sconceLightOn)
    {
        vec3 lightDir2 = normalize(sconceLightPos - FragPos);
        float diff2 = max(dot(norm, lightDir2), 0.0);
        vec3 diffuse2 = sconceLightColor * diff2 * texture(material.texture_diffuse1, TexCoords).rgb;
        
        vec3 reflectDir2 = reflect(-lightDir2, norm);
        float spec2 = pow(max(dot(viewDir, reflectDir2), 0.0), material.shininess);
        vec3 specular2 = spec2 * texture(material.texture_specular1, TexCoords).rgb;

        float distance = length(sconceLightPos - FragPos);
        float attenuation = 1.0 / (sconceConstant + sconceLinear * distance + sconceQuadratic * distance * distance);

        result += (diffuse2 + specular2) * attenuation;
    }

    // --- lamp model itself looks lit (emissive) ---
    if (isLamp)
    {
        float emissionOn = sconceLightOn ? 1.0 : 0.2; // dim when off
        vec3 emission = lampEmissionColor * lampEmissionStrength * emissionOn;
        result += emission;
    }
    //-- for weather changes
    result *= exposure; 

    //for fog
    float dist = length(viewPos - FragPos);
    float fogFactor = 1.0 - exp(-fogDensity * dist * dist);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    vec3 finalColor = mix(result, fogColor, fogFactor);
    FragColor = vec4(finalColor, 1.0);



    //FragColor = vec4(result, 1.0);
} 


