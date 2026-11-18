// SPDX-License-Identifier: MIT
// Palette quantisation with 8x8 ordered dithering (reference:
// zb_palette_quantize in src/render/retro.c). The LUT is 1024 x 32 texels:
// x = blue * 32 + red, y = green, 32 levels per channel.
in vec2 v_ndc;
uniform sampler2D u_scene;
uniform sampler2D u_lut;
uniform sampler2D u_bayer;
uniform float u_dither;
uniform int u_palette;
out vec4 o_color;
void main() {
  ivec2 p = ivec2(gl_FragCoord.xy);
  vec3 c = texelFetch(u_scene, p, 0).rgb;
  if (u_palette == 0) {
    o_color = vec4(c, 1.0);
    return;
  }
  float t = texelFetch(u_bayer, ivec2(p.x & 7, p.y & 7), 0).r;
  c = clamp(c + (t - 0.5) * u_dither, 0.0, 1.0);
  ivec3 q = ivec3(floor(c * 31.0 + 0.5));
  o_color = vec4(texelFetch(u_lut, ivec2(q.b * 32 + q.r, q.g), 0).rgb, 1.0);
}
