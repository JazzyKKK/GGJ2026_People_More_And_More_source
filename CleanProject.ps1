param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot
)

$ErrorActionPreference = 'Stop'

try {
    $projectRootPath = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
    $markerPath = Join-Path $projectRootPath 'GGJ_gamedemo.uproject'

    # The project marker prevents this script from cleaning an unrelated directory.
    if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
        throw 'This directory is not the GGJ_gamedemo project. Cleanup cancelled.'
    }

    # Generated files may be locked while either application is running.
    $runningProcesses = Get-Process UnrealEditor, devenv -ErrorAction SilentlyContinue
    if ($runningProcesses) {
        $processNames = ($runningProcesses.ProcessName | Sort-Object -Unique) -join ', '
        throw "Close Unreal Editor and Visual Studio first. Still running: $processNames"
    }

    # Keep Autosaves, Collections, Config, Screenshots, SourceControl and SaveGames.
    $relativeTargets = @(
        'Binaries',
        'Intermediate',
        'DerivedDataCache',
        '.vs',
        'Saved\Automation',
        'Saved\Cooked',
        'Saved\Crashes',
        'Saved\Interchange',
        'Saved\Logs',
        'Saved\MaterialStats',
        'Saved\ShaderDebugInfo',
        'Saved\Shaders',
        'Saved\StagedBuilds',
        'Saved\Temp',
        'Saved\UnrealBuildTool',
        'Saved\webcache_4430'
    )

    Write-Host 'Cleaning regeneratable Unreal Engine files...' -ForegroundColor Cyan

    foreach ($relativeTarget in $relativeTargets) {
        $targetPath = [System.IO.Path]::GetFullPath(
            (Join-Path $projectRootPath $relativeTarget))

        # Every recursive-delete target must remain below the verified project root.
        if (-not $targetPath.StartsWith(
            $projectRootPath + '\',
            [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe cleanup target detected: $targetPath"
        }

        if (Test-Path -LiteralPath $targetPath) {
            Remove-Item -LiteralPath $targetPath -Recurse -Force
            Write-Host "[Removed] $relativeTarget" -ForegroundColor Green
        }
    }

    $solutionPath = [System.IO.Path]::GetFullPath(
        (Join-Path $projectRootPath 'GGJ_gamedemo.sln'))
    if (-not $solutionPath.StartsWith(
        $projectRootPath + '\',
        [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe cleanup target detected: $solutionPath"
    }

    if (Test-Path -LiteralPath $solutionPath) {
        Remove-Item -LiteralPath $solutionPath -Force
        Write-Host '[Removed] GGJ_gamedemo.sln' -ForegroundColor Green
    }

    Write-Host ''
    Write-Host 'Cleanup complete. Assets, source, config, autosaves, screenshots and saves were preserved.' `
        -ForegroundColor Cyan
    exit 0
}
catch {
    Write-Host ''
    Write-Host "[Error] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
