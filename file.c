#include "file.h"
#include "lib.h"

void
clean_up_slashes(IN OUT CHAR16 *PathName)
{
    CHAR16 *NewName;
    UINTN i, FinalChar = 0;
    BOOLEAN LastWasSlash = FALSE;
    
    NewName = AllocateZeroPool(sizeof(CHAR16) * (StrLen(PathName) + 2));
    if (NewName != NULL)
    {
        for (i = 0; i < StrLen(PathName); ++i)
        {
            if ((PathName[i] == L'/') || (PathName[i] == L'\\'))
            {
                if ((!LastWasSlash) && (FinalChar != 0))
                    NewName[FinalChar++] = L'\\';
                
                LastWasSlash = TRUE;
            }
            else
            {
                NewName[FinalChar++] = PathName[i];
                LastWasSlash = FALSE;
            }
        }
        
        NewName[FinalChar] = 0;
        if ((FinalChar > 0) && (NewName[FinalChar - 1] == L'\\'))
            NewName[--FinalChar] = 0;
        
        if (FinalChar == 0)
        {
            NewName[0] = L'\\';
            NewName[1] = 0;
        }
        
        // copy stuff back
        StrCpy(PathName, NewName);
        FreePool(NewName);
    }
}

CHAR16*
find_path(IN CHAR16* FullPath)
{
    UINTN i, LastBackslash = 0;
    CHAR16 *PathOnly = NULL;
    
    if (FullPath != NULL)
    {
        for (i = 0; i < StrLen(FullPath); ++i)
        {
            if (FullPath[i] == '\\')
                LastBackslash = i;
        }
        
        PathOnly = StrDuplicate(FullPath);
        PathOnly[LastBackslash] = 0;
    }
    
    return (PathOnly);
}

CHAR16*
split_device_string(IN OUT CHAR16 *InString)
{
    INTN i;
    CHAR16 *FileName = NULL;
    BOOLEAN Found = FALSE;
    
    if (InString != NULL)
    {
        i = StrLen(InString) - 1;
        while ((i >= 0) && (!Found))
        {
            if (InString[i] == L')')
            {
                Found = TRUE;
                FileName = StrDuplicate(InString + i + 1);
                clean_up_slashes(FileName);
                InString[i + 1] = 0;
            }
            
            i--;
        }
        
        if (FileName == NULL)
            FileName = StrDuplicate(InString);
    }
    
    return FileName; 
}

EFI_STATUS
delete_file(IN EFI_FILE_HANDLE BaseDir, IN CONST CHAR16 *FileName)
{
    EFI_STATUS Status;
    EFI_FILE_HANDLE FileHandle;
    
    Status = uefi_call_wrapper(BaseDir->Open, 5, BaseDir, &FileHandle, FileName, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    RET_ERROR(Status);
    
    Status = uefi_call_wrapper(FileHandle->Delete, 1, FileHandle);
    RET_ERROR(Status);
    
    return EFI_SUCCESS;
}

EFI_STATUS
read_file(IN EFI_FILE_HANDLE BaseDir, IN CONST CHAR16 *FileName, IN OUT APP_FILE *File)
{
    EFI_STATUS Status;
    EFI_FILE_HANDLE FileHandle;
    EFI_FILE_INFO *FileInfo;
    UINT64 ReadSize;
    
    File->Buffer = NULL;
    File->Size = 0;
    
    Status = uefi_call_wrapper(BaseDir->Open, 5, BaseDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
    RET_ERROR(Status);
    
    FileInfo = LibFileInfo(FileHandle);
    if (FileInfo == NULL)
    {
        uefi_call_wrapper(FileHandle->Close, 1, FileHandle);
        return EFI_LOAD_ERROR;
    }
    ReadSize = FileInfo->FileSize;
    FreePool(FileInfo);
    
    File->Size = ReadSize;
    File->Buffer = AllocatePool(File->Size);
    if (File->Buffer == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }
    
    Status = uefi_call_wrapper(FileHandle->Read, 3, FileHandle, &File->Size, File->Buffer);
    if (EFI_ERROR(Status))
    {
        FreePool(File->Buffer);
        File->Buffer = NULL;
        uefi_call_wrapper(FileHandle->Close, 1, FileHandle);
        return Status;
    }
    
    uefi_call_wrapper(FileHandle->Close, 1, FileHandle);
    
    File->Current8Ptr = (CHAR8 *)File->Buffer;
    File->End8Ptr = File->Current8Ptr + File->Size;
    
    return EFI_SUCCESS;
}

CHAR16*
read_all(APP_FILE *File)
{
    CHAR16 *Data;
    
    if (File->Buffer == NULL)
        return NULL;
    
    Data = convert_to_16bit(File->Current8Ptr, File->End8Ptr);
    File->Current8Ptr = File->End8Ptr;
    return Data;
}

CHAR16*
read_line(APP_FILE *File)
{
    if (File->Buffer == NULL)
        return NULL;
    
    CHAR8 *p, *LineStart, *LineEnd;
    p = File->Current8Ptr;
    if (p >= File->End8Ptr)
        return NULL;
    
    LineStart = p;
    for (; p < File->End8Ptr; ++p)
        if (*p == 13 || *p == 10)
            break;
    LineEnd = p;
    for (; p < File->End8Ptr; ++p)
        if (*p != 13 && *p != 10)
            break;
    File->Current8Ptr = p;
    
    return convert_to_16bit(LineStart, LineEnd);
}

EFI_STATUS
run_file(CONST CHAR16 *InPath)
{
    EFI_STATUS Status;
    BOOLEAN HasLabel;
    CHAR16 *Path, *VolName = NULL;
    EFI_HANDLE DevHandle;
    
    Path = StrDuplicate(InPath);
    // check if we have a partition label
    HasLabel = split_label(&Path, &VolName);
    trim(Path);
    
    if (HasLabel)
    {
        // try to get handle by label
        trim(VolName);
        Print(L"Searching volume named \"%s\"\n", VolName);
        DevHandle = find_volume_named(VolName);
        if (DevHandle == NULL)
        {
            Print(L"Could not find volume named \"%s\". Booting from own EFI partition.\n", VolName);
            Print(L"Press any key to continue...\n");
            wait_for_keystroke();
            DevHandle = SelfLoadedImage->DeviceHandle;
        }
    }
    else
    {
        DevHandle = SelfLoadedImage->DeviceHandle;
    }
    
    Print(L"Loading EFI shell at %s\n", Path);
    EFI_DEVICE_PATH *ShellDevicePath = FileDevicePath(DevHandle, (CHAR16 *) Path);
    
    Print(L"Trying to load...\n");
    Status = uefi_call_wrapper(BS->Stall, 1, 500000);
    NOTIFY_ERROR(Status);
    
    EFI_HANDLE LoadedImageHandle;
    Status = uefi_call_wrapper(BS->LoadImage, 6, FALSE, SelfImageHandle, ShellDevicePath, NULL, 0, &LoadedImageHandle);
    PRINT_ERROR(Status, L"while loading EFI image");
    
    Print(L"Loaded successfully!\n");
    
    Print(L"Chainloading...\n");
    Status = uefi_call_wrapper(BS->Stall, 1, 500000);
    NOTIFY_ERROR(Status);
    uninit_lib();
    Status = uefi_call_wrapper(BS->StartImage, 3, LoadedImageHandle, NULL, NULL);
    PRINT_ERROR(Status, L"while starting EFI image");
    
    Print(L"Control returned!\n");
    Print(L"Press any key to continue...\n");
    Status = wait_for_keystroke();
    NOTIFY_ERROR(Status);
    
    return EFI_SUCCESS;
}
