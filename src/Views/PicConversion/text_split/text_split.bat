
@echo off
setlocal enabledelayedexpansion
set n=1
for /f "delims=" %%a in (test.txt) do (
   if "%%a"=="unsigned" (set /a n+=1) else echo>>app_gif_!n!.txt %%a
)
pause

