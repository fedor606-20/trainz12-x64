param(
    [string]$Dir = 'D:\universal modder\trainz12-x64\testdir-u9',
    [string]$Build = 'D:\universal modder\trainz12-x64\build32\Release',
    [int]$Rounds = 500
)
$ErrorActionPreference = 'Continue'
Copy-Item "$Build\diff_r3d_*" "$Dir\" -Force
Copy-Item "$Build\Render3DCore_7c.dll" "$Dir\Render3DCore_7c.dll" -Force
$fail = 0
$exes = Get-ChildItem "$Dir\diff_r3d_*.exe" | Sort-Object Name
foreach ($e in $exes) {
    $log = Join-Path $env:TEMP ("r3d_{0}.log" -f $e.BaseName)
    $rounds = $Rounds
    if ($e.BaseName -eq 'diff_r3d_livepaths') { $rounds = 100 }
    $p = Start-Process -FilePath $e.FullName -ArgumentList '.', "$rounds" -WorkingDirectory $Dir -NoNewWindow -Wait -PassThru -RedirectStandardOutput $log -RedirectStandardError ($log + '.err')
    $text = @()
    if (Test-Path $log) { $text += Get-Content $log }
    if (Test-Path ($log + '.err')) { $text += Get-Content ($log + '.err') }
    $last = ($text | Where-Object { $_ -match 'checks' } | Select-Object -Last 1)
    if (-not $last) { $last = ($text | Select-Object -Last 1) }
    if (($p.ExitCode -ne 0) -or ($last -notmatch '0 mismatches')) {
        Write-Output ("FAIL {0} (exit {1}): {2}" -f $e.Name, $p.ExitCode, $last)
        $fail++
    } else {
        Write-Output ("ok   {0}: {1}" -f $e.Name, $last)
    }
}
Write-Output ("failures: {0} / {1}" -f $fail, $exes.Count)
