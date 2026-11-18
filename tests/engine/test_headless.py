# SPDX-License-Identifier: MIT
"""Engine-level tests that run the binary headless (every platform, including web-node)."""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from engine_support import ASSETS, SOURCE_DIR, EngineTestCase, runner_prefix  # noqa: E402

REPLAY = SOURCE_DIR / "tests" / "replays" / "demo.zbrec"


class StartupTests(EngineTestCase):
    # spec: game-loop: Run a fixed number of ticks
    def test_fixed_number_of_ticks(self):
        result = self.run_engine("--headless", "--ticks", "600")
        self.assert_ok(result)
        self.assertIn("state hash at tick 600:", result.stdout)
        self.assertNotIn("renderer:", result.stdout, "no graphics context in headless mode")

    # spec: platform-targets: Missing content folder
    def test_missing_content_folder(self):
        missing = self.tmp / "nowhere" / "assets"
        result = self.run_engine("--headless", "--ticks", "1", assets=missing)
        self.assert_fails(result)
        self.assertIn("The game content was not found", result.stdout)
        self.assertIn(str(missing), result.stdout)

    @unittest.skipIf(runner_prefix(), "web-node looks for content relative to the working directory")
    def test_missing_content_beside_binary(self):
        staged = self.tmp / "bin"
        staged.mkdir()
        binary = staged / self.bin.name
        for part in self.bin.parent.glob(self.bin.stem + ".*"):  # zauberer.js needs zauberer.wasm
            shutil.copy2(part, staged / part.name)
        shutil.copy2(self.bin, binary)
        result = self.run_engine("--headless", "--ticks", "1", assets=None, binary=binary)
        self.assert_fails(result)
        self.assertIn("Expected the 'assets' folder at", result.stdout)

    # spec: platform-targets: Content path override
    def test_content_path_override(self):
        copy = self.tmp / "content"
        shutil.copytree(ASSETS, copy)
        result = self.run_engine("--headless", "--ticks", "10", assets=copy)
        self.assert_ok(result)
        self.assert_loaded_from(result.stdout, copy / "base")

    def test_no_usable_package(self):
        empty = self.tmp / "empty-assets"
        empty.mkdir()
        result = self.run_engine("--headless", "--ticks", "1", assets=empty)
        self.assert_fails(result)
        self.assertIn("No usable content package", result.stdout)

    def test_help_version_and_bad_arguments(self):
        self.assertIn("usage: zauberer", self.run_engine("--help", assets=None).stdout)
        version = self.run_engine("--version", assets=None)
        self.assert_ok(version)
        self.assertRegex(version.stdout, r"zauberer \d+\.\d+\.\d+")
        bad = self.run_engine("--ticks", "many", assets=None)
        self.assert_fails(bad)
        self.assertIn("invalid tick count", bad.stdout)

    # spec: platform-targets: Script requests quit
    def test_script_requests_quit(self):
        self.install_mods("order_a", "order_b")
        result = self.scenario("quit")
        self.assert_ok(result)
        lines = result.stdout.splitlines()
        b = next(i for i, l in enumerate(lines) if "shutdown order_b" in l)
        a = next(i for i, l in enumerate(lines) if "shutdown order_a" in l)
        self.assertLess(b, a, "shutdown hooks run in reverse load order")

    # spec: platform-targets: Data persists across runs
    # spec: platform-targets: Override for tests
    def test_data_persists_in_user_dir(self):
        self.assert_ok(self.scenario("save_write"))
        self.assertTrue((self.user_dir / "save.lua").is_file(), "save data lives below --user-dir")
        self.assert_ok(self.scenario("save_read"))
        other = self.tmp / "other-user"
        other.mkdir()
        self.assert_fails(self.scenario("save_read", user_dir=other))

    def test_user_config_is_applied(self):
        (self.user_dir / "config.lua").write_text(
            "return { bindings = { chime = { 'key:j' }, ghost = { 'key:k' } },"
            " volume = { master = 0.5, music = 0.25, effects = 1 } }\n", encoding="utf-8")
        script = self.tmp / "config.lua"
        script.write_text(
            "scenario { name = 'config' }\n"
            "expect_eq(input.bindings('chime')[1], 'key:j')\n"
            "expect_eq(audio.group_volume('music'), 0.25)\n", encoding="utf-8")
        result = self.run_engine("--headless", "--scenario", str(script))
        self.assert_ok(result)
        self.assertIn("ghost", result.stdout, "a stale entry is reported")
        (self.user_dir / "config.lua").write_text("return {", encoding="utf-8")
        result = self.run_engine("--headless", "--ticks", "1")
        self.assert_ok(result)
        self.assertIn("configuration ignored", result.stdout)

    def test_broken_input_data(self):
        self.install_mods("override")
        (self.user_dir / "mods" / "override" / "data" / "input.lua").write_text("return { chime = {", encoding="utf-8")
        result = self.run_engine("--headless", "--ticks", "1")
        self.assert_ok(result)
        self.assertIn("data/input.lua", result.stdout)
        (self.user_dir / "mods" / "override" / "data" / "input.lua").write_text(
            "return { chime = { type = 'button', bind = { 'key:nope' } } }", encoding="utf-8")
        result = self.run_engine("--headless", "--ticks", "1")
        self.assertIn("invalid binding 'key:nope'", result.stdout)


class ScenarioTests(EngineTestCase):
    # spec: test-automation: Passing scenario
    def test_passing_scenario(self):
        result = self.scenario("demo_runs")
        self.assert_ok(result)
        self.assertIn("scenario 'demo runs for ten seconds': passed after 600 ticks", result.stdout)

    # spec: test-automation: Failing scenario
    def test_failing_scenario(self):
        result = self.run_engine("--headless", "--scenario", "tests/fixtures/scenarios/failing.lua")
        self.assert_fails(result)
        self.assertRegex(result.stdout, r"scenario 'deliberate failure' FAILED at tick 42: .*failing.lua:5: tick count")

    def test_missing_and_broken_scenarios(self):
        self.assert_fails(self.run_engine("--headless", "--scenario", "tests/scenarios/none.lua"))
        broken = self.tmp / "broken.lua"
        broken.write_text("scenario {", encoding="utf-8")
        self.assert_fails(self.run_engine("--headless", "--scenario", str(broken)))

    def test_input_scenario(self):
        self.assert_ok(self.scenario("input"))

    def test_stream_focus_scenario(self):
        self.assert_ok(self.scenario("stream_focus"))

    def test_two_processes_fly_together(self):
        if os.environ.get("ZB_RUNNER"):
            self.skipTest("the host needs native sockets; the browser client runs in tests/web")
        host = self.start_engine("--headless", "--host", "0", "--scenario", "tests/scenarios/coop_host.lua")
        port = None
        lines = []
        for line in host.stdout:
            lines.append(line)
            m = re.search(r"hosting a co-op session on port (\d+)", line)
            if m:
                port = m.group(1)
                break
        self.assertIsNotNone(port, "".join(lines))
        client = self.run_engine("--headless", "--join", f"ws://127.0.0.1:{port}", "--scenario",
                                 "tests/scenarios/coop_client.lua")
        rest, _ = host.communicate(timeout=120)
        host_out = "".join(lines) + rest
        self.assert_ok(client)
        self.assertEqual(host.returncode, 0, host_out)
        self.assertIn("scenario 'co-op host': passed", host_out)

    # spec: world-persistence: Worlds from before saves move into a save
    def test_create_world(self):
        self.assert_ok(self.scenario("create_world"))
        self.assertTrue(any((self.user_dir / "saves").glob("*/bundle.zbw")), "the world is saved below --user-dir")
        # The next start continues in it.
        self.assert_ok(self.scenario("create_resume"))
        # A world kept the old way (worlds/<name>.zbw) is moved into a save.
        save_dir = next((self.user_dir / "saves").glob("*/bundle.zbw")).parent
        (self.user_dir / "worlds").mkdir()
        shutil.move(save_dir / "bundle.zbw", self.user_dir / "worlds" / f"{save_dir.name}.zbw")
        shutil.rmtree(save_dir)
        result = self.scenario("create_resume")
        self.assert_ok(result)
        self.assertIn(f"moved the world {save_dir.name}", result.stdout)
        self.assertTrue((save_dir / "world.lua").is_file())
        self.assertTrue((save_dir / "bundle.zbw").is_file())
        # A saved world that has gone is reported, and the noise world is played.
        script = self.tmp / "gone.lua"
        script.write_text("scenario { name = 'gone' }\nsave.set('world', 'gone')\nexpect(save.flush())\n",
                          encoding="utf-8")
        self.assert_ok(self.run_engine("--headless", "--scenario", str(script)))
        script.write_text("scenario { name = 'noise' }\nexpect(not world.info().created)\n", encoding="utf-8")
        result = self.run_engine("--headless", "--scenario", str(script))
        self.assert_ok(result)
        self.assertIn("could not load the game gone: no such save", result.stdout)

    def test_water_bodies(self):
        self.assert_ok(self.scenario("water"))

    def test_plants(self):
        self.assert_ok(self.scenario("plants"))

    def test_day_and_night(self):
        self.assert_ok(self.scenario("day_and_night"))

    # spec: world-persistence: Save and continue
    def test_save_and_continue(self):
        self.assert_ok(self.scenario("persistence_save"))
        world = (self.user_dir / "saves" / "default" / "world.lua").read_text(encoding="utf-8")
        self.assertIn("format = 1", world)
        self.assertTrue(any((self.user_dir / "saves" / "default" / "regions").glob("*.bin")))
        bodies = self.user_dir / "saves" / "default" / "bodies.lua"
        text = bodies.read_text(encoding="utf-8")
        self.assertIn('["shape"]="box"', text)
        # A damaged entry is left out with a warning; the crate still comes back.
        bodies.write_text(text.rstrip()[:-1] + '[2]={["shape"]="cone",},}\n', encoding="utf-8")
        result = self.scenario("persistence_load")
        self.assert_ok(result)
        self.assertIn("saved body 2 left out", result.stdout)
        self.assertIn("loaded the game default (1 region(s) of edits, 1 body)", result.stdout)

    # spec: world-persistence: Saves of an earlier version still load
    def test_load_saved_fixture(self):
        # A save written by format 1 of the game, kept in the repository.
        shutil.copytree(SOURCE_DIR / "tests" / "fixtures" / "saves" / "v1", self.user_dir, dirs_exist_ok=True)
        result = self.scenario("persistence_load")
        self.assert_ok(result)
        self.assertIn("loaded the game default (1 region(s) of edits, 0 bodies)", result.stdout)

    # spec: materials: Materials from Lua and mods
    def test_materials(self):
        self.install_mods("alchemy")
        result = self.scenario("materials")
        self.assert_ok(result)
        self.assertIn("materials: base:sand patched by alchemy (hardness, mana)", result.stdout)
        self.assertIn("materials: alchemy:phlogiston left out: phase must be solid, powder, liquid or gas",
                      result.stdout)

    def test_fire(self):
        self.assert_ok(self.scenario("fire"))

    def test_tier(self):
        self.assert_ok(self.scenario("tier"))

    def test_weather(self):
        result = self.scenario("weather")
        self.assert_ok(result)
        self.assertIn("lightning struck", result.stdout)

    def test_air(self):
        self.assert_ok(self.scenario("air"))

    def test_surface_water(self):
        self.assert_ok(self.scenario("surface_water"))

    def test_deform(self):
        self.assert_ok(self.scenario("deform"))

    def test_bodies(self):
        self.assert_ok(self.scenario("bodies"))

    def test_mana(self):
        self.assert_ok(self.scenario("mana"))

    def test_weaves(self):
        self.assert_ok(self.scenario("weaves"))

    def test_spellbook(self):
        self.assert_ok(self.scenario("spellbook"))

    def test_creatures(self):
        self.assert_ok(self.scenario("creatures"))

    def test_motes(self):
        self.assert_ok(self.scenario("motes"))

    def test_damage(self):
        self.assert_ok(self.scenario("damage"))

    def test_walking(self):
        self.assert_ok(self.scenario("walking"))

    # spec: vertical-slice: Vertical Slice A
    def test_slice_a(self):
        self.assert_ok(self.scenario("slice_a"))

    def test_plant_life(self):
        self.assert_ok(self.scenario("plant_life"))

    def test_ecology(self):
        self.assert_ok(self.scenario("ecology"))

    def test_fauna(self):
        self.assert_ok(self.scenario("fauna"))

    def test_smalllife(self):
        self.assert_ok(self.scenario("smalllife"))

    def test_survival(self):
        self.assert_ok(self.scenario("survival"))

    def test_harvest(self):
        self.assert_ok(self.scenario("harvest"))

    def test_codex(self):
        self.assert_ok(self.scenario("codex"))

    def test_living_things(self):
        self.assert_ok(self.scenario("living_things"))

    def test_bag(self):
        self.assert_ok(self.scenario("bag"))

    def test_condition(self):
        self.assert_ok(self.scenario("condition"))

    # spec: simulation-tiers: The M3 exit demo
    def test_living_physics(self):
        self.assert_ok(self.scenario("living_physics"))

    def test_flight_controls(self):
        self.assert_ok(self.scenario("flight_controls"))

    def test_ten_minutes_of_flight(self):
        self.assert_ok(self.scenario("flight_endless"))

    # spec: webxr-session: No session on the desktop
    # spec: webxr-session: Head pose reaches Lua
    # spec: webxr-session: Select presses an action
    def test_xr_emulated_scenario(self):
        self.install_mods("xr_events")
        result = self.scenario("xr_emulated")
        self.assert_ok(result)
        self.assertIn("WebXR: session started (emulated headset)", result.stdout)
        self.assertIn("WebXR: session ended after", result.stdout)

    # spec: game-loop: Paused game keeps rendering
    def test_pause_scenario(self):
        self.assert_ok(self.scenario("pause"))

    # spec: asset-packages: Invalid manifest
    # spec: asset-packages: Dependency cycle
    # spec: asset-packages: Package targets a future API
    # spec: lua-scripting: Failing tick hook
    # spec: content-licensing: Mod assets appear in credits
    def test_fixture_packages(self):
        self.install_mods("override", "broken", "cycle_a", "cycle_b", "future", "failing_hook")
        result = self.scenario("packages")
        self.assert_ok(result)
        self.assertIn("failing_hook fixture: deliberate error at tick 5", result.stdout)
        self.assertIn("'tick' hook disabled", result.stdout)

    # spec: input-actions: Phone starts in touch mode
    def test_touch_first_device(self):
        self.assert_ok(self.scenario("touch_mode", env={"ZB_TOUCH_PRIMARY": "1"}))

    def test_recording_from_a_scenario(self):
        record = self.tmp / "rec.zbrec"
        result = self.scenario("record_demo", "--record", str(record), "--seed", "7")
        self.assert_ok(result)
        self.assertTrue(record.is_file())
        hashes = Path(str(record) + ".hashes").read_text(encoding="utf-8").split()
        self.assertEqual(hashes[0], "60")
        self.assertEqual(len(hashes), 20, "a hash every 60 ticks for 600 ticks")


class LuaTests(EngineTestCase):
    # spec: test-automation: Run Lua tests
    def test_run_lua_tests(self):
        junit = self.tmp / "lua.xml"
        result = self.run_engine("--headless", "--test", "tests/lua", "--junit", str(junit))
        self.assert_ok(result)
        suite = ET.parse(junit).getroot()
        self.assertEqual(suite.tag, "testsuite")
        self.assertGreaterEqual(int(suite.get("tests")), 10)
        self.assertEqual(suite.findall(".//failure"), [])
        files = {case.get("classname") for case in suite.iter("testcase")}
        self.assertIn("tests/lua/test_demo_state.lua", files)

    def test_failing_lua_test(self):
        tests = self.tmp / "lua"
        tests.mkdir()
        (tests / "test_fail.lua").write_text("test.case('fails', function() test.expect_eq(1, 2) end)\n",
                                             encoding="utf-8")
        result = self.run_engine("--headless", "--test", str(tests))
        self.assert_fails(result)
        self.assertIn("FAIL", result.stdout)


class ReplayTests(EngineTestCase):
    # spec: input-actions: Replay reproduces actions
    # spec: game-loop: Same inputs, same hash
    # spec: game-loop: Cross-platform hash
    def test_replay_matches_recorded_hashes(self):
        first = self.run_engine("--headless", "--replay", str(REPLAY))
        self.assert_ok(first)
        self.assertIn("checking 10 state hash(es)", first.stdout)
        second = self.run_engine("--headless", "--replay", str(REPLAY))
        self.assert_ok(second)
        self.assertEqual(self.final_hash(first.stdout), self.final_hash(second.stdout))
        expected = Path(str(REPLAY) + ".hashes").read_text(encoding="utf-8").split()[-1]
        self.assertEqual(self.final_hash(first.stdout), expected)
        out = os.environ.get("ZB_HASH_REPORT")
        if out:  # collected by the CI determinism job
            Path(out).write_text(f"{self.final_hash(first.stdout)}\n", encoding="utf-8")

    # spec: test-automation: Divergence detected
    def test_divergence_detected(self):
        self.install_mods("drift")
        result = self.run_engine("--headless", "--replay", str(REPLAY))
        self.assert_fails(result)
        self.assertRegex(result.stdout, r"replay diverged at tick 300: expected state hash [0-9a-f]{16}, got [0-9a-f]{16}")

    def test_bad_replay_files(self):
        self.assert_fails(self.run_engine("--headless", "--replay", str(self.tmp / "missing.zbrec")))
        junk = self.tmp / "junk.zbrec"
        junk.write_bytes(b"not a recording")
        result = self.run_engine("--headless", "--replay", str(junk))
        self.assert_fails(result)
        self.assertIn("not an input recording", result.stdout)

    def test_replay_without_hashes(self):
        copy = self.tmp / "copy.zbrec"
        shutil.copy2(REPLAY, copy)
        result = self.run_engine("--headless", "--replay", str(copy), "--seed", "7")
        self.assert_ok(result)
        self.assertNotIn("checking", result.stdout)


class FailingUnitTestFixture(unittest.TestCase):
    # spec: test-automation: Failing unit test
    def test_failing_unit_test_reports_location(self):
        fixture = os.environ.get("ZB_FAILING_UNIT_FIXTURE")
        if not fixture:
            self.skipTest("ZB_FAILING_UNIT_FIXTURE is not set")
        result = subprocess.run(runner_prefix() + [fixture], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertRegex(result.stdout, r"test_failing\.c:\d+:test_deliberate_failure:FAIL")


if __name__ == "__main__":
    unittest.main()
