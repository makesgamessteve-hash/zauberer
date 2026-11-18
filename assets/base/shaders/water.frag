// SPDX-License-Identifier: MIT
// Translucent water: two drifting ripple trains, a sun glint, and more of the
// sky's colour (and less transparency) at glancing angles.
in vec4 v_color;
in vec2 v_uv;
in vec3 v_world;
in vec3 v_normal;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform vec3 u_eye;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform float u_time;
out vec4 o_color;
void main() {
  float ripple = sin(v_uv.x * 0.35 + u_time * 1.3) * sin(v_uv.y * 0.29 - u_time * 1.1) +
                 0.5 * sin((v_uv.x + v_uv.y) * 0.8 + u_time * 2.1);
  vec3 n = normalize(v_normal);
  vec3 to_eye = normalize(u_eye - v_world);
  float lit = max(dot(n, -u_light_dir), 0.0);
  vec3 base = v_color.rgb * (u_ambient + u_light_color * lit) * (0.92 + 0.08 * ripple);
  vec3 h = normalize(to_eye - u_light_dir);
  float glint = pow(max(dot(n, h), 0.0), 80.0) * (0.55 + 0.45 * ripple);
  float fresnel = pow(1.0 - max(dot(n, to_eye), 0.0), 4.0);
  vec3 color = mix(base, u_fog_color, 0.3 * fresnel) + u_light_color * glint;
  float k = length(v_world - u_eye) * u_fog_density;
  o_color = vec4(mix(color, u_fog_color, 1.0 - exp(-k * k)), mix(v_color.a, 1.0, fresnel));
}
