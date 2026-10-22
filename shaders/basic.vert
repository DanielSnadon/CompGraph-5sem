#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outWorldPosition;

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
    vec4 specularColor;
} object;

layout(location = 0) out vec3 outColor;

void main()
{
    vec4 worldPosition = object.model * vec4(inPosition, 1.0);
    gl_Position = scene.viewProjection * worldPosition;
    outWorldPosition = worldPosition.xyz;

    outNormal = normalize(mat3(transpose(inverse(object.model))) * inNormal);

    outColor = object.color.rgb;
    // outColor = inColor * ubo.color.rgb;
}