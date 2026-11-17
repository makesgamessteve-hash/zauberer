// SPDX-License-Identifier: MIT
in vec4 v_color;
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 o_color;
void main() {
  vec4 c = v_color * texture(u_tex, v_uv);
  if (c.a < 0.5) discard;
  o_color = vec4(c.rgb, 1.0);
}
