#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec3 vFillColor;
layout(location = 2) in vec4 vStrokeColor;
layout(location = 3) in float vTime;
layout(location = 4) in vec2 vSpotDir;
layout(location = 5) in float vSpotRadius;
layout(location = 6) in float vSpotSoftness;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D uTexture; // binding 1 for the texture (adjust if needed)

const vec3 ID_FILL = vec3(0.0, 0.0, 1.0);   // Blue in SVG
const vec3 ID_STROKE = vec3(0.0, 1.0, 0.0); // Green in SVG

void main() {
vec4 samp = texture(uTexture, vUV);
if (samp.a < 0.01) discard;

// 1. Sweep/Shimmer Effect (Android Boot style)
// We project the UV onto a direction vector and move it over time
float sweepPos = fract(vUV.x - vTime * 0.5); 
float shimmer = smoothstep(0.0, 0.1, sweepPos) * smoothstep(0.2, 0.1, sweepPos);
shimmer *= 0.5; // Intensity of the sweeping line

// 2. Spotlight Logic
vec2 center = fract(vec2(0.5) + vSpotDir * vTime);
float dist = distance(vUV, center);
float spot = smoothstep(vSpotRadius, vSpotRadius * (1.0 - vSpotSoftness), dist);

// 3. Identify Layers
float isFill = smoothstep(0.5, 0.0, distance(samp.rgb, ID_FILL));
float isStroke = smoothstep(0.5, 0.0, distance(samp.rgb, ID_STROKE));

// 4. Layer Lighting Logic
// Stroke gets higher ambient (0.6) so it's always "there"
float strokeLight = 0.6 + (spot * 0.4) + shimmer;
// Fill is darker (0.2) and only pops when the spot hits it
float fillLight = 0.2 + (spot * 0.8) + (shimmer * 0.3);

// 5. Final Composition
vec3 finalRGB = vec3(0.0);

// Apply colors
vec3 fillResult = vFillColor * fillLight;
vec3 strokeResult = vStrokeColor.rgb * strokeLight;

// Blend layers based on the SVG "ID" colors
finalRGB = mix(finalRGB, fillResult, isFill);
finalRGB = mix(finalRGB, strokeResult, isStroke);

// Blend with transparency (the samp.a ensures it fits the SVG shape)
outColor = vec4(finalRGB, samp.a * mix(1.0, vStrokeColor.a, isStroke));
}