#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} fs_in;

// directional light struct matches how you set uniforms in C++
struct DirLight {
    vec3 direction;
    vec3 color;
};
uniform DirLight dirLight;

uniform vec3 viewPos;

// shadow map
uniform sampler2D shadowMap; // bound to texture unit 1 in your C++ code
// optional diffuse texture (if your Model supplies one)
uniform sampler2D texture_diffuse1;
uniform bool useDiffuseTexture = false;

// sconce point light
uniform bool sconceLightOn;
uniform vec3 sconceLightPos;
uniform vec3 sconceLightColor;
uniform float sconceConstant;
uniform float sconceLinear;
uniform float sconceQuadratic;

// global / weather uniforms
uniform vec3 globalLightColor;
uniform float ambientStrength;
uniform float exposure;
uniform float fogDensity;
uniform vec3 fogColor;

// lamp emission
uniform vec3 lampEmissionColor;
uniform float lampEmissionStrength;
uniform bool isLamp = false;

// small PCF kernel size for soft shadows
const float PCF_RADIUS = 1.0 / 1024.0; // tweak relative to shadow map size

// helper: compute shadow factor [0..1] where 1 means fully in shadow
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    // perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1]
    projCoords = projCoords * 0.5 + 0.5;

    // if outside the shadow map, don't shadow
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    // closest depth from shadow map
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, -lightDir)), 0.001);
    // PCF sampling
    float shadow = 0.0;
    int samples = 3;
    float texelSize = PCF_RADIUS;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            if (currentDepth - bias > pcfDepth)
                shadow += 1.0;
        }
    }
    shadow /= 9.0;

    // clamp
    return clamp(shadow, 0.0, 1.0);
}

void main()
{
    // base albedo: sample texture if available otherwise use grey
    vec3 albedo = vec3(0.8);
    if (useDiffuseTexture) {
        albedo = texture(texture_diffuse1, fs_in.TexCoords).rgb;
    }

    vec3 normal = normalize(fs_in.Normal);
    vec3 ambient = ambientStrength * globalLightColor * albedo;

    // directional (sun) diffuse
    vec3 lightDir = normalize(-dirLight.direction); // dirLight.direction points from scene to light
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * albedo * dirLight.color;

    // compute shadow factor
    float shadow = ShadowCalculation(fs_in.FragPosLightSpace, normal, lightDir);

    // point (sconce) light (Blinn-Phong-lite)
    vec3 result = ambient + (1.0 - shadow) * diffuse;

    if (sconceLightOn) {
        vec3 toLight = sconceLightPos - fs_in.FragPos;
        float dist = length(toLight);
        vec3 L = normalize(toLight);
        float diffS = max(dot(normal, L), 0.0);
        float attenuation = 1.0 / (sconceConstant + sconceLinear * dist + sconceQuadratic * dist * dist);
        vec3 sconceContrib = sconceLightColor * diffS * albedo * attenuation;
        result += sconceContrib;
    }

    // lamp objects can emit light color (self-illumination)
    if (isLamp) {
        result += lampEmissionColor * lampEmissionStrength;
    }

    // tone mapping / exposure (simple)
    result = vec3(1.0) - exp(-result * exposure);

    // fog (exponential)
    if (fogDensity > 0.0) {
        float distance = length(viewPos - fs_in.FragPos);
        float fogFactor = 1.0 - exp(-pow(distance * fogDensity, 2.0));
        result = mix(result, fogColor, clamp(fogFactor, 0.0, 1.0));
    }

    FragColor = vec4(result, 1.0);
}
