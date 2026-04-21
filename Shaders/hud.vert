#version 330 core
layout (location = 0) in vec2 aPos; // unit quad [0..1] x [0..1]

uniform mat4 uOrtho;
uniform vec2 uOffset;   // pixel position of quad bottom-left
uniform vec2 uSize;     // pixel size of quad

void main()
{
    vec2 p = aPos * uSize + uOffset;
    gl_Position = uOrtho * vec4(p, 0.0, 1.0);
}
