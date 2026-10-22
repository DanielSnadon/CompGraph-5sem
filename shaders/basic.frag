#version 450 core

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPosition;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 color;
    vec4 cameraPosition;
    vec4 materialParameters;
    // x = яркость блика | y = эмбиент | z = количество лампочек
    vec4 specularColor;
    vec4 lightDirection;
    vec4 lightColorIntensity;
} ubo;

struct PointLight
{
    vec4 position;
    vec4 colorIntensity;
};

layout(set = 0, binding = 1) readonly buffer PointLightBuffer
{
    PointLight pointLights[];
} pointLightData;

void main() {

    // Цилиндр

    vec3 normal = normalize(inNormal);
    vec3 lightDirection = normalize(ubo.lightDirection.xyz);

    float diffuse = max(dot(normal, lightDirection), 0.0);

    vec3 viewDirection = normalize(ubo.cameraPosition.xyz - inWorldPosition);
    vec3 halfVector = normalize(lightDirection + viewDirection);
    float specular = pow(max(dot(normal, halfVector), 0.0), ubo.materialParameters.x);

    vec3 light = ubo.lightColorIntensity.rgb * ubo.lightColorIntensity.w;
    vec3 finalColor = inColor * ubo.materialParameters.y + light * (inColor * (0.85 * diffuse) + ubo.specularColor.rgb * specular);

    // Окружение

    int lightCount = clamp(int(ubo.materialParameters.z), 0, pointLightData.pointLights.length());

    for (int i = 0; i < lightCount; ++i)
    {
        vec3 toLight = pointLightData.pointLights[i].position.xyz - inWorldPosition;

        float distanceSq = dot(toLight, toLight);

        vec3 pointDirection = normalize(toLight);

        float pointDiffuse = max(dot(normal, pointDirection), 0.0);
        float pointSpecular = 0.0;

        if (pointDiffuse > 0.0)
        {
            vec3 pointHalfVector = normalize(pointDirection + viewDirection);
            pointSpecular = pow(max(dot(normal, pointHalfVector), 0.0), ubo.materialParameters.x);
        }

        vec3 pointLight = pointLightData.pointLights[i].colorIntensity.rgb * pointLightData.pointLights[i].colorIntensity.w / distanceSq;

        finalColor += pointLight * (inColor * (0.85 * pointDiffuse) + ubo.specularColor.rgb * pointSpecular);
    }

    outColor = vec4(finalColor, 1.0);
}