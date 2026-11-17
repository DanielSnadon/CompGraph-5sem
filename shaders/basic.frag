#version 450 core

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPosition;
layout(location = 3) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 1) uniform sampler2D albedoTexture;
layout(set = 1, binding = 2) uniform sampler2D specularTexture;
layout(set = 1, binding = 3) uniform sampler2D emissiveTexture;

layout(set = 0, binding = 0) uniform SceneUniformBufferObject
{
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 ambientColorIntensity;
    vec4 lightDirection;
    vec4 lightColorIntensity;
    ivec4 lightCounts;
    mat4 sunViewProjection;
    mat4 spotViewProjection;
} scene;

layout(set = 1, binding = 0) uniform ModelUniformBufferObject
{
    mat4 model;
    vec4 color;
    vec4 materialParameters;
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

layout(push_constant) uniform TextureEffect
{
    float time;
    float strength;
    float parameter;
    float speed;
    float uvScale;
    uint enabled;
} effect;

layout(set = 0, binding = 3) uniform sampler2DShadow sunShadowMap;
layout(set = 0, binding = 4) uniform sampler2DShadow spotShadowMap;

float shadowVisibility(sampler2DShadow shadowMap, mat4 lightViewProjection)
{
    vec4 lightClip = lightViewProjection * vec4(inWorldPosition, 1.0);
    if (lightClip.w <= 0.0)
    {
        return 1.0;
    }
    vec3 lightNdc = lightClip.xyz / lightClip.w;
    if (any(greaterThan(abs(lightNdc.xy), vec2(1.0))) || lightNdc.z < 0.0 || lightNdc.z >= 1.0)
    {
        return 1.0;
    }
    vec2 uv = lightNdc.xy * 0.5 + 0.5;
    return textureLod(shadowMap, vec3(uv, lightNdc.z), 0.0);
}

void main()
{
    if (object.materialParameters.y > 0.5) // Off
    {
        outColor = vec4(inColor, 1.0);
        return;
    }

    vec3 textureColor = texture(albedoTexture, inUV).rgb;

    if (effect.enabled != 0u)
    {
        vec2 uv = inUV * effect.uvScale;

        float wave = 0.04 * sin(uv.y * effect.parameter + effect.time * effect.speed);

        vec3 firstColor = texture(albedoTexture, uv + vec2(wave, 0.0)).rgb;
        vec3 secondColor = texture(albedoTexture, uv - vec2(wave, 0.0)).rgb;

        vec3 effColor = (firstColor + secondColor) * 0.7;

        textureColor = mix(textureColor, effColor, effect.strength);
    }

    vec3 albedo = textureColor * inColor;
    vec3 materialSpecular = texture(specularTexture, inUV).rgb * object.specularColor.rgb;
    vec3 emission = texture(emissiveTexture, inUV).rgb;

    if (object.materialParameters.z > 0.5)
    {
        vec2 tiles = floor(inUV * 12.0);
        albedo = inColor * mix(0.72, 1.0, mod(tiles.x + tiles.y, 2.0));
        materialSpecular = vec3(0.0);
        emission = vec3(0.0);
    }

    // Цилиндр

    vec3 normal = normalize(inNormal);
    vec3 lightDirection = normalize(scene.lightDirection.xyz);

    float diffuse = max(dot(normal, lightDirection), 0.0);

    vec3 viewDirection = normalize(scene.cameraPosition.xyz - inWorldPosition);
    float specular = 0.0;

    if (diffuse > 0.0)
    {
        vec3 halfVector = normalize(lightDirection + viewDirection);

        specular = pow(max(dot(normal, halfVector), 0.0), object.materialParameters.x);
    }

    float sunVisibility = shadowVisibility(sunShadowMap, scene.sunViewProjection);
    vec3 light = scene.lightColorIntensity.rgb * scene.lightColorIntensity.w * sunVisibility;
    vec3 finalColor = albedo * scene.ambientColorIntensity.rgb * scene.ambientColorIntensity.w + light * (albedo * (0.85 * diffuse) + materialSpecular * specular);

    // Окружение

    int lightCount = clamp(scene.lightCounts.x, 0, pointLightData.pointLights.length());

    for (int i = 0; i < lightCount; ++i)
    {
        vec3 toLight = pointLightData.pointLights[i].position.xyz - inWorldPosition;

        float distanceSq = max(dot(toLight, toLight), 1e-6);

        vec3 pointDirection = toLight * inversesqrt(max(dot(toLight, toLight), 1e-8));

        float pointDiffuse = max(dot(normal, pointDirection), 0.0);
        float pointSpecular = 0.0;

        if (pointDiffuse > 0.0)
        {
            vec3 pointHalfVector = normalize(pointDirection + viewDirection);
            pointSpecular = pow(max(dot(normal, pointHalfVector), 0.0), object.materialParameters.x);
        }

        vec3 pointLight = pointLightData.pointLights[i].colorIntensity.rgb * pointLightData.pointLights[i].colorIntensity.w / distanceSq;

        finalColor += pointLight * (albedo * (0.85 * pointDiffuse) + materialSpecular * pointSpecular);
    }

    int spotCount = clamp(scene.lightCounts.y, 0, spotLightData.spotLights.length());

    for (int i = 0; i < spotCount; ++i)
    {
        SpotLight spot = spotLightData.spotLights[i];

        vec3 toLight = spot.position.xyz - inWorldPosition;
        float distanceSquared = max(dot(toLight, toLight), 1e-6);

        vec3 lightDirection = toLight * inversesqrt(max(dot(toLight, toLight), 1e-8));
        vec3 spotDirection = dot(spot.direction.xyz, spot.direction.xyz) > 1e-8
            ? normalize(spot.direction.xyz) : vec3(0.0, -1.0, 0.0);
    
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

        if (i == 0 && scene.lightCounts.z != 0)
        {
            spotLight *= shadowVisibility(spotShadowMap, scene.spotViewProjection);
        }
        finalColor += spotLight * (albedo * (0.85 * spotDiffuse) + materialSpecular * spotSpecular);
    }

    finalColor += emission;

    outColor = vec4(finalColor, 1.0);
}