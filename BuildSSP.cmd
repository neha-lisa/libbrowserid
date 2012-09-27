@ECHO OFF
::
:: @(#)BuildSSP.cmd
::
:: Copyright (C) 2012 PADL Software Pty Ltd.
:: All rights reserved.
:: Use is subject to license.
::
:: CONFIDENTIAL
::
:: Build EapSSP and dependencies
::
 
:: set these as appropriate
SET PATH=C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin;%PATH%
SET SSP_ROOT=C:\padlssp
SET MOONSHOT_DIR=%SSP_ROOT%\mech_eap
SET HEIMDAL_DIR=%SSP_ROOT%\heimdal
SET LEVENT_DIR=%SSP_ROOT%\levent
SET SIGNTOOL_C=/f C:\MinGW\heimdal\heimdal\PSSPC.pfx
SET CODESIGN_PKT=b28275d2c2cf35e7
::SET CODESIGN_PKT=9ee12c909527c5c4
::SET CODESIGN_PKT=b5d6533a4b40f4c5

:: useful if you try to install the package
SET _DFX_INSTALL_UNSIGNED_DRIVER=1

SET JANSSON_DIR=%SSP_ROOT%\jansson
SET RADSEC_DIR=%SSP_ROOT%\libradsec\lib
SET EAPSSP_DIR=%SSP_ROOT%\mech_eap

PUSHD %MOONSHOT_DIR%

CALL SETENV.CMD /Debug /x86 /win7

SET INCLUDE=%LEVENT_DIR%\obj\dest_i386\inc;%LEVENT_DIR%\compat;%INCLUDE%

ECHO ======== Entering %HEIMDAL_DIR%: (x86)
CD %HEIMDAL_DIR%
NMAKE /f NTMakefile %1
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

ECHO ======== Entering %JANSSON_DIR%: (x86)
CD %JANSSON_DIR%
NMAKE /f NTMakefile %1
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

ECHO ======== Entering %LEVENT_DIR%: (x86)
CD %LEVENT_DIR%
NMAKE /f NTMakefile %1
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

ECHO ======== Entering %RADSEC_DIR%: (x86)
CD %RADSEC_DIR%
NMAKE /f NTMakefile %1
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

ECHO ======== Entering %EAPSSP_DIR%: (x86)
CD %EAPSSP_DIR%
NMAKE /f NTMakefile %1
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

CALL SETENV.CMD /Debug /x64 /win7

SET INCLUDE=%LEVENT_DIR%\obj\dest_i386\inc;%LEVENT_DIR%\compat;%INCLUDE%

ECHO ======== Entering %HEIMDAL_DIR%: (x64)
CD %HEIMDAL_DIR%
NMAKE /f NTMakefile MULTIPLATFORM_INSTALLER=1 %1
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

ECHO ======== Entering %JANSSON_DIR%: (x64)
CD %JANSSON_DIR%
NMAKE /f NTMakefile %1
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

ECHO ======== Entering %LEVENT_DIR%: (x64)
CD %LEVENT_DIR%
NMAKE /f NTMakefile %1
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

ECHO ======== Entering %RADSEC_DIR%: (x64)
CD %RADSEC_DIR%
NMAKE /f NTMakefile %1
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

ECHO ======== Entering %EAPSSP_DIR%: (x64)
CD %EAPSSP_DIR%
NMAKE /f NTMakefile MULTIPLATFORM_INSTALLER=1 %1
IF %ERRORLEVEL% NEQ 0 EXIT /B %ERRORLEVEL%

POPD
