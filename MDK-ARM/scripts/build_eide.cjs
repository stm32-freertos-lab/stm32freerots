// Use the installed EIDE builder and AC5, with sources/options from .eide/eide.yml.
const fs = require('fs');
const path = require('path');
const cp = require('child_process');
const mdk = path.resolve(__dirname, '..');
const extension = fs.readdirSync(path.join(process.env.USERPROFILE, '.vscode', 'extensions'))
  .filter(n => n.startsWith('cl.eide-')).sort().pop();
const extRoot = path.join(process.env.USERPROFILE, '.vscode', 'extensions', extension);
const yaml = require(path.join(extRoot, 'node_modules', 'yaml'));
const config = yaml.parse(fs.readFileSync(path.join(mdk, '.eide', 'eide.yml'), 'utf8'));
const target = config.targets[config.name];
if (target.toolchain !== 'AC5') throw Error('Expected AC5');
const params = JSON.parse(fs.readFileSync(path.join(mdk, 'phase1', 'baseline.params'), 'utf8'));
const tc = target.toolchainConfigMap.AC5;
const rom = tc.storageLayout.ROM.find(m => m.isChecked);
const ram = tc.storageLayout.RAM.find(m => m.isChecked);
if (Number(rom.mem.size) !== 65536 || Number(ram.mem.size) !== 20480) throw Error('Expected 64KB Flash / 20KB RAM');
const sources = [];
function walk(folder, parent = '') {
  const vpath = parent ? `${parent}/${folder.name}` : folder.name;
  const disabled = p => target.excludeList.some(e => p === e || p.startsWith(e + '/'));
  for (const f of folder.files || []) {
    if (!disabled(`${vpath}/${path.posix.basename(f.path)}`)) sources.push(f.path);
  }
  for (const child of folder.folders || []) walk(child, vpath);
}
walk(config.virtualFolder);
params.name = params.target = config.name;
params.rootDir = mdk;
params.toolchainCfgFile = path.join(extRoot, 'res', 'data', 'models', 'arm.v5.model.json');
params.sourceList = sources.sort();
params.incDirs = target.cppPreprocessAttrs.incList;
params.defines = target.cppPreprocessAttrs.defineList;
params.libDirs = target.cppPreprocessAttrs.libList;
params.options = {...tc.options, global: {...params.options.global, ...tc.options.global}};
const out = path.join(mdk, config.outDir, config.name);
fs.mkdirSync(out, {recursive: true});
params.dumpPath = params.outDir = path.relative(mdk, out);
params.options.linker = {...params.options.linker, 'link-scatter': [path.join(out, config.name + '.sct')]};
fs.writeFileSync(path.join(out, config.name + '.sct'), `LR_IROM1 ${rom.mem.startAddr} ${rom.mem.size} {\n  ER_IROM1 ${rom.mem.startAddr} ${rom.mem.size} {\n    *.o (RESET, +First)\n    *(InRoot$$Sections)\n    .ANY (+RO)\n  }\n  RW_IRAM1 ${ram.mem.startAddr} ${ram.mem.size} {\n    .ANY (+RW +ZI)\n  }\n}\n`);
const paramsPath = path.join(out, 'builder.params');
fs.writeFileSync(paramsPath, JSON.stringify(params, null, 4));
if (process.argv.includes('--configure-only')) {
  console.log(`Generated EIDE AC5 parameters (${sources.length} sources).`);
  process.exit(0);
}
const result = cp.spawnSync(path.join(extRoot, 'res', 'tools', 'win32', 'unify_builder', 'unify_builder.exe'),
  ['-p', paramsPath, '--rebuild', '--no-color'], {cwd: mdk, encoding: 'utf8', windowsHide: true});
const log = (result.stdout || '') + (result.stderr || '');
fs.writeFileSync(path.join(out, 'build-output.log'), log);
process.stdout.write(log);
if (result.error) throw result.error;
process.exit(result.status === null ? 1 : result.status);
