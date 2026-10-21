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
    vec4 specularColor;
} ubo;

void main() {
    vec3 normal = normalize(inNormal);
    vec3 lightDirection = normalize(vec3(-0.5, 1.0, 0.3));

    float diffuse = max(dot(normal, lightDirection), 0.0);

    vec3 viewDirection = normalize(ubo.cameraPosition.xyz - inWorldPosition);
    vec3 halfVector = normalize(lightDirection + viewDirection);
    float specular = pow(max(dot(normal, halfVector), 0.0), ubo.materialParameters.x);

    vec3 finalColor = inColor * ubo.materialParameters.y + inColor * (0.85 * diffuse) + ubo.specularColor.rgb * specular;

    outColor = vec4(finalColor, 1.0);
}