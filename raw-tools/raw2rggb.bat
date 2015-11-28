@ECHO OFF
SET EXIFTOOL=exiftool.exe
SET DCRAW=dcraw.exe
SET SED=sed.exe

SET CMDFI=%0
SET INFI=%1
SET OUTFI=%2
SET NOTFI=%3

IF NOT "%NOTFI%"=="" IF "%OUFI%"=="" (
   ECHO Usage: raw2rggb.bat input_raw_file output.rggb
   EXIT /B 1
)


IF "%DCRAW% -i %INFI%"=="" (
   ECHO Not a camera raw file: %INFI%
   EXIT /B 1
)


REM get Filter pattern //Fuji_xf1: BGGRBGGRBGGRBGGR, Pana_gh3: GBRGGBRGGBRGGBRG, leica_m82: RGGBRGGBRGGBRGGB
SET RCOLOR=
SET PNMTYPE=P5
SET Rotate=
SET CFAP=
for /f "delims=" %%a in ('%DCRAW% -i -v %INFI% ^| findstr "Filter pattern: " ^| %SED% "s/Filter pattern: //g" ^| %SED% "s/\///g"') do @set CFAP=%%a
for /f "delims=" %%a in ('%DCRAW% -i -v %INFI% ^| findstr "Raw colors: " ^| %SED% "s/Raw colors: //g"') do @set RCOLOR=%%a
IF "%CFAP%"=="" (
   IF "%RCOLOR%"=="3" (
      SET PNMTYPE=P6
      GOTO GT_DECOMP
   ) ELSE (
      ECHO This raw color is unknown: %RCOLOR%
      EXIT /B 1
   )
)

REM get Orientation: "Horizontal (normal)", "Rotate 90 CW", "Rotate 180 CW", "Rotate 270 CW"
SET Orientation=
for /f "delims=" %%a in ('%EXIFTOOL% -Orientation %1 ^| %SED% "s/Orientation                     : //g" ^| %SED% "s/ (normal)//g"') do @set Orientation=%%a
IF !Orientation!=="Horizontal" SET Orientation=Horizontal
IF !Orientation!=="Rotate 90 CW" SET Orientation=90CW
IF !Orientation!=="Rotate 180 CW" SET Orientation=180CW
IF !Orientation!=="Rotate 270 CW" SET Orientation=270CW

REM SET Rotate=-t 0
REM TODO: Remove this case part when FLIF can handle the "CFAP" comment
REM IF "%CFAP%"=="RGGB" (
REM   SET Rotate=-t 0
REM) ELSE IF "%CFAP%"=="GRBG" (
REM   SET Rotate=-t 270
REM ) ELSE IF "%CFAP%"=="BGGR" (
REM   SET Rotate=-t 180
REM) ELSE IF "%CFAP%"=="GBRG" (
REM   SET Rotate=-t 90
REM) ELSE (
REM   ECHO This pattern is unknown: %CFAP%
REM   EXIT /B 1
REM)

REM Determine the CFA pattern of output image (without the -t parameter)
IF "%CFAP%"=="RGGB" (
   IF "%Orientation%"=="90CW" (
      SET CFAP=GRBG
   ) ELSE IF "%Orientation%"=="180CW" (
      SET CFAP=BGGR
   ) ELSE IF "%Orientation%"=="270CW" (
      SET CFAP=GBRG
   ) ELSE IF NOT "%Orientation%"=="Horizontal" (
      ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
   )
) ELSE IF "%CFAP%"=="GRBG" (
   IF "%Orientation%"=="90CW" (
      SET CFAP=BGGR
   ) ELSE IF "%Orientation%"=="180CW" (
      SET CFAP=GBRG
   ) ELSE IF "%Orientation%"=="270CW" (
      SET CFAP=RGGB
   ) ELSE IF NOT "%Orientation%"=="Horizontal" (
      ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
   )
) ELSE IF "%CFAP%"=="BGGR" (
   IF "%Orientation%"=="90CW" (
      SET CFAP=GBRG
   ) ELSE IF "%Orientation%"=="180CW" (
      SET CFAP=RGGB
   ) ELSE IF "%Orientation%"=="270CW" (
      SET CFAP=GRBG
   ) ELSE IF NOT "%Orientation%"=="Horizontal" (
      ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
   )
) ELSE IF "%CFAP%"=="GBRG" (
   IF "%Orientation%"=="90CW" (
      SET CFAP=RGGB
   ) ELSE IF "%Orientation%"=="180CW" (
      SET CFAP=GRBG
   ) ELSE IF "%Orientation%"=="270CW" (
      SET CFAP=BGGR
   ) ELSE IF NOT "%Orientation%"=="Horizontal" (
      ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
   )
) ELSE (
   ECHO This pattern is unknown: %CFAP%
   EXIT /B 1
)

:GT_DECOMP
REM Add CFAP comment
REM ECHO %PNMTYPE%>%OUTFI%
IF "%PNMTYPE%"=="P5" ECHO # CFAPattern: %CFAP%>%OUTFI%
REM extract raw data, presumably in some kind of RGGB format
%DCRAW% -E -4 -c %INFI%>>%OUTFI%
