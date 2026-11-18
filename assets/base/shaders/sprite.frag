// SPDX-License-Identifier: MIT
in vec2 v_uv;
in float v_dist;
uniform sampler2D u_tex;
uniform vec3 u_fog_color;
uniform float u_fog_density;
out vec4 o_color;
void main() {
  vec4 c = texture(u_tex, v_uv);
  if (c.a < 0.5) discard;
  float k = v_dist * u_fog_density;
  o_color = vec4(mix(c.rgb, u_fog_color, 1.0 - exp(-k * k)), 1.0);
}
