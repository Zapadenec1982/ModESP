@echo off
REM Archive deprecated UI/API documentation

echo Archiving deprecated UI/API documentation...

REM Create archive directory if not exists
if not exist "Docs\_archive\ui_api_deprecated" mkdir "Docs\_archive\ui_api_deprecated"

REM Archive files
set FILES=API_UI_ARCHITECTURE_ANALYSIS.md UI_EXTENSIBILITY_ARCHITECTURE.md UI_COMPILE_TIME_GENERATION.md UI_RESOURCE_COMPARISON.md EXAMPLE_NEW_MODULE_WITH_AUTO_UI.md web_ui_module_optimized_example.cpp

for %%f in (%FILES%) do (
    if exist "Docs\%%f" (
        echo Archiving: %%f
        move "Docs\%%f" "Docs\_archive\ui_api_deprecated\"
    ) else (
        echo File not found: %%f
    )
)

echo.
echo Archive complete!
echo.
echo Active UI/API documentation:
echo - Docs\UI_API_INDEX.md (start here)
echo - Docs\UI_API_SYSTEM.md
echo - Docs\UI_API_QUICK_START.md
echo - Docs\UI_API_TECHNICAL_REFERENCE.md
echo - Docs\API_CONTRACT.md
pause