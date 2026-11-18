// SPDX-License-Identifier: MIT
// Plant billboards: each instance stands on its base and turns to the camera
// about the chunk's up axis; the top sways in the wind.
in vec2 a_corner; // 0..1
in vec3 i_pos;
in vec2 i_size;
in vec4 i_uv;
in vec4 i_tint; // rgb shade, w: species
uniform mat4 u_view_proj;
uniform mat4 u_model;
uniform vec3 u_right;
uniform vec3 u_eye;
uniform float u_time;
uniform vec3 u_tints[64];
out vec2 v_uv;
out vec3 v_tint;
out float v_dist;
void main() {
  vec3 base = (u_model * vec4(i_pos, 1.0)).xyz;
  vec3 up = normalize((u_model * vec4(0.0, 1.0, 0.0, 0.0)).xyz);
  vec3 right = u_right - up * dot(u_right, up);
  right = dot(right, right) > 1e-6 ? normalize(right) : normalize(cross(up, vec3(0.0, 0.0, 1.0)));
  float sway = sin(u_time * 1.6 + dot(i_pos.xz, vec2(0.37, 0.23))) * 0.03 * a_corner.y * a_corner.y;
  vec3 p = base + right * ((a_corner.x - 0.5) * i_size.x + sway * i_size.y) + up * (a_corner.y * i_size.y);
  v_uv = vec2(mix(i_uv.x, i_uv.z, a_corner.x), mix(i_uv.w, i_uv.y, a_corner.y));
  v_tint = i_tint.rgb * u_tints[int(i_tint.w + 0.5)];
  v_dist = length(p - u_eye);
  gl_Position = u_view_proj * vec4(p, 1.0);
}
