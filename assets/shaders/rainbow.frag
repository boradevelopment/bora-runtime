#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float outTime;
layout(location = 0) out vec4 outColor;

// HSV to RGB
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    float hue = mod(fragUV.x + outTime * 0.1, 1.0);
    vec3 rgb = hsv2rgb(vec3(hue, 1.0, 1.0));
    outColor = vec4(rgb, 1.0);
}
