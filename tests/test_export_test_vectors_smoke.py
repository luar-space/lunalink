"""Smoke tests for the public-repo test-vector producer scripts.

The producers in ``scripts/export_test_vectors_*.py`` are the lunalink
side of the public ``luar-space/lsis-afs-test-vectors`` repository.
They must:

- be importable as Python modules without side effects (no I/O at
  import time, no eager package discovery that would fail in CI)
- expose a ``main(argv)`` callable so they can be wired into
  orchestrators and CI from Python directly
- list the right subset of test cases — drift between the four
  producers is a release-blocker because L5 (and the public repo's
  validate.py) depends on each L2 frame having a matching parsed JSON

These checks catch the kind of breakage that doesn't show up in the
substantive interop suite: an unrelated refactor that breaks
``import lunalink.afs.vectors``, a typo in the L3 ``TEST_CASES`` table,
a divergence between L2's six frames and L5's seven inputs.
"""

from __future__ import annotations

import importlib
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = REPO_ROOT / "scripts"


@pytest.fixture(autouse=True)
def _scripts_on_path():
    sys.path.insert(0, str(SCRIPTS_DIR))
    yield
    sys.path.remove(str(SCRIPTS_DIR))


PRODUCER_MODULES = [
    "export_test_vectors_l1",
    "export_test_vectors_l2",
    "export_test_vectors_fec",
    "export_test_vectors_l3",
    "export_test_vectors_l5",
    "export_test_vectors_all",
]


@pytest.mark.parametrize("module_name", PRODUCER_MODULES)
def test_producer_module_importable(module_name: str) -> None:
    """Every producer must import cleanly with no side effects."""
    if module_name in sys.modules:
        del sys.modules[module_name]
    mod = importlib.import_module(module_name)
    assert callable(mod.main), f"{module_name} must expose a main(argv) callable"


def test_l2_l5_frame_set_consistency() -> None:
    """L5 must produce a parsed JSON for every L2 frame.

    Drift here means a released L5 set will be missing the parsed
    JSON for a shipped L2 frame (validate.py PARSED_TEST_VECTORS
    catches it on the consumer side, but should be caught here too).
    """
    if "export_test_vectors_l2" in sys.modules:
        del sys.modules["export_test_vectors_l2"]
    if "export_test_vectors_l5" in sys.modules:
        del sys.modules["export_test_vectors_l5"]
    l2 = importlib.import_module("export_test_vectors_l2")
    l5 = importlib.import_module("export_test_vectors_l5")

    l2_frame_filenames = {row[0] for row in l2.TEST_MESSAGES}
    l5_frame_filenames = {row[0] for row in l5.TEST_MESSAGES}

    assert l2_frame_filenames == l5_frame_filenames, (
        f"L2 and L5 frame sets diverged.\n"
        f"  L2 only: {sorted(l2_frame_filenames - l5_frame_filenames)}\n"
        f"  L5 only: {sorted(l5_frame_filenames - l2_frame_filenames)}"
    )


def test_l2_l3_frame_set_consistency() -> None:
    """L3 must reference only frames that L2 produces.

    L3 modulates the 6000-symbol payload of each frame; if L3 lists
    a frame L2 doesn't produce, regen will SKIP that case and ship
    a partial L3 set.
    """
    if "export_test_vectors_l2" in sys.modules:
        del sys.modules["export_test_vectors_l2"]
    if "export_test_vectors_l3" in sys.modules:
        del sys.modules["export_test_vectors_l3"]
    l2 = importlib.import_module("export_test_vectors_l2")
    l3 = importlib.import_module("export_test_vectors_l3")

    l2_frame_filenames = {row[0] for row in l2.TEST_MESSAGES}
    l3_frame_inputs = {row[1] for row in l3.TEST_CASES}

    missing = l3_frame_inputs - l2_frame_filenames
    assert not missing, f"L3 references frames L2 doesn't produce: {sorted(missing)}"


def test_l2_fec_frame_set_consistency() -> None:
    """The FEC stitch-check frame set must match the frames L2 produces.

    export_test_vectors_fec.FRAME_CASES drives the stitch-to-frames
    self-check; _stitch_check SKIPs (does not fail) frames it has no
    entry for, so silent drift from the L2 set would ship an
    unverified vector set. Guard it the same way L5/L3 are guarded.
    """
    for mod in ("export_test_vectors_l2", "export_test_vectors_fec"):
        sys.modules.pop(mod, None)
    l2 = importlib.import_module("export_test_vectors_l2")
    fec = importlib.import_module("export_test_vectors_fec")

    l2_stems = {
        row[0].removeprefix("frame_").removesuffix(".bin") for row in l2.TEST_MESSAGES
    }
    fec_stems = {row[0] for row in fec.FRAME_CASES}

    assert l2_stems == fec_stems, (
        f"L2 and FEC frame sets diverged.\n"
        f"  L2 only: {sorted(l2_stems - fec_stems)}\n"
        f"  FEC only: {sorted(fec_stems - l2_stems)}"
    )


def test_l3_prn_range_within_interim_codes() -> None:
    """L3 modulates with ``generate_iq_frame`` which only handles PRN 1..12.

    Catches a typo or out-of-range PRN in L3's TEST_CASES at import time
    rather than failing partway through a 30-min regeneration.
    """
    if "export_test_vectors_l3" in sys.modules:
        del sys.modules["export_test_vectors_l3"]
    l3 = importlib.import_module("export_test_vectors_l3")
    out_of_range = [(name, prn) for name, _, prn in l3.TEST_CASES if not 1 <= prn <= 12]
    assert not out_of_range, (
        f"L3 has PRN(s) outside the interim 1..12 range: {out_of_range}"
    )


def test_export_parsed_json_interop_v1_emits_strict_pdf_shape() -> None:
    """Emit JSON whose spec-mandated fields match interoperability.pdf.

    The shape must match the §"Parsed Data Export Format" example exactly.

    Spec keys (no leading underscore) must be present at the documented
    positions and have the documented value-type (scalar 0 for health,
    empty object for ced/time_conversions/data, etc.).  This guards
    against future refactors that re-introduce non-spec object shapes
    on the V1.0-TBW fields.
    """
    import json

    import numpy as np

    from lunalink.afs import frame_build
    from lunalink.afs._receiver import receive_frame
    from lunalink.afs.vectors import export_parsed_json_interop_v1

    # Build a minimal known-good frame and run it through the L5 emitter
    # via tmpfile so we exercise the same path as the real producer.
    sb2 = np.zeros(1176, dtype=np.uint8)
    sb3 = np.zeros(846, dtype=np.uint8)
    sb4 = np.zeros(846, dtype=np.uint8)
    syms = frame_build(0, 0, sb2, sb3, sb4)
    bipolar = np.where(np.asarray(syms) == 0, 1.0, -1.0).astype(np.float32)
    result = receive_frame(bipolar, raise_on_error=True)
    assert result.all_ok, "fixture frame must round-trip cleanly"

    import tempfile

    with tempfile.NamedTemporaryFile(mode="r", suffix=".json", delete=False) as tmp:
        tmp_path = Path(tmp.name)
    try:
        export_parsed_json_interop_v1(result, tmp_path, frame_id="smoke_test")
        doc = json.loads(tmp_path.read_text())
    finally:
        tmp_path.unlink(missing_ok=True)

    # Top-level spec keys (interop-PDF page 3-4)
    spec_top = {
        "version",
        "timestamp",
        "frame_id",
        "subframe1",
        "subframe2",
        "subframe3",
        "subframe4",
        "time_of_transmission",
    }
    assert spec_top <= doc.keys(), f"missing spec keys: {spec_top - doc.keys()}"

    # Spec-mandated subframe shapes — strict match to PDF page 4
    assert doc["subframe2"]["ced"] == {}, "subframe2.ced must be empty object"
    assert doc["subframe2"]["health"] == 0, "subframe2.health must be scalar 0"
    assert doc["subframe2"]["time_conversions"] == {}, (
        "subframe2.time_conversions must be empty object"
    )
    assert doc["subframe3"]["data"] == {}, "subframe3.data must be empty object"
    assert doc["subframe4"]["data"] == {}, "subframe4.data must be empty object"

    # Timestamp must use Z suffix per the PDF example
    assert doc["timestamp"].endswith("Z"), (
        f"timestamp must end with Z, got {doc['timestamp']!r}"
    )


def test_bits_to_hex_annex3_rejects_non_nibble_aligned_input() -> None:
    """``_bits_to_hex_annex3`` refuses bit counts that aren't nibble-aligned.

    The Annex 3 hex encoding packs 4 bits per hex digit, so a bit count
    that isn't divisible by 4 (after any ``prepend_zeros``) has no
    well-defined representation.  Silently truncating or zero-filling
    would emit a vector that byte-compares unequal against the normative
    attachment, so the producer must fail loudly instead.
    """
    import numpy as np

    from lunalink.afs.vectors import _bits_to_hex_annex3

    # 10 bits, no prepend — 10 % 4 == 2, so not representable.
    bits = np.zeros(10, dtype=np.uint8)
    with pytest.raises(ValueError, match="not divisible by 4"):
        _bits_to_hex_annex3(bits)

    # prepend_zeros participates in the alignment check: 10 + 2 == 12 is
    # divisible by 4 and must be accepted.
    assert _bits_to_hex_annex3(bits, prepend_zeros=2) == "000"


def test_export_frame_vector_defaults_timestamp_to_wall_clock() -> None:
    """Omitting ``timestamp`` stamps the current time into the L2 header.

    Published vectors pass an explicit epoch for byte-stability, so the
    default branch is only exercised by ad-hoc/interactive use.  Guard it
    anyway: a regression here writes a garbage or zero timestamp into
    every hand-produced frame without any test noticing.
    """
    import struct
    import tempfile
    import time

    import numpy as np

    from lunalink.afs.vectors import export_frame_vector

    sb2 = np.zeros(1176, dtype=np.uint8)
    sb3 = np.zeros(846, dtype=np.uint8)
    sb4 = np.zeros(846, dtype=np.uint8)

    before = int(time.time())
    with tempfile.TemporaryDirectory() as tmpdir:
        path = Path(tmpdir) / "frame_default_ts.bin"
        export_frame_vector(1, 0, 0, sb2, sb3, sb4, path)
        header = path.read_bytes()[:64]
    after = int(time.time())

    (stamped,) = struct.unpack_from("<q", header, 20)
    assert before <= stamped <= after, (
        f"default timestamp {stamped} outside [{before}, {after}]"
    )

    # The explicit-timestamp path is what published vectors use: the same
    # inputs must produce byte-identical output across runs, which is the
    # whole reason the parameter exists.
    pinned = 1738108800  # 2025-01-29T00:00:00Z — LSIS V1.0 publication date
    with tempfile.TemporaryDirectory() as tmpdir:
        first = Path(tmpdir) / "pinned_a.bin"
        second = Path(tmpdir) / "pinned_b.bin"
        export_frame_vector(1, 0, 0, sb2, sb3, sb4, first, timestamp=pinned)
        export_frame_vector(1, 0, 0, sb2, sb3, sb4, second, timestamp=pinned)
        blob = first.read_bytes()
        assert blob == second.read_bytes(), "pinned timestamp must be reproducible"

    (pinned_stamp,) = struct.unpack_from("<q", blob[:64], 20)
    assert pinned_stamp == pinned


def test_export_parsed_json_interop_v1_emits_optional_provenance() -> None:
    """``input_frame_sha256`` and ``description`` land as underscore keys.

    Both are optional provenance the public repo attaches so a consumer
    can tie a parsed JSON back to the exact ``frame_*.bin`` it came from.
    They must be omitted entirely when not supplied — an explicit ``null``
    would break byte-equality against vectors produced without them.
    """
    import json
    import tempfile

    import numpy as np

    from lunalink.afs import frame_build
    from lunalink.afs._receiver import receive_frame
    from lunalink.afs.vectors import export_parsed_json_interop_v1

    sb2 = np.zeros(1176, dtype=np.uint8)
    sb3 = np.zeros(846, dtype=np.uint8)
    sb4 = np.zeros(846, dtype=np.uint8)
    syms = frame_build(0, 0, sb2, sb3, sb4)
    bipolar = np.where(np.asarray(syms) == 0, 1.0, -1.0).astype(np.float32)
    result = receive_frame(bipolar, raise_on_error=True)

    sha = "a" * 64
    with tempfile.TemporaryDirectory() as tmpdir:
        with_prov = Path(tmpdir) / "with_prov.json"
        export_parsed_json_interop_v1(
            result,
            with_prov,
            frame_id="prov_test",
            input_frame_sha256=sha,
            description="All zeros (baseline)",
        )
        doc = json.loads(with_prov.read_text())

        without_prov = Path(tmpdir) / "without_prov.json"
        export_parsed_json_interop_v1(result, without_prov, frame_id="prov_test")
        bare = json.loads(without_prov.read_text())

    assert doc["_input_frame_sha256"] == sha
    assert doc["_description"] == "All zeros (baseline)"
    assert "_input_frame_sha256" not in bare
    assert "_description" not in bare
