@ECHO OFF
REM ==================================================================
REM  Forgescript Build System
REM  Author: Tuomo Kanniainen
REM  License: MIT (see LICENSE file)
REM ==================================================================

REM TODO Add log initialize to top, get rid of echoes. <-- store .config/cmd args first in vars, initialize logging, and
REM only after that process the args/flags etc. Start by looking for the log dir arg, so that we get the logging up and
REM running asap.
REM Convert "\" to "/" in all filepaths
REM remove default vars completely? Only use cmd args and .config??
REM Option to turn off logging completely, maybe a flag?

SETLOCAL EnableDelayedExpansion
ECHO [SCRIPT] Running from: %~f0

REM === Ensure we're in script dir ===
CD /D "%~dp0" || ECHO "Failed to change to script directory"

REM ===== Create a timestamp =====
CALL :MAKETIMESTAMP timestamp

REM IMPORTANT: DO NOT EDIT THESE or it can lead to stale/lost data when cleaning up project
SET "fbs_path=%~dp0forgescript\"
SET "fbs_log_file_name=forgescript_build_%timestamp%.log"
SET "fbs_script_name=%~n0"
SET "fbs_config_file_name=%fbs_script_name%.conf"
SET "fbs_info_file_name=%fbs_script_name%.info"

REM Create forgescript directory and conf file
IF NOT EXIST "%fbs_path%%fbs_config_file_name%" (
   ECHO No forgescript config file found. Initializing forgescript. Run %~n0%~x0 --help for help.
   IF NOT EXIST "%fbs_path%" MKDIR "%fbs_path%" 2>NUL
   ECHO compiler:> "%fbs_path%%fbs_config_file_name%"
   ECHO src_dir:>> "%fbs_path%%fbs_config_file_name%"
   ECHO build_dir:>> "%fbs_path%%fbs_config_file_name%"
   ECHO intermediate_dir:>>"%fbs_path%%fbs_config_file_name%"
   ECHO output_name:>> "%fbs_path%%fbs_config_file_name%"
   ECHO log_dir:>> "%fbs_path%%fbs_config_file_name%"
   ECHO include_dirs:>> "%fbs_path%%fbs_config_file_name%"
   ECHO lib_dirs:>> "%fbs_path%%fbs_config_file_name%"
   ECHO libs:>> "%fbs_path%%fbs_config_file_name%"
   ECHO compiler_flags:>> "%fbs_path%%fbs_config_file_name%"
   ECHO linker_flags:>> "%fbs_path%%fbs_config_file_name%"
   EXIT /B 0
)

REM ===== DEFAULT (Low  precedence: can be overwritten by CUSTOM, config file, or cmd args) =====
:: NOTE: These can be edited
SET "default_compiler="
SET "default_src_dir="
SET "default_build_dir="
SET "default_intermediate_dir="
SET "default_output_name="
SET "default_log_dir="
SET "default_include_dirs="
SET "default_lib_dirs="
SET "default_libs="
SET "default_compiler_flags="
SET "default_linker_flags="

REM ===== CONFIG FILE (Mid precedence: can be overwritten by cmd args) =====
SET "conf_compiler="
SET "conf_src_dir="
SET "conf_build_dir="
SET "conf_intermediate_dir="
SET "conf_output_name="
SET "conf_log_dir="
SET "conf_include_dirs="
SET "conf_lib_dirs="
SET "conf_libs="
SET "conf_compiler_flags="
SET "conf_linker_flags="

REM ===== CMD (High precedence: cannot be overwritten) =====
SET "cmd_compiler="
SET "cmd_src_dir="
SET "cmd_build_dir="
SET "cmd_intermediate_dir="
SET "cmd_output_name="
SET "cmd_log_dir="
SET "cmd_include_dirs="
SET "cmd_lib_dirs="
SET "cmd_libs="
SET "cmd_compiler_flags="
SET "cmd_linker_flags="

REM === Parse config file ===
CALL :READ_KEY_VAL_PAIRS_FROM_FILE "%fbs_path%%fbs_config_file_name%" PROCESS_CONF_KEY_VAL

REM === Parse command-line arguments ===
:PARSE_ARGS
IF "%~1"=="" GOTO :ARGS_DONE
SET "arg=%~1"
:: Handle flags
IF /I "%arg%"=="--run"           SET "run_after_build=1"          & SHIFT & GOTO :PARSE_ARGS
IF /I "%arg%"=="--help"          CALL :PRINT_HELP                  & EXIT /B 0
IF /I "%arg%"=="--clean-logs"    CALL :CLEAN_LOGS                  & EXIT /B 0
IF /I "%arg%"=="--clean-build"   CALL :CLEAN_BUILD                 & EXIT /B 0
IF /I "%arg%"=="--clean"         CALL :CLEAN_BUILD & CALL :CLEAN_LOGS & EXIT /B 0

:: Unknown flag
ECHO "%arg%" | FINDSTR /B /I /C:"--" >NUL
IF NOT ERRORLEVEL 1 (
    ECHO "Unknown flag: %arg%"
    SHIFT
    GOTO :PARSE_ARGS
)

::Handle key:value
ECHO "%arg%" | FINDSTR /C:":" >NUL
IF ERRORLEVEL 1 (
    ECHO "Unknown argument: %arg% (use key:value)" & SHIFT & GOTO :PARSE_ARGS
)

:: Split on first ':' 
FOR /F "tokens=1,* delims=:" %%A IN ("%arg%") DO (
    SET "cmd_arg_key=%%A"
    SET "cmd_arg_val=%%B"
)

:: Remove surrounding quotes from key and value if present
CALL :STRIP_QUOTES_VAR cmd_arg_key
CALL :STRIP_QUOTES_VAR cmd_arg_val

::Map key to conf variable
IF /I "!cmd_arg_key!"=="compiler"         SET "cmd_compiler=!cmd_arg_val!"
IF /I "!cmd_arg_key!"=="src_dir"          SET "cmd_src_dir=!cmd_arg_val!"
IF /I "!cmd_arg_key!"=="build_dir"        SET "cmd_build_dir=!cmd_arg_val!"
IF /I "!cmd_arg_key!"=="intermediate_dir" SET "cmd_intermediate_dir=!cmd_arg_val!"
IF /I "!cmd_arg_key!"=="output_name"      SET "cmd_output_name=!cmd_arg_val!"
IF /I "!cmd_arg_key!"=="log_dir"          SET "cmd_log_dir=!cmd_arg_val!"
IF /I "!cmd_arg_key!"=="include_dirs"     SET "cmd_include_dirs=!cmd_arg_val!"
IF /I "!cmd_arg_key!"=="lib_dirs"         SET "cmd_lib_dirs=!cmd_arg_val!"
IF /I "!cmd_arg_key!"=="libs"             SET "cmd_libs=!cmd_arg_val!"
IF /I "!cmd_arg_key!"=="compiler_flags"   SET "cmd_compiler_flags=!cmd_arg_val!"
IF /I "!cmd_arg_key!"=="linker_flags"     SET "cmd_linker_flags=!cmd_arg_val!"
SHIFT
GOTO :PARSE_ARGS
:ARGS_DONE

REM === Set variables to cmd var > conf var > default var ===
CALL :SETOR compiler            cmd_compiler            conf_compiler            default_compiler
CALL :SETOR src_dir             cmd_src_dir             conf_src_dir             default_src_dir
CALL :SETOR build_dir           cmd_build_dir           conf_build_dir           default_build_dir
CALL :SETOR intermediate_dir    cmd_intermediate_dir    conf_intermediate_dir    default_intermediate_dir
CALL :SETOR output_name         cmd_output_name         conf_output_name         default_output_name
CALL :SETOR log_dir             cmd_log_dir             conf_log_dir             default_log_dir
CALL :SETOR include_dirs        cmd_include_dirs        conf_include_dirs        default_include_dirs
CALL :SETOR lib_dirs            cmd_lib_dirs            conf_lib_dirs            default_lib_dirs
CALL :SETOR libs                cmd_libs                conf_libs                default_libs
CALL :SETOR compiler_flags      cmd_compiler_flags      conf_compiler_flags      default_compiler_flags
CALL :SETOR linker_flags        cmd_linker_flags        conf_linker_flags        default_linker_flags

REM === Create project folders if they do not exist
:: Create build directory
IF NOT EXIST "%build_dir%" MKDIR "%build_dir%" 2>NUL

:: Create intermediate directory
IF NOT EXIST "%intermediate_dir%" MKDIR "%intermediate_dir%" 2>NUL

:: Create source directory
IF NOT EXIST "%src_dir%" MKDIR "%src_dir%" 2>NUL

:: Create log directory
IF NOT EXIST "%log_dir%" MKDIR "%log_dir%" 2>NUL

:: Create include directories
SET "list=!include_dirs!"
:CREATE_INCLUDE_DIRS_LOOP
IF NOT DEFINED list GOTO :CREATE_INCLUDE_DIRS_LOOP_DONE
:: Split off the first path (%%A) and keep the rest (%%B)
FOR /F "tokens=1,* delims=;" %%A IN ("!list!") DO (
    :: include_dirs contain paths that are not quoted
    SET "clean_path=%%A"

    :: Create directory
    IF NOT EXIST "!clean_path!" MKDIR "!clean_path!" 2>NUL

    :: Prepare the remaining part for next iteration
    SET "list=%%B"
)
GOTO :CREATE_INCLUDE_DIRS_LOOP
:CREATE_INCLUDE_DIRS_LOOP_DONE

:: Create lib directories
SET "list=!lib_dirs!"
:CREATE_LIB_DIRS_LOOP
IF NOT DEFINED list GOTO :CREATE_LIB_DIRS_LOOP_DONE
:: Split off the first path (%%A) and keep the rest (%%B)
FOR /F "tokens=1,* delims=;" %%A IN ("!list!") DO (
    :: lib_dirs contain paths that are not quoted
    SET "clean_path=%%A"

    :: Create directory
    IF NOT EXIST "!clean_path!" MKDIR "!clean_path!" 2>NUL

    :: Prepare the remaining part for next iteration
    SET "list=%%B"
)
GOTO :CREATE_LIB_DIRS_LOOP
:CREATE_LIB_DIRS_LOOP_DONE

REM === Save latest build config to info file(used when cleaning build files/logs ===
ECHO compiler:%compiler%> "%fbs_path%%fbs_info_file_name%"
ECHO src_dir:%src_dir%>> "%fbs_path%%fbs_info_file_name%"
ECHO build_dir:%build_dir%>> "%fbs_path%%fbs_info_file_name%"
ECHO intermediate_dir:%intermediate_dir%>> "%fbs_path%%fbs_info_file_name%"
ECHO output_name:%output_name%>> "%fbs_path%%fbs_info_file_name%"
ECHO log_dir:%log_dir%>> "%fbs_path%%fbs_info_file_name%"
ECHO include_dirs:%include_dirs%>> "%fbs_path%%fbs_info_file_name%"
ECHO lib_dirs:%lib_dirs%>> "%fbs_path%%fbs_info_file_name%"
ECHO libs:%libs%>> "%fbs_path%%fbs_info_file_name%"
ECHO compiler_flags:%compiler_flags%>> "%fbs_path%%fbs_info_file_name%"
ECHO linker_flags:%linker_flags%>> "%fbs_path%%fbs_info_file_name%"

REM === Initialize log ===
(
    ECHO.
    ECHO ========================================
    ECHO  BUILD STARTED: %DATE% %TIME%
    ECHO  Script: %~f0
    ECHO  Compiler: %compiler%
    ECHO  src_dir: %src_dir%
    ECHO  build_dir: %build_dir%
    ECHO  intermediate_dir: %intermediate_dir%
    ECHO  output_name: %output_name%
    ECHO  log_dir: %log_dir%
    ECHO  include_dirs: %include_dirs%
    ECHO  lib_dirs: %lib_dirs%
    ECHO  libs: %libs%
    ECHO  compiler_flags: %compiler_flags%
    ECHO  linker_flags: %linker_flags%
    ECHO ========================================
    ECHO.
) > "%log_dir%%fbs_log_file_name%"

GOTO :MAIN

REM == Print help message ===
:PRINT_HELP
ECHO.
ECHO %~n0%~x0 [KEY:VAL ...] [--FLAG ...]
ECHO [KEY]:
ECHO compiler:
ECHO    Compiler to use. Must be one of the following: clang++, clang, clang-cl
ECHO    Example: compiler:clang++
ECHO src_dir
ECHO    Directory path to search for source files. Subdirectories will be searched too. Should be enclosed in quotes.
ECHO    Example: "src_dir:C:\Users\my_user\Projects\MyProject\src\"
ECHO build_dir
ECHO    Directory path where to place the program executables. Should be enclosed in quotes.
ECHO    Example: "build_dir:C:\Users\my_user\Projects\MyProject\build\"
ECHO intermediate_dir
ECHO    Directory path where to place the object files. Should be enclosed in quotes.
ECHO    Example: "intermediate_dir:C:\Users\my_user\Projects\MyProject\build\intermediate\"
ECHO output_name
ECHO    Name of the executable. Should contain the extension.
ECHO    Example: output_name:program.exe
ECHO log_dir
ECHO    Directory path where to store forgescript logs. Should be enclosed in quotes.
ECHO    Example: "log_dir:C:\Users\my_user\Projects\MyProject\forgescript\log\"
ECHO include_dirs
ECHO    Additional include directories' paths. Should be enclosed in quotes and separated by a ";" symbol.
ECHO    Example: "include_dirs:C:\Users\my_user\Projects\MyProject\include\;C:\Users\my_user\Projects\MyProject\include2\"
ECHO lib_dirs
ECHO    Additional library directories' paths. Should be enclosed in quotes and separated by a ";" symbol.
ECHO    Example: "lib_dirs:C:\Users\my_user\Projects\MyProject\libraries\;C:\Users\my_user\Projects\libraries2\"
ECHO libs
ECHO    Libraries to link to the program. Should be enclosed in quotes and separated by a ";" symbol.
ECHO    Example: "libs:glfw3;opengl32;gdi32;user32"
ECHO compiler_flags
ECHO    Flags for the clang compiler. Should be enclosed in quotes and separated by a ";" symbol.
ECHO    Example(clang/clang++): "compiler_flags:-g;-O0;-Wall"
ECHO    Example(clang-cl): "compiler_flags:/Zi;/Od;/Wall"
ECHO linker_flags
ECHO    Flags for the clang linker. Should be enclosed in quotes and separated by a ";" symbol.
ECHO    Example(clang/clang++): "linker_flags:-Wl,--verbose;-shared"
ECHO    EXAMPLE(clang-cl): "linker_flags: /SUBSYSTEM:CONSOLE;/DLL"
ECHO.
ECHO [FLAG]:
ECHO --help
ECHO    Print this help message.
ECHO --run
ECHO    Run the program after compiling.
ECHO --clean-logs
ECHO    Clean the logs in the log folder.
ECHO --clean-build
ECHO    Clean all of the build files in the build folder.
ECHO --clean
ECHO    Clean both logs and build files.
ECHO --force
ECHO    If build/log files are stored in a folder outside of the project folder, this flag must be used when cleaning the project.
ECHO.
ECHO Full working example with the command line arguments (note that missing key:val pairs are drawn from defaults or .config file:
ECHO   %~n0%~x0 "build_dir:C:\Users\my_user\Projects\MyProject\build\" output_name:hello_world.exe "compiler_flags:-g;-O0;-Wall"
ECHO.
ECHO NOTE:
ECHO   Command line arguments should only be used for flags, or testing/trivial projects.
ECHO   It is recommended to use the %fbs_config_file_name% file to configure the script!
ECHO   .conf file location: %fbs_path%%fbs_config_file_name%
ECHO.
ECHO Example .conf file (note that quotes are not required, unlike with the cmd line args):
ECHO compiler:clang++
ECHO src_dir:C:\Users\my_user\Projects\MyProject\src\
ECHO build_dir:C:\Users\my_user\Projects\MyProject\build\
ECHO intermediate_dir:C:\Users\my_user\Projects\MyProject\build\intermediate\
ECHO output_name:hello_world.exe
ECHO log_dir:C:\Users\my_user\Projects\MyProject\forgescript\log\
ECHO include_dirs:C:\Users\my_user\Projects\MyProject\include\;C:\Users\my_user\Projects\MyProject\include2\
ECHO lib_dirs:C:\Users\my_user\Projects\MyProject\libraries\
ECHO libs:glfw3;opengl32;gdi32;user32
ECHO compiler_flags:-g;-O0;-Wall
ECHO linker_flags:-Wl,--verbose;-shared
ECHO.
ECHO in addition to the conf file and command line arguments, you can also edit the default variable values in the %~n0%~x0 script. These variables are:
ECHO default_compiler
ECHO default_src_dir
ECHO default_build_dir
ECHO default_intermediate_dir
ECHO default_output_name
ECHO default_log_dir
ECHO default_include_dirs
ECHO default_lib_dirs
ECHO default_libs
ECHO default_compiler_flags
ECHO default_linker_flags
ECHO.
ECHO IMPORTANT: configuration settings have precedences: HIGH - command line arguments, MID - config file, LOW - defaults in script
ECHO Higher precedence values overwrite lower precedence values!
ECHO.
ECHO User does not have to worry about adding -L, -l, /LIBPATH: linker flags with the paths. The script handles it.
ECHO.
ECHO Further documentation: https://github.com/tuomok1010/forgescript-build-system
GOTO :EOF

REM === Clean the build directories ===
:CLEAN_BUILD
CALL :READ_KEY_VAL_PAIRS_FROM_FILE "%fbs_path%%fbs_info_file_name%" PROCESS_CLEAN_KEY_VAL

:: Clean build dir
IF NOT EXIST "%build_dir%" GOTO :EOF
ECHO Cleaning build directory: "%build_dir%"...
CALL :IS_SUBDIR "%build_dir%" "%~dp0" is_safe
IF /I "%is_safe%"=="YES" (
    ECHO Cleaning project-local build files: "%build_dir%"
    DEL /Q /F "%build_dir%%output_name%" 2>NUL
    DEL /Q /F "%build_dir%*.exe" 2>NUL
    DEL /Q /F "%build_dir%*.ilk" 2>NUL
    DEL /Q /F "%build_dir%*.pdb" 2>NUL
    DEL /Q /F "%build_dir%*.exp" 2>NUL
    DEL /Q /F "%build_dir%*.lib" 2>NUL
    DEL /Q /F "%build_dir%*.dll" 2>NUL
) ELSE IF DEFINED force_clean (
    ECHO FORCE: Cleaning external build dir: "%build_dir%"
    DEL /Q /F "%build_dir%*%output_name%*.exe" 2>NUL
    DEL /Q /F "%build_dir%*%output_name%*.ilk" 2>NUL
    DEL /Q /F "%build_dir%*%output_name%*.pdb" 2>NUL
    DEL /Q /F "%build_dir%*%output_name%*.exp" 2>NUL
    DEL /Q /F "%build_dir%*%output_name%*.lib" 2>NUL
    DEL /Q /F "%build_dir%*%output_name%*.dll" 2>NUL
) ELSE (
    ECHO build_dir outside project. Use --clean --force to clean.
)

:: Clean intermediate dir
IF NOT EXIST "%intermediate_dir%" GOTO :EOF
ECHO Cleaning intermediate directory: "%intermediate_dir%"...
CALL :IS_SUBDIR "%intermediate_dir%" "%~dp0" is_safe
IF /I "%is_safe%"=="YES" (
    ECHO Cleaning project-local intermediate files: "%intermediate_dir%"
    DEL /Q /F "%intermediate_dir%*.obj" 2>NUL
    DEL /Q /F "%intermediate_dir%*.o" 2>NUL
) ELSE IF DEFINED force_clean (
    ECHO FORCE: Cleaning external intermediate dir: "%intermediate_dir%"
    DEL /Q /F "%intermediate_dir%*.obj" 2>NUL
    DEL /Q /F "%intermediate_dir%*.o" 2>NUL
) ELSE (
    ECHO intermediate_dir outside project. Use --clean --force to clean.
)
ECHO Done.
GOTO :EOF


REM === Clean the log directory ===
:CLEAN_LOGS
CALL :READ_KEY_VAL_PAIRS_FROM_FILE "%fbs_path%%fbs_info_file_name%" PROCESS_CLEAN_KEY_VAL
IF NOT EXIST "%log_dir%" GOTO :EOF
ECHO Cleaning log directory: "%log_dir%"...
CALL :IS_SUBDIR "%log_dir%" "%~dp0" is_safe
IF /I "%is_safe%"=="YES" (
    ECHO Cleaning project-local logs: "%log_dir%"
    DEL /Q /F "%log_dir%forgescript_build_*.log" 2>NUL
) ELSE IF DEFINED force_clean (
    ECHO FORCE: Cleaning external log dir: "%log_dir%"
    DEL /Q /F "%log_dir%forgescript_build_*.log" 2>NUL
) ELSE (
    ECHO log_dir outside project. Use --clean --force to clean.
)
ECHO Done.
GOTO :EOF

REM === Logging Function ===
:LOG
SET "level=%~1"
SET "msg=%~2"
SET "log_line=[%timestamp%] [%level%] %msg%"
ECHO !log_line!
ECHO !log_line! >> "%log_dir%%fbs_log_file_name%"
IF /I "%level%"=="ERROR" (
    EXIT /B 1
)
EXIT /B 0

:MAIN
CALL :LOG INFO "Building %output_name%"

REM === Collect source files ===
SET "src_files="
SET "file_count=0"

FOR /R "%src_dir%" %%F IN (*.cpp *.c) DO (
    IF EXIST "%%F" (
        SET "src_files=!src_files! "%%F""
        SET /A file_count+=1
        CALL :LOG INFO "Found source: %%F"
    )
)

REM remove leading space
IF DEFINED src_files SET "src_files=!src_files:~1!"

IF %file_count% EQU 0 (
    CALL :LOG INFO "No .cpp or .c files found in '%src_dir%', exiting."
    EXIT /B 0
)

CALL :LOG INFO "Found %file_count% source file(s)"

REM Collect the include dirs
SET "list=!include_dirs!"
SET "include_dirs_prefixed="
:COLLECT_INCLUDE_DIRS_LOOP
IF NOT DEFINED list GOTO :COLLECT_INCLUDE_DIRS_LOOP_DONE
:: Split off the first path (%%A) and keep the rest (%%B)
FOR /F "tokens=1,* delims=;" %%A IN ("!list!") DO (
    :: include_dirs contain paths that are not quoted
    SET "clean_path=%%A"

    :: Remove trailing backslash if present
    IF "!clean_path:~-1!"=="\" SET "clean_path=!clean_path:~0,-1!"

    :: Quote the path properly
    SET "quoted_path="!clean_path!""

    IF /I "!compiler!"=="clang-cl" (
       REM Append  MSVC-style prefix(/I)
       SET "include_dirs_prefixed=!include_dirs_prefixed! /I!quoted_path!"
    ) ELSE (
       :: Append GNU-style prefix(-I)
       SET "include_dirs_prefixed=!include_dirs_prefixed! -I!quoted_path!"    
    )

    :: Prepare the remaining part for next iteration
    SET "list=%%B"
)
GOTO :COLLECT_INCLUDE_DIRS_LOOP
:COLLECT_INCLUDE_DIRS_LOOP_DONE

REM Collect the lib dirs
SET "list=!lib_dirs!"
SET "lib_dirs_prefixed="
:COLLECT_LIB_DIRS_LOOP
IF NOT DEFINED list GOTO :COLLECT_LIB_DIRS_LOOP_DONE
:: Split off the first path (%%A) and keep the rest (%%B)
FOR /F "tokens=1,* delims=;" %%A IN ("!list!") DO (
    :: lib_dirs contain paths that are not quoted
    SET "clean_path=%%A"

    :: Remove trailing backslash if present
    IF "!clean_path:~-1!"=="\" SET "clean_path=!clean_path:~0,-1!"

    :: Quote the path properly
    SET "quoted_path="!clean_path!""

    IF /I "!compiler!"=="clang-cl" (
       REM Append  MSVC-style prefix(/LIBPATH:)
       SET "lib_dirs_prefixed=!lib_dirs_prefixed! /LIBPATH:!quoted_path!"
    ) ELSE (
       :: Append GNU-style prefix(-L)
       SET "lib_dirs_prefixed=!lib_dirs_prefixed! -L!quoted_path!"
    )

    :: Prepare the remaining part for next iteration
    SET "list=%%B"
)
GOTO :COLLECT_LIB_DIRS_LOOP
:COLLECT_LIB_DIRS_LOOP_DONE

REM Collect the libs
SET "list=!libs!"
SET "libs_prefixed="
:COLLECT_LIBS_LOOP
IF NOT DEFINED list GOTO :COLLECT_LIBS_LOOP_DONE
:: Split off the first path (%%A) and keep the rest (%%B)
FOR /F "tokens=1,* delims=;" %%A IN ("!list!") DO (
    :: libs contain values that are not quoted
    SET "clean_lib=%%A"

    IF /I "!compiler!"=="clang-cl" (
       REM Append  MSVC-style postfix(.lib)
       SET "libs_prefixed=!libs_prefixed! !clean_lib!.lib"
    ) ELSE (
       :: Append GNU-style prefix(-L)
       SET "libs_prefixed=!libs_prefixed! -l!clean_lib!"
    )

    :: Prepare the remaining part for next iteration
    SET "list=%%B"
)
GOTO :COLLECT_LIBS_LOOP
:COLLECT_LIBS_LOOP_DONE

REM Collect the compiler flags (replace ; with a space)
SET "list=!compiler_flags!"
SET "compiler_flags_parsed="
:COLLECT_COMPILER_FLAGS_LOOP
IF NOT DEFINED list GOTO :COLLECT_COMPILER_FLAGS_LOOP_DONE
:: Split off the first path (%%A) and keep the rest (%%B)
FOR /F "tokens=1,* delims=;" %%A IN ("!list!") DO (
    :: compiler flags contain values that are not quoted
    SET "clean_compiler_flag=%%A"

    :: Append to the final argument list
    SET "compiler_flags_parsed=!compiler_flags_parsed! !clean_compiler_flag!"

    :: Prepare the remaining part for next iteration
    SET "list=%%B"
)
GOTO :COLLECT_COMPILER_FLAGS_LOOP
:COLLECT_COMPILER_FLAGS_LOOP_DONE

REM Collect the linker flags (replace ; with a space)
SET "list=!linker_flags!"
SET "linker_flags_parsed="
:COLLECT_LINKER_FLAGS_LOOP
IF NOT DEFINED list GOTO :COLLECT_LINKER_FLAGS_LOOP_DONE
:: Split off the first path (%%A) and keep the rest (%%B)
FOR /F "tokens=1,* delims=;" %%A IN ("!list!") DO (
    :: linker flags contain values that are not quoted
    SET "clean_linker_flag=%%A"

    :: Append to the final argument list
    SET "linker_flags_parsed=!linker_flags_parsed! !clean_linker_flag!"

    :: Prepare the remaining part for next iteration
    SET "list=%%B"
)
GOTO :COLLECT_LINKER_FLAGS_LOOP
:COLLECT_LINKER_FLAGS_LOOP_DONE

REM Remove leading spaces
IF DEFINED include_dirs_prefixed SET "include_dirs_prefixed=!include_dirs_prefixed:~1!"
IF DEFINED lib_dirs_prefixed SET "lib_dirs_prefixed=!lib_dirs_prefixed:~1!"
IF DEFINED libs_prefixed SET "libs_prefixed=!libs_prefixed:~1!"
IF DEFINED compiler_flags_parsed SET "compiler_flags_parsed=!compiler_flags_parsed:~1!"
IF DEFINED linker_flags_parsed SET "linker_flags_parsed=!linker_flags_parsed:~1!"

REM === Compile sources (incremental) ===
FOR %%F IN (!src_files!) DO (
    SET "src=%%F"
    SET "obj=%intermediate_dir%%%~nF.obj"
    SET "needs_compile=1"
    
    CALL :STRIP_QUOTES_VAR src
    CALL :STRIP_QUOTES_VAR obj

    IF EXIST "!obj!" (
	XCOPY /L /D /Y /Q "!src!" "!obj!" | FINDSTR /B /C:"0 " >NUL && SET "needs_compile=0"
    )

    IF "!needs_compile!"=="1" (
        CALL :LOG INFO "Compiling: !src!"

        IF /I "!compiler!"=="clang-cl" (
            !compiler! !compiler_flags_parsed! !include_dirs_prefixed! /c "!src!" /Fo"!obj!"
        ) ELSE (
            !compiler! !compiler_flags_parsed! !include_dirs_prefixed! -c "!src!" -o "!obj!"
        )

        IF ERRORLEVEL 1 (
            CALL :LOG ERROR "Compilation failed for: !src!"
            GOTO :EOF
        )
    ) ELSE (
        CALL :LOG INFO "Skipping (up-to-date): !src!"
    )
)

REM === Link object files ===
CALL :LOG INFO "Linking executable: %output_name%"

:: Collect .obj files
SET "obj_files=%intermediate_dir%*.obj"

IF /I "!compiler!"=="clang-cl" (	
    !compiler! ^
        /Fe"%build_dir%%output_name%" ^
	"%obj_files%" ^
	/link !linker_flags_parsed! !lib_dirs_prefixed! !libs_prefixed! ^
	2>> "%log_dir%%fbs_log_file_name%"
) ELSE (
    !compiler! ^
        -o "%build_dir%%output_name%" ^
	"%obj_files%" ^
	!linker_flags_parsed! !lib_dirs_prefixed! !libs_prefixed! ^
	2>> "%log_dir%%fbs_log_file_name%"
)

IF ERRORLEVEL 1 (
    CALL :LOG ERROR "Linking failed! See "%log_dir%%fbs_log_file_name%" for details"
    GOTO :EOF
) ELSE (
    CALL :LOG SUCCESS "Build succeeded: "%build_dir%%output_name%""
)

ENDLOCAL
EXIT /B 0


:SETOR
:: Set target = cmd var > conf var > default var
:: %1 = target
:: %2 = cmd var
:: %3 = conf var
:: %4 = default var
IF DEFINED %2 (
    SET "%~1=!%~2!"
    GOTO :EOF
)
IF DEFINED %3 (
    SET "%~1=!%~3!"
    GOTO :EOF
)
SET "%~1=!%~4!"
GOTO :EOF


:MAKETIMESTAMP
:: Make a time stamp suitable for file names
SET "d=%DATE%"
SET "t=%TIME%"

:: List of characters to replace (must be quoted and safe)
FOR %%s IN ("/" "\" "|" "-" "." "," ":" " " "%%" "&" "[" "]" "(" ")") DO (
    SET "d=!d:%%~s=_!"
    SET "t=!t:%%~s=_!"
)

:: Remove AM/PM
FOR %%a IN (" AM" " PM" " am" " pm") DO (
    SET "t=!t:%%~a=!"
)

:: Combine with underscore
SET "%~1=%d%_%t%"
GOTO :EOF

:IS_SUBDIR
SET "child=%~f1"
SET "parent=%~f2"
SET "result=NO"

:: Normalize paths (remove trailing slashes)
IF "%child:~-1%"=="\" SET "child=%child:~0,-1%"
IF "%parent:~-1%"=="\" SET "parent=%parent:~0,-1%"

CALL SET "parent_uppercased=%%parent%%"
CALL SET "child_uppercased=%%child%%"

ECHO %child_uppercased% | FINDSTR /I /B /C:"%parent_uppercased%" >NUL
IF NOT ERRORLEVEL 1 SET "result=YES"

SET "%~3=%result%"
GOTO :EOF


:READ_KEY_VAL_PAIRS_FROM_FILE
:: Parameters:
:: %1 = file path
:: %2 = processing label(e.g. PROCESS_CONFIG_KEY_VAL or PROCESS_CLEAN_KEY_VAL)
SET "fpath=%~f1"
SET "processor=%~2"

IF NOT EXIST "%fpath%" (
    ECHO Not found: %fpath%
    EXIT /B 1
)

IF "%processor%"=="" (
    ECHO Error: No processing label specified.
    EXIT /B 1
)

FOR /F "usebackq tokens=1* delims=:" %%A IN ("%fpath%") DO (
    SET "key=%%A"
    SET "value=%%B"
    CALL :%processor% key value
)
GOTO :EOF

:PROCESS_CONF_KEY_VAL
IF NOT DEFINED value (
    REM Skip lines without value  do nothing
) ELSE (
    REM Trim key
    FOR /F "tokens=*" %%K IN ("!key!") DO SET "key=%%K"

    REM Trim value
    FOR /F "tokens=*" %%V IN ("!value!") DO SET "value=%%V"

    REM Remove surrounding quotes from value
    IF "!value:~0,1!"=="""" SET "value=!value:~1,-1!"

    REM Safe assignment
    ENDLOCAL
    SET "conf_!key!=!value!"
    SETLOCAL EnableDelayedExpansion
)
GOTO :EOF

:PROCESS_CLEAN_KEY_VAL
IF NOT DEFINED value (
    REM Skip lines without value  do nothing
) ELSE (
    REM Trim key
    FOR /F "tokens=*" %%K IN ("!key!") DO SET "key=%%K"

    REM Trim value
    FOR /F "tokens=*" %%V IN ("!value!") DO SET "value=%%V"

    REM Remove surrounding quotes from value
    IF "!value:~0,1!"=="""" SET "value=!value:~1,-1!"

    REM Safe assignment
    ENDLOCAL
    SET "!key!=!value!"
    SETLOCAL EnableDelayedExpansion
)
GOTO :EOF

:STRIP_QUOTES_VAR
:: %1 = variable name to strip surrounding quotes from (in place)
IF NOT DEFINED %~1 GOTO :EOF
SET "tmp=!%~1!"

:: Remove quotes by replacing them with nothing first (handles embedded quotes too)
SET "tmp=%tmp:"=%"

:: Then remove leading/trailing quote if present (in case of only surrounding quotes)
IF "!tmp:~0,1!"=="""" SET "tmp=!tmp:~1!"
IF "!tmp:~-1!"=="""" SET "tmp=!tmp:~0,-1!"

SET "%~1=!tmp!"
SET "tmp="
GOTO :EOF
