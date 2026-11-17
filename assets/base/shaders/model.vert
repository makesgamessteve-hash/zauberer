// SPDX-License-Identifier: MIT
// Per-vertex (Gouraud) lighting: one directional light plus ambient.
in vec3 a_pos;
in vec3 a_normal;
in vec4 a_color;
in vec2 a_uv;
uniform mat4 u_view_proj;
uniform mat4 u_model;
uniform mat4 u_normal;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
out vec4 v_color;
out vec2 v_uv;
void main() {
  vec3 n = normalize((u_normal * vec4(a_normal, 0.0)).xyz);
  float diffuse = max(dot(n, -u_light_dir), 0.0);
  v_color = vec4(a_color.rgb * (u_ambient + u_light_color * diffuse), a_color.a);
  v_uv = a_uv;
  gl_Position = u_view_proj * u_model * vec4(a_pos, 1.0);
}
