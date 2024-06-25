@echo off

:: Check for administrative permissions
net session >nul 2>&1
if %errorlevel% neq 0 (
    goto RunAsAdmin
) else (
    goto AdminPrivilegesGranted
)

:RunAsAdmin
:: Create a temporary VBScript to prompt for UAC
set "tempVBS=%temp%\getadmin.vbs"
echo Set UAC = CreateObject^("Shell.Application"^) > "%tempVBS%"
echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%tempVBS%"
:: Run the VBScript
cscript //nologo "%tempVBS%"
:: Clean up
del "%tempVBS%"
exit /b

:AdminPrivilegesGranted
:: Get the directory of this bat file
set scriptDir=%~dp0

:: Change to the bat file's directory
cd /d %scriptDir%

install-interception.exe /install

pause