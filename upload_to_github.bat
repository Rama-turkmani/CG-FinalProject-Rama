@echo off
REM ============================================================================
REM  CG Final Project - One-click GitHub upload script
REM  Repo name (per rubric format CG-FinalProject-YourName) : CG-FinalProject-Rama
REM
REM  The .gitignore in this folder already enforces exactly what the rubric
REM  requires:
REM    [UPLOAD]   Source/  Shaders/  Assets/  ThirdParty/  project1/
REM               project1.slnx  README.md  .gitignore
REM    [IGNORED]  .git (managed)  .vs/  x64/  bin/  obj/  *.exe  *.pdb
REM               *.obj  *.ilk  *.tlog  Rama_Final_temp/
REM
REM  PREREQUISITES (do these ONCE, before running this script):
REM    1) Install Git for Windows           : https://git-scm.com/download/win
REM    2) Create an EMPTY repo on github.com named exactly
REM         CG-FinalProject-Rama
REM       (Public visibility, no README/gitignore/license tick-boxes)
REM    3) Have your GitHub account logged in (the first push will open a
REM       browser for authentication via Git Credential Manager).
REM ============================================================================

setlocal EnableDelayedExpansion

REM Work from the folder that contains this .bat file.
cd /d "%~dp0"

echo.
echo ============================================================================
echo   CG Final Project - GitHub Upload
echo   Folder : %CD%
echo ============================================================================
echo.

REM ---- Sanity : is git available? --------------------------------------------
where git >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Git is not installed or not in PATH.
    echo         Download it from https://git-scm.com/download/win and retry.
    pause
    exit /b 1
)

REM ---- Sanity : are we in the right folder? ----------------------------------
if not exist "README.md"    ( echo [ERROR] README.md not found here.    & pause & exit /b 1 )
if not exist "Source"       ( echo [ERROR] Source/ folder not found.    & pause & exit /b 1 )
if not exist "Shaders"      ( echo [ERROR] Shaders/ folder not found.   & pause & exit /b 1 )
if not exist "Assets"       ( echo [ERROR] Assets/ folder not found.    & pause & exit /b 1 )
if not exist "project1.slnx" ( echo [ERROR] project1.slnx not found.    & pause & exit /b 1 )
if not exist ".gitignore"   ( echo [ERROR] .gitignore not found.        & pause & exit /b 1 )

REM ---- Ask for the GitHub username -------------------------------------------
set /p GH_USER=Enter your GitHub username : 
if "%GH_USER%"=="" (
    echo [ERROR] Username is empty. Aborting.
    pause
    exit /b 1
)

set REPO_NAME=CG-FinalProject-Rama
set REMOTE_URL=https://github.com/%GH_USER%/%REPO_NAME%.git

echo.
echo   Target repo : %REMOTE_URL%
echo.
echo   *** Make sure that EMPTY repo exists on GitHub before continuing. ***
echo.
pause

REM ---- Git identity : init first, then set repo-local identity if missing ---
if not exist ".git" (
    echo.
    echo [1/6] git init ...
    git init
    if errorlevel 1 goto :fail
) else (
    echo.
    echo [1/6] Repository already initialised, skipping init.
)

REM git config returns non-zero if the key is not set anywhere.
git config user.name >nul 2>nul
if errorlevel 1 call :prompt_name
git config user.email >nul 2>nul
if errorlevel 1 call :prompt_email
goto :after_identity

:prompt_name
set /p GIT_NAME_IN=Enter your name for Git commit history : 
git config user.name "%GIT_NAME_IN%"
exit /b 0

:prompt_email
set /p GIT_EMAIL_IN=Enter your email for Git commit history : 
git config user.email "%GIT_EMAIL_IN%"
exit /b 0

:after_identity

REM ---- 2. rename branch to main ----------------------------------------------
echo [2/6] Setting branch name to main ...
git branch -M main 2>nul

REM ---- 3. stage all files (respecting .gitignore) ----------------------------
echo [3/6] git add .
git add .
if errorlevel 1 goto :fail

REM Show a short summary of what will be uploaded.
echo.
echo Files staged for commit (top-level) :
git status --short
echo.

REM ---- 4. commit --------------------------------------------------------------
echo [4/6] git commit ...
git commit -m "CG Final Project - 3D Maze Collector (Rama)"
if errorlevel 1 (
    echo (Nothing new to commit, continuing.)
)

REM ---- 5. configure origin ----------------------------------------------------
echo [5/6] Configuring remote 'origin' ...
git remote remove origin >nul 2>nul
git remote add origin %REMOTE_URL%
if errorlevel 1 goto :fail

REM ---- 6. push ---------------------------------------------------------------
echo [6/6] Pushing to GitHub (first time may open a browser for login) ...
git push -u origin main
if errorlevel 1 goto :fail

echo.
echo ============================================================================
echo   SUCCESS - Project uploaded.
echo   Link  : https://github.com/%GH_USER%/%REPO_NAME%
echo.
echo   Next steps (from the rubric) :
echo     1) Open the link above in a browser and confirm it is PUBLIC.
echo     2) Do the CLEAN-CLONE TEST :
echo          git clone https://github.com/%GH_USER%/%REPO_NAME%.git test_clone
echo        Then open test_clone\project1.slnx in Visual Studio and press F5.
echo     3) Email t-hariri@aiu.edu.sy with the subject format from the brief
echo        and paste the link above.
echo ============================================================================
echo.
pause
exit /b 0

:fail
echo.
echo ============================================================================
echo   FAILED - see the error message above.
echo   Common causes :
echo     - The remote repo does not exist yet on github.com.
echo     - You cancelled the browser login pop-up.
echo     - Wrong username typed at the prompt.
echo ============================================================================
echo.
pause
exit /b 1
