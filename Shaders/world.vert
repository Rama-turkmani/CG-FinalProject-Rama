#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vUV;
out vec3 vNormal;
out vec3 vWorldPos;
out float vViewDist;

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    vec4 viewP = uView * world;
    gl_Position = uProjection * viewP;

    vUV       = aUV;
    vNormal   = mat3(transpose(inverse(uModel))) * aNormal;
    vWorldPos = world.xyz;
    vViewDist = length(viewP.xyz);
}
