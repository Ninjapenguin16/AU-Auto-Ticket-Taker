#include <stdio.h>
#include <windows.h>
#include <psapi.h>
#include <interception.h>
#include <stdbool.h>
#include <shellapi.h>
#include <stdatomic.h>

#define IDI_APP_ICON 101

// Custom codes for message loop
#define ID_TRAY_APP_ICON  1001
#define ID_TRAY_EXIT      1002
#define WM_TRAYICON       (WM_USER + 1)

// True: All Good | False: Exit interception thread asap
atomic_bool RunInterceptionThread = ATOMIC_VAR_INIT(true);

// Pixel position and what it should be
typedef struct{
    int x;
    int y;
    COLORREF color;
} pixel;

// Function to check if the active window is gmod.exe
bool is_gmod_active(){
    HWND hwnd = GetForegroundWindow();
    if(hwnd){
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if(process){
            char process_name[MAX_PATH];
            if(GetModuleBaseName(process, NULL, process_name, sizeof(process_name))){
                CloseHandle(process);
                return _stricmp(process_name, "gmod.exe") == 0;
            }
            CloseHandle(process);
        }
    }
    return 0;
}

bool AllColorsMatch(pixel* Pixels, int size){

    // Get the device context of the screen
    HDC hdcScreen = GetDC(NULL);
    if(hdcScreen == NULL){

        MessageBeep(MB_ICONWARNING);

        MessageBox(
            NULL,
            (LPCSTR)"Failed to get device context",
            (LPCSTR)"AU Auto Ticket Taker",
            MB_ICONWARNING | MB_OK | MB_SETFOREGROUND
        );
        return CLR_INVALID;

    }

    // Checks if all pixels match their saved color
    for(int i = 0; i < size; i++)
        if(GetPixel(hdcScreen, Pixels[i].x, Pixels[i].y)){
            ReleaseDC(NULL, hdcScreen);
            return false;
        }

    ReleaseDC(NULL, hdcScreen);

    // All colors matches
    return true;
}

void AcceptTicket(InterceptionContext *context, InterceptionDevice *device, InterceptionStroke *stroke){

    InterceptionMouseStroke mouse_stroke;
    InterceptionKeyStroke *keystroke = (InterceptionKeyStroke *) stroke;

    // Send F3 key press
    keystroke->code = 0x3D; // Scan code for F3
    keystroke->state = INTERCEPTION_KEY_DOWN;
    interception_send(*context, *device, stroke, 1);

    keystroke->state = INTERCEPTION_KEY_UP;
    interception_send(*context, *device, stroke, 1);

    // Slight wait for gmod to free the cursor
    Sleep(50);

    // Start moving mouse
    mouse_stroke.state = INTERCEPTION_MOUSE_MOVE_ABSOLUTE;

    // Move mouse by exact pixel (Can't be precalculated)
    // mouse_stroke.x = 26 * 65535 / GetSystemMetrics(SM_CXSCREEN);
    // mouse_stroke.y = 77 * 65535 / GetSystemMetrics(SM_CYSCREEN);

    // Move mouse by percent of screen (Can be precalculated)
    mouse_stroke.x = 662;
    mouse_stroke.y = 3506;
    mouse_stroke.flags = INTERCEPTION_MOUSE_MOVE_ABSOLUTE;
    interception_send(*context, INTERCEPTION_MOUSE(0), (InterceptionStroke *) &mouse_stroke, 1);

    mouse_stroke.state = INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN;
    interception_send(*context, INTERCEPTION_MOUSE(0), (InterceptionStroke *) &mouse_stroke, 1);

    mouse_stroke.state = INTERCEPTION_MOUSE_LEFT_BUTTON_UP;
    interception_send(*context, INTERCEPTION_MOUSE(0), (InterceptionStroke *) &mouse_stroke, 1);

    // Allow gmod time to see the press
    Sleep(30);

    // Send F3 key press again to exit free mouse
    keystroke->state = INTERCEPTION_KEY_DOWN;
    interception_send(*context, *device, stroke, 1);

    keystroke->state = INTERCEPTION_KEY_UP;
    interception_send(*context, *device, stroke, 1);

}

// Displays the context menu at the given point
void ShowContextMenu(HWND hwnd, POINT pt){
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// Handles messages sent to the hidden window
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch(uMsg){
        case WM_TRAYICON:
            if(LOWORD(lParam) == WM_RBUTTONUP){
                POINT pt;
                GetCursorPos(&pt);
                ShowContextMenu(hwnd, pt);
            }
            break;
        case WM_COMMAND:
            switch(LOWORD(wParam)){
                case ID_TRAY_EXIT:
                    PostQuitMessage(0);
                    break;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

DWORD WINAPI Interception(){

    // Initialize interception library
    InterceptionContext context = interception_create_context();
    InterceptionDevice device;
    InterceptionStroke stroke;
    InterceptionKeyStroke *keystroke = (InterceptionKeyStroke *) &stroke;

    interception_set_filter(context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_DOWN | INTERCEPTION_FILTER_KEY_UP);

    bool LDown = false; // 0x26
    bool CommaDown = false; // 0x33

    unsigned short CurCode = 0;

    // While main thread says to not stop
    while(atomic_load(&RunInterceptionThread)){

        // Wait for user input with 100ms timeout
        device = interception_wait_with_timeout(context, 100);

        // Reads the user input
        interception_receive(context, device, &stroke, 1);

        // Checks that the user input is from the keyboard
        if(interception_is_keyboard(device)){

            // Gets the scancode of the key pressed
            CurCode = keystroke->code;

            //printf("0x%02X\n", CurCode);

            // Updates key down variables
            if(CurCode == 0x26)
                LDown = (keystroke->state == INTERCEPTION_KEY_DOWN);
            else if(CurCode == 0x33)
                CommaDown = (keystroke->state == INTERCEPTION_KEY_DOWN);

            // Check that 'L' and ',' are pressed
            if(LDown && CommaDown){

                MessageBox(
                    NULL,
                    (LPCSTR)"Combo Seen",
                    (LPCSTR)"AU Auto Ticket Taker",
                    MB_ICONINFORMATION | MB_OK | MB_SETFOREGROUND
                );

                // Check if the active window is gmod.exe before passing keystroke
                if(is_gmod_active())
                    AcceptTicket(&context, &device, &stroke);

            }

            interception_send(context, device, &stroke, 1);

        }

    }

    // Destroy Interception context
    interception_destroy_context(context);

    return 0;

}

bool InstanceAlreadyRunning(){

    // Create a named mutex
    HANDLE hMutex = CreateMutex(NULL, FALSE, "AuAutoTicketTakerMutex");

    // Check if the mutex creation failed
    if(hMutex == NULL){
        MessageBeep(MB_ICONERROR);

        MessageBox(
            NULL,
            (LPCSTR)"Error checking whether an instance of the program is already running",
            (LPCSTR)"AU Auto Ticket Taker",
            MB_ICONERROR | MB_OK | MB_SETFOREGROUND
        );

        return true;
    }

    // Check if another instance is already running
    if(GetLastError() == ERROR_ALREADY_EXISTS){
        // Clean up
        CloseHandle(hMutex);

        MessageBeep(MB_ICONINFORMATION);
        
        MessageBox(
            NULL,
            (LPCSTR)"Another instance of the program is already running",
            (LPCSTR)"AU Auto Ticket Taker",
            MB_ICONINFORMATION | MB_OK | MB_SETFOREGROUND
        );

        return true;
    }

    return false;

}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){

    if(InstanceAlreadyRunning())
        return 1;

    DWORD InterceptionThreadID;

    // Start separate thread for catching interceptions, message proccessing left on main thread
    HANDLE InterceptionThread = CreateThread(NULL, 0, Interception, (LPVOID)1, 0, &InterceptionThreadID);

    if(InterceptionThread == NULL){

        MessageBeep(MB_ICONERROR);
        
        MessageBox(
            NULL,
            (LPCSTR)"Failed to start interception thread",
            (LPCSTR)"AU Auto Ticket Taker",
            MB_ICONERROR | MB_OK | MB_SETFOREGROUND
        );
        return 1;

    }

    HWND hwnd;
    MSG msg;
    WNDCLASS wc = {0};

    // Define the window class
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "AUTicketTakerClass";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));

    // Register the window class
    RegisterClass(&wc);

    // Create a hidden window
    hwnd = CreateWindow("AUTicketTakerClass", "AuTicketTaker", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);

    NOTIFYICONDATA nid;

    // Initialize NOTIFYICONDATA structure
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    //nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    strcpy(nid.szTip, "AUTicketTaker");

    // Add the icon to the system tray
    Shell_NotifyIcon(NIM_ADD, &nid);

    // Message loop
    while(GetMessage(&msg, NULL, 0, 0)){

        TranslateMessage(&msg);
        DispatchMessage(&msg);

    }

    // Tell interception function to exit asap
    atomic_store(&RunInterceptionThread, false);

    // Wait till interception function exits
    WaitForSingleObject(InterceptionThread, INFINITE);

    // Fully close out interception thread
    CloseHandle(InterceptionThread);

    // Remove the icon from the system tray
    Shell_NotifyIcon(NIM_DELETE, &nid);

    return 0;
}