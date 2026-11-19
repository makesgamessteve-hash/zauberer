// SPDX-License-Identifier: MIT
// Screen-space text in internal-framebuffer pixels (origin top-left), mapped
// to clip space by u_view_proj: an orthographic map of the viewport, or a
// plane in front of the head in headset views (render.c, explicit_text).
in vec2 a_pos;
in vec2 a_uv;
in vec4 a_color;
uniform mat4 u_view_proj;
out vec2 v_uv;
out vec4 v_color;
void main() {
  v_uv = a_uv;
  v_color = a_color;
  gl_Position = u_view_proj * vec4(a_pos, 0.0, 1.0);
}
