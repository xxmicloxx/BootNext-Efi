#ifndef _FILE_H_
#define _FILE_H_

#include <efi.h>
#include <efilib.h>

typedef struct {
    VOID *Buffer;
    UINT64 Size;
    
    CHAR8 *Current8Ptr;
    CHAR8 *End8Ptr;
} APP_FILE;

extern void clean_up_slashes(IN OUT CHAR16 *PathName);
extern CHAR16* find_path(IN CHAR16* FullPath);
extern CHAR16* split_device_string(IN OUT CHAR16 *InString);

extern EFI_STATUS read_file(IN EFI_FILE_HANDLE BaseDir, IN CONST CHAR16 *FileName, IN OUT APP_FILE *File);
extern CHAR16 *read_all(APP_FILE *File);
extern CHAR16 *read_line(APP_FILE *File);

extern EFI_STATUS delete_file(IN EFI_FILE_HANDLE BaseDir, IN CONST CHAR16 *FileName);

extern EFI_STATUS run_file(CONST CHAR16 *Path);

#endif // _FILE_H_
