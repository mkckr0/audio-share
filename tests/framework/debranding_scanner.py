"""
AudioStream Repository Debranding & Asset Pruning Scanner.
Scans source files, manifests, and build scripts for:
1. Upstream author identifiers (mkckr0)
2. Upstream repository URLs (github.com/mkckr0/audio-share, CapJack-cloud/audio-share)
3. Upstream package names (io.github.mkckr0.audio_share_app)
4. Legacy MFC project files (server-mfc/)
"""

import os
import re
from typing import List, Dict, Any


PROHIBITED_PATTERNS = [
    re.compile(r"mkckr0", re.IGNORECASE),
    re.compile(r"CapJack-cloud/audio-share", re.IGNORECASE),
    re.compile(r"github\.com/mkckr0", re.IGNORECASE),
    re.compile(r"io\.github\.mkckr0", re.IGNORECASE),
]

IGNORED_DIRS = {
    ".git", ".github", ".gradle", "build", "dist", ".idea",
    "tests", ".agents", ".gemini", "node_modules", "bin", "obj"
}

TEXT_EXTENSIONS = {
    ".cpp", ".hpp", ".c", ".h", ".kt", ".java", ".xml", ".gradle",
    ".kts", ".proto", ".cmake", ".txt", ".json", ".md", ".in"
}


class DebrandingScanResult:
    def __init__(self):
        self.scanned_files_count = 0
        self.violations: List[Dict[str, Any]] = []
        self.legacy_mfc_present = False
        self.legacy_mfc_files: List[str] = []

    @property
    def is_clean(self) -> bool:
        return len(self.violations) == 0 and not self.legacy_mfc_present


class DebrandingScanner:
    def __init__(self, root_dir: str):
        self.root_dir = root_dir

    def scan(self) -> DebrandingScanResult:
        result = DebrandingScanResult()
        
        # Check for legacy MFC directory
        mfc_path = os.path.join(self.root_dir, "server-mfc")
        if os.path.exists(mfc_path):
            result.legacy_mfc_present = True
            for root, _, files in os.walk(mfc_path):
                for f in files:
                    result.legacy_mfc_files.append(os.path.relpath(os.path.join(root, f), self.root_dir))

        # Text scan across repository
        for root, dirs, files in os.walk(self.root_dir):
            # Prune ignored directories
            dirs[:] = [d for d in dirs if d not in IGNORED_DIRS]
            
            for file in files:
                ext = os.path.splitext(file)[1].lower()
                # Skip binary and non-text files
                if ext not in TEXT_EXTENSIONS and file not in {"CMakeLists.txt", "Dockerfile"}:
                    continue
                    
                file_path = os.path.join(root, file)
                rel_path = os.path.relpath(file_path, self.root_dir)
                
                # Skip files inside server-mfc (already flagged as legacy folder)
                if rel_path.startswith("server-mfc"):
                    continue
                # Skip plan file if it's the specification document
                if rel_path == "plan" or rel_path == "ORIGINAL_REQUEST.md":
                    continue
                    
                result.scanned_files_count += 1
                try:
                    with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                        for line_no, line in enumerate(f, 1):
                            for pattern in PROHIBITED_PATTERNS:
                                match = pattern.search(line)
                                if match:
                                    result.violations.append({
                                        "file": rel_path,
                                        "line": line_no,
                                        "match": match.group(0),
                                        "snippet": line.strip()
                                    })
                except Exception:
                    pass

        return result
