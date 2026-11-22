// SPDX-License-Identifier: MIT
// The player's brightness, the comfort vignette (reference: zb_vignette_factor in src/render/retro.c),
// then palette quantisation with 8x8 ordered dithering (reference:
// zb_palette_quantize). The LUT is 1024 x 32 texels: x = blue * 32 + red,
// y = green, 32 levels per channel. Drawn once per view.
in vec2 v_ndc;
uniform sampler2D u_scene;
uniform sampler2D u_lut;
uniform sampler2D u_bayer;
uniform float u_dither;
uniform int u_palette;
uniform float u_vignette;      // 0 .. 1 (0 = off)
uniform vec2 u_vignette_center; // the view's optical centre (framebuffer pixels)
uniform vec2 u_vignette_half;   // half the view's size (pixels)
uniform float u_brightness;     // the player's brightness (1: as it is)
out vec4 o_color;
void main() {
  ivec2 p = ivec2(gl_FragCoord.xy);
  vec3 c = min(texelFetch(u_scene, p, 0).rgb * u_brightness, vec3(1.0));
  if (u_vignette > 0.0) {
    vec2 d = (gl_FragCoord.xy - u_vignette_center) / u_vignette_half;
    float t = clamp((length(d) - 0.35) / 0.75, 0.0, 1.0);
    c *= 1.0 - u_vignette * t * t * (3.0 - 2.0 * t);
  }
  if (u_palette == 0) {
    o_color = vec4(c, 1.0);
    return;
  }
  float t = texelFetch(u_bayer, ivec2(p.x & 7, p.y & 7), 0).r;
  c = clamp(c + (t - 0.5) * u_dither, 0.0, 1.0);
  ivec3 q = ivec3(floor(c * 31.0 + 0.5));
  o_color = vec4(texelFetch(u_lut, ivec2(q.b * 32 + q.r, q.g), 0).rgb, 1.0);
}
