
@echo off
setlocal
REM Windows launcher for cpp.py (assumes Python 3 is on PATH as py)
py -3 "%~dp0\cpp.py" %*
endlocal
