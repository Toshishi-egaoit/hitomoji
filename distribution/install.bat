@echo off
setlocal
cd /d %~dp0

:: --- 管理者権限チェック ---
openfiles >nul 2>&1
if %errorlevel% neq 0 (
    echo 管理者権限が必要です。昇格ダイアログを表示します...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

:: --- ここから下にコピー・登録の処理を書く ---
echo 管理者権限で実行中...

copy /y "x64\hitomoji.dll" "%SystemRoot%\System32\"
"%SystemRoot%\System32\regsvr32.exe" /s "%SystemRoot%\System32\hitomoji.dll"

if exist "%SystemRoot%\SysWOW64" (
    copy /y "x86\hitomoji.dll" "%SystemRoot%\SysWOW64\"
    "%SystemRoot%\SysWOW64\regsvr32.exe" /s "%SystemRoot%\SysWOW64\hitomoji.dll"
)

:: プロファイル登録用EXEの実行（あれば）
:: start /wait regHitomoji.exe

echo 全てのインストール工程が完了しました！
pause