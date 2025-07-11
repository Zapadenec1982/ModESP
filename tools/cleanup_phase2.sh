#!/bin/bash
# cleanup_phase2.sh - Script to clean up Phase 2 artifacts

echo "=== Cleaning up Phase 2 UI Generation ==="

# 1. Remove old generated UI files
echo "Removing old generated UI files..."
rm -f main/generated/generated_ui_schemas.h
rm -f main/generated/lcd_menu_generated.h
rm -f main/generated/web_ui_generated.h
rm -f main/generated/ui_registry_generated.h

# 2. Archive old UI documentation
echo "Archiving old UI documentation..."
mkdir -p Docs/_archive/phase2_ui
mv Docs/_archive/ui_api_deprecated/* Docs/_archive/phase2_ui/ 2>/dev/null
mv Docs/05_UI_API/refactoring_notes.md Docs/_archive/phase2_ui/ 2>/dev/null

# 3. Clean up old UI schema references in manifests
echo "Cleaning will be done manually in manifests..."

# 4. List files that need manual review
echo ""
echo "=== Files that need manual review ==="
echo "1. All module_manifest.json files - remove 'ui_schema' sections"
echo "2. process_manifests.py - remove old UI generation code"
echo "3. Module implementations - update to use adaptive UI"

echo ""
echo "=== Phase 2 cleanup complete ==="
echo "Now you can focus on Phase 5 implementation!"
