rem @echo off
setlocal
cd /d %~dp0

:: --- 管理者権限チェック ---
openfiles >nul 2>&1
if %errorlevel% neq 0 (
    echo 管理者権限が必要です。昇格ダイアログを表示します...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

set CONFIG_DIR=%appdata%\Hitomoji

set SYSTEM_DIR=%SystemRoot%\System32
set TARGET_DIR=%SYSTEM_DIR%\hitomoji
set TARGET_DLL=%TARGET_DIR%\Hitomoji.dll

set SYSTEM_DIR32=%SystemRoot%\SysWOW64
set TARGET_DIR32=%SYSTEM_DIR32%\hitomoji
set TARGET_DLL32=%TARGET_DIR32%\Hitomoji.dll

:: 前回のゴミがあれば掃除（リブート後なら消せる）
if exist "%TARGET_DLL%.old" (
    del /q "%TARGET_DLL%.old" >nul 2>&1
)
if exist "%TARGET_DLL32%.old" (
    del /q "%TARGET_DLL32%.old" >nul 2>&1
)

:: 2. フォルダがなければ作成
if not exist "%CONFIG_DIR%" (
    mkdir "%CONFIG_DIR%"
)

if not exist "%TARGET_DIR%" (
    mkdir "%TARGET_DIR%"
)

:: 3. 使用中の現行版を .old に退避（これが「上書き不可」を回避するコツ）
if exist "%TARGET_DLL%" (
    move /y "%TARGET_DLL%" "%TARGET_DLL%.old" >nul 2>&1
)
if exist "%TARGET_DLL32%" (
    move /y "%TARGET_DLL32%" "%TARGET_DLL32%.old" >nul 2>&1
)

echo 管理者権限で実行中...

echo "64ビット版DLLのコピーと登録"
copy /y "x64\hitomoji.dll" "%TARGET_DIR%"
"%SYSTEM_DIR%\regsvr32.exe" /s "%TARGET_DIR%\hitomoji.dll"

echo "32ビット版DLLのコピーと登録"
if exist "%SYSTEM_DIR32%" (
	if not exist "%TARGET_DIR32%" mkdir "%TARGET_DIR32%"
    copy /y "x86\hitomoji.dll" "%TARGET_DIR32%"
    "%SYSTEM_DIR32%\regsvr32.exe" /s "%TARGET_DIR32%\hitomoji.dll"
)

echo "プロファイル登録"
copy /y "x64\regHitomoji.exe" "%TARGET_DIR%"
"%TARGET_DIR%\regHitomoji.exe"

echo "設定ファイルのコピー"
copy /y "hitomoji.ini" "%CONFIG_DIR%"

echo 全てのインストール工程が完了しました
pause
