-- SPDX-License-Identifier: MIT
-- Test fixture: overrides a default binding and credits two assets.
return {
  id = "override",
  name = "Override fixture",
  version = "1.0.0",
  api = "1.0",
  license = { code = "MIT", assets = "CC-BY-4.0" },
  requires = { base = ">=0.1" },
}
