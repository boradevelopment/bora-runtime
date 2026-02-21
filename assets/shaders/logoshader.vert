#version 450
layout(location = 0) in vec2 inPosition;  
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec3 vFillColor;   // forwarded chosen fill color
layout(location = 2) out vec4 vStrokeColor; // forwarded chosen stroke color
layout(location = 3) out float vTime;       // forwarded time
layout(location = 4) out vec2 vSpotDir;     // spotlight direction/speed
layout(location = 5) out float vSpotRadius;
layout(location = 6) out float vSpotSoftness;

// Uniform buffer (read in VS; forwarded values will be copied to varyings)
layout(std140, binding = 0) uniform UBO
{
    vec4 uFillColor;    // .xyz used (vec3) (.w unused/pad)
    vec4 uStrokeColor;  // .xyz used
    float uTime;        // time in seconds
    // padding to 16 bytes boundary for std140
    float _pad0;
    vec2  uSpotDir;     // direction or speed of spotlight, in UV space units/sec
    float uSpotRadius;  // in UV units (0..1)
    float uSpotSoftness; // softness (0..1)
} ub;

layout(location = 0) out gl_PerVertex { vec4 gl_Position; };

void main()
{
    gl_Position = vec4(inPosition.xy, 0.0, 1.0);

    vUV = inUV;

    // forward chosen colors and spotlight params to fragment
    vFillColor = ub.uFillColor.xyz;
    vStrokeColor = ub.uStrokeColor;
    vTime = ub.uTime;
    vSpotDir = ub.uSpotDir;
    vSpotRadius = ub.uSpotRadius;
    vSpotSoftness = ub.uSpotSoftness;
}
