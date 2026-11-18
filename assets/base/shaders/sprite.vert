// SPDX-License-Identifier: MIT
// Instanced camera-facing billboards.
in vec2 a_corner;
in vec3 i_pos;
in vec4 i_size_pivot;
in vec4 i_uv;
uniform mat4 u_view_proj;
uniform vec3 u_right;
uniform vec3 u_up;
uniform vec3 u_eye;
out vec2 v_uv;
out float v_dist;
void main() {
  vec2 offset = (a_corner - i_size_pivot.zw) * i_size_pivot.xy;
  vec3 p = i_pos + u_right * offset.x + u_up * offset.y;
  v_uv = vec2(mix(i_uv.x, i_uv.z, a_corner.x), mix(i_uv.w, i_uv.y, a_corner.y));
  v_dist = length(p - u_eye);
  gl_Position = u_view_proj * vec4(p, 1.0);
}
