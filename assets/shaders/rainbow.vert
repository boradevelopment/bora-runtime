#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out float outTime;

layout(std140, binding = 0) uniform GlobalTime {
    float time;
};

void main() {
    fragUV = inUV;
    float wobble = sin(time + inPosition.x * 10.0) * 0.05;
    vec4 pos = vec4(inPosition.x, inPosition.y + wobble, inPosition.z, 1.0);
    outTime = time;
    gl_Position = pos;
}


