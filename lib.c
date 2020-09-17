#include "lib.h"
#include "file.h"

EFI_GUID gAppGuid = { 0xD8D7FF6A, 0x2C3D, 0x4DAB, {0xA1, 0x2, 0x62, 0xEC, 0xD1, 0x3C, 0x53, 0xCF} };

EFI_LOADED_IMAGE *SelfLoadedImage;
EFI_HANDLE *SelfImageHandle;

CHAR16 *SelfDirPath;
EFI_FILE *SelfRootDir;
EFI_FILE *SelfDir;

void
log_error(EFI_STATUS Status)
{
    Print(L"Error: %r\n", Status);
}

void
log_error_custom(EFI_STATUS Status, IN CONST CHAR16 *Prompt)
{
    Print(L"Error %s: %r\n", Prompt, Status);
}

EFI_STATUS
finish_init_lib()
{
    EFI_STATUS Status;
    
    SelfRootDir = LibOpenRoot(SelfLoadedImage->DeviceHandle);
    if (SelfRootDir == NULL) {
        Print(L"Error while (re)opening root EFI partition!\n");
        return EFI_LOAD_ERROR;
    }
    
    Status = uefi_call_wrapper(SelfRootDir->Open, 5, SelfRootDir, &SelfDir, SelfDirPath, EFI_FILE_MODE_READ, 0);
    PRINT_ERROR(Status, L"while opening the installation directory");
    
    Print(L"Reinitialized EFI file handles\n");
    
    return EFI_SUCCESS;
}

EFI_STATUS
init_lib()
{
    EFI_STATUS Status;
    CHAR16 *DevicePathString, *Temp, *Temp2;
    
    Status = uefi_call_wrapper(BS->OpenProtocol, 6, SelfImageHandle, &LoadedImageProtocol, (VOID **) &SelfLoadedImage, SelfImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    NOTIFY_ERROR(Status);
    
    DevicePathString = DevicePathToStr(SelfLoadedImage->FilePath);
    Print(L"Initial device path string: %s\n", DevicePathString);
    clean_up_slashes(DevicePathString);
    
    Temp = find_path(DevicePathString);
    Temp2 = split_device_string(Temp);
    SelfDirPath = PoolPrint(L"\\%s", Temp2);
    FreePool(Temp);
    FreePool(Temp2);
    FreePool(DevicePathString);
    
    Print(L"Install dir on EFI: %s\n", SelfDirPath);
    
    return finish_init_lib();
}

void
uninit_lib()
{
    if (SelfDir != NULL) {
        uefi_call_wrapper(SelfDir->Close, 1, SelfDir);
        FreePool(SelfDir);
        SelfDir = NULL;
    }
    
    if (SelfRootDir != NULL) {
        uefi_call_wrapper(SelfRootDir->Close, 1, SelfRootDir);
        FreePool(SelfRootDir);
        SelfRootDir = NULL;
    }
}

EFI_STATUS
reinit_lib()
{
    return finish_init_lib();
}

EFI_STATUS
wait_for_keystroke()
{
    EFI_STATUS Status;
    EFI_INPUT_KEY Key;
    
    Status = uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
    NOTIFY_ERROR(Status);
    
    while ((Status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key)) == EFI_NOT_READY);
    return Status;
}

EFI_HANDLE
find_volume_named(CHAR16 *Name)
{
    EFI_STATUS Status;
    UINTN HandleCount = 0, i;
    EFI_HANDLE *Handles, DevHandle;
    EFI_FILE_HANDLE RootHandle;
    EFI_FILE_SYSTEM_INFO *FSInfo;
    BOOLEAN Found = FALSE;
    CHAR16 *LwrName, *LwrVolume;
    
    if (Name == NULL)
        return NULL;
    
    LwrName = StrDuplicate(Name);
    StrLwr(LwrName);
    
    Status = LibLocateHandle(ByProtocol, &FileSystemProtocol, NULL, &HandleCount, &Handles);
    if (Status == EFI_NOT_FOUND)
        return NULL;
    else if (EFI_ERROR(Status))
    {
        Print(L"Error while searching for volume labels: %r\n", Status);
        return NULL;
    }
    
    for (i = 0; i < HandleCount && !Found; ++i)
    {
        DevHandle = Handles[i];
        RootHandle = LibOpenRoot(DevHandle);
        FSInfo = LibFileSystemInfo(RootHandle);
        if (FSInfo->VolumeLabel != NULL)
        {
            LwrVolume = StrDuplicate(FSInfo->VolumeLabel);
            StrLwr(LwrVolume);
            
            if (StrCmp(LwrName, LwrVolume) == 0)
            {
                Found = TRUE;
            }
            
            FreePool(LwrVolume);
        }
        
        FreePool(FSInfo);
    }
    
    FreePool(Handles);
    FreePool(LwrName);
    return Found ? DevHandle : NULL;
}

BOOLEAN
split_label(CHAR16 **Path, CHAR16 **VolName)
{
    UINTN i = 0, Length;
    CHAR16 *Filename;
    
    if (*Path == NULL)
        return FALSE;
    
    if (*VolName != NULL)
    {
        FreePool(*VolName);
        *VolName = NULL;
    }
    
    Length = StrLen(*Path);
    while ((i < Length) && ((*Path)[i] != L':'))
    {
        ++i;
    }
    
    if (i < Length)
    {
        Filename = StrDuplicate((*Path) + i + 1);
        (*Path)[i] = 0;
        *VolName = *Path;
        *Path = Filename;
        return TRUE;
    }
    
    return FALSE;
}

void
trim(CHAR16 *Str)
{
    CHAR16 *r, *w, *lastChar;
    
    w = Str;
    r = Str;
    lastChar = NULL;
    
    while (*r != 0 && (*r == L' ' || *r == L'\t' || *r == L'\n' || *r == L'\r'))
        ++r;
    
    while (*r != 0)
    {
        if (*r != L' ' && *r != L'\t' && *r != L'\n' && *r != L'\r')
            lastChar = w;
        
        *w++ = *r++;
    }
    
    if (lastChar != NULL)
        lastChar[1] = 0;
}

void
trim_comments(CHAR16 *Str)
{
    for (; *Str != 0; ++Str)
    {
        if (*Str == L'#')
        {
            // found a comment
            // just end the string here
            *Str = 0;
            return;
        }
    }
}

CHAR16*
convert_to_16bit(CHAR8 *Start, CHAR8 *End)
{
    CHAR16 *Line, *q;
    CHAR8 *p;
    UINTN LineLength;
    
    LineLength = (UINTN)(End - Start) + 1;
    Line = AllocatePool(LineLength * sizeof(CHAR16));
    if (Line == NULL)
        return NULL;
    
    q = Line;
    for (p = Start; p < End; )
        *q++ = *p++;
    
    *q = 0;
    return Line;
}

CHAR16*
read_nvram_string(CONST CHAR16 *Key)
{
    CHAR8 *ValueStart, *ValueEnd;
    CHAR16 *Value;
    
    UINTN Size;
    ValueStart = (CHAR8 *) LibGetVariableAndSize((CHAR16 *) Key, &APP_GUID, &Size);
    ValueEnd = ValueStart + Size;
    Value = convert_to_16bit(ValueStart, ValueEnd);
    FreePool(ValueStart);
    
    return Value;
}

EFI_STATUS
write_nvram_string(CONST CHAR16 *Key, CONST CHAR16 *Value)
{
    CHAR8 *Line8Bit;
    UINTN Len, i;
    
    Len = StrLen(Value);
    Line8Bit = AllocatePool(Len);
    
    for (i = 0; i < Len; ++i)
        Line8Bit[i] = (CHAR8) Value[i];
    
    return LibSetVariable((CHAR16 *) Key, &APP_GUID, Len, (VOID *) Line8Bit);
}
