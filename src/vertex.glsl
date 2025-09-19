#version 330 core

layout (location = 0) in vec3 i_position;
layout (location = 1) in vec4 i_color;
layout (location = 2) in vec2 i_uv;

out vec4 v_color;
out vec2 v_uv;

uniform mat4 u_mvp;

void main() {
	gl_Position = u_mvp * vec4(i_position, 1.0);
	v_color = i_color;
	v_uv = i_uv;
}
