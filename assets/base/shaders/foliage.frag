// SPDX-License-Identifier: MIT
// Plant billboards: alpha-tested atlas texels, shaded by species and season.
in vec2 v_uv;
in vec3 v_tint;
in float v_dist;
uniform sampler2D u_tex;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform vec3 u_fog_color;
uniform float u_fog_density;
out vec4 o_color;
void main() {
  vec4 t = texture(u_tex, v_uv);
  if (t.a < 0.5) {
    discard;
  }
  vec3 c = t.rgb * v_tint * (u_ambient + u_light_color * 0.7);
  float k = v_dist * u_fog_density;
  o_color = vec4(mix(c, u_fog_color, 1.0 - exp(-k * k)), 1.0);
}
