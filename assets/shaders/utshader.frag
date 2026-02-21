#version 450
layout(location = 0) in vec2 fragUV; 
layout(location = 1) in float oInvAlpha;
layout(binding = 1) uniform sampler2D shaderTexture;

layout(location = 0) out vec4 outColor;

void main() {
vec4 normalColor = texture(shaderTexture, fragUV);
vec3 invertedRGB = vec3(1.0) - normalColor.rgb;

    outColor =  vec4(mix(normalColor.rgb, invertedRGB, oInvAlpha), 1.0);
}