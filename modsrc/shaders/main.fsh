#version 330 core

out vec4 o_color;

in vec4 v_color;
in vec2 v_uv;

uniform sampler2D u_texture;
uniform float u_alpha_test;
uniform float u_stencil;

void main() {
	vec4 sample = texture(u_texture, v_uv);
	if (u_alpha_test > 0.) {
		if (sample.a < u_alpha_test)
			discard;
		sample.a = 1.;
	}

	o_color.rgb = v_color.rgb * mix(sample.rgb, vec3(1.), u_stencil);
	o_color.a = v_color.a * sample.a;
}
