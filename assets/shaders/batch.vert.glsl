#version 460 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUv;
layout (location = 2) in vec4 aColor;
layout (location = 3) in float aTextureIndex;

out vec2 uv;
out vec4 color;
out float textureIndex;

uniform mat4 projection;
uniform mat4 view;

void main() {
    uv = aUv;
    color = aColor;
    textureIndex = aTextureIndex;
    gl_Position = vec4(aPos, 0.0, 1.0) * view * projection;
}
