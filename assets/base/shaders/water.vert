// SPDX-License-Identifier: MIT
// Water surfaces: colour alpha is the opacity, a_uv holds metres across the
// chunk for the ripples, and the normal points up.
in vec3 a_pos;
in vec3 a_normal;
in vec4 a_color;
in vec2 a_uv;
uniform mat4 u_view_proj;
uniform mat4 u_model;
uniform mat4 u_normal;
out vec4 v_color;
out vec2 v_uv;
out vec3 v_world;
out vec3 v_normal;
void main() {
  vec4 world = u_model * vec4(a_pos, 1.0);
  v_world = world.xyz;
  v_normal = normalize((u_normal * vec4(a_normal, 0.0)).xyz);
  v_color = a_color;
  v_uv = a_uv;
  gl_Position = u_view_proj * world;
}
