#!/usr/bin/env python3
"""
AudioStream E2E Automated Test Suite Master Runner.
Executes all 4 Tiers of E2E verification:
- Tier 1: Feature Coverage (Protobuf schemas, binary framing, Opus layout, jitter buffer, virtual mic, Android services, PE/APK headers)
- Tier 2: Boundary & Corner Cases (16-bit sequence wrap-around, out-of-order reordering, PLC gap concealment, port/IP bounds, 0-byte frames, MTU clamping)
- Tier 3: Cross-Feature Combinations (Dynamic codec switching, bidirectional concurrent streaming, USB tether disconnect/reconnect, multi-peer heartbeats)
- Tier 4: Real-World Scenarios (High-load gaming stream with jitter/loss, Discord on-demand mic activation/debounce, multi-server LAN discovery)

Usage:
    python3 tests/e2e_runner.py [--tier {1,2,3,4,all}] [--verbose] [--json]
"""

import sys
import os
import time
import argparse
import unittest
import json
from typing import List, Dict, Any

# Ensure tests package is in sys.path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)
if PARENT_DIR not in sys.path:
    sys.path.insert(0, PARENT_DIR)

# Import test suites
try:
    from test_cases.test_tier1_features import TestTier1Features
    from test_cases.test_tier2_boundaries import TestTier2Boundaries
    from test_cases.test_tier3_combinations import TestTier3Combinations
    from test_cases.test_tier4_scenarios import TestTier4Scenarios
except ImportError:
    # Handle relative imports if invoked from tests package
    from .test_cases.test_tier1_features import TestTier1Features
    from .test_cases.test_tier2_boundaries import TestTier2Boundaries
    from .test_cases.test_tier3_combinations import TestTier3Combinations
    from .test_cases.test_tier4_scenarios import TestTier4Scenarios


TIER_MAP = {
    1: ("Tier 1: Feature Coverage", TestTier1Features),
    2: ("Tier 2: Boundary & Corner Cases", TestTier2Boundaries),
    3: ("Tier 3: Cross-Feature Combinations", TestTier3Combinations),
    4: ("Tier 4: Real-World Application Scenarios", TestTier4Scenarios),
}


class CustomTestResult(unittest.TestResult):
    def __init__(self):
        super().__init__()
        self.test_records: List[Dict[str, Any]] = []
        self._start_time = 0.0

    def startTest(self, test):
        super().startTest(test)
        self._start_time = time.time()

    def addSuccess(self, test):
        super().addSuccess(test)
        elapsed_ms = (time.time() - self._start_time) * 1000.0
        doc = test._testMethodDoc or test._testMethodName
        self.test_records.append({
            "test_id": test.id(),
            "name": test._testMethodName,
            "doc": doc.strip() if doc else test._testMethodName,
            "status": "PASS",
            "duration_ms": round(elapsed_ms, 2),
            "error": None
        })

    def addFailure(self, test, err):
        super().addFailure(test, err)
        elapsed_ms = (time.time() - self._start_time) * 1000.0
        doc = test._testMethodDoc or test._testMethodName
        self.test_records.append({
            "test_id": test.id(),
            "name": test._testMethodName,
            "doc": doc.strip() if doc else test._testMethodName,
            "status": "FAIL",
            "duration_ms": round(elapsed_ms, 2),
            "error": self._exc_info_to_string(err, test)
        })

    def addError(self, test, err):
        super().addError(test, err)
        elapsed_ms = (time.time() - self._start_time) * 1000.0
        doc = test._testMethodDoc or test._testMethodName
        self.test_records.append({
            "test_id": test.id(),
            "name": test._testMethodName,
            "doc": doc.strip() if doc else test._testMethodName,
            "status": "ERROR",
            "duration_ms": round(elapsed_ms, 2),
            "error": self._exc_info_to_string(err, test)
        })


def run_e2e_tests(selected_tiers: List[int], verbose: bool = False, output_json: bool = False) -> int:
    start_total_time = time.time()
    all_results = []
    overall_passed = True

    if not output_json:
        print("=" * 80)
        print(" " * 22 + "AudioStream E2E Automated Test Suite")
        print("=" * 80)
        print(f"Target: AudioStream Bidirectional Audio & Codec Engine")
        print(f"Executing Tiers: {selected_tiers}")
        print("-" * 80)

    total_tests = 0
    total_passed = 0
    total_failed = 0
    total_errors = 0

    for tier_num in sorted(selected_tiers):
        if tier_num not in TIER_MAP:
            continue

        tier_title, test_class = TIER_MAP[tier_num]
        suite = unittest.TestLoader().loadTestsFromTestCase(test_class)
        result = CustomTestResult()
        
        suite_start = time.time()
        suite.run(result)
        suite_duration_ms = (time.time() - suite_start) * 1000.0

        tier_pass = len(result.failures) == 0 and len(result.errors) == 0
        if not tier_pass:
            overall_passed = False

        total_tests += result.testsRun
        total_passed += (result.testsRun - len(result.failures) - len(result.errors))
        total_failed += len(result.failures)
        total_errors += len(result.errors)

        if not output_json:
            print(f"\n[{tier_title}] ({result.testsRun} tests)")
            print("-" * 80)
            print(f"{'Status':<8} | {'Duration':<10} | {'Test Description'}")
            print("-" * 80)
            for record in result.test_records:
                status_str = f"\033[92m{record['status']}\033[0m" if record['status'] == "PASS" else f"\033[91m{record['status']}\033[0m"
                dur_str = f"{record['duration_ms']:.2f}ms"
                print(f"{status_str:<17} | {dur_str:<10} | {record['doc']}")
                if verbose and record['error']:
                    print(f"\n    Error Details:\n{record['error']}")

        all_results.append({
            "tier": tier_num,
            "title": tier_title,
            "tests_run": result.testsRun,
            "passed": result.testsRun - len(result.failures) - len(result.errors),
            "failed": len(result.failures),
            "errors": len(result.errors),
            "duration_ms": round(suite_duration_ms, 2),
            "tests": result.test_records
        })

    total_duration_sec = time.time() - start_total_time

    if output_json:
        report = {
            "summary": {
                "total_tests": total_tests,
                "passed": total_passed,
                "failed": total_failed,
                "errors": total_errors,
                "duration_seconds": round(total_duration_sec, 3),
                "success": overall_passed
            },
            "tiers": all_results
        }
        print(json.dumps(report, indent=2))
    else:
        print("\n" + "=" * 80)
        print(" " * 28 + "E2E TEST SUMMARY")
        print("=" * 80)
        print(f"Total Tests Executed : {total_tests}")
        print(f"Passed               : \033[92m{total_passed}\033[0m")
        print(f"Failed               : \033[91m{total_failed}\033[0m" if total_failed > 0 else f"Failed               : {total_failed}")
        print(f"Errors               : \033[91m{total_errors}\033[0m" if total_errors > 0 else f"Errors               : {total_errors}")
        print(f"Total Execution Time : {total_duration_sec:.3f}s")
        print("-" * 80)
        if overall_passed:
            print("\033[92mOVERALL RESULT: ALL TIERS PASSED (100% SUCCESS)\033[0m")
        else:
            print("\033[91mOVERALL RESULT: FAILURES DETECTED\033[0m")
        print("=" * 80)

    return 0 if overall_passed else 1


def main():
    parser = argparse.ArgumentParser(description="AudioStream E2E Automated Test Suite Runner")
    parser.add_argument("--tier", choices=["1", "2", "3", "4", "all"], default="all",
                        help="Select test tier to execute (default: all)")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose test failure output")
    parser.add_argument("--json", action="store_true", help="Output results in JSON format")
    args = parser.parse_args()

    if args.tier == "all":
        tiers = [1, 2, 3, 4]
    else:
        tiers = [int(args.tier)]

    exit_code = run_e2e_tests(selected_tiers=tiers, verbose=args.verbose, output_json=args.json)
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
