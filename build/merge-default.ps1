# Merge the UTF-8 sources without a BOM; never use the active console code page.
$ErrorActionPreference = 'Stop'
try {
    $utf8 = New-Object System.Text.UTF8Encoding($false, $true)
    $sources = @(
        (Join-Path $PSScriptRoot 'hitomoji-default.ini'),
        (Join-Path $PSScriptRoot '..\distribution\hikanji.ini')
    )
    $parts = foreach ($source in $sources) {
        $text = $utf8.GetString([System.IO.File]::ReadAllBytes($source))
        $text.TrimStart([char]0xFEFF).TrimEnd([char[]]"`r`n")
    }
    # A blank line separates the basic settings from hikanji.ini's [key-table].
    $merged = ($parts -join "`r`n`r`n") + "`r`n"
    $outputPath = Join-Path $PSScriptRoot 'merged-default.ini'
    [System.IO.File]::WriteAllBytes($outputPath, $utf8.GetBytes($merged))
    Write-Output "Generated $outputPath"
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
