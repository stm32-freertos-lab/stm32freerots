const fs = require('fs');
const path = require('path');
const mdk = path.resolve(__dirname, '..');
const ext = fs.readdirSync(path.join(process.env.USERPROFILE,'.vscode','extensions')).filter(x=>x.startsWith('cl.eide-')).sort().pop();
const yaml = require(path.join(process.env.USERPROFILE,'.vscode','extensions',ext,'node_modules','yaml'));
const configPath = path.join(mdk,'.eide','eide.yml');
const uvPath = path.join(mdk,'NWatch-DShanMCU-F103.uvprojx');
const config = yaml.parse(fs.readFileSync(configPath,'utf8'));
const target = config.targets[config.name];
const kernel = ['tasks.c','queue.c','list.c','portable/RVDS/ARM_CM3/port.c','portable/MemMang/heap_4.c'].map(p=>'ThirdParty/FreeRTOS-Kernel/'+p);
const includes = ['RTOS','ThirdParty/FreeRTOS-Kernel/include','ThirdParty/FreeRTOS-Kernel/portable/RVDS/ARM_CM3'];
const fullUi = process.argv.includes('--full-ui');
if (!config.virtualFolder.folders.some(f=>f.name==='FreeRTOS-Kernel')) {
  config.virtualFolder.folders.push({name:'FreeRTOS-Kernel',files:kernel.map(p=>({path:p})),folders:[]});
}
if (!config.virtualFolder.folders.some(f=>f.name==='RTOS')) {
  config.virtualFolder.folders.push({name:'RTOS',files:[{path:'RTOS/rtos_app.c'}],folders:[]});
}
target.cppPreprocessAttrs.incList = [...new Set([...target.cppPreprocessAttrs.incList,...includes])];
target.cppPreprocessAttrs.defineList = target.cppPreprocessAttrs.defineList.filter(x=>!x.startsWith('NWATCH_RTOS_BRINGUP_ONLY'));
if (!fullUi) target.cppPreprocessAttrs.defineList.push('NWATCH_RTOS_BRINGUP_ONLY=1');
fs.writeFileSync(configPath,yaml.stringify(config,{lineWidth:0}));
let uv=fs.readFileSync(uvPath,'utf8');
// Header paths belong to Cads/VariousControls, not TargetCommonOption.
uv=uv.replace(/<TargetCommonOption>[\s\S]*?<\/TargetCommonOption>/,block=>block.replace(/<IncludePath>([^<]*)<\/IncludePath>/,(_,p)=>
  `<IncludePath>${p.split(';').filter(x=>!includes.includes(x.replaceAll('\\','/').replace(/^\.\//,''))).join(';')}</IncludePath>`));
uv=uv.replace(/<Cads>[\s\S]*?<\/Cads>/,block=>block.replace(/<IncludePath>([^<]*)<\/IncludePath>/,(_,p)=>
  `<IncludePath>${[...new Set([...p.split(';').filter(Boolean),...includes.map(x=>'./'+x)])].join(';')}</IncludePath>`));
if (!uv.includes('<GroupName>FreeRTOS-Kernel</GroupName>')) {
  const group=(name,files)=>`      <Group>\r\n        <GroupName>${name}</GroupName>\r\n        <Files>\r\n${files.map(p=>`          <File>\r\n            <FileName>${path.posix.basename(p)}</FileName>\r\n            <FileType>1</FileType>\r\n            <FilePath>.\\${p.replaceAll('/','\\')}</FilePath>\r\n          </File>`).join('\r\n')}\r\n        </Files>\r\n      </Group>\r\n`;
  uv=uv.replace('    </Groups>',group('FreeRTOS-Kernel',kernel)+group('RTOS',['RTOS/rtos_app.c'])+'    </Groups>');
}
uv=uv.replace(',NWATCH_RTOS_BRINGUP_ONLY=1','');
if (!fullUi) uv=uv.replace('<Define>USE_HAL_DRIVER,STM32F103xB</Define>','<Define>USE_HAL_DRIVER,STM32F103xB,NWATCH_RTOS_BRINGUP_ONLY=1</Define>');
fs.writeFileSync(uvPath,uv);
console.log(fullUi?'Enabled full NWatch GUI.':'Configured official kernel and scheduler-only bring-up.');
