#version 450
layout(location = 0) in vec2 fragUV; 
//layout(location = 1) in vec4 fragColor;   nto needed
layout(location = 1) in vec1 invAlpha;
layout(binding = 0) uniform sampler2D shaderTexture;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 normalColor = texture(shaderTexture, fragUV);
    vec4 invertedColor = vec4(1.0 - normalColor.rgb, normalColor.a);

    // Blend between normal and inverted based on invAlpha
    outColor = mix(normalColor, invertedColor, invAlpha);
}