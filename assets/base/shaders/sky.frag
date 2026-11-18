// SPDX-License-Identifier: MIT
// Gradient sky with a sun (reference: zb_sky_color in src/render/retro.c).
in vec2 v_ndc;
uniform vec3 u_cam_right;
uniform vec3 u_cam_up;
uniform vec3 u_cam_fwd;
uniform vec2 u_tan_half;
uniform vec3 u_world_up;
uniform vec3 u_zenith;
uniform vec3 u_horizon;
uniform float u_dip;
uniform vec3 u_sun_dir;
uniform vec3 u_sun_color;
out vec4 o_color;
void main() {
  vec3 ray = normalize(u_cam_fwd + v_ndc.x * u_tan_half.x * u_cam_right + v_ndc.y * u_tan_half.y * u_cam_up);
  float elevation = asin(clamp(dot(ray, u_world_up), -1.0, 1.0)) + u_dip;
  float t = clamp(elevation / 0.6, 0.0, 1.0);
  t = t * (2.0 - t);
  vec3 c = mix(u_horizon, u_zenith, t);
  float sun = dot(ray, u_sun_dir);
  if (sun > 0.0) {
    float glow = sun * sun;
    glow *= glow;
    glow *= glow;
    glow *= glow;
    c += u_sun_color * (0.25 * glow);
  }
  if (sun > 0.9994) {
    c = u_sun_color;
  }
  o_color = vec4(clamp(c, 0.0, 1.0), 1.0);
}
