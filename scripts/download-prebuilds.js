// scripts/download-prebuilds.js
'use strict';

var fs = require('fs');
var path = require('path');

var pkg = require('../package.json');
var token = process.env.GITHUB_TOKEN;

if (!token) {
  console.error('Error: GITHUB_TOKEN environment variable is required to download prebuilds.');
  process.exit(1);
}

// Parse owner/repo from repository.url
// Handles: https://github.com/owner/repo or git+https://github.com/owner/repo
var repoMatch = pkg.repository.url.match(/github\.com[/:]([^/]+)\/([^/.]+)/);
if (!repoMatch) {
  console.error('Error: Cannot parse GitHub owner/repo from: ' + pkg.repository.url);
  process.exit(1);
}
var owner = repoMatch[1];
var repo = repoMatch[2].replace(/\.git$/, '');
var tag = 'v' + pkg.version;
var rootDir = path.join(__dirname, '..');

console.log('Downloading prebuilds for ' + owner + '/' + repo + '@' + tag + '...');

async function main() {
  var apiUrl = 'https://api.github.com/repos/' + owner + '/' + repo + '/releases/tags/' + tag;
  var apiRes = await fetch(apiUrl, {
    headers: {
      'Authorization': 'Bearer ' + token,
      'Accept': 'application/vnd.github.v3+json',
      'User-Agent': repo + '-prebuild-downloader'
    }
  });

  if (!apiRes.ok) {
    var body = await apiRes.text();
    throw new Error('GitHub API ' + apiRes.status + ' for ' + apiUrl + ': ' + body);
  }

  var release = await apiRes.json();
  var assets = release.assets.filter(function(a) { return /\.abi\d+\.node$/.test(a.name); });

  if (assets.length === 0) {
    console.error(
      'Error: No prebuilt binaries found in GitHub Release ' + tag + '.\n' +
      'Wait for CI to finish uploading prebuilts before running npm publish.'
    );
    process.exit(1);
  }

  console.log('Found ' + assets.length + ' prebuild asset(s).');

  for (var asset of assets) {
    // Asset name format: linux-x64--BluetoothSerialPort.abi108.node
    var sep = asset.name.indexOf('--');
    if (sep === -1) {
      console.warn('Skipping unrecognised asset name: ' + asset.name);
      continue;
    }
    var platformArch = asset.name.slice(0, sep);
    var filename = asset.name.slice(sep + 2);
    var destDir = path.join(rootDir, 'prebuilds', platformArch);
    var destFile = path.join(destDir, filename);

    fs.mkdirSync(destDir, { recursive: true });

    var dlRes = await fetch(asset.browser_download_url, {
      headers: {
        'Authorization': 'Bearer ' + token,
        'User-Agent': repo + '-prebuild-downloader'
      }
    });

    if (!dlRes.ok) {
      throw new Error('Download failed for ' + asset.name + ': HTTP ' + dlRes.status);
    }

    var buf = await dlRes.arrayBuffer();
    fs.writeFileSync(destFile, Buffer.from(buf));
    console.log('  -> ' + path.relative(rootDir, destFile));
  }

  console.log('All prebuilds downloaded successfully.');
}

main().catch(function(err) {
  console.error('Error:', err.message);
  process.exit(1);
});
