@ECHO OFF
IF EXIST build.xml (
	ant
	PAUSE
) ELSE (
	IF "%CD%"=="%~dp1" (
		ECHO No build script found!
		PAUSE
	) ELSE (
		CD ..
		run_ant %CD%
	)
)