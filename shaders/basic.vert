#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outWorldPosition;

layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 color;
} ubo;

layout(location = 0) out vec3 outColor;

void main()
{
    vec4 worldPosition = ubo.model * vec4(inPosition, 1.0);
    gl_Position = ubo.projection * ubo.view * worldPosition;
    outWorldPosition = worldPosition.xyz;
    // gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPosition, 1.0);

    outNormal = normalize(mat3(transpose(inverse(ubo.model))) * inNormal);

    outColor = ubo.color.rgb;
    // outColor = inColor * ubo.color.rgb;
}