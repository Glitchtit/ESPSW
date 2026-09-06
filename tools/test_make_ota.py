#!/usr/bin/env python3
"""Self-test for make_ota.py. Run: python tools/test_make_ota.py"""
import os, sys, struct, json
sys.path.insert(0, os.path.dirname(__file__))
import make_ota as mo

def test_parse_define():
    text = "#define ESPSW_FW_VERSION 0x00010000u\n#define ESPSW_MANUF_CODE 0x1037\n"
    assert mo.parse_define(text, "ESPSW_FW_VERSION") == 0x00010000
    assert mo.parse_define(text, "ESPSW_MANUF_CODE") == 0x1037

def test_build_and_parse_header():
    fw = b"\xAA" * 100
    blob = mo.build_ota(fw, manuf=0x1037, image_type=0x0010,
                        file_version=0x00010000, header_string=b"ESPSW-1CH")
    h = mo.parse_ota_header(blob)
    assert h["magic"] == 0x0BEEF11E
    assert h["manufacturer_code"] == 0x1037
    assert h["image_type"] == 0x0010
    assert h["file_version"] == 0x00010000
    assert h["header_length"] == 56
    assert h["total_size"] == 56 + 6 + len(fw)

def test_subelement_carries_firmware():
    fw = b"firmware-bytes-123"
    blob = mo.build_ota(fw, 0x1037, 0x0010, 0x00010000, b"ESPSW-1CH")
    tag, length = struct.unpack_from("<HI", blob, 56)
    assert tag == 0x0000
    assert length == len(fw)
    assert blob[62:62 + length] == fw

def test_index_entry(tmp_path="/tmp/claude-make-ota-test.ota"):
    fw = b"\x01\x02\x03" * 50
    blob = mo.build_ota(fw, 0x1037, 0x0010, 0x00010000, b"ESPSW-1CH")
    with open(tmp_path, "wb") as f:
        f.write(blob)
    entry = mo.index_entry(tmp_path, "https://example/espsw.ota",
                           manuf=0x1037, image_type=0x0010,
                           file_version=0x00010000, model_id="ESPSW-1CH")
    assert entry["fileVersion"] == 65536
    assert entry["manufacturerCode"] == 4151
    assert entry["imageType"] == 16
    assert entry["modelId"] == "ESPSW-1CH"
    assert entry["fileSize"] == len(blob)
    assert len(entry["sha512"]) == 128  # hex of 64 bytes
    os.remove(tmp_path)

def test_upsert_index(tmp_path="/tmp/claude-make-ota-index.json"):
    if os.path.exists(tmp_path):
        os.remove(tmp_path)
    def entry(image_type, ver):
        return {"fileVersion": ver, "fileSize": 1, "manufacturerCode": 0x1037,
                "imageType": image_type, "sha512": "x", "url": "u",
                "modelId": "ESPSW-1CH" if image_type == 16 else "OTHER"}
    # fresh index -> 1 entry
    idx = mo.upsert_index(tmp_path, entry(16, 65536))
    assert len(idx) == 1
    json.dump(idx, open(tmp_path, "w"))
    # different imageType -> added (2 entries), sorted by imageType
    idx = mo.upsert_index(tmp_path, entry(17, 65536))
    assert [e["imageType"] for e in idx] == [16, 17]
    json.dump(idx, open(tmp_path, "w"))
    # same imageType, newer version -> replaced in place, still 2 entries
    idx = mo.upsert_index(tmp_path, entry(16, 65537))
    assert len(idx) == 2
    main_entry = [e for e in idx if e["imageType"] == 16][0]
    assert main_entry["fileVersion"] == 65537
    os.remove(tmp_path)

def test_parse_define_respects_prefix():
    text = "#define ESPSW_FW_VERSION 0x00010100u\n#define ESPIR_FW_VERSION 0x00020000u\n"
    assert mo.parse_define(text, "ESPSW_FW_VERSION") == 0x00010100
    assert mo.parse_define(text, "ESPIR_FW_VERSION") == 0x00020000

if __name__ == "__main__":
    failures = 0
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            try:
                fn()
                print(f"ok   {name}")
            except Exception as e:
                failures += 1
                print(f"FAIL {name}: {e}")
    sys.exit(1 if failures else 0)
