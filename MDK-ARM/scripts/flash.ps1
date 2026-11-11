<#
    编译并烧录 NWatch-DShanMCU-F103（EIDE + AC5 构建，ST-Link 烧录）

    用法：
      flash                  增量编译 + 烧录 + 校验 + 复位运行
      flash -Full            完整重建 + 烧录
      flash -NoBuild         只烧录现有的 hex

    在 VSCode 终端里输入 flash 即可，函数定义在 PowerShell profile 中，
    它会从当前目录向上找到本脚本并调用。
#>
param(
    [switch]$NoBuild,
    [switch]$Full
)

$ErrorActionPreference = 'Stop'

$mdk       = Split-Path -Parent $PSScriptRoot
$outDir    = Join-Path $mdk 'build\NWatch-DShanMCU-F103'
$hex       = Join-Path $outDir 'NWatch-DShanMCU-F103.hex'
$buildLog  = Join-Path $mdk 'build\build-output.log'
$flashLog  = Join-Path $mdk 'build\flash.log'

function Fail([string]$message) {
    Write-Host "[flash] $message" -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------- 编译
if (-not $NoBuild) {
    Write-Host '[flash] 编译中...' -ForegroundColor Cyan

    Push-Location $mdk
    try {
        node scripts/build_eide.cjs --configure-only
        if ($LASTEXITCODE -ne 0) {
            Fail '生成 EIDE 构建参数失败（node scripts/build_eide.cjs）。'
        }

        $eideExtension = Get-ChildItem (Join-Path $env:USERPROFILE '.vscode\extensions') -Directory -Filter 'cl.eide-*' |
            Sort-Object Name | Select-Object -Last 1
        if (-not $eideExtension) {
            Fail '没有找到 EIDE 扩展（cl.eide-*），无法编译。'
        }

        $builder     = Join-Path $eideExtension.FullName 'res\tools\win32\unify_builder\unify_builder.exe'
        $paramsFile  = Join-Path $outDir 'builder.params'
        $builderArgs = @('-p', $paramsFile, '--no-color')
        if ($Full) { $builderArgs += '--rebuild' }

        & $builder @builderArgs 2>&1 | Tee-Object -FilePath $buildLog
        if ($LASTEXITCODE -ne 0) {
            Write-Host '[flash] 编译失败，未烧录。日志末尾：' -ForegroundColor Red
            Get-Content $buildLog -Tail 30
            exit 1
        }
    }
    finally {
        Pop-Location
    }

    Write-Host '[flash] 编译成功。' -ForegroundColor Green
}

if (-not (Test-Path $hex)) {
    Fail "找不到 hex 文件：$hex（请先不带 -NoBuild 运行一次）。"
}

# ---------------------------------------------------------------- 烧录
$programmer = Get-Command 'STM32_Programmer_CLI.exe' -ErrorAction SilentlyContinue
if ($programmer) {
    $programmerPath = $programmer.Source
}
else {
    $candidates = @(
        (Join-Path $env:USERPROFILE '.eide\tools\st_cube_programer\bin\STM32_Programmer_CLI.exe'),
        'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe',
        'C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
    )
    $programmerPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $programmerPath) {
        Fail '没有找到 STM32_Programmer_CLI.exe，请确认已安装 STM32CubeProgrammer。'
    }
}

Write-Host '[flash] 烧录中（ST-Link / SWD）...' -ForegroundColor Cyan
& $programmerPath -c port=SWD freq=1800 -w $hex -v -rst 2>&1 | Tee-Object -FilePath $flashLog
$flashExitCode = $LASTEXITCODE

if ($flashExitCode -ne 0) {
    Write-Host '[flash] 烧录失败。日志末尾：' -ForegroundColor Red
    Get-Content $flashLog -Tail 25
    Write-Host '[flash] 常见原因：ST-Link 未连接、被调试会话占用、目标板未上电。' -ForegroundColor Yellow
    exit $flashExitCode
}

Write-Host '[flash] 烧录完成，已校验并复位运行。' -ForegroundColor Green
