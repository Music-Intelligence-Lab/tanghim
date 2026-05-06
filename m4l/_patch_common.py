"""Shared helpers for the Tanghīm Max for Live device generators.

This module centralises the binary `.amxd` "freeze" logic so the various
device generators (the legacy combined receiver, plus the upcoming MPE
and Mono-PB variants) can share a single, audited implementation.

The `.amxd` format is reverse-engineered from Ableton factory frozen
devices (e.g. LFO.amxd, Max MIDI Receiver.amxd):

    ampf  (4-byte magic)
    <little-endian u32 version = 4>
    mmmmmeta  (8-byte tag — `mmmmm` = MIDI effect)
    <little-endian u32 meta-data length = 4>
    <little-endian u32 meta value = 4 — frozen MIDI effect>
    ptch  (4-byte tag)
    <little-endian u32 ptch length>
    <ptch payload: mx@c header + concatenated files + dlst directory>

The `dlst` directory is a sequence of `dire` entries; each `dire` carries
sub-fields `type`, `fnam`, `sz32`, `of32`, `vers`, `flag`, `mdat`, all
big-endian.

This module deliberately preserves the legacy script's byte layout —
do not refactor or "improve" it without re-verifying SHA-256 parity
against the previously-committed `.amxd`.
"""

import json
import struct


def _build_dire(name: str, type_tag: bytes, size: int, offset: int, flag: int) -> bytes:
    """Build a single `dire` entry with sub-fields (all big-endian).

    Args:
      name: filename (ASCII)
      type_tag: 4-byte type identifier (e.g. b"JSON", b"TEXT")
      size: file content size in bytes
      offset: file content offset relative to the mx@c tag start
      flag: 17 for the main patch, 0 for dependencies
    """
    # fnam: null-terminated filename padded to 4-byte alignment
    name_bytes = name.encode("ascii") + b"\x00"
    name_padded = name_bytes + b"\x00" * ((4 - len(name_bytes) % 4) % 4)
    fnam_size = 8 + len(name_padded)  # tag(4) + size(4) + data

    entry = b""
    entry += b"type" + struct.pack(">I", 12) + type_tag  # 4-byte type padded
    entry += b"fnam" + struct.pack(">I", fnam_size) + name_padded
    entry += b"sz32" + struct.pack(">I", 12) + struct.pack(">I", size)
    entry += b"of32" + struct.pack(">I", 12) + struct.pack(">I", offset)
    entry += b"vers" + struct.pack(">I", 12) + struct.pack(">I", 0)
    entry += b"flag" + struct.pack(">I", 12) + struct.pack(">I", flag)
    entry += b"mdat" + struct.pack(">I", 12) + struct.pack(">I", 0)

    return b"dire" + struct.pack(">I", 8 + len(entry)) + entry


# Type tags for known embed kinds. Extend as needed.
_TYPE_TAG_BY_EXT = {
    ".js": b"TEXT",
}


def _type_tag_for(filename: str) -> bytes:
    """Return the 4-byte `type` tag for a given embed filename."""
    for ext, tag in _TYPE_TAG_BY_EXT.items():
        if filename.endswith(ext):
            return tag
    raise ValueError(
        f"_patch_common: no known dlst type tag for embed {filename!r}; "
        "extend _TYPE_TAG_BY_EXT to add support"
    )


def _code_kind_for(filename: str) -> str:
    """Return the `project.contents.code` kind string for an embed."""
    if filename.endswith(".js"):
        return "javascript"
    raise ValueError(
        f"_patch_common: no known project.code kind for embed {filename!r}"
    )


def freeze_and_save(p, maxpat_path: str, amxd_path: str,
                    embedded_files=None) -> None:
    """Save the patcher as a frozen `.amxd` MIDI effect.

    The function:
      1. Calls ``p.save_as(maxpat_path)`` to produce the `.maxpat` — but
         only if the file does not already exist on disk. Callers that
         need to apply M4L-specific post-processing to the patcher JSON
         (e.g. ``is_mpe``, ``title``, ``project`` metadata, comment
         outlet fixups) should write the post-processed `.maxpat` to
         disk *before* calling this function; we will read whatever is
         already at ``maxpat_path``.
      2. If ``embedded_files`` is non-empty, registers each embed in the
         patcher JSON (``dependency_cache`` + ``project.contents.code``)
         and rewrites the `.maxpat`.
      3. Builds the binary `.amxd` wrapper around the final `.maxpat`
         bytes plus any embeds, and writes it to ``amxd_path``.

    Args:
      p: py2max Patcher (already populated). May be ``None`` if the
        `.maxpat` is already on disk and no further save is needed.
      maxpat_path: path to write the `.maxpat`.
      amxd_path: path to write the `.amxd`.
      embedded_files: optional list of ``(filename, contents_bytes)``
        tuples to embed via the dlst directory. Default ``None`` (no
        embeds).
    """
    import os

    embeds = list(embedded_files or [])

    # Step 1: ensure a .maxpat exists on disk. If the caller already
    # wrote a post-processed .maxpat (the common case for M4L devices
    # that need is_mpe / title / project metadata that py2max cannot
    # express directly), respect it.
    if not os.path.exists(maxpat_path):
        if p is None:
            raise ValueError(
                "freeze_and_save: maxpat_path does not exist and no "
                "patcher was provided to save it."
            )
        p.save_as(maxpat_path)

    # Step 2: register embeds in the patcher JSON, rewrite .maxpat.
    with open(maxpat_path) as f:
        data = json.load(f)
    patcher = data["patcher"]

    # Embed registrations are applied to the in-memory `data` only — they
    # are needed inside the `.amxd`'s embedded JSON so Max knows about the
    # dependencies, but we deliberately do NOT rewrite the on-disk
    # `.maxpat` with them. This matches the legacy script's behaviour
    # (the on-disk `.maxpat` carries `dependency_cache: []`) and keeps the
    # `.maxpat` portable / unfrozen for editing in standalone Max.
    if embeds:
        patcher["dependency_cache"] = [
            {"name": name, "bootpath": ".", "type": _type_tag_for(name).decode("ascii"),
             "implicit": 1}
            for name, _ in embeds
        ]
        contents = patcher.setdefault("project", {}).setdefault("contents", {})
        code = contents.setdefault("code", {})
        for name, _ in embeds:
            code[name] = {"kind": _code_kind_for(name), "local": 1}

    # Step 3: build the .amxd binary wrapper.
    json_bytes = json.dumps(data, indent="\t").encode("utf-8") + b"\x00"

    # The patch itself is always the first file (flag=17); embeds follow
    # as dependencies (flag=0). The patch's filename inside the .amxd
    # matches the .amxd basename (legacy behaviour preserved).
    patch_filename = os.path.basename(amxd_path)
    files = [(patch_filename, b"JSON", json_bytes, 17)]
    for name, content in embeds:
        files.append((name, _type_tag_for(name), content, 0))

    # Compute offsets (relative to mx@c tag start; first file at offset
    # 16 after the mx@c header).
    MX_HEADER_SIZE = 16  # mx@c(4) + size(4) + flags(4) + dlst_offset(4)
    offset = MX_HEADER_SIZE
    dire_entries = b""
    for name, type_tag, content, flag in files:
        dire_entries += _build_dire(name, type_tag, len(content), offset, flag)
        offset += len(content)

    dlst_offset = offset  # offset of dlst from mx@c start
    dlst_body = dire_entries
    dlst = b"dlst" + struct.pack(">I", 8 + len(dlst_body)) + dlst_body

    # Assemble ptch content: mx@c header + file data + dlst.
    mx_header = (
        b"mx@c"
        + struct.pack(">I", MX_HEADER_SIZE)
        + struct.pack(">I", 0)
        + struct.pack(">I", dlst_offset)
    )
    ptch_content = mx_header
    for _, _, content, _ in files:
        ptch_content += content
    ptch_content += dlst

    with open(amxd_path, "wb") as f:
        f.write(b"ampf")                              # magic
        f.write(struct.pack("<I", 4))                 # version
        f.write(b"mmmmmeta")                          # meta tag (mmmmm = MIDI effect)
        f.write(struct.pack("<I", 4))                 # meta data length
        f.write(struct.pack("<I", 4))                 # meta value 4 = frozen MIDI effect
        f.write(b"ptch")                              # patch section tag
        f.write(struct.pack("<I", len(ptch_content))) # patch data length
        f.write(ptch_content)                         # mx@c + files + dlst
