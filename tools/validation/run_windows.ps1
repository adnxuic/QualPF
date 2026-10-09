param(
    [ValidateSet('gnu','msvc')][string]$Compiler,
    [ValidateSet('build','test')][string]$Action='build',
    [string]$Build,[string]$Raw,[string]$Cmake,[string]$Ninja,
    [string]$QtPrefix,[string]$EigenDirectory,[string]$BoostDirectory,
    [string]$CompilerPath,[string]$Vcvars,[string]$Objdump
)
$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$taskBuild=[System.IO.Path]::GetFullPath($Build)
$taskRaw=[System.IO.Path]::GetFullPath($Raw)
$taskId="windows-$Compiler"
if ($Compiler -eq 'msvc') {
    $env:VSLANG='1033'
    & 'C:/Windows/System32/chcp.com' 65001 > $null
    $taskVcCommand='"'+$Vcvars+'" -vcvars_ver=14.40 >nul && set'
    $taskVcEnv=& 'C:/Windows/System32/cmd.exe' /d /s /c $taskVcCommand
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    foreach ($taskLine in $taskVcEnv) {
        if ($taskLine -match '^(INCLUDE|LIB|LIBPATH|PATH|VCINSTALLDIR|VCToolsInstallDir|VCToolsRedistDir|WindowsSdkDir|WindowsSDKVersion|VSINSTALLDIR)=') {
            $taskParts=$taskLine -split '=',2
            [System.Environment]::SetEnvironmentVariable($taskParts[0],$taskParts[1],'Process')
        }
    }
}
$env:PATH=(Join-Path $QtPrefix 'bin')+';'+(Split-Path $CompilerPath)+';'+$env:PATH
$env:QT_QPA_PLATFORM='offscreen'
$env:MKL_NUM_THREADS='1'
New-Item -ItemType Directory -Force -Path (Join-Path $taskBuild 'tmp') | Out-Null
$env:TEMP=(Join-Path $taskBuild 'tmp')
$env:TMP=$env:TEMP
$taskCtest=Join-Path (Split-Path $Cmake) 'ctest.exe'
New-Item -ItemType Directory -Force -Path $taskRaw | Out-Null
function Invoke-Recorded([string]$program,[string[]]$arguments,[string]$log) {
    $ErrorActionPreference='Continue'
    & $program @arguments *> $log
    $taskCode=$LASTEXITCODE
    Get-Content -LiteralPath $log -Tail 5
    if ($taskCode -ne 0) { throw "Command failed ($taskCode): $program; preserved $log" }
}
if ($Action -eq 'build') {
    if (Test-Path -LiteralPath (Join-Path $taskBuild 'CMakeCache.txt')) { throw 'Fresh build directory already contains a cache' }
    if (Test-Path -LiteralPath (Join-Path $taskBuild 'CMakeFiles')) { throw 'Fresh build directory already contains objects/configuration' }
    $taskStart=[ordered]@{id=$taskId;build=$taskBuild;cache_existed_before=$false;objects_existed_before=$false;utc=[DateTimeOffset]::UtcNow.ToString('o')}
    $taskStart | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskRaw "$taskId-fresh-start.json") -Encoding utf8
    $taskQuery=Join-Path $taskBuild '.cmake/api/v1/query'
    New-Item -ItemType Directory -Force -Path $taskQuery | Out-Null
    New-Item -ItemType File -Path (Join-Path $taskQuery 'codemodel-v2') | Out-Null
    $taskConfigure=@('-S',$taskRoot,'-B',$taskBuild,'-G','Ninja','-DCMAKE_BUILD_TYPE=RELEASE',
        "-DCMAKE_MAKE_PROGRAM=$Ninja","-DCMAKE_CXX_COMPILER=$CompilerPath","-DCMAKE_PREFIX_PATH=$QtPrefix",
        "-DEigen3_DIR=$EigenDirectory","-DQUALPF_CORE_BOOST_INCLUDE_DIR=$BoostDirectory",
        "-DQUALPF_POWDER_BOOST_INCLUDE_DIR=$BoostDirectory",'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
        '-DQUALPF_BUILD_PRODUCT_TESTS=ON','-DQUALPF_BUILD_PREPROCESSING_TESTS=ON')
    if ($Compiler -eq 'msvc') { $taskConfigure+=@('-DCMAKE_TRY_COMPILE_CONFIGURATION=Release','-DCMAKE_CL_SHOWINCLUDES_PREFIX=注意: 包含文件:  ') }
    [ordered]@{configure=@($Cmake)+$taskConfigure;build=@($Cmake,'--build',$taskBuild,'--parallel','4');test=@($taskCtest,'--test-dir',$taskBuild,'-L','normal','--output-on-failure');qtfree_source='tests/architecture'} |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskRaw "$taskId-commands-used.json") -Encoding utf8
    Invoke-Recorded $Cmake $taskConfigure (Join-Path $taskRaw "$taskId-configure.log")
    Invoke-Recorded $Cmake @('--build',$taskBuild,'--parallel','4') (Join-Path $taskRaw "$taskId-build.log")
    $taskPure=$taskBuild+'-application'
    if (Test-Path -LiteralPath (Join-Path $taskPure 'CMakeCache.txt')) { throw 'Fresh Application client directory already contains a cache' }
    $taskPureArgs=@('-S',(Join-Path $taskRoot 'tests/architecture'),'-B',$taskPure,'-G','Ninja',
        '-DCMAKE_BUILD_TYPE=RELEASE',"-DCMAKE_MAKE_PROGRAM=$Ninja","-DCMAKE_CXX_COMPILER=$CompilerPath",
        "-DEigen3_DIR=$EigenDirectory","-DQUALPF_CORE_BOOST_INCLUDE_DIR=$BoostDirectory","-DQUALPF_POWDER_BOOST_INCLUDE_DIR=$BoostDirectory")
    if ($Compiler -eq 'msvc') { $taskPureArgs+=@('-DCMAKE_TRY_COMPILE_CONFIGURATION=Release','-DCMAKE_CL_SHOWINCLUDES_PREFIX=注意: 包含文件:  ') }
    Invoke-Recorded $Cmake $taskPureArgs (Join-Path $taskRaw "$taskId-qtfree-configure.log")
    Invoke-Recorded $Cmake @('--build',$taskPure,'--parallel','4') (Join-Path $taskRaw "$taskId-qtfree-build.log")
    Invoke-Recorded (Join-Path $taskPure 'application_boundary.exe') @() (Join-Path $taskRaw "$taskId-qtfree-test.log")
} else {
    Invoke-Recorded $taskCtest @('--test-dir',$taskBuild,'-L','normal','--output-on-failure','--output-junit',(Join-Path $taskRaw "$taskId-tests.xml")) (Join-Path $taskRaw "$taskId-test.log")
    Invoke-Recorded 'python' @('-B',(Join-Path $taskRoot 'tools/architecture/capture_build.py'),'--build',$taskBuild,'--id',$taskId,'--raw',$taskRaw) (Join-Path $taskRaw "$taskId-capture.log")
    $taskObjdump=$Objdump
    if (-not $taskObjdump) { $taskObjdump=(Get-Command objdump.exe).Source }
    $taskNm=Join-Path (Split-Path $taskObjdump) 'nm.exe'
    Invoke-Recorded 'python' @('-B',(Join-Path $taskRoot 'tools/validation/check_zero_fortran.py'),'--build',$taskBuild,'--objdump',$taskObjdump,'--nm',$taskNm,'--output',(Join-Path $taskRaw "$taskId-zero-fortran.json")) (Join-Path $taskRaw "$taskId-zero-fortran.log")
}
