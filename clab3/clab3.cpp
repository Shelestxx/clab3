#include "framework.h"
#include "clab3.h"
#include <vector>
#include <ctime>
#include <cstdlib>

#define MAX_LOADSTRING 100

// Структура, що описує стан кожного рухомого кола
struct Circle {
    int x, y;          // Координати центру
    int radius;        // Радіус кола
    int dx, dy;        // Напрямок та крок переміщення (швидкість)
    COLORREF color;    // Колір заливки
    DWORD threadId;    // Ідентифікатор потоку
};

// Глобальні змінні:
HINSTANCE           hInst;
WCHAR               szTitle[MAX_LOADSTRING];
WCHAR               szWindowClass[MAX_LOADSTRING];
HWND                g_hWnd = nullptr;

// Синхронізація та багатопоточність
std::vector<Circle*> g_circles;          // Спільний список кіл
std::vector<HANDLE>  g_hThreads;         // Дескриптори створених потоків
CRITICAL_SECTION    g_cs;               // Критична секція для захисту списку кіл
volatile bool       g_bRunning = true;  // Прапорець життєвого циклу потоків

// Прототипи функцій:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI        CircleThreadProc(LPVOID lpParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Ініціалізація генератора випадкових чисел та критичної секції
    srand((unsigned int)time(nullptr));
    InitializeCriticalSection(&g_cs);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CLAB3, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        DeleteCriticalSection(&g_cs);
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CLAB3));
    MSG msg;

    // Головний цикл повідомлень
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CLAB3));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_CLAB3);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    g_hWnd = hWnd;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

// Функція робочого потоку, що керує рухом одного кола
DWORD WINAPI CircleThreadProc(LPVOID lpParam)
{
    Circle* pCircle = (Circle*)lpParam;

    while (g_bRunning)
    {
        RECT rc;
        GetClientRect(g_hWnd, &rc);

        if (rc.right > 0 && rc.bottom > 0)
        {
            // Оновлюємо позицію
            pCircle->x += pCircle->dx;
            pCircle->y += pCircle->dy;

            // Відбивання по горизонталі
            if (pCircle->dx != 0)
            {
                if (pCircle->x - pCircle->radius <= 0)
                {
                    pCircle->x = pCircle->radius;
                    pCircle->dx = -pCircle->dx; // Реверс напрямку
                }
                else if (pCircle->x + pCircle->radius >= rc.right)
                {
                    pCircle->x = rc.right - pCircle->radius;
                    pCircle->dx = -pCircle->dx; // Реверс напрямку
                }
            }

            // Відбивання по вертикалі
            if (pCircle->dy != 0)
            {
                if (pCircle->y - pCircle->radius <= 0)
                {
                    pCircle->y = pCircle->radius;
                    pCircle->dy = -pCircle->dy; // Реверс напрямку
                }
                else if (pCircle->y + pCircle->radius >= rc.bottom)
                {
                    pCircle->y = rc.bottom - pCircle->radius;
                    pCircle->dy = -pCircle->dy; // Реверс напрямку
                }
            }
        }

        // Запит на перемалювання клієнтської області
        InvalidateRect(g_hWnd, nullptr, FALSE);

        // Затримка ~16 мс для плавної анімації (близько 60 FPS)
        Sleep(16);
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN: // Натискання будь-якої клавіші породжує новий потік
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        if (rc.right <= 100 || rc.bottom <= 100)
            break;

        Circle* pCircle = new Circle();
        pCircle->radius = 15 + rand() % 25; // Радіус від 15 до 40 px

        // Початкові координати у межах вікна
        pCircle->x = pCircle->radius + rand() % (rc.right - 2 * pCircle->radius);
        pCircle->y = pCircle->radius + rand() % (rc.bottom - 2 * pCircle->radius);

        // Випадковий яскравий колір
        pCircle->color = RGB(rand() % 230, rand() % 230, rand() % 230);

        // Швидкість руху від 3 до 6 пікселів
        int speed = 3 + rand() % 4;
        int direction = (rand() % 2 == 0) ? speed : -speed;

        // Варіант 1: рух строго горизонтально АБО вертикально
        if (rand() % 2 == 0)
        {
            pCircle->dx = direction;
            pCircle->dy = 0; // Лише горизонтальний рух
        }
        else
        {
            pCircle->dx = 0; // Лише вертикальний рух
            pCircle->dy = direction;
        }

        // Захист запису у спільний вектор за допомогою критичної секції
        EnterCriticalSection(&g_cs);
        g_circles.push_back(pCircle);
        LeaveCriticalSection(&g_cs);

        // Створення нового робочого потоку
        DWORD thId = 0;
        HANDLE hThread = CreateThread(nullptr, 0, CircleThreadProc, pCircle, 0, &thId);

        if (hThread != nullptr)
        {
            pCircle->threadId = thId;
            EnterCriticalSection(&g_cs);
            g_hThreads.push_back(hThread);
            LeaveCriticalSection(&g_cs);
        }
    }
    break;

    case WM_ERASEBKGND:
        // Перехоплюємо стирання фону, щоб уникнути мерехтіння при подвійній буферизації
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rc;
        GetClientRect(hWnd, &rc);

        // Подвійна буферизація (Double Buffering)
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        // Заливка фону вікна білим кольором
        FillRect(memDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

        // Інформаційний рядок зі статистикою
        EnterCriticalSection(&g_cs);
        wchar_t infoText[128];
        swprintf_s(infoText, L"Активних потоків: %zu | Натискайте будь-які клавіші для додавання кіл", g_hThreads.size());
        SetBkMode(memDC, TRANSPARENT);
        TextOutW(memDC, 12, 12, infoText, (int)wcslen(infoText));

        // Малювання всіх активних кіл
        for (const auto* circle : g_circles)
        {
            HBRUSH hBrush = CreateSolidBrush(circle->color);
            HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, hBrush);
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
            HPEN oldPen = (HPEN)SelectObject(memDC, hPen);

            Ellipse(memDC,
                circle->x - circle->radius,
                circle->y - circle->radius,
                circle->x + circle->radius,
                circle->y + circle->radius);

            SelectObject(memDC, oldPen);
            SelectObject(memDC, oldBrush);
            DeleteObject(hPen);
            DeleteObject(hBrush);
        }
        LeaveCriticalSection(&g_cs);

        // Перенесення зібраного кадру з пам'яті на екран
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

        // Звільнення ресурсів подвійної буферизації
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_DESTROY:
    {
        // 1. Подаємо сигнал усім потокам вийти з циклу
        g_bRunning = false;

        // 2. Коректно дочікуємося завершення та закриваємо дескриптори
        EnterCriticalSection(&g_cs);
        for (HANDLE hThread : g_hThreads)
        {
            WaitForSingleObject(hThread, 500); // Очікування до 500 мс
            CloseHandle(hThread);
        }

        // 3. Звільняємо виділену під структури пам'ять
        for (auto* circle : g_circles)
        {
            delete circle;
        }
        g_circles.clear();
        g_hThreads.clear();
        LeaveCriticalSection(&g_cs);

        // 4. Видаляємо критичну секцію
        DeleteCriticalSection(&g_cs);

        PostQuitMessage(0);
    }
    break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}