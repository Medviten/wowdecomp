#if defined(WOWDECOMP_ARCH_64)

#include <cstdint>
#include <cstring>
#include <malloc.h>
#include <windows.h>
#include <imm.h>
#include "Engine/Source/Base/ConvertUTF.h"
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/Os/OsIME.h"
#include "Engine/Source/Storm/H/storm.h"

// STATIC: 0x00D4165C
static HIMC s_context;

// STATIC: 0x00D41660
static uint32_t s_enableCount;

// OFFSET: 0x0086CC20
int32_t OsIMEGetLanguageMode() {
    HWND window = static_cast<HWND>(OsGuiGetWindow(0));
    HIMC context = ImmGetContext(window);
    if (!context) {
        return 0;
    }

    int32_t mode = 0;
    DWORD conversion = 0;
    DWORD sentence;
    ImmGetConversionStatus(context, &conversion, &sentence);

    if (conversion & IME_CMODE_NATIVE) {
        HKL layout = GetKeyboardLayout(0);

        switch (reinterpret_cast<uintptr_t>(layout) & 0xFF) {
            case LANG_CHINESE:
                mode = reinterpret_cast<uintptr_t>(layout) == 0x08040804 ? 0 : 3;
                break;
            case LANG_JAPANESE:
                mode = 1;
                break;
            case LANG_KOREAN:
                mode = 2;
                break;
        }
    }

    ImmReleaseContext(window, context);
    return mode;
}

// OFFSET: 0x0086CCD0
static int32_t GetCompositionString(DWORD index, char* buffer, uint32_t bufferSize) {
    memset(buffer, 0, bufferSize);

    HWND window = static_cast<HWND>(OsGuiGetWindow(0));
    HIMC context = ImmGetContext(window);
    if (!context) {
        return 0;
    }

    // Custom: read the composition as UTF-16. The original used ImmGetCompositionStringA and the ANSI code page, which
    // turns every character outside that code page into '?' (e.g. Chinese typed on a Western-locale Windows).
    uint16_t wide[512];
    uint32_t written = 0;
    LONG bytes = ImmGetCompositionStringW(context, index, wide, sizeof(wide) - sizeof(wide[0]));
    wide[bytes > 0 ? bytes / sizeof(wide[0]) : 0] = 0;
    ConvertUTF16toUTF8(buffer, bufferSize - 1, wide, 512, &written, nullptr);
    buffer[written] = '\0';

    ImmReleaseContext(window, context);
    return 1;
}

// OFFSET: 0x0086CD70
int32_t OsIMEGetCompositionString(char* buffer, uint32_t bufferSize) {
    return GetCompositionString(GCS_COMPSTR, buffer, bufferSize);
}

// OFFSET: 0x0086CD90
int32_t OsIMEGetCompositionResult(char* buffer, uint32_t bufferSize) {
    return GetCompositionString(GCS_RESULTSTR, buffer, bufferSize);
}

// OFFSET: 0x0086CDB0
int32_t OsIMEGetClauseInfo(uint32_t& clauseStart, uint32_t& clauseEnd, uint32_t& cursor) {
    HWND window = static_cast<HWND>(OsGuiGetWindow(0));
    HIMC context = ImmGetContext(window);
    if (!context) {
        return 0;
    }

    // Custom: the W functions report the cursor, clause offsets and attributes in UTF-16 units, which is what the
    // callers expect, so no ANSI code page conversion (which broke on characters outside that code page) is needed.
    uint32_t cursorPos = ImmGetCompositionStringW(context, GCS_CURSORPOS, nullptr, 0) & 0xFFFF;

    uint32_t clauseSize = ImmGetCompositionStringW(context, GCS_COMPCLAUSE, nullptr, 0);
    if (clauseSize <= 4) {
        ImmReleaseContext(window, context);
        return 0;
    }

    DWORD* clauses = static_cast<DWORD*>(_alloca(clauseSize));
    memset(clauses, 0, clauseSize);

    clauseSize = ImmGetCompositionStringW(context, GCS_COMPCLAUSE, clauses, clauseSize);

    uint32_t clause = 0;

    if (clauseSize == IMM_ERROR_NODATA || clauseSize == IMM_ERROR_GENERAL) {
        ImmReleaseContext(window, context);
        return 0;
    }

    uint32_t clauseCount = clauseSize >> 2;
    clauses[0] = 0;

    if (clauseCount > 2) {
        for (uint32_t i = 0; i < clauseCount - 1; i++) {
            if (cursorPos >= clauses[i] && cursorPos < clauses[i + 1]) {
                clause = i;
            }
        }
    }

    char attributes[512];
    memset(attributes, 0, sizeof(attributes));
    ImmGetCompositionStringW(context, GCS_COMPATTR, attributes, sizeof(attributes));

    for (uint32_t i = 0; i < clauseCount - 1; i++) {
        if (clauses[i] < sizeof(attributes) && attributes[clauses[i]] == ATTR_INPUT) {
            clause = i;
        }
    }

    ImmReleaseContext(window, context);

    clauseStart = clauses[clause];
    clauseEnd = clauses[clause + 1];
    cursor = cursorPos;

    return 1;
}

// OFFSET: 0x0086CF90
void OsIMEEnable(int32_t enable) {
    HWND window = static_cast<HWND>(OsGuiGetWindow(0));

    if (enable) {
        if (++s_enableCount == 1) {
            ImmAssociateContext(window, s_context);
        }
    } else if (s_enableCount) {
        if (--s_enableCount == 0) {
            ImmAssociateContext(window, nullptr);
        }
    }
}

// OFFSET: 0x0086CFF0
void OsIMESync() {
    HWND window = static_cast<HWND>(OsGuiGetWindow(0));
    HIMC context = ImmGetContext(window);

    if (s_enableCount) {
        if (context != s_context) {
            ImmAssociateContext(window, s_context);
        }
    } else if (context) {
        ImmAssociateContext(window, nullptr);
    }
}

// OFFSET: 0x0086D030
void OsIMESetLanguageMode(int32_t native) {
    HIMC context = ImmGetContext(static_cast<HWND>(OsGuiGetWindow(0)));
    if (!context) {
        return;
    }

    DWORD conversion = 0;
    DWORD sentence;
    ImmGetConversionStatus(context, &conversion, &sentence);

    if (native) {
        conversion |= IME_CMODE_NATIVE;
    } else {
        conversion &= ~IME_CMODE_NATIVE;
    }

    ImmSetConversionStatus(context, conversion, sentence);
}

// OFFSET: 0x0086D0A0
void OsIMEInitialize() {
    s_context = ImmAssociateContext(static_cast<HWND>(OsGuiGetWindow(0)), nullptr);
}

// OFFSET: 0x0086D0C0
void OsIMEDestroy() {
    ImmAssociateContextEx(static_cast<HWND>(OsGuiGetWindow(0)), nullptr, IACE_DEFAULT);
    s_context = nullptr;
}

// OFFSET: 0x0086D1D0
int32_t OsIMEGetCandidates(uint32_t listMask, uint32_t& pageStart, uint32_t& pageSize, uint32_t& count, uint32_t& selection, TSGrowableArray<OsIMECandidate>& candidates) {
    candidates.Clear();

    HWND window = static_cast<HWND>(OsGuiGetWindow(0));
    HIMC context = ImmGetContext(window);
    if (!context) {
        return 0;
    }

    if (!listMask) {
        ImmReleaseContext(window, context);
        return 0;
    }

    DWORD index = -1;
    while (listMask & 1) {
        listMask >>= 1;
        index++;
    }

    // Custom: UTF-16 candidate list, so candidates outside the ANSI code page don't show as '?'
    DWORD listSize = ImmGetCandidateListW(context, index, nullptr, 0);
    if (!listSize) {
        ImmReleaseContext(window, context);
        return 0;
    }

    int32_t changed = 0;
    CANDIDATELIST* list = static_cast<CANDIDATELIST*>(SMemAlloc(listSize, __FILE__, __LINE__, 0));
    ImmGetCandidateListW(context, index, list, listSize);

    if (!list->dwPageSize) {
        ImmNotifyIME(context, NI_SETCANDIDATE_PAGESIZE, index, 9);
        SMemFree(list, __FILE__, __LINE__, 0);
        ImmReleaseContext(window, context);
        return 0;
    }

    while (list->dwSelection < list->dwPageStart) {
        changed = 1;
        list->dwPageStart = list->dwPageStart > list->dwPageSize ? list->dwPageStart - list->dwPageSize : 0;
    }

    while (list->dwPageStart + list->dwPageSize <= list->dwSelection) {
        list->dwPageStart += list->dwPageSize;
        changed = 1;
    }

    if (changed) {
        ImmNotifyIME(context, NI_SETCANDIDATE_PAGESTART, index, list->dwPageStart);
        SMemFree(list, __FILE__, __LINE__, 0);
        ImmReleaseContext(window, context);
        return 0;
    }

    pageStart = list->dwPageStart;
    pageSize = list->dwPageSize;
    count = list->dwCount;
    selection = list->dwSelection;

    for (uint32_t i = 0; i < list->dwPageSize; i++) {
        OsIMECandidate* candidate = candidates.New();
        uint32_t candidateIndex = i + list->dwPageStart;
        uint32_t written = 0;

        if (candidateIndex < list->dwCount) {
            const uint16_t* wide = reinterpret_cast<const uint16_t*>(reinterpret_cast<char*>(list) + list->dwOffset[candidateIndex]);
            ConvertUTF16toUTF8(candidate->m_text, sizeof(candidate->m_text) - 1, wide, STORM_MAX_STR, &written, nullptr);
        }

        candidate->m_text[written] = '\0';
    }

    SMemFree(list, __FILE__, __LINE__, 0);
    ImmReleaseContext(window, context);
    return 1;
}

#endif
