@echo off
rem : When passing %~dp0 as the path to set the directory to in the git bash, 
rem : the trailing '.' character is necessary due to stupid reasons; %~dp0 will 
rem : return a path with a trailing '\' character, and git-bash doesn't like 
rem : that at all!
call "%programfiles%\Git\git-bash.exe" --cd="%~dp0."
