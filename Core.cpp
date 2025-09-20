#include <windows.h>
#include <tchar.h>
#include <string>
#include <cmath>
#include <cstring>

#define IDC_SECTOR_COUNT   101
#define IDC_SECTOR_SIZE    102
#define IDC_CALCULATE      103
#define IDC_RESULT         104
#define IDC_FILE_SIZE      105
#define IDC_MODE_FORWARD   106
#define IDC_MODE_REVERSE   107
#define IDC_COMBO_SECTOR   108
#define IDC_CLEAR          109
#define IDC_COPY           110
#define IDC_STATUS         111
#define IDC_GET_DRIVE_INFO 112
#define IDC_DEV_INFO       113
#define IDC_DRIVE_COMBO    114

// Static variables
static unsigned long long g_lastTotalBytes = 0;
static wchar_t g_lastFormattedResult[2048] = L"";
static bool g_bDevPanelVisible = false;
static HWND g_hDevPanel = NULL;

// Helper: format numbers with commas
void FormatNumberWithCommas(unsigned long long number, wchar_t* output, size_t outputSize) {
    wchar_t temp[32];
    swprintf(temp, 32, L"%llu", number);

    int len = wcslen(temp);
    int commaCount = (len - 1) / 3;
    int newLen = len + commaCount;

    int j = newLen - 1;
    int commaPosition = 0;

    for (int i = len - 1; i >= 0; i--) {
        output[j--] = temp[i];
        commaPosition++;
        if (commaPosition == 3 && i > 0) {
            output[j--] = L',';
            commaPosition = 0;
        }
    }
    output[newLen] = L'\0';
}

// Get total size of a logical drive in bytes
ULONGLONG GetLogicalDriveSizeInBytes(const wchar_t* driveLetter) {
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExW(driveLetter, &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        return totalBytes.QuadPart;
    }
    return 0;
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Create dev panel (initially hidden and positioned lower to avoid collisions)
        g_hDevPanel = CreateWindow(L"STATIC",
            L"SectorCalcPro v1.0\r\n\r\nCreated by: Salah Adel\r\nEmail: salahredwansss@gmail.com\r\n\r\nA lightweight sector calculator utility",
            WS_CHILD | WS_BORDER | SS_LEFT | SS_NOPREFIX,
            500, 80, 250, 120, hwnd, NULL, NULL, NULL);  // Moved down to Y=80

        // Set font for dev panel
        HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (hFont) {
            SendMessage(g_hDevPanel, WM_SETFONT, (WPARAM)hFont, TRUE);
        }

        // Button to toggle panel - moved to right side to avoid collisions
        CreateWindow(L"BUTTON", L"Developer Info", WS_CHILD | WS_VISIBLE,
            400, 20, 100, 30, hwnd, (HMENU)IDC_DEV_INFO, NULL, NULL);

        // Labels
        CreateWindow(L"STATIC", L"Sectors:", WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 20, 100, 25, hwnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"Sector Size:", WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 55, 100, 25, hwnd, NULL, NULL, NULL);

        CreateWindow(L"STATIC", L"File Size (optional):", WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 90, 150, 25, hwnd, NULL, NULL, NULL);

        // Inputs
        CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
            120, 20, 100, 25, hwnd, (HMENU)IDC_SECTOR_COUNT, NULL, NULL);

        // Combo box: sector sizes
        HWND hCombo = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            120, 55, 100, 200, hwnd, (HMENU)IDC_COMBO_SECTOR, NULL, NULL);

        // Fixed combo box population
        const wchar_t* sectorOptions[] = { L"512", L"1024", L"2048", L"4096", L"8192", L"16384", L"32768" };
        for (int i = 0; i < sizeof(sectorOptions) / sizeof(sectorOptions[0]); i++) {
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)sectorOptions[i]);
        }
        SendMessage(hCombo, CB_SETCURSEL, 0, 0); // default 512

        CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
            180, 90, 100, 25, hwnd, (HMENU)IDC_FILE_SIZE, NULL, NULL);

        // Drive selection label
        CreateWindow(L"STATIC", L"Select Drive:", WS_CHILD | WS_VISIBLE | SS_LEFT,
            240, 55, 80, 25, hwnd, NULL, NULL, NULL);

        // Drive combo box
        HWND hDriveCombo = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            320, 55, 100, 200, hwnd, (HMENU)IDC_DRIVE_COMBO, NULL, NULL);

        // Populate all available drives
        DWORD drivesBitmask = GetLogicalDrives();
        for (char letter = 'A'; letter <= 'Z'; ++letter) {
            if (drivesBitmask & (1 << (letter - 'A'))) {
                wchar_t driveStr[4];
                swprintf(driveStr, 4, L"%c:\\", letter);
                SendMessage(hDriveCombo, CB_ADDSTRING, 0, (LPARAM)driveStr);
            }
        }

        // Select first drive by default (if any)
        if (SendMessage(hDriveCombo, CB_GETCOUNT, 0, 0) > 0)
            SendMessage(hDriveCombo, CB_SETCURSEL, 0, 0);

        // Mode selection
        CreateWindow(L"BUTTON", L"Sectors → Size", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
            20, 130, 150, 25, hwnd, (HMENU)IDC_MODE_FORWARD, NULL, NULL);

        CreateWindow(L"BUTTON", L"File Size → Sectors", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            200, 130, 170, 25, hwnd, (HMENU)IDC_MODE_REVERSE, NULL, NULL);

        SendDlgItemMessage(hwnd, IDC_MODE_FORWARD, BM_SETCHECK, BST_CHECKED, 0);

        // Action buttons - moved down slightly to make room
        CreateWindow(L"BUTTON", L"Calculate", WS_CHILD | WS_VISIBLE,
            20, 170, 100, 30, hwnd, (HMENU)IDC_CALCULATE, NULL, NULL);

        CreateWindow(L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE,
            130, 170, 80, 30, hwnd, (HMENU)IDC_CLEAR, NULL, NULL);

        CreateWindow(L"BUTTON", L"Copy Result", WS_CHILD | WS_VISIBLE,
            220, 170, 100, 30, hwnd, (HMENU)IDC_COPY, NULL, NULL);

        // Drive info button
        CreateWindow(L"BUTTON", L"Get Drive Info", WS_CHILD | WS_VISIBLE,
            330, 170, 120, 30, hwnd, (HMENU)IDC_GET_DRIVE_INFO, NULL, NULL);

        // Result box - made slightly wider to fill available space
        CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
            20, 210, 460, 150, hwnd, (HMENU)IDC_RESULT, NULL, NULL);

        // Status
        CreateWindow(L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 370, 460, 20, hwnd, (HMENU)IDC_STATUS, NULL, NULL);
        break;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_CALCULATE: {
            SetDlgItemText(hwnd, IDC_STATUS, L"Calculating...");

            wchar_t sectorCountText[32] = { 0 };
            wchar_t fileSizeText[64] = { 0 };
            wchar_t sectorSizeText[32] = { 0 };

            GetDlgItemText(hwnd, IDC_SECTOR_COUNT, sectorCountText, 32);
            GetDlgItemText(hwnd, IDC_FILE_SIZE, fileSizeText, 64);

            HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_SECTOR);
            int sel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            SendMessage(hCombo, CB_GETLBTEXT, sel, (LPARAM)sectorSizeText);
            int sectorSize = _wtoi(sectorSizeText);

            BOOL forwardMode = (SendDlgItemMessage(hwnd, IDC_MODE_FORWARD, BM_GETCHECK, 0, 0) == BST_CHECKED);

            if (forwardMode) {
                int sectorCount = _wtoi(sectorCountText);
                if (sectorCount <= 0 || sectorSize <= 0) {
                    SetDlgItemText(hwnd, IDC_RESULT, L"Please enter valid sector count and size.");
                    SetDlgItemText(hwnd, IDC_STATUS, L"Error: Invalid input");
                    break;
                }

                unsigned long long totalBytes = (unsigned long long)sectorCount * sectorSize;
                g_lastTotalBytes = totalBytes;

                double KiB = totalBytes / 1024.0;
                double MiB = KiB / 1024.0;
                double GiB = MiB / 1024.0;
                double TiB = GiB / 1024.0;

                wchar_t bytesWithCommas[64];
                FormatNumberWithCommas(totalBytes, bytesWithCommas, 64);

                swprintf(g_lastFormattedResult, 2048,
                    L"Sectors → Size Calculation\r\n════════════════════════════\r\n"
                    L"Sectors: %d\r\nSector Size: %d bytes\r\n\r\nTotal Size:\r\n%s bytes\r\n%.3f KiB\r\n%.6f MiB\r\n%.9f GiB\r\n%.12f TiB",
                    sectorCount, sectorSize, bytesWithCommas, KiB, MiB, GiB, TiB);

                SetDlgItemText(hwnd, IDC_RESULT, g_lastFormattedResult);
                SetDlgItemText(hwnd, IDC_STATUS, L"Calculation complete");
            }
            else {
                if (wcslen(fileSizeText) == 0 || sectorSize <= 0) {
                    SetDlgItemText(hwnd, IDC_RESULT, L"Please enter valid file size and sector size.");
                    SetDlgItemText(hwnd, IDC_STATUS, L"Error: Invalid input");
                    break;
                }

                unsigned long long fileSize = _wcstoui64(fileSizeText, NULL, 10);
                unsigned long long sectorsNeeded = (fileSize + sectorSize - 1) / sectorSize;
                unsigned long long actualSize = sectorsNeeded * sectorSize;
                unsigned long long wastedBytes = actualSize - fileSize;
                double wastePercentage = (wastedBytes * 100.0) / actualSize;

                wchar_t fileSizeCommas[64], actualSizeCommas[64], wastedCommas[64];
                FormatNumberWithCommas(fileSize, fileSizeCommas, 64);
                FormatNumberWithCommas(actualSize, actualSizeCommas, 64);
                FormatNumberWithCommas(wastedBytes, wastedCommas, 64);

                swprintf(g_lastFormattedResult, 2048,
                    L"File Size → Sectors Calculation\r\n════════════════════════════\r\n"
                    L"File Size: %s bytes\r\nSector Size: %d bytes\r\n\r\n"
                    L"Sectors Needed: %llu\r\nActual Allocation: %s bytes\r\n"
                    L"Wasted: %s bytes (%.1f%%)\r\nEfficiency: %.1f%%",
                    fileSizeCommas, sectorSize, sectorsNeeded, actualSizeCommas,
                    wastedCommas, wastePercentage, 100.0 - wastePercentage);

                SetDlgItemText(hwnd, IDC_RESULT, g_lastFormattedResult);
                SetDlgItemText(hwnd, IDC_STATUS, L"Calculation complete");
            }
            break;
        }

        case IDC_CLEAR:
            SetDlgItemText(hwnd, IDC_SECTOR_COUNT, L"");
            SetDlgItemText(hwnd, IDC_FILE_SIZE, L"");
            SetDlgItemText(hwnd, IDC_RESULT, L"");
            SetDlgItemText(hwnd, IDC_STATUS, L"Cleared");
            g_lastTotalBytes = 0;
            g_lastFormattedResult[0] = L'\0';
            break;

        case IDC_DEV_INFO: {
            const wchar_t* devInfo =
                L"Advanced Sector Calculator\r\n"
                L"Created by: Salah Aldean Zaher Redwan\r\n"
                L"If You Have Any Suggestions You Can Contact Me Here\r\n"
                L"If You Have Any Issues With Your Device that I May be Able to Help With\r\n"
                L"You Can Contact me in the Same Email i Provided\r\n"
                L"Email: salahredwansss@gmail.com\r\n"
                L"A lightweight sector calculator utility\r\n"
                L"Many Thanks For Using My Tool Consider Staring it In GitHub\r\n";
            SetDlgItemText(hwnd, IDC_RESULT, devInfo);
            SetDlgItemText(hwnd, IDC_STATUS, L"Developer Info displayed");
            break;
        }

        case IDC_GET_DRIVE_INFO: {
            HWND hDriveCombo = GetDlgItem(hwnd, IDC_DRIVE_COMBO);
            int sel = (int)SendMessage(hDriveCombo, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) {
                SetDlgItemText(hwnd, IDC_RESULT, L"No drive selected.");
                SetDlgItemText(hwnd, IDC_STATUS, L"Error");
                break;
            }

            wchar_t selectedDrive[4];
            SendMessage(hDriveCombo, CB_GETLBTEXT, sel, (LPARAM)selectedDrive);

            ULONGLONG diskSize = GetLogicalDriveSizeInBytes(selectedDrive);
            if (!diskSize) {
                SetDlgItemText(hwnd, IDC_RESULT, L"Failed to read drive size.");
                SetDlgItemText(hwnd, IDC_STATUS, L"Error");
                break;
            }

            // Get the selected sector size
            wchar_t sectorSizeText[32] = { 0 };
            HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_SECTOR);
            int sectorSel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            SendMessage(hCombo, CB_GETLBTEXT, sectorSel, (LPARAM)sectorSizeText);
            int sectorSize = _wtoi(sectorSizeText);

            // Calculate sectors based on sector size
            unsigned long long totalSectors = diskSize / sectorSize;
            unsigned long long remainderBytes = diskSize % sectorSize;

            g_lastTotalBytes = diskSize;

            double KiB = diskSize / 1024.0;
            double MiB = KiB / 1024.0;
            double GiB = MiB / 1024.0;
            double TiB = GiB / 1024.0;

            wchar_t bytesWithCommas[64], sectorsWithCommas[64], remainderWithCommas[64];
            FormatNumberWithCommas(diskSize, bytesWithCommas, 64);
            FormatNumberWithCommas(totalSectors, sectorsWithCommas, 64);
            FormatNumberWithCommas(remainderBytes, remainderWithCommas, 64);

            swprintf(g_lastFormattedResult, 2048,
                L"%s Drive Information\r\n════════════════════════════\r\n"
                L"Total Size: %s bytes\r\nSector Size: %d bytes\r\n\r\n"
                L"Total Sectors: %s sectors\r\nRemainder: %s bytes\r\n\r\n"
                L"Size in Different Units:\r\n%.3f KiB\r\n%.6f MiB\r\n%.9f GiB\r\n%.12f TiB",
                selectedDrive, bytesWithCommas, sectorSize,
                sectorsWithCommas, remainderWithCommas,
                KiB, MiB, GiB, TiB);

            SetDlgItemText(hwnd, IDC_RESULT, g_lastFormattedResult);
            SetDlgItemText(hwnd, IDC_STATUS, L"Drive info retrieved");
            break;
        }

        case IDC_COPY:
            if (wcslen(g_lastFormattedResult) > 0) {
                if (OpenClipboard(hwnd)) {
                    EmptyClipboard();
                    size_t len = wcslen(g_lastFormattedResult) + 1;
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
                    if (hMem) {
                        wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
                        wcscpy_s(pMem, len, g_lastFormattedResult);
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                    CloseClipboard();
                    SetDlgItemText(hwnd, IDC_STATUS, L"Result copied to clipboard");
                }
            }
            break;
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"SectorCalcPro";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    // Wider window to accommodate dev panel
    HWND hwnd = CreateWindow(wc.lpszClassName, L"Advanced Sector Calculator",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 450,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
} 
