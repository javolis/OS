#!/usr/bin/env bash
# smoke-uefi.sh — boot the ISO under UEFI firmware (OVMF) and confirm the
# kernel reaches its boot marker. This is the Hyper-V Generation 2 path:
# UEFI GRUB hands off, and the kernel must come up on a GOP framebuffer
# (no legacy VGA text exists). We only assert boot + framebuffer here; the
# full interactive session is covered by the BIOS smoke test.
set -uo pipefail

ISO="${1:-os.iso}"
MARKER="KERNEL_BOOT_OK"
SERIAL_LOG="$(mktemp)"
trap 'rm -f "$SERIAL_LOG" OVMF_VARS.test.fd' EXIT

if [ ! -f "$ISO" ]; then
    echo "FAIL: ISO not found: $ISO" >&2
    exit 1
fi

# Locate OVMF firmware (path differs across distros/package versions).
CODE=""
for c in /usr/share/OVMF/OVMF_CODE.fd /usr/share/ovmf/OVMF.fd \
         /usr/share/qemu/OVMF.fd /usr/share/edk2-ovmf/x64/OVMF_CODE.fd; do
    [ -f "$c" ] && CODE="$c" && break
done
if [ -z "$CODE" ]; then
    echo "FAIL: no OVMF firmware found" >&2
    exit 1
fi

echo "Booting $ISO under UEFI (OVMF: $CODE)..."
timeout 60 qemu-system-x86_64 \
    -bios "$CODE" \
    -cdrom "$ISO" \
    -display none \
    -serial "file:$SERIAL_LOG" \
    -no-reboot >/dev/null 2>&1

echo "----- serial output -----"
cat "$SERIAL_LOG"
echo
echo "-------------------------"

fail=0
if grep -q "$MARKER" "$SERIAL_LOG"; then
    echo "PASS: kernel booted under UEFI (reached '$MARKER')"
else
    echo "FAIL: kernel did not reach '$MARKER' under UEFI" >&2
    fail=1
fi

# Under UEFI there is no legacy VGA text mode, so the kernel must be on the
# GOP framebuffer — the exact Hyper-V Gen 2 condition.
if grep -q "framebuffer console:" "$SERIAL_LOG"; then
    echo "PASS: UEFI framebuffer console active"
else
    echo "FAIL: no framebuffer console under UEFI" >&2
    fail=1
fi

exit $fail
