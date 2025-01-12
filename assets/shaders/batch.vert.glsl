#version 460 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUv;
layout (location = 2) in vec4 aColor;

out vec2 uv;
out vec4 color;

void main() {
    uv = aUv;
    color = aColor;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
