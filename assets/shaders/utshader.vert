#version 450
layout(location = 0) in vec3 inPos; 
layout(location = 1) in vec2 inUV;  

layout(location = 0) out vec2 fragUV;  
layout(location = 1) out float oInvAlpha;

layout(set = 0, binding = 0) uniform FadeUBO {
    float invAlpha;
};

void main() {
vec3 pos = inPos;
gl_Position = vec4(pos, 1.0);
fragUV = inUV;
oInvAlpha = invAlpha;
}
