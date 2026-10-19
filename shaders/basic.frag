#version 450 core

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;
layout(location = 1) in vec3 inNormal;

void main() {
    vec3 normal = normalize(inNormal);
    vec3 lightDirection = normalize(vec3(-0.6, 0.5, 0.8));

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float lighting = 0.01 + 0.85 * diffuse;

    outColor = vec4(inColor * lighting, 1.0);
}