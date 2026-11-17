-- SPDX-License-Identifier: MIT
-- Test fixture: a manifest that is not a literal table.
return { id = "broken", version = "1.0.0", api = "1.0", license = "MIT", oops = os.time() }
