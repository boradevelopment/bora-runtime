#version 450
layout(location = 0) in vec3 inPos; 
layout(location = 1) in vec2 inUV;   

layout(location = 0) out vec2 fragUV;   // pass UV or dummy if needed
layout(location = 1) out vec4 fragColor; // pass uniform color to pixel shader

layout(set = 0, binding = 0) uniform FadeUBO {
    vec4 color;
};

void main() {
    fragUV = inUV;          // optional if your pixel shader uses it
    fragColor = color;       // pass color along
  vec3 pos = inPos;
//pos.y = -pos.y; // Flip Y
gl_Position = vec4(pos, 1.0);
}
