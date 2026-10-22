#version 450 core

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPosition;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform SceneUniformBufferObject
{
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 ambientColorIntensity;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    ivec4 lightCounts;
} scene;

layout(set = 1, binding = 0) uniform ModelUniformBufferObject
{
    mat4 model;
    vec4 color;
    vec4 materialParameters;
    // x - яркость блика
    vec4 specularColor;
} object;

struct PointLight
{
    vec4 position;
    vec4 colorIntensity;
};

layout(set = 0, binding = 1) readonly buffer PointLightBuffer
{
    PointLight pointLights[];
} pointLightData;

struct SpotLight
{
    vec4 position;
    vec4 direction;
    vec4 colorIntensity;
    vec4 coneAnglesDegrees;
};

layout(set = 0, binding = 2) readonly buffer SpotLightBuffer
{
    SpotLight spotLights[];
} spotLightData;

void main() {

    // Цилиндр

    vec3 normal = normalize(inNormal);
    vec3 lightDirection = normalize(scene.lightDirection.xyz);

    float diffuse = max(dot(normal, lightDirection), 0.0);

    vec3 viewDirection = normalize(scene.cameraPosition.xyz - inWorldPosition);
    vec3 halfVector = normalize(lightDirection + viewDirection);
    float specular = pow(max(dot(normal, halfVector), 0.0), object.materialParameters.x);

    vec3 light = scene.lightColorIntensity.rgb * scene.lightColorIntensity.w;
    vec3 finalColor = inColor * scene.ambientColorIntensity.rgb * scene.ambientColorIntensity.w + light * (inColor * (0.85 * diffuse) + object.specularColor.rgb * specular);

    // Окружение

    int lightCount = clamp(scene.lightCounts.x, 0, pointLightData.pointLights.length());

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
            pointSpecular = pow(max(dot(normal, pointHalfVector), 0.0), object.materialParameters.x);
        }

        vec3 pointLight = pointLightData.pointLights[i].colorIntensity.rgb * pointLightData.pointLights[i].colorIntensity.w / distanceSq;

        finalColor += pointLight * (inColor * (0.85 * pointDiffuse) + object.specularColor.rgb * pointSpecular);
    }

    int spotCount = clamp(scene.lightCounts.y, 0, spotLightData.spotLights.length());

    for (int i = 0; i < spotCount; ++i)
    {
        SpotLight spot = spotLightData.spotLights[i];

        vec3 toLight = spot.position.xyz - inWorldPosition;
        float distanceSquared = dot(toLight, toLight);

        vec3 lightDirection = normalize(toLight);
        vec3 spotDirection = normalize(spot.direction.xyz);
    
        float cosAngle = dot(-lightDirection, spotDirection);
        float innerCos = cos(radians(spot.coneAnglesDegrees.x));
        float outerCos = cos(radians(spot.coneAnglesDegrees.y));

        float coneFactor = smoothstep(outerCos, innerCos, cosAngle);
        if (coneFactor <= 0.0)
        {
            continue;
        }

            float spotDiffuse = max(dot(normal, lightDirection), 0.0);

        float spotSpecular = 0.0;
        if (spotDiffuse > 0.0)
        {
            vec3 spotHalfVector = normalize(lightDirection + viewDirection);
            spotSpecular = pow(max(dot(normal, spotHalfVector), 0.0), object.materialParameters.x);
        }

        vec3 spotLight = spot.colorIntensity.rgb * spot.colorIntensity.w * coneFactor / distanceSquared;

        finalColor += spotLight * (inColor * (0.85 * spotDiffuse) + object.specularColor.rgb * spotSpecular);
    }

    outColor = vec4(finalColor, 1.0);
}