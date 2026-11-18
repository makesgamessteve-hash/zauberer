// SPDX-License-Identifier: MIT
// A triangle covering the viewport, from gl_VertexID (no vertex buffer).
out vec2 v_ndc;
void main() {
  vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
  v_ndc = p;
  gl_Position = vec4(p, 1.0, 1.0);
}
