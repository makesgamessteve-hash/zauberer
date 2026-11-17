-- SPDX-License-Identifier: CC-BY-4.0
-- Test fixture: a manifest that is not a literal table.
return { id = "broken", version = "1.0.0", api = "1.0", license = "MIT", oops = os.time() }
