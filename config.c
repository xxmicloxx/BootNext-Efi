#include "config.h"

#include "lib.h"
#include "file.h"

APP_CONFIG GlobalConfig = {0};

APP_BOOT_ENTRY*
find_last_boot_entry()
{
    if (GlobalConfig.FirstBootEntry == NULL)
        return NULL;
    
    APP_BOOT_ENTRY *Entry = GlobalConfig.FirstBootEntry;
    while (Entry->Next != NULL)
        Entry = Entry->Next;
    
    return Entry;
}

APP_BOOT_ENTRY*
find_boot_entry(CONST CHAR16 *Key)
{
    if (GlobalConfig.FirstBootEntry == NULL)
        return NULL;
    
    APP_BOOT_ENTRY *Entry = GlobalConfig.FirstBootEntry;
    while (Entry != NULL && StrCmp(Entry->Key, Key) != 0)
        Entry = Entry->Next;
    
    return Entry;
}

static EFI_STATUS
handle_section(CHAR16 *Section)
{
    if (StrCmp(Section, L"General") == 0)
    {
        // we are in general section
    }
    else if (StrnCmp(Section, L"B:", 2) == 0)
    {
        // we have a bootloader section
        Section += 2;
        trim(Section);
        
        // try to see if it already exists
        APP_BOOT_ENTRY *Entry;
        Entry = find_boot_entry(Section);
        if (Entry != NULL)
        {
            Print(L"Warning: Duplicate boot section [B:%s]\n", Section);
            Print(L"This may not be intended! Be careful!\n");
            return EFI_SUCCESS;
        }
        
        // add the boot section
        Entry = (APP_BOOT_ENTRY *) AllocateZeroPool(sizeof(APP_BOOT_ENTRY));
        if (Entry != NULL)
        {
            Entry->Key = StrDuplicate(Section);
            
            // prepend to list
            Entry->Next = GlobalConfig.FirstBootEntry;
            GlobalConfig.FirstBootEntry = Entry;
        }
        else
        {
            Print(L"FATAL: Ran out of memory while adding boot entry!\n");
            return EFI_OUT_OF_RESOURCES;
        }
    }
    else
    {
        Print(L"Unknown section [%s]\n", Section);
    }
    
    return EFI_SUCCESS;
}

static void unknown_key(CHAR16 *Section, CHAR16 *Key)
{
    Print(L"Unknown key \"%s\" in section [%s]\n", Key, Section);
}

static EFI_STATUS
handle_key_value(CHAR16 *Section, CHAR16 *Key, CHAR16 *Value)
{
    if (StrCmp(Section, L"General") == 0)
    {
        if (StrCmp(Key, L"Default File") == 0)
        {
            if (GlobalConfig.DefaultFile != NULL)
                FreePool(GlobalConfig.DefaultFile);
            
            GlobalConfig.DefaultFile = StrDuplicate(Value);
        }
        else
        {
            unknown_key(Section, Key);
        }
    }
    else if (StrnCmp(Section, L"B:", 2) == 0)
    {
        // we're in a boot section
        Section += 2;
        trim(Section);
        
        APP_BOOT_ENTRY *Entry;
        Entry = find_boot_entry(Section);
        if (Entry == NULL)
        {
            Print(L"FATAL: Boot entry not added!\n");
            return EFI_OUT_OF_RESOURCES;
        }
        
        if (StrCmp(Key, L"File") == 0)
        {
            if (Entry->File != NULL)
                FreePool(Entry->File);
            
            Entry->File = StrDuplicate(Value);
        }
        else if (StrCmp(Key, L"Title") == 0)
        {
            if (Entry->Title != NULL)
                FreePool(Entry->Title);
            
            Entry->Title = StrDuplicate(Value);
        }
        else if (StrCmp(Key, L"Icon") == 0)
        {
            if (Entry->Icon != NULL)
                FreePool(Entry->Icon);
            
            Entry->Icon = StrDuplicate(Value);
        }
        else
        {
            unknown_key(Section, Key);
        }
    }
    
    return EFI_SUCCESS;
}

static EFI_STATUS
check_required()
{
    EFI_STATUS Status = EFI_SUCCESS;
    
    if (GlobalConfig.DefaultFile == NULL)
    {
        Print(L"FATAL: Default file not configured!\n");
        Status = EFI_LOAD_ERROR;
    }
    
    APP_BOOT_ENTRY *Entry = GlobalConfig.FirstBootEntry;
    for (; Entry != NULL; Entry = Entry->Next)
    {
        if (Entry->File == NULL)
        {
            Print(L"FATAL: Boot file not specified for [B:%s]\n", Entry->Key);
            Status = EFI_LOAD_ERROR;
        }
    }
    
    return Status;
}

static EFI_STATUS
handle_line(CHAR16 *Line, CHAR16 **Section)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINTN i;
    UINTN Len = 0;
    
    trim_comments(Line);
    trim(Line);
    Len = StrLen(Line);
    if (Len == 0)
        return EFI_SUCCESS;
    
    if (Line[0] == L'[' && Line[Len-1] == L']')
    {
        // begin of a section
        if (*Section != NULL)
        {
            FreePool(*Section);
        }
        
        Line[Len-1] = 0;
        trim(Line + 1);
        *Section = StrDuplicate(Line + 1);
        Status = handle_section(*Section);
    }
    else if (*Section == NULL)
    {
        // we have content on a line that is invalid
        Print(L"Found invalid content outside a section: %s\n", Line);
    }
    else
    {
        CHAR16 *Key = NULL;
        CHAR16 *Value = NULL;
        
        for (i = 0; i < Len; ++i)
        {
            if (Line[i] == L'=')
            {
                Line[i] = 0;
                Key = StrDuplicate(Line);
                trim(Key);
                Value = StrDuplicate(Line + i + 1);
                trim(Value);
                break;
            }
        }
        
        if (Key == NULL)
        {
            Print(L"Found invalid config content: %s\n", Line);
            FreePool(Line);
            return EFI_SUCCESS;
        }
        
        Print(L"[%s] %s: %s\n", *Section, Key, Value);
        Status = handle_key_value(*Section, Key, Value);
        
        FreePool(Key);
        FreePool(Value);
    }
    
    return Status;
}

EFI_STATUS
load_config(void)
{
    EFI_STATUS Status;
    
    Print(L"Parsing config...\n");
    
    APP_FILE File;
    Status = read_file(SelfDir, L"config.conf", &File);
    PRINT_ERROR(Status, L"while reading the config file");
    
    CHAR16 *Line = NULL;
    CHAR16 *Section = NULL;
    
    do {
        Line = read_line(&File);
        if (Line == NULL)
            break;
        
        Status = handle_line(Line, &Section);
        NOTIFY_ERROR(Status);
        
        FreePool(Line);
    } while (Line != NULL);
    
    FreePool(File.Buffer);
    
    return check_required();
}
