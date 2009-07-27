REM *********************************************************************************
REM *** Netgen Demonstration Application (demoapp) Windows Post-Build Script
REM *** Author: Philippose Rajan
REM *** Date: 05/04/2009
REM ***
REM *** Used to perform an "Install" of the generated Dynamic Link Library (DLL)  
REM *** and the corresponding TCL file(s).
REM ***
REM *** NOTE: This script copies all the TCL files present in the folder into the 
REM *** installation folder.
REM ***
REM *** Call from Visual C++ using:
REM *** postBuild.bat $(ProjectName) $(TargetName) $(ConfigurationName) $(ProjectDir)
REM *********************************************************************************
if [%1]==[] goto InputParamsFailed
set PROJ_NAME=%~1
set PROJ_EXEC=%~2
set BUILD_TYPE=%~3
set BUILD_ARCH=%~4
set PROJ_DIR=%~5

REM *** Change these Folders if required ***
REM Check if the environment variable NETGENDIR exists, 
REM and use it as the installation folder
if defined NETGENDIR (
   echo Environment variable NETGENDIR found: %NETGENDIR%
   set INSTALL_FOLDER=%NETGENDIR%\..
) else (
   echo Environment variable NETGENDIR not found.... using default location!!!
   set INSTALL_FOLDER=%PROJ_DIR%..\..\%PROJ_NAME%-inst_%BUILD_ARCH%
)

set APP_TCLSRC=%PROJ_DIR%..


echo POSTBUILD Script for %PROJ_NAME% ........

REM *** Embed the Windows Manifest into the Executable File ***
REM echo Embedding Manifest into the DLL: %PROJ_EXEC%.dll ....
REM mt.exe -manifest "%BUILD_TYPE%\%PROJ_EXEC%.dll.intermediate.manifest" "-outputresource:%BUILD_TYPE%\%PROJ_EXEC%.dll;2" 
REM if errorlevel 1 goto ManifestFailed
REM echo Embedding Manifest into the DLL: Completed OK!!

REM *** Copy the DLL and LIB Files into the install folder ***
echo Installing application DLL file into %INSTALL_FOLDER%\bin ....
if /i "%BUILD_ARCH%" == "win32" (
   xcopy "%PROJ_DIR%\%BUILD_TYPE%\%PROJ_EXEC%.dll" "%INSTALL_FOLDER%\bin\" /i /d /y
   if errorlevel 1 goto DLLInstallFailed
)
if /i "%BUILD_ARCH%" == "x64" (
   xcopy "%PROJ_DIR%\%BUILD_ARCH%\%BUILD_TYPE%\%PROJ_EXEC%.dll" "%INSTALL_FOLDER%\bin\" /i /d /y
   if errorlevel 1 goto DLLInstallFailed
)   
echo Installing %PROJ_EXEC%: Completed OK!!

REM echo Installing application Lib file into %INSTALL_FOLDER%\lib ....
REM xcopy "%PROJ_DIR%\%BUILD_TYPE%\%PROJ_EXEC%.lib" "%INSTALL_FOLDER%\lib\" /i /d /y
REM if errorlevel 1 goto LibInstallFailed
REM echo Installing %PROJ_EXEC%.lib: Completed OK!!

REM *** Copy the application TCL files into the Install Folder ***
echo Installing application TCL files into %INSTALL_FOLDER% ....
xcopy  "%APP_TCLSRC%\*.tcl" "%INSTALL_FOLDER%\bin\" /i /d /y
if errorlevel 1 goto AppTCLFailed
echo Installing application TCL Files: Completed OK!!

REM *** Clean up the build directory by deleting the OBJ files ***
REM echo Deleting the %PROJ_NAME% build folder %PROJ_DIR%%PROJ_NAME% ....
REM rmdir %PROJ_DIR%%PROJ_NAME% /s /q

REM *** If there have been no errors so far, we are done ***
goto BuildEventOK

REM *** Error Messages for each stage of the post build process ***
:InputParamsFailed
echo POSTBUILD Script for %PROJ_NAME% FAILED..... Invalid number of input parameters!!!
exit 1
:ManifestFailed
echo POSTBUILD Script for %PROJ_NAME% FAILED..... Manifest not successfully embedded!!!
exit 1
:AppTCLFailed
echo POSTBUILD Script for %PROJ_NAME% FAILED..... Error copying application TCL Files into install folder!!!
exit 1
:DLLInstallFailed
echo POSTBUILD Script for %PROJ_NAME% FAILED..... Error copying %PROJ_EXEC%.dll into install folder!!!
exit 1
:LibInstallFailed
echo POSTBUILD Script for %PROJ_NAME% FAILED..... Error copying %PROJ_EXEC%.lib into install folder!!!
exit 1

:BuildEventOK
echo POSTBUILD Script for %PROJ_NAME% completed OK.....!!
