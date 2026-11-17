// SPDX-License-Identifier: MIT
// web-node builds: let getenv() see the real process environment (tests pass
// ZB_SOURCE_DIR and friends through it).
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  if (typeof ENV === 'undefined') {
    return; /* the program never calls getenv */
  }
  for (var key in process.env) {
    ENV[key] = process.env[key];
  }
});
