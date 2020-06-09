@rem prerequisites: KML_HOME environment variable set
@rem simply launch a command prompt with the proper environment variables 
@rem necessary to build the project
set kmlApplicationName=Win32-KML
set kmlApplicationVersion=r0
set kmlGameDllFileName=game
call %windir%\system32\cmd.exe /k %KML_HOME%\misc\env.bat %~dp0