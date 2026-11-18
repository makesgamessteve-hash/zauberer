// SPDX-License-Identifier: MIT
// Screen-space text in internal-framebuffer pixels (origin top-left).
in vec2 a_pos;
in vec2 a_uv;
in vec4 a_color;
uniform vec2 u_screen;
out vec2 v_uv;
out vec4 v_color;
void main() {
  v_uv = a_uv;
  v_color = a_color;
  gl_Position = vec4(a_pos.x / u_screen.x * 2.0 - 1.0, 1.0 - a_pos.y / u_screen.y * 2.0, 0.0, 1.0);
}
