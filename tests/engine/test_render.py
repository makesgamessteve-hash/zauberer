# SPDX-License-Identifier: MIT
"""Engine-level tests with a window and OpenGL (Linux CI: SDL offscreen driver on llvmpipe)."""
from __future__ import annotations

import os
import shutil
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from engine_support import ASSETS, EngineTestCase  # noqa: E402


@unittest.skipUnless(os.environ.get("ZB_RENDER_TESTS") == "1", "rendered tests are disabled (ZB_RENDER_TESTS)")
class RenderTests(EngineTestCase):
    def windowed(self, name: str, *args: str, size: str = "640x360", **kwargs):
        env = {"SDL_VIDEO_DRIVER": os.environ.get("SDL_VIDEO_DRIVER", "offscreen"),
               "SDL_AUDIO_DRIVER": os.environ.get("SDL_AUDIO_DRIVER", "dummy"), "ZB_WINDOW_SIZE": size}
        env.update(kwargs.pop("env", {}))
        return self.scenario(name, *args, headless=False, env=env, **kwargs)

    # spec: rendering-core: Render the carpet model
    # spec: rendering-core: Desktop and web agree
    def test_reference_scene(self):
        self.assert_ok(self.windowed("render_carpet"))

    def test_terrain_view(self):
        self.assert_ok(self.windowed("terrain_view"))

    def test_creation_globe(self):
        self.assert_ok(self.windowed("create_globe"))

    def test_water_view(self):
        self.assert_ok(self.windowed("water_view"))

    # spec: rendering-core: Missing glyph
    def test_missing_glyph(self):
        self.install_mods("glyphs")
        self.assert_ok(self.windowed("glyphs"))

    # spec: rendering-core: Forced stereo
    def test_forced_stereo(self):
        self.assert_ok(self.windowed("stereo", "--debug-views", "2"))

    # spec: webxr-session: Emulated headset on the desktop
    def test_xr_emulated_stereo(self):
        self.install_mods("xr_events")
        self.assert_ok(self.windowed("xr_emulated"))

    # spec: rendering-core: Full HD window
    def test_full_hd_window(self):
        self.assert_ok(self.windowed("layout_fullhd", size="1920x1080"))

    # spec: rendering-core: Non-integer window size
    def test_fractional_window(self):
        self.assert_ok(self.windowed("layout_fractional", size="1000x700"))

    # spec: platform-targets: Resize at runtime
    def test_resize(self):
        self.assert_ok(self.windowed("resize"))

    def test_golden_mismatch_is_reported(self):
        golden = self.tmp / "tests"
        shutil.copytree(Path(ASSETS).parent / "tests" / "scenarios", golden / "scenarios")
        (golden / "golden").mkdir()
        shutil.copy2(Path(ASSETS).parent / "tests" / "golden" / "stereo.png", golden / "golden" / "carpet.png")
        result = self.run_engine("--scenario", str(golden / "scenarios" / "render_carpet.lua"),
                                 env={"SDL_VIDEO_DRIVER": os.environ.get("SDL_VIDEO_DRIVER", "offscreen"),
                                      "ZB_WINDOW_SIZE": "640x360", "ZB_GOLDEN_OUT": str(self.tmp / "golden-out")})
        self.assert_fails(result)
        self.assertIn("pixels differ", result.stdout)
        self.assertTrue((self.tmp / "golden-out" / "carpet.png").is_file(), "the actual image is kept")

    # spec: platform-targets: Normal start
    def test_normal_start_beside_binary(self):
        staged = self.tmp / "install"
        staged.mkdir()
        binary = staged / self.bin.name
        shutil.copy2(self.bin, binary)
        shutil.copytree(ASSETS, staged / "assets")
        result = self.run_engine("--ticks", "30", assets=None, binary=binary, cwd=self.tmp,
                                 env={"SDL_VIDEO_DRIVER": os.environ.get("SDL_VIDEO_DRIVER", "offscreen"),
                                      "SDL_AUDIO_DRIVER": "dummy"})
        self.assert_ok(result)
        self.assert_loaded_from(result.stdout, staged / "assets" / "base")
        self.assertIn("first frame presented", result.stdout)

    # spec: platform-targets: Unsupported graphics context
    def test_unsupported_graphics_context(self):
        if os.environ.get("SDL_VIDEO_DRIVER", "offscreen") not in ("offscreen", "x11"):
            self.skipTest("needs Mesa")
        result = self.run_engine("--ticks", "5", env={"SDL_VIDEO_DRIVER": os.environ.get("SDL_VIDEO_DRIVER", "offscreen"),
                                                      "MESA_GL_VERSION_OVERRIDE": "2.1"})
        self.assert_fails(result)
        self.assertIn("OpenGL 3.3 core profile is required", result.stdout)
        self.assertIn("OpenGL 2.1", result.stdout)


if __name__ == "__main__":
    unittest.main()
