// SPDX-License-Identifier: MIT
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 o_color;
void main() {
  vec4 c = texture(u_tex, v_uv);
  if (c.a < 0.5) discard;
  o_color = vec4(c.rgb, 1.0);
}
