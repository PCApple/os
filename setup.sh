#!/usr/bin/env bash

set -e

IMAGE="os.img"
IMAGE_SIZE_MB=128
MOUNT_POINT="/mnt/casos"

echo "=== CASOS disk setup ==="

# ------------------------------------------------------------
# 1. Enter src directory
# ------------------------------------------------------------
cd ./src

# ------------------------------------------------------------
# 2. Create a fresh 128 MB disk image
# ------------------------------------------------------------
echo "[1/8] Creating ${IMAGE_SIZE_MB} MB disk image..."

dd if=/dev/zero \
   of="$IMAGE" \
   bs=1048576 \
   count="$IMAGE_SIZE_MB"

# ------------------------------------------------------------
# 3. Create an MBR partition table with one bootable partition.
#
# Equivalent to:
#
# fdisk os.img
#   n
#   p
#   1
#   <enter>
#   <enter>
#   a
#   1
#   w
# ------------------------------------------------------------
echo "[2/8] Creating partition table..."

fdisk "$IMAGE" <<EOF
n
p
1


a
w
EOF

# ------------------------------------------------------------
# 4. Attach the entire disk image to a loop device
# ------------------------------------------------------------
echo "[3/8] Creating loop device for disk..."

DISK_LOOP=$(sudo losetup --find --show "$IMAGE")

echo "Disk loop device: $DISK_LOOP"

# ------------------------------------------------------------
# 5. Attach the partition using the 1 MB offset
# ------------------------------------------------------------
echo "[4/8] Creating loop device for partition..."

PART_LOOP=$(sudo losetup --find --show \
    --offset 1048576 \
    "$IMAGE")

echo "Partition loop device: $PART_LOOP"

# ------------------------------------------------------------
# Format the partition
#
# Your original instructions did not include this step, but a
# fresh image needs a filesystem before it can be mounted.
# ------------------------------------------------------------
echo "[5/8] Creating ext2 filesystem..."

sudo mkfs.ext2 -F "$PART_LOOP"

# ------------------------------------------------------------
# 6-9. Create mount point and mount partition
# ------------------------------------------------------------
echo "[6/8] Mounting filesystem..."

sudo mkdir -p "$MOUNT_POINT"

sudo mount "$PART_LOOP" "$MOUNT_POINT"

sudo mkdir -p "$MOUNT_POINT/boot"

# ------------------------------------------------------------
# 10. Install GRUB
# ------------------------------------------------------------
echo "[7/8] Installing GRUB..."

sudo grub-install \
    --target=i386-pc \
    --root-directory="$MOUNT_POINT" \
    --modules="normal part_msdos ext2 multiboot biosdisk" \
    --recheck \
    "$DISK_LOOP"

# ------------------------------------------------------------
# 11-12. Create grub.cfg
# ------------------------------------------------------------
echo "[8/8] Creating GRUB configuration..."

sudo mkdir -p "$MOUNT_POINT/boot/grub"

sudo tee "$MOUNT_POINT/boot/grub/grub.cfg" > /dev/null <<'EOF'
menuentry "CASOS" {
    multiboot /boot/casos
    boot
}
EOF

echo
echo "========================================"
echo " CASOS disk setup complete"
echo "========================================"
echo
echo "Disk image:       $(pwd)/$IMAGE"
echo "Disk loop:        $DISK_LOOP"
echo "Partition loop:   $PART_LOOP"
echo "Mounted at:       $MOUNT_POINT"
echo
echo "You can now use:"
echo
echo "    ./build"
echo "    ./run"
echo