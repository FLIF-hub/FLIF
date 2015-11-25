@ECHO OFF
SET EXIFTOOL=exiftool.exe
SET DCRAW=dcraw.exe
SET SED=sed.exe

SET CMDFI=%0
SET INFI=%1
SET OUTFI=%2
SET NOTFI=%3

IF "%NOTFI%"=="" (
IF NOT "%OUTFI%"=="" (
    IF NOT "%DCRAW% -i %INFI%"=="" (
        REM get Filter pattern //Fuji_xf1: BGGRBGGRBGGRBGGR, Pana_gh3: GBRGGBRGGBRGGBRG, leica_m82: RGGBRGGBRGGBRGGB
		SET CFAPattern=""
        for /f "delims=" %%a in ('%DCRAW% -i -v %1 ^| findstr "Filter pattern: " ^| %SED% "s/Filter pattern: //g" ^| %SED% "s/\///g"') do @set CFAPattern=%%a
        REM get Orientation: "Horizontal (normal)", "Rotate 90 CW", "Rotate 180 CW", "Rotate 270 CW"
		SET Orientation=""
		for /f "delims=" %%a in ('%EXIFTOOL% -Orientation %1 ^| %SED% "s/Orientation                     : //g" ^| %SED% "s/ (normal)//g"') do @set Orientation=%%a
		IF !Orientation!=="Horizontal" SET Orientation=Horizontal
		IF !Orientation!=="Rotate 90 CW" SET Orientation=90CW
		IF !Orientation!=="Rotate 180 CW" SET Orientation=180CW
		IF !Orientation!=="Rotate 270 CW" SET Orientation=270CW
		REM TODO: Remove this case part when FLIF can handle the "CFAPattern" comment
		CALL :RM_%CFAPattern%
		IF ERRORLEVEL 1 (
			ECHO This pattern is unknown: %CFAPattern%
			EXIT /B 1
		)
		:RM_RGGB
			SET Rotate=-t 0
			GOTO END_RM
		:RM_GRBG
			SET Rotate=-t 270
			GOTO END_RM
		:RM_BGGR
			SET Rotate=-t 180
			GOTO END_RM
		:RM_GBRG
			SET Rotate=-t 90
		REM Determine the CFA pattern of output image (without the -t parameter)
		CALL :CFA_%CFAPattern%
		IF ERRORLEVEL 1 (
			ECHO This pattern is unknown: %CFAPattern%
			EXIT /B 1
		)
		:CFA_RGGB
			CALL :RGGB_%Orientation%
			IF ERRORLEVEL 1 (
				ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
			)
			:RGGB_Horizontal
				SET CFAPattern=RGGB
				GOTO END_CFA
			:RGGB_90CW
				SET CFAPattern=GRBG
				GOTO END_CFA
			:RGGB_180CW
				SET CFAPattern=BGGR
				GOTO END_CFA
			:RGGB_270CW
				SET CFAPattern=GBRG
				GOTO END_CFA
		:CFA_GRBG
			CALL :GRBG_%Orientation%
			IF ERRORLEVEL 1 (
				ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
			)
			:GRBG_Horizontal
				SET CFAPattern=GRBG
				GOTO END_CFA
			:GRBG_90CW
				SET CFAPattern=BGGR
				GOTO END_CFA
			:GRBG_180CW
				SET CFAPattern=GBRG
				GOTO END_CFA
			:GRBG_270CW
				SET CFAPattern=RGGB
				GOTO END_CFA
		:CFA_BGGR
			CALL :BGGR_%Orientation%
			IF ERRORLEVEL 1 (
				ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
			)
			:BGGR_Horizontal
				SET CFAPattern=BGGR
				GOTO END_CFA
			:BGGR_90CW
				SET CFAPattern=GBRG
				GOTO END_CFA
			:BGGR_180CW
				SET CFAPattern=RGGB
				GOTO END_CFA
			:BGGR_270CW
				SET CFAPattern=GRBG
				GOTO END_CFA
		:CFA_GBRG
			CALL :GBRG_%Orientation%
			IF ERRORLEVEL 1 (
				ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
			)
			:GBRG_Horizontal
				SET CFAPattern=GBRG
				GOTO END_CFA
			:GBRG_90CW
				SET CFAPattern=RGGB
				GOTO END_CFA
			:GBRG_180CW
				SET CFAPattern=GRBG
				GOTO END_CFA
			:GBRG_270CW
				SET CFAPattern=BGGR
		:END_CFA
		REM Add CFAPattern comment
		ECHO IN %OUTFI%
		@ECHO # CFAPattern: %CFAPattern% > %OUTFI%
		REM extract raw data, presumably in some kind of RGGB format
        %DCRAW% -E -4 -c %Rotate% %INFI% >> %OUTFI%
    ) ELSE (
        ECHO Not a camera raw file: %INFI%
    )
	)
) ELSE (
    ECHO Usage: raw2rggb.bat input_raw_file output.rggb
)