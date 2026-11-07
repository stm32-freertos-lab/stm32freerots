// Keep EIDE and uVision exclusions consistent; no source files are deleted.
const fs = require('fs');
const path = require('path');
const mdk = path.resolve(__dirname, '..');
const extension = fs.readdirSync(path.join(process.env.USERPROFILE, '.vscode', 'extensions'))
  .filter(n => n.startsWith('cl.eide-')).sort().pop();
const yaml = require(path.join(process.env.USERPROFILE, '.vscode', 'extensions', extension, 'node_modules', 'yaml'));
const configPath = path.join(mdk, '.eide', 'eide.yml');
const config = yaml.parse(fs.readFileSync(configPath, 'utf8'));
const disabledNames = new Set([
  'alarm.c', 'alarms.c', 'game1.c', 'game2.c', 'game3.c', 'games.c',
  'settings.c', 'stopwatch.c', 'timedate.c', 'torch.c', 'tune.c', 'tunes.c',
  'sound.c', 'sleep.c', 'diag.c', 'reset.c',
  'driver_color_led.c', 'driver_uart.c', 'driver_passive_buzzer.c',
  'gui_log_console.c', 'driver_dht11.c', 'driver_spiflash_w25q64.c',
  'stm32f1xx_hal_spi.c', 'stm32f1xx_hal_tim.c', 'stm32f1xx_hal_tim_ex.c',
  'stm32f1xx_hal_uart.c'
]);
const excludes = [];
function walk(folder, parent = '') {
  const vpath = parent ? `${parent}/${folder.name}` : folder.name;
  for (const file of folder.files || []) {
    if (disabledNames.has(path.posix.basename(file.path))) excludes.push(`${vpath}/${path.posix.basename(file.path)}`);
  }
  for (const child of folder.folders || []) walk(child, vpath);
}
walk(config.virtualFolder);
const target = config.targets[config.name];
if (target.toolchain !== 'AC5') throw Error('Expected AC5');
target.excludeList = excludes;
const before = path.join(mdk, 'phase1', 'before', 'MDK-ARM');
fs.mkdirSync(path.join(before, '.eide'), {recursive: true});
fs.copyFileSync(configPath, path.join(before, '.eide', 'eide.yml'), fs.constants.COPYFILE_EXCL);
fs.writeFileSync(configPath, yaml.stringify(config, {lineWidth: 0}));
const uvPath = path.join(mdk, 'NWatch-DShanMCU-F103.uvprojx');
fs.copyFileSync(uvPath, path.join(before, path.basename(uvPath)), fs.constants.COPYFILE_EXCL);
let uv = fs.readFileSync(uvPath, 'utf8');
uv = uv.replace(/<File>\s*<FileName>([^<]+)<\/FileName>[\s\S]*?<\/File>/g, (block, name) => {
  if (!disabledNames.has(name)) return block;
  if (block.includes('<IncludeInBuild>')) return block.replace(/<IncludeInBuild>\d<\/IncludeInBuild>/, '<IncludeInBuild>0</IncludeInBuild>');
  return block.replace('</FilePath>', '</FilePath>\n              <FileOption>\n                <CommonProperty>\n                  <IncludeInBuild>0</IncludeInBuild>\n                </CommonProperty>\n              </FileOption>');
});
fs.writeFileSync(uvPath, uv);
fs.writeFileSync(path.join(mdk, 'phase1', 'excluded-files.json'), JSON.stringify([...disabledNames], null, 2));
console.log(`Excluded ${excludes.length} files in EIDE and uVision.`);
