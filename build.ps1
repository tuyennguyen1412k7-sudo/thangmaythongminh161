
$root = $PSScriptRoot
$compiler = "C:\mingw64\bin\g++.exe"
$includeDir = Join-Path $root "include"

# Tim tat ca file CPP
$allSources = @(
    Get-ChildItem -LiteralPath $root -Recurse -File -Filter "*.cpp" |
    ForEach-Object { $_.FullName }
)

if ($allSources.Count -eq 0) {
    Write-Host "Khong tim thay
    
    file CPP!" -ForegroundColor Red
    exit 1
}

# Tach file kiem thu khoi chuong trinh chinh
$appSources = @(
    $allSources | Where-Object {
        $_ -notmatch '[\\/]tests[\\/]'
    }
)

# Cac file test va cac file xu ly dung chung trong src
$testSources = @(
    $allSources | Where-Object {
        $_ -match '[\\/]tests[\\/]' -or
        $_ -match '[\\/]src[\\/]'
    }
)

function Build-Program($outputName, $sources) {
    if ($sources.Count -eq 0) {
        Write-Host "Khong co file de bien dich: $outputName" -ForegroundColor Red
        return 1
    }

    $output = Join-Path $root $outputName

    $compileArgs = @(
        "-std=c++20"
        "-Wall"
        "-Wextra"
        "-g"
        "-I$includeDir"
        "-finput-charset=UTF-8"
"-fexec-charset=UTF-8"
    ) + $sources + @(
        "-o"
        $output
    )

    Write-Host "`nDang bien dich $outputName ..." -ForegroundColor Yellow

    & $compiler @compileArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Bien dich that bai: $outputName" -ForegroundColor Red
        return $LASTEXITCODE
    }

    Write-Host "Bien dich thanh cong: $outputName" -ForegroundColor Green
    return 0
}

# Bien dich chuong trinh chinh
$result = Build-Program "app.exe" $appSources

if ($result -ne 0) {
    exit $result
}

# Bien dich chuong trinh kiem thu neu co
if ($testSources.Count -gt 0) {
    $result = Build-Program "tests.exe" $testSources

    if ($result -ne 0) {
        exit $result
    }
}

Write-Host "`nDa hoan tat bien dich!" -ForegroundColor Green
