// SPDX-License-Identifier: MIT
// Static file server for the web tests: optional cross-origin isolation
// headers (COOP/COEP) and a log of requested paths.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';

const TYPES = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript', '.mjs': 'text/javascript',
  '.wasm': 'application/wasm', '.tsv': 'text/tab-separated-values', '.lua': 'text/plain; charset=utf-8',
  '.png': 'image/png', '.glb': 'model/gltf-binary', '.ogg': 'audio/ogg', '.wav': 'audio/wav',
  '.json': 'application/json', '.vert': 'text/plain', '.frag': 'text/plain',
};

/** Starts a server for root on a free port. Returns { url, requests, close }. */
export function startServer(root, { isolated = false } = {}) {
  const requests = [];
  const server = http.createServer((req, res) => {
    const url = new URL(req.url, 'http://localhost');
    const rel = decodeURIComponent(url.pathname).replace(/^\/+/, '') || 'index.html';
    const file = path.resolve(root, rel);
    requests.push(rel);
    if (!file.startsWith(path.resolve(root)) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('not found');
      return;
    }
    const headers = {
      'Content-Type': TYPES[path.extname(file)] || 'application/octet-stream',
      // Only Cache Storage may keep assets: the HTTP cache must not hide requests.
      'Cache-Control': 'no-store',
    };
    if (isolated) {
      headers['Cross-Origin-Opener-Policy'] = 'same-origin';
      headers['Cross-Origin-Embedder-Policy'] = 'require-corp';
      headers['Cross-Origin-Resource-Policy'] = 'same-origin';
    }
    res.writeHead(200, headers);
    fs.createReadStream(file).pipe(res);
  });
  return new Promise((resolve) => {
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address();
      resolve({
        root,
        url: `http://localhost:${port}/`,
        requests,
        close: () => new Promise((done) => { server.close(done); server.closeAllConnections(); }),
      });
    });
  });
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const root = process.argv[2] || '.';
  const isolated = process.argv.includes('--isolated');
  const s = await startServer(root, { isolated });
  console.log(`serving ${root} at ${s.url}${isolated ? ' (cross-origin isolated)' : ''}`);
}
