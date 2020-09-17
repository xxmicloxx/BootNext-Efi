#include <efi.h>
#include <efilib.h>

#include "lib.h"
#include "file.h"
#include "config.h"

static CHAR16*
find_target_file()
{
    EFI_STATUS Status;
    CHAR16 *TargetKey;
    APP_FILE File;
    APP_BOOT_ENTRY *Entry;
    
    Status = read_file(SelfDir, L"next_boot", &File);
    if (Status == EFI_NOT_FOUND)
    {
        Print(L"Target file not set. Booting default file.\n");
        return GlobalConfig.DefaultFile;
    }
    else if (EFI_ERROR(Status))
    {
        Print(L"Could not open target file: %r\n", Status);
        goto bailout;
    }
    
    TargetKey = read_all(&File);
    trim(TargetKey);
    FreePool(File.Buffer);
    
    if (TargetKey == NULL)
    {
        Print(L"Could not read target file!\n");
        goto bailout;
    }
    
    // delete target file
    Status = delete_file(SelfDir, L"next_boot");
    if (EFI_ERROR(Status))
    {
        Print(L"Could not delete target file!\n");
        goto bailout;
    }
    
    Print(L"Target key is %s\n", TargetKey);
    Entry = find_boot_entry(TargetKey);
    FreePool(TargetKey);
    
    if (Entry == NULL)
    {
        Print(L"Could not find target to boot!\n");
        goto bailout;
    }
    
    return Entry->File;
    
bailout:
    Print(L"Press any key to continue...\n");
    wait_for_keystroke();
    
    return GlobalConfig.DefaultFile;
}

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    InitializeLib(ImageHandle, SystemTable);
    SelfImageHandle = ImageHandle;
    
    SelfDir = SelfRootDir = NULL;
    SelfDirPath = NULL;
    
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    
    Status = init_lib(ImageHandle);
    WAIT_ERROR(Status);
    
    Status = load_config();
    WAIT_ERROR(Status);
    
    Status = uefi_call_wrapper(BS->Stall, 1, 1000000);
    NOTIFY_WAIT_ERROR(Status);
    
    CHAR16 *TargetFile;
    Print(L"Loading target file...\n");
    TargetFile = find_target_file();
    
    Status = run_file(TargetFile);
    WAIT_ERROR(Status);
    
    return EFI_SUCCESS;
}
