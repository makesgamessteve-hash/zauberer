// SPDX-License-Identifier: MIT
// Gradient sky with a sun (reference: zb_sky_color in src/render/retro.c), and
// by night stars that turn with the sky and a moon lit by its phase.
in vec2 v_ndc;
uniform vec3 u_cam_right;
uniform vec3 u_cam_up;
uniform vec3 u_cam_fwd;
uniform vec2 u_tan_half;
uniform vec2 u_tan_offset; // off-axis (headset) projections; 0 for the camera's
uniform vec3 u_world_up;
uniform vec3 u_zenith;
uniform vec3 u_horizon;
uniform float u_dip;
uniform vec3 u_sun_dir;
uniform vec3 u_sun_color;
uniform float u_night;
uniform vec3 u_moon_dir;
uniform float u_moon_light;
uniform mat4 u_celestial;
out vec4 o_color;
void main() {
  vec2 tangent = v_ndc * u_tan_half + u_tan_offset;
  vec3 ray = normalize(u_cam_fwd + tangent.x * u_cam_right + tangent.y * u_cam_up);
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
  if (u_night > 0.01 && elevation > 0.0) {
    vec3 s = (u_celestial * vec4(ray, 0.0)).xyz;
    vec3 cell = floor(s * 180.0);
    float h = fract(sin(dot(cell, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
    if (h > 0.9965) {
      c += vec3(0.75 + 0.25 * fract(h * 97.0)) * u_night * clamp(elevation / 0.15, 0.0, 1.0);
    }
  }
  if (u_moon_light > 0.0) {
    float m = dot(ray, u_moon_dir);
    if (m > 0.0) {
      float halo = m * m;
      halo *= halo;
      halo *= halo;
      halo *= halo;
      c += vec3(0.12, 0.14, 0.2) * u_moon_light * halo * u_night;
    }
    if (m > 0.99985) {
      c = mix(c, vec3(0.92, 0.94, 1.0), 0.25 + 0.75 * u_moon_light);
    }
  }
  if (sun > 0.9994) {
    c = u_sun_color;
  }
  o_color = vec4(clamp(c, 0.0, 1.0), 1.0);
}
