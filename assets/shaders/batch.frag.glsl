#version 460 core

layout (location = 0) out vec4 FragColor;

in vec2 uv;
in vec4 color;
in float textureIndex;

uniform sampler2D textures[32];

// Stolen from: https://jorenjoestar.github.io/post/pixel_art_filtering/
// Shader from: Inigo Quilez (<3)
vec2 uv_iq(vec2 uv, ivec2 texture_size) {
    vec2 pixel = uv * texture_size;

    vec2 seam = floor(pixel + 0.5);
    vec2 dudv = fwidth(pixel);
    pixel = seam + clamp( (pixel - seam) / dudv, -0.5, 0.5);

    return pixel / texture_size;
}

void main() {
    int iTextureIndex = int(textureIndex);
    FragColor = texture(textures[iTextureIndex], uv_iq(uv, textureSize(textures[iTextureIndex], 0))) * color;
}
