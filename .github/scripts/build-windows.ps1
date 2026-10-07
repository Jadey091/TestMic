param(
    [Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)]
    [string[]]$BuildCommand
)

$tempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { '.' }
$log = Join-Path $tempRoot 'bongo-cat-native-build.log'
$executable = $BuildCommand[0]
$arguments = if ($BuildCommand.Count -gt 1) { $BuildCommand[1..($BuildCommand.Count - 1)] } else { @() }

& $executable @arguments 2>&1 | Tee-Object -FilePath $log
$status = $LASTEXITCODE
if ($status -eq 0) { exit 0 }

$pattern = 'FAILED:|fatal error|(?:warning|error) [A-Z]+\d+:|LNK\d+|MSB\d+: error|unresolved external|cannot open file|ninja: build stopped'
Get-Content -LiteralPath $log |
    Where-Object { $_ -match $pattern } |
    Select-Object -Last 30 |
    ForEach-Object {
        $message = $_.Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
        Write-Output "::error title=Windows build failure::$message"
    }
exit $status
