#!/bin/bash
# Archive deprecated UI/API documentation

echo "Archiving deprecated UI/API documentation..."

# Create archive directory if not exists
mkdir -p Docs/_archive/ui_api_deprecated

# List of files to archive
FILES_TO_ARCHIVE=(
    "API_UI_ARCHITECTURE_ANALYSIS.md"
    "UI_EXTENSIBILITY_ARCHITECTURE.md"
    "UI_COMPILE_TIME_GENERATION.md"
    "UI_RESOURCE_COMPARISON.md"
    "EXAMPLE_NEW_MODULE_WITH_AUTO_UI.md"
    "web_ui_module_optimized_example.cpp"
)

# Move files to archive
for file in "${FILES_TO_ARCHIVE[@]}"; do
    if [ -f "Docs/$file" ]; then
        echo "Archiving: $file"
        mv "Docs/$file" "Docs/_archive/ui_api_deprecated/"
    else
        echo "File not found: $file"
    fi
done

echo "Archive complete!"
echo ""
echo "Active UI/API documentation:"
echo "- Docs/UI_API_INDEX.md (start here)"
echo "- Docs/UI_API_SYSTEM.md"  
echo "- Docs/UI_API_QUICK_START.md"
echo "- Docs/UI_API_TECHNICAL_REFERENCE.md"
echo "- Docs/API_CONTRACT.md"