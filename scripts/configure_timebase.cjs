const fs = require('fs');
const path = require('path');
const mdk = path.resolve(__dirname, '..');
const extension = fs.readdirSync(path.join(process.env.USERPROFILE, '.vscode', 'extensions'))
  .filter(n => n.startsWith('cl.eide-')).sort().pop();
const yaml = require(path.join(process.env.USERPROFILE, '.vscode', 'extensions', extension, 'node_modules', 'yaml'));
const configPath = path.join(mdk, '.eide', 'eide.yml');
const uvPath = path.join(mdk, 'NWatch-DShanMCU-F103.uvprojx');
const before = path.join(mdk, 'phase2_timebase', 'before', 'MDK-ARM');
fs.mkdirSync(path.join(before, '.eide'), {recursive: true});
fs.copyFileSync(configPath, path.join(before, '.eide', 'eide.yml'), fs.constants.COPYFILE_EXCL);
fs.copyFileSync(uvPath, path.join(before, path.basename(uvPath)), fs.constants.COPYFILE_EXCL);
const config = yaml.parse(fs.readFileSync(configPath, 'utf8'));
const target = config.targets[config.name];
const oldCount = target.excludeList.length;
target.excludeList = target.excludeList.filter(p => !/\/stm32f1xx_hal_tim(?:_ex)?\.c$/.test(p));
if (target.excludeList.length !== oldCount - 2) throw Error('Expected two HAL TIM sources to enable');
fs.writeFileSync(configPath, yaml.stringify(config, {lineWidth: 0}));
const uv = fs.readFileSync(uvPath, 'utf8').replace(
  /<File>\s*<FileName>stm32f1xx_hal_tim(?:_ex)?\.c<\/FileName>[\s\S]*?<\/File>/g,
  s => s.replace('<IncludeInBuild>0</IncludeInBuild>', '<IncludeInBuild>1</IncludeInBuild>'));
fs.writeFileSync(uvPath, uv);
console.log('Enabled HAL TIM/TIMEx sources in EIDE and uVision.');
