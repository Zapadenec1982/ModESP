#!/usr/bin/env python3
"""
Validation module for manifest consistency checking
"""

from typing import Dict, List, Set, Tuple
from dataclasses import dataclass
import logging

@dataclass
class ValidationIssue:
    """Represents a validation issue found in manifests"""
    severity: str  # ERROR, WARNING, INFO
    category: str  # EVENT, STATE, API
    message: str
    source_module: str = ""
    target: str = ""

class ManifestValidator:
    """Validates consistency across all module manifests"""
    
    def __init__(self):
        self.logger = logging.getLogger("ManifestValidator")
        self.issues: List[ValidationIssue] = []
        
        # Collections for validation
        self.event_publishers: Dict[str, List[str]] = {}  # event -> [modules]
        self.event_subscribers: Dict[str, List[str]] = {}  # event -> [modules]
        self.state_publishers: Dict[str, List[str]] = {}  # state -> [modules]
        self.state_subscribers: Dict[str, List[str]] = {}  # state -> [modules]
        self.api_providers: Dict[str, str] = {}  # api_method -> module
        self.api_consumers: Set[str] = set()  # api methods used in code
    
    def collect_manifest_data(self, module_name: str, manifest: dict):
        """Collect all publish/subscribe information from a manifest"""
        
        # Collect event bus data
        if 'event_bus' in manifest:
            # Publishers
            if 'publishes' in manifest['event_bus']:
                for event_name in manifest['event_bus']['publishes']:
                    if event_name not in self.event_publishers:
                        self.event_publishers[event_name] = []
                    self.event_publishers[event_name].append(module_name)
            
            # Subscribers
            if 'subscribes' in manifest['event_bus']:
                for event_name in manifest['event_bus']['subscribes']:
                    if event_name not in self.event_subscribers:
                        self.event_subscribers[event_name] = []
                    self.event_subscribers[event_name].append(module_name)
        
        # Collect shared state data
        if 'shared_state' in manifest:
            # Publishers (writers)
            if 'publishes' in manifest['shared_state']:
                for state_key in manifest['shared_state']['publishes']:
                    if state_key not in self.state_publishers:
                        self.state_publishers[state_key] = []
                    self.state_publishers[state_key].append(module_name)
            
            # Subscribers (readers)
            if 'subscribes' in manifest['shared_state']:
                for state_key in manifest['shared_state']['subscribes']:
                    if state_key not in self.state_subscribers:
                        self.state_subscribers[state_key] = []
                    self.state_subscribers[state_key].append(module_name)
        
        # Collect API data
        if 'apis' in manifest:
            for api_type in ['static', 'dynamic']:
                if api_type in manifest['apis']:
                    apis = manifest['apis'][api_type]
                    if isinstance(apis, list):
                        for api in apis:
                            method = api.get('method', '')
                            if method:
                                self.api_providers[method] = module_name
    
    def validate_events(self):
        """Validate event publishers and subscribers"""
        
        # Find orphan events (published but no subscribers)
        for event, publishers in self.event_publishers.items():
            if event not in self.event_subscribers or not self.event_subscribers[event]:
                self.issues.append(ValidationIssue(
                    severity="WARNING",
                    category="EVENT",
                    message=f"Event '{event}' is published but has no subscribers",
                    source_module=", ".join(publishers),
                    target=event
                ))
        
        # Find missing events (subscribed but no publishers)
        for event, subscribers in self.event_subscribers.items():
            if event not in self.event_publishers or not self.event_publishers[event]:
                self.issues.append(ValidationIssue(
                    severity="ERROR",
                    category="EVENT",
                    message=f"Event '{event}' has subscribers but no publisher",
                    source_module=", ".join(subscribers),
                    target=event
                ))
        
        # Check for multiple publishers of same event (might be intentional)
        for event, publishers in self.event_publishers.items():
            if len(publishers) > 1:
                self.issues.append(ValidationIssue(
                    severity="INFO",
                    category="EVENT",
                    message=f"Event '{event}' is published by multiple modules",
                    source_module=", ".join(publishers),
                    target=event
                ))
    
    def validate_shared_state(self):
        """Validate SharedState publishers and subscribers"""
        
        # Find orphan states (written but never read)
        for state, publishers in self.state_publishers.items():
            if state not in self.state_subscribers or not self.state_subscribers[state]:
                self.issues.append(ValidationIssue(
                    severity="WARNING",
                    category="STATE",
                    message=f"State '{state}' is written but never read",
                    source_module=", ".join(publishers),
                    target=state
                ))
        
        # Find missing states (read but never written)
        for state, subscribers in self.state_subscribers.items():
            if state not in self.state_publishers or not self.state_publishers[state]:
                self.issues.append(ValidationIssue(
                    severity="ERROR",
                    category="STATE",
                    message=f"State '{state}' is read but never written",
                    source_module=", ".join(subscribers),
                    target=state
                ))
        
        # Check for multiple writers of same state (potential conflict)
        for state, publishers in self.state_publishers.items():
            if len(publishers) > 1:
                self.issues.append(ValidationIssue(
                    severity="WARNING",
                    category="STATE",
                    message=f"State '{state}' is written by multiple modules - potential conflict",
                    source_module=", ".join(publishers),
                    target=state
                ))
    
    def validate_dependencies(self, modules: List[dict]):
        """Validate module dependencies"""
        
        module_names = {m['module']['name'] for m in modules}
        
        for module in modules:
            module_name = module['module']['name']
            dependencies = module['module'].get('dependencies', [])
            
            for dep in dependencies:
                # Skip system dependencies
                if dep in ['ESPhal', 'SharedState', 'EventBus', 'ConfigManager']:
                    continue
                
                if dep not in module_names:
                    self.issues.append(ValidationIssue(
                        severity="ERROR",
                        category="DEPENDENCY",
                        message=f"Module depends on non-existent module '{dep}'",
                        source_module=module_name,
                        target=dep
                    ))
    
    def validate_all(self, modules: List[dict]) -> Tuple[bool, List[ValidationIssue]]:
        """Run all validations and return results"""
        
        self.issues.clear()
        
        # Collect data from all modules
        for module in modules:
            module_name = module['module']['name']
            self.collect_manifest_data(module_name, module)
        
        # Run validations
        self.validate_events()
        self.validate_shared_state()
        self.validate_dependencies(modules)
        
        # Sort issues by severity
        severity_order = {"ERROR": 0, "WARNING": 1, "INFO": 2}
        self.issues.sort(key=lambda x: severity_order.get(x.severity, 999))
        
        # Check if build should fail
        has_errors = any(issue.severity == "ERROR" for issue in self.issues)
        
        return not has_errors, self.issues
    
    def generate_validation_report(self) -> str:
        """Generate a markdown report of validation results"""
        
        if not self.issues:
            return "# ✅ Validation Passed\n\nNo issues found!"
        
        report = ["# 🔍 Manifest Validation Report", ""]
        
        # Summary
        errors = sum(1 for i in self.issues if i.severity == "ERROR")
        warnings = sum(1 for i in self.issues if i.severity == "WARNING")
        infos = sum(1 for i in self.issues if i.severity == "INFO")
        
        report.append("## Summary")
        report.append(f"- 🔴 Errors: {errors}")
        report.append(f"- 🟡 Warnings: {warnings}")
        report.append(f"- 🔵 Info: {infos}")
        report.append("")
        
        # Group by category
        categories = {}
        for issue in self.issues:
            if issue.category not in categories:
                categories[issue.category] = []
            categories[issue.category].append(issue)
        
        # Report by category
        for category, issues in categories.items():
            report.append(f"## {category} Issues")
            report.append("")
            
            for issue in issues:
                icon = {"ERROR": "🔴", "WARNING": "🟡", "INFO": "🔵"}.get(issue.severity, "❓")
                report.append(f"### {icon} {issue.message}")
                if issue.source_module:
                    report.append(f"- **Module(s)**: {issue.source_module}")
                if issue.target:
                    report.append(f"- **Target**: `{issue.target}`")
                report.append("")
        
        # Recommendations
        if errors > 0:
            report.append("## ❌ Build Failed")
            report.append("Fix all ERROR level issues before proceeding.")
        elif warnings > 0:
            report.append("## ⚠️ Build Warning")
            report.append("Consider fixing WARNING level issues.")
        
        return "\n".join(report)
