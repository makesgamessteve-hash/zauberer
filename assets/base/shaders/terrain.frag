// SPDX-License-Identifier: MIT
in vec3 v_light;
in vec3 v_color;
in vec2 v_uv;
flat in float v_layer;
in float v_dist;
in float v_snow;
uniform sampler2D u_tex;   // material atlas: layers stacked vertically
uniform float u_layers;    // number of atlas layers
uniform vec3 u_fog_color;
uniform float u_fog_density;
out vec4 o_color;
void main() {
  vec3 base = v_color;
  if (v_layer < u_layers) {
    vec2 uv = vec2(fract(v_uv.x), (v_layer + fract(v_uv.y)) / u_layers);
    base = texture(u_tex, uv).rgb;
  }
  base = mix(base, vec3(0.93, 0.95, 1.0), v_snow);
  float k = v_dist * u_fog_density;
  o_color = vec4(mix(base * v_light, u_fog_color, 1.0 - exp(-k * k)), 1.0);
}
