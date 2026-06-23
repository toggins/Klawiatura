#version 300 es

out lowp vec4 o_color;

in lowp vec4 v_color;
in lowp vec2 v_uv;

uniform lowp sampler2D u_texture;
uniform lowp float u_alpha_test;
uniform lowp vec4 u_stencil;

void main() {
	lowp vec4 texsample = texture(u_texture, v_uv);
	if (u_alpha_test > 0.) {
		if (texsample.a < u_alpha_test)
			discard;
		texsample.a = 1.;
	}

	o_color.rgb = mix(v_color.rgb * texsample.rgb, u_stencil.rgb, u_stencil.a);
	o_color.a = v_color.a * texsample.a;
}
