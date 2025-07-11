/**
 * @file test_hybrid_api.h
 * @brief Test functions for Hybrid API System
 * 
 * Part of TODO-006: Hybrid API Contract Implementation
 * Phase 5: Integration and Testing
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Test function for hybrid API system
 * 
 * Tests basic functionality of the hybrid API system including:
 * - Static API registration
 * - Dynamic API building  
 * - Configuration management
 * - Sample API calls
 */
void test_hybrid_api_system();

/**
 * @brief Test configuration change workflow
 * 
 * Tests the configuration change and restart pattern workflow.
 */
void test_configuration_workflow();

#ifdef __cplusplus
}
#endif

