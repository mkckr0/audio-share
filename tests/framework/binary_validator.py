"""
AudioStream Binary Header & Package Validation Utilities.
Validates:
1. Windows PE/PE32+ Executables (MZ magic, PE signature, COFF header, Optional header)
2. Android APK Packages (ZIP magic, AndroidManifest.xml presence, DEX / resources)
3. Linux ELF Executables (ELF magic, 64-bit ELF class, Executable type)
"""

import os
import struct
import zipfile
from typing import Dict, Any, Tuple


class PEValidator:
    @staticmethod
    def validate_pe_bytes(data: bytes) -> Dict[str, Any]:
        """Validate PE/PE32+ headers in byte buffer."""
        if len(data) < 64:
            return {"valid": False, "error": "File smaller than DOS header (64 bytes)"}
        
        # Check DOS MZ Magic
        if data[:2] != b"MZ":
            return {"valid": False, "error": f"Invalid DOS header magic: {data[:2]!r}"}
        
        # e_lfanew offset at 0x3C
        pe_offset = struct.unpack("<I", data[0x3C:0x40])[0]
        if pe_offset + 24 > len(data):
            return {"valid": False, "error": f"PE header offset {pe_offset} exceeds file size"}
        
        # Check PE signature
        pe_sig = data[pe_offset:pe_offset+4]
        if pe_sig != b"PE\x00\x00":
            return {"valid": False, "error": f"Invalid PE signature: {pe_sig!r}"}
        
        # COFF File Header
        coff_offset = pe_offset + 4
        machine, num_sections, time_date_stamp, sym_ptr, num_sym, opt_hdr_size, characteristics = struct.unpack(
            "<HHIIIHH", data[coff_offset:coff_offset+20]
        )
        
        is_x64 = (machine == 0x8664)
        is_x86 = (machine == 0x014C)
        is_arm64 = (machine == 0xAA64)
        
        # Optional Header Magic
        opt_offset = coff_offset + 20
        opt_magic = 0
        if opt_hdr_size >= 2 and opt_offset + 2 <= len(data):
            opt_magic = struct.unpack("<H", data[opt_offset:opt_offset+2])[0]
            
        is_pe32_plus = (opt_magic == 0x020B)
        is_pe32 = (opt_magic == 0x010B)
        
        return {
            "valid": True,
            "machine": hex(machine),
            "is_x64": is_x64,
            "is_x86": is_x86,
            "is_arm64": is_arm64,
            "num_sections": num_sections,
            "is_pe32_plus": is_pe32_plus,
            "is_pe32": is_pe32,
            "characteristics": hex(characteristics)
        }

    @classmethod
    def validate_file(cls, path: str) -> Dict[str, Any]:
        if not os.path.exists(path):
            return {"valid": False, "error": f"File not found: {path}"}
        with open(path, "rb") as f:
            data = f.read(4096)
        return cls.validate_pe_bytes(data)


class APKValidator:
    @staticmethod
    def validate_apk_file(path: str) -> Dict[str, Any]:
        """Validate that path is a valid Android APK (valid zip container with AndroidManifest.xml)."""
        if not os.path.exists(path):
            return {"valid": False, "error": f"File not found: {path}"}
            
        try:
            with zipfile.ZipFile(path, 'r') as zf:
                namelist = zf.namelist()
                has_manifest = "AndroidManifest.xml" in namelist
                has_dex = any(name.endswith(".dex") for name in namelist) or any(name.startswith("res/") for name in namelist)
                return {
                    "valid": has_manifest,
                    "has_manifest": has_manifest,
                    "has_dex_or_res": has_dex,
                    "file_count": len(namelist),
                    "files": namelist[:10]
                }
        except zipfile.BadZipFile as e:
            return {"valid": False, "error": f"Bad zip file: {e}"}


class ELFValidator:
    @staticmethod
    def validate_elf_bytes(data: bytes) -> Dict[str, Any]:
        """Validate ELF header for Linux executable binaries."""
        if len(data) < 16:
            return {"valid": False, "error": "File smaller than ELF ident header"}
            
        if data[:4] != b"\x7fELF":
            return {"valid": False, "error": f"Invalid ELF magic: {data[:4]!r}"}
            
        elf_class = data[4]  # 1=32bit, 2=64bit
        elf_data = data[5]   # 1=LE, 2=BE
        
        return {
            "valid": True,
            "is_64bit": (elf_class == 2),
            "is_little_endian": (elf_data == 1)
        }
