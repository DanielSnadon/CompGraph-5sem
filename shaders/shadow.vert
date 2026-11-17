#version 450

layout(location = 0) in vec3 inPosition;

layout(std140, set = 1, binding = 0) uniform ModelUniformBufferObject
{
    mat4 model;
} object;

layout(push_constant) uniform ShadowTransform
{
    layout(offset = 32) mat4 lightViewProjection;
} shadow;

void main()
{
    gl_Position = shadow.lightViewProjection * object.model * vec4(inPosition, 1.0);
}
