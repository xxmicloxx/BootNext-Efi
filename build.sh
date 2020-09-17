#!/bin/bash

set -e

C_FILES=(main lib file config)

for file in "${C_FILES[@]}"; do
    gcc "$file.c"                   \
        -c                          \
        -fno-stack-protector        \
        -fpic                       \
        -fshort-wchar               \
        -mno-red-zone               \
        -Wall -Wextra               \
        -Werror                     \
        -I /usr/include/efi         \
        -I /usr/include/efi/x86_64  \
        -DEFI_FUNCTION_WRAPPER      \
        -o "$file.o"
done

obj_files=$(for f in "${C_FILES[@]}"; do echo "$f.o"; done)

ld ${obj_files[*]}                  \
    /usr/lib/crt0-efi-x86_64.o      \
    -nostdlib                       \
    -znocombreloc                   \
    -T /usr/lib/elf_x86_64_efi.lds  \
    -shared                         \
    -Bsymbolic                      \
    -L /usr/lib                     \
    -l:libgnuefi.a                  \
    -l:libefi.a                     \
    -o main.so

objcopy -j .text                    \
    -j .sdata                       \
    -j .data                        \
    -j .dynamic                     \
    -j .dynsym                      \
    -j .rel                         \
    -j .rela                        \
    -j .reloc                       \
    --target=efi-app-x86_64         \
    main.so                         \
    main.efi

dd if=/dev/zero of=fat.img bs=1k count=102400
mformat -i fat.img -v test -F ::
mmd -i fat.img ::/EFI
mmd -i fat.img ::/EFI/BOOT
mcopy -i fat.img main.efi ::/EFI/BOOT/BOOTX64.EFI
qemu-system-x86_64 -cpu qemu64 -bios /usr/share/ovmf/OVMF.fd -drive file=fat.img,if=ide,format=raw -net none
