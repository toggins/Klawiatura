#version 330 core

out vec4 o_color;

in vec4 v_color;
in vec2 v_uv;

uniform sampler2D u_texture;
uniform vec2 u_wave;
uniform float u_time;

vec2 wave(vec2 p) {
    return vec2(p.x + (sin((p.y * u_wave.y) + u_time) * u_wave.x), p.y);
}

void main() {
	o_color = v_color * texture(u_texture, wave(v_uv));
}
