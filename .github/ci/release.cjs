'use strict';
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { execFileSync } = require('node:child_process');

const hash = data => crypto.createHash('sha256').update(data).digest('hex');
function assertVersion(version) {
  if (typeof version !== 'string' || version !== version.trim() || !/^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$/.test(version) ||
      version.split('.').some(part => part.length > 5 || Number(part) > 65535)) throw Error('Invalid three-part VERSIONINFO version.');
}

function createApi(repository, token) {
  if (!/^[\w.-]+\/[\w.-]+$/.test(repository) || !token) throw Error('Repository and GITHUB_TOKEN are required.');
  const base = `https://api.github.com/repos/${repository}`;
  return async (method, route, body, binary = false) => {
    const url = route.startsWith('https://uploads.github.com/') ? route : base + route;
    const response = await fetch(url, {
      method,
      headers: { Authorization: `Bearer ${token}`, Accept: binary ? 'application/octet-stream' : 'application/vnd.github+json',
        'X-GitHub-Api-Version': '2022-11-28', 'Content-Type': Buffer.isBuffer(body) ? 'application/zip' : 'application/json' },
      body: body === undefined ? undefined : Buffer.isBuffer(body) ? body : JSON.stringify(body),
      signal: AbortSignal.timeout(120000)
    });
    if (method === 'GET' && response.status === 404) return null;
    if (!response.ok) throw Error(`GitHub ${method} ${route}: HTTP ${response.status}`);
    if (binary) return Buffer.from(await response.arrayBuffer());
    return response.status === 204 ? null : response.json();
  };
}

async function resolveTag(api, tag) {
  const ref = await api('GET', `/git/ref/tags/${encodeURIComponent(tag)}`);
  if (!ref) return null;
  let object = ref.object;
  for (let depth = 0; depth < 8; depth++) {
    if (object.type === 'commit') return object.sha;
    if (object.type !== 'tag') throw Error('Tag does not resolve to a commit.');
    object = (await api('GET', `/git/tags/${object.sha}`)).object;
  }
  throw Error('Annotated tag chain is too deep.');
}

async function findReleases(api, tag) {
  const matches = [];
  for (let page = 1; ; page++) {
    const releases = await api('GET', `/releases?per_page=100&page=${page}`);
    matches.push(...releases.filter(release => release.tag_name === tag));
    if (releases.length < 100) return matches;
  }
}

async function publishRelease({ api, version, commit, zip, zipHash, ref }) {
  assertVersion(version);
  if (commit.length !== 40 || !/^[0-9a-f]{40}$/.test(commit) || hash(zip) !== zipHash) throw Error('Commit or ZIP checksum mismatch.');
  const repository = await api('GET', '');
  if (ref !== `refs/heads/${repository.default_branch}`) throw Error('Release is restricted to the default branch.');
  const tag = `v${version}`;
  const matches = await findReleases(api, tag);
  if (matches.some(release => release.draft)) throw Error('An unfinished draft exists. Inspect it manually before retrying.');
  if (matches.length) { console.log(`${tag} is already published; skipping.`); return { skipped: true }; }
  const existingCommit = await resolveTag(api, tag);

  const filename = `MainUICustomizer-${version}.zip`;
  // Any subsequent failure deliberately leaves this draft for manual inspection.
  const draft = await api('POST', '/releases', { tag_name: tag, target_commitish: commit,
    name: `MainUICustomizer ${version}`, draft: true, prerelease: false,
    body: `Commit: ${commit}\n\nSHA256 (${filename}): ${zipHash}` });
  if (!draft.draft || draft.tag_name !== tag || (draft.assets || []).length) throw Error('Unexpected newly created release state.');
  const currentReleases = await findReleases(api, tag);
  if (currentReleases.some(release => release.id !== draft.id)) throw Error('Another release appeared while creating the draft; tag retained.');
  const draftTagCommit = await resolveTag(api, tag);
  if (draftTagCommit && draftTagCommit !== commit) {
    if (draftTagCommit !== existingCommit) throw Error('Tag changed concurrently while creating the draft.');
    // An orphan tag left by a deleted release can be reused for a manual retry.
    // Published releases and pre-existing drafts were rejected before this point.
    await api('PATCH', `/git/refs/tags/${encodeURIComponent(tag)}`, { sha: commit, force: true });
    console.log(`Updated unpublished ${tag}: ${draftTagCommit} -> ${commit}`);
  }
  if (!draftTagCommit) await api('POST', '/git/refs', { ref: `refs/tags/${tag}`, sha: commit });
  if (await resolveTag(api, tag) !== commit) throw Error('Tag target changed before upload.');
  const upload = new URL(draft.upload_url.split('{')[0]);
  if (upload.origin !== 'https://uploads.github.com') throw Error('Unexpected upload origin.');
  upload.searchParams.set('name', filename);
  const asset = await api('POST', upload.href, zip);
  const current = await api('GET', `/releases/${draft.id}`);
  if (!current.draft || current.tag_name !== tag || current.name !== `MainUICustomizer ${version}` || current.assets.length !== 1) throw Error('Draft state changed unexpectedly.');
  const stored = current.assets[0];
  if (stored.id !== asset.id || stored.name !== filename || stored.size !== zip.length || stored.state !== 'uploaded' ||
      (stored.digest && stored.digest !== `sha256:${zipHash}`)) throw Error('Uploaded ZIP metadata mismatch.');
  const downloaded = await api('GET', `/releases/assets/${asset.id}`, undefined, true);
  if (!downloaded || hash(downloaded) !== zipHash) throw Error('Uploaded ZIP read-back SHA256 mismatch.');
  if (await resolveTag(api, tag) !== commit) throw Error('Tag changed before publication; draft retained.');
  const published = await api('PATCH', `/releases/${draft.id}`, { draft: false });
  if (published.draft || published.tag_name !== tag) throw Error('Publication response mismatch.');
  console.log(`Published ${published.html_url}`);
  return { skipped: false, url: published.html_url };
}

async function main() {
  const env = process.env;
  if (env.GITHUB_EVENT_NAME !== 'workflow_dispatch') throw Error('Only workflow_dispatch may publish.');
  const event = JSON.parse(fs.readFileSync(env.GITHUB_EVENT_PATH, 'utf8'));
  const version = env.RELEASE_VERSION;
  assertVersion(version);
  if (event.inputs?.version !== version) throw Error('Dispatch version differs from validated build version.');
  const commit = env.GITHUB_SHA;
  if (execFileSync('git', ['rev-parse', 'HEAD'], { encoding: 'utf8' }).trim() !== commit) throw Error('Checkout differs from build commit.');
  const directory = path.resolve('dist/ci');
  const name = `MainUICustomizer-${version}.zip`;
  const zip = fs.readFileSync(path.join(directory, name));
  const zipHash = env.ZIP_SHA256;
  const report = JSON.parse(fs.readFileSync(path.join(directory, 'compression-report.json'), 'utf8'));
  if (hash(zip) !== zipHash || fs.readFileSync(path.join(directory, `${name}.sha256`), 'utf8').trim() !== `${zipHash}  ${name}` ||
      report.version !== version || report.commit !== commit || report.zipSHA256 !== zipHash ||
      !report.integrityPassed || !report.decompressionPassed || !report.loadPassed || report.packedBytes >= report.originalBytes) throw Error('Release payload is not validated.');
  await publishRelease({ api: createApi(env.GITHUB_REPOSITORY, env.GH_TOKEN), version, commit, zip, zipHash, ref: env.GITHUB_REF });
}

module.exports = { publishRelease, assertVersion };
if (require.main === module) main().catch(error => { console.error(error.message); process.exitCode = 1; });
