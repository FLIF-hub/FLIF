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
			IF %CFAPattern%==RGGB (
				SET Rotate=-t 0
			) ELSE IF %CFAPattern%==GRBG (
				SET Rotate=-t 270
			) ELSE IF %CFAPattern%==BGGR (
				SET Rotate=-t 180
			) ELSE IF %CFAPattern%==GBRG (
				SET Rotate=-t 90
			) ELSE (
				ECHO This pattern is unknown: %CFAPattern%
				EXIT /B 1
			)
			REM Determine the CFA pattern of output image (without the -t parameter)
			IF %CFAPattern%==RGGB (
				IF %Orientation%==90CW (
					SET CFAPattern=GRBG
				) ELSE IF %Orientation%==180CW (
					SET CFAPattern=BGGR
				) ELSE IF %Orientation%==270CW (
					SET CFAPattern=GBRG
				) ELSE IF NOT %Orientation%==Horizontal (
					ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
				)
			) ELSE IF %CFAPattern%==GRBG (
				IF %Orientation%==90CW (
					SET CFAPattern=BGGR
				) ELSE IF %Orientation%==180CW (
					SET CFAPattern=GBRG
				) ELSE IF %Orientation%==270CW (
					SET CFAPattern=RGGB
				) ELSE IF NOT %Orientation%==Horizontal (
					ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
				)
			) ELSE IF %CFAPattern%==BGGR (
				IF %Orientation%==90CW (
					SET CFAPattern=GBRG
				) ELSE IF %Orientation%==180CW (
					SET CFAPattern=RGGB
				) ELSE IF %Orientation%==270CW (
					SET CFAPattern=GRBG
				) ELSE IF NOT %Orientation%==Horizontal (
					ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
				)
			) ELSE IF %CFAPattern%==GBRG (
				IF %Orientation%==90CW (
					SET CFAPattern=RGGB
				) ELSE IF %Orientation%==180CW (
					SET CFAPattern=GRBG
				) ELSE IF %Orientation%==270CW (
					SET CFAPattern=BGGR
				) ELSE IF NOT %Orientation%==Horizontal (
					ECHO This orientation is unknown: %Orientation%. Assuming Horizontal.
				)
			) ELSE (
				ECHO This pattern is unknown: %CFAPattern%
				EXIT /B 1
			)
			REM Add CFAPattern comment
			@ECHO P5 > %OUTFI%
			@ECHO # CFAPattern: %CFAPattern% >> %OUTFI%
			REM extract raw data, presumably in some kind of RGGB format
			%DCRAW% -E -4 -c %Rotate% %INFI% | %SED% -e "1d" >> %OUTFI%
		) ELSE (
			ECHO Not a camera raw file: %INFI%
		)
	)
) ELSE (
	ECHO Usage: raw2rggb.bat input_raw_file output.rggb
)
