# SPDX-License-Identifier: MIT
"""Regenerates every generated asset of the base package."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import gen_carpet  # noqa: E402
import gen_flora  # noqa: E402
import gen_font  # noqa: E402
import gen_scripts  # noqa: E402
import gen_home  # noqa: E402
import gen_ornaments  # noqa: E402
import gen_palette  # noqa: E402
import gen_sounds  # noqa: E402
import gen_sprites  # noqa: E402
import gen_terrain  # noqa: E402


def main(base):
    gen_font.main(os.path.join(base, "fonts"))
    gen_scripts.main(os.path.join(base, "fonts"))
    gen_sprites.main(os.path.join(base, "sprites"))
    gen_flora.main(os.path.join(base, "sprites"))
    gen_carpet.main(os.path.join(base, "models", "carpet.glb"))
    gen_home.main(os.path.join(base, "models"))
    gen_sounds.main(base)
    gen_palette.main(os.path.join(base, "palettes"))
    gen_terrain.main(os.path.join(base, "textures", "terrain"))
    gen_ornaments.main(os.path.join(base, "textures", "ornaments"))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "assets", "base"))
