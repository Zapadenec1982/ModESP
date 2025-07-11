#!/bin/bash
# Script to reorganize core component structure

echo "🔧 Reorganizing core component..."

# Create new structure
mkdir -p components/core/src/application
mkdir -p components/core/src/module_manager  
mkdir -p components/core/src/config_manager
mkdir -p components/core/src/event_bus
mkdir -p components/core/src/shared_state

# Move Application files
echo "📁 Moving Application system..."
mv components/core/application.cpp components/core/src/application/
mv components/core/application.h components/core/src/application/
mv components/core/application_wrapper.cpp components/core/src/application/

# Move Module Manager files
echo "📁 Moving Module Manager system..."
mv components/core/module_manager.cpp components/core/src/module_manager/
mv components/core/module_manager.h components/core/src/module_manager/
mv components/core/module_registry.cpp components/core/src/module_manager/
mv components/core/module_registry.h components/core/src/module_manager/
mv components/core/module_factory.cpp components/core/src/module_manager/
mv components/core/module_factory.h components/core/src/module_manager/
mv components/core/module_heartbeat.cpp components/core/src/module_manager/
mv components/core/module_heartbeat.h components/core/src/module_manager/

# Move Config Manager files
echo "📁 Moving Config Manager system..."
mv components/core/config_manager.cpp components/core/src/config_manager/
mv components/core/config_manager.h components/core/src/config_manager/
mv components/core/config_manager_async.cpp components/core/src/config_manager/
mv components/core/config_manager_async.h components/core/src/config_manager/

# Move Event Bus files
echo "📁 Moving Event Bus system..."
mv components/core/event_bus.cpp components/core/src/event_bus/
mv components/core/event_bus.h components/core/src/event_bus/
mv components/core/event_helpers.h components/core/src/event_bus/

# Move Shared State files
echo "📁 Moving Shared State system..."
mv components/core/shared_state.cpp components/core/src/shared_state/
mv components/core/shared_state.h components/core/src/shared_state/

# Create memory_pool component
echo "📦 Creating memory_pool component..."
mkdir -p components/memory_pool
mv components/core/memory_pool.cpp components/memory_pool/
mv components/core/memory_pool.h components/memory_pool/
mv components/core/memory_diagnostics.cpp components/memory_pool/
mv components/core/memory_diagnostics.h components/memory_pool/
mv components/core/pooled_event.h components/memory_pool/

# Create utils component
echo "📦 Creating core_utils component..."
mkdir -p components/core_utils
mv components/core/diagnostic_tools.cpp components/core_utils/
mv components/core/diagnostic_tools.h components/core_utils/
mv components/core/error_handling.h components/core_utils/

# Keep manifest_reader in core for now (used by module_manager)
echo "📁 Moving manifest_reader to module_manager..."
mv components/core/manifest_reader.cpp components/core/src/module_manager/
mv components/core/manifest_reader.h components/core/src/module_manager/

# Remove archive
echo "🗑️ Removing archive..."
rm -rf components/core/_archive_config

echo "✅ Reorganization complete!"
