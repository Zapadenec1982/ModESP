#!/bin/bash
# test_phase5.sh - Quick test script for Phase 5

echo "=== Phase 5 Adaptive UI Test Script ==="
cd /c/ModESP_dev

# Step 1: Update process_manifests.py
echo "Step 1: Integrating adaptive UI generator..."
if ! grep -q "adaptive_ui_generator" tools/process_manifests.py; then
    echo "Adding import to process_manifests.py..."
    # Add import at the beginning
    sed -i '1s/^/from adaptive_ui_generator import generate_adaptive_ui\n/' tools/process_manifests.py
    
    # Add generation call in generate_code method
    # This is simplified - you may need to do this manually
fi

# Step 2: Run manifest processor
echo "Step 2: Running manifest processor..."
python tools/process_manifests.py --project-root . --output-dir main/generated

# Step 3: Check generated files
echo "Step 3: Checking generated files..."
if [ -f "main/generated/generated_ui_components.h" ]; then
    echo "✓ generated_ui_components.h created"
    head -20 main/generated/generated_ui_components.h
else
    echo "✗ generated_ui_components.h not found"
fi

# Step 4: Build the project
echo "Step 4: Building project..."
idf.py build

# Step 5: Show build results
if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
else
    echo "✗ Build failed"
    echo "Check the error messages above"
fi

echo "=== Test complete ==="
