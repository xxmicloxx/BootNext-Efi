#ifndef _LIB_H_
#define _LIB_H_

#include <efi.h>
#include <efilib.h>

// GUID D8D7FF6A-2C3D-4DAB-A102-62ECD13C53CF
extern EFI_GUID gAppGuid;
#define APP_GUID gAppGuid

extern EFI_LOADED_IMAGE *SelfLoadedImage;
extern EFI_HANDLE *SelfImageHandle;

extern CHAR16 *SelfDirPath;
extern EFI_FILE *SelfRootDir;
extern EFI_FILE *SelfDir;

#define NOTIFY_WAIT_ERROR(X) do {\
if (EFI_ERROR((X))) {\
    log_error((X));\
    wait_for_keystroke();\
    return (X);\
}\
} while (0)

#define NOTIFY_ERROR(X) do {\
if (EFI_ERROR((X))) {\
    log_error((X));\
    return (X);\
}\
} while (0)

#define PRINT_ERROR(X, Y) do {\
if (EFI_ERROR((X))) {\
    log_error_custom((X), (Y));\
    return (X);\
}\
} while (0)

#define PRINT_WAIT_ERROR(X, Y) do {\
if (EFI_ERROR((X))) {\
    log_error_custom((X), (Y));\
    wait_for_keystroke();\
    return (X);\
}\
} while (0)

#define WAIT_ERROR(X) do {\
if (EFI_ERROR((X))) {\
    Print(L"Press any key to continue...\n");\
    wait_for_keystroke();\
    return (X);\
}\
} while(0)

#define RET_ERROR(X) do {\
if (EFI_ERROR((X))) {\
    return (X);\
}\
} while (0)

extern void log_error(EFI_STATUS Status);
extern void log_error_custom(EFI_STATUS Status, IN CONST CHAR16 *Prompt);

extern EFI_STATUS init_lib();
extern EFI_STATUS finish_init_lib();
extern void uninit_lib();
extern EFI_STATUS reinit_lib();

extern EFI_STATUS wait_for_keystroke();

extern void trim(CHAR16 *Str);
extern void trim_comments(CHAR16 *Str);

extern BOOLEAN split_label(CHAR16 **Path, CHAR16 **VolName);

extern EFI_HANDLE find_volume_named(CHAR16 *Name);

extern CHAR16 *convert_to_16bit(CHAR8 *Start, CHAR8 *End);

#endif // _LIB_H_
