#version 450 core

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inWorldPosition;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(inNormal);
    vec3 lightDirection = normalize(vec3(-0.5, 1.0, 0.3));

    float diffuse = max(dot(normal, lightDirection), 0.0);

    vec3 cameraPosition = vec3(2.0, 1.8, 2.5);
    vec3 viewDirection = normalize(cameraPosition - inWorldPosition);
    vec3 halfVector = normalize(lightDirection + viewDirection);
    float specular = pow(max(dot(normal, halfVector), 0.0), 32.0);

    vec3 finalColor = inColor * 0.15 + inColor * (0.85 * diffuse) + vec3(0.8) * specular;

    outColor = vec4(finalColor, 1.0);
}