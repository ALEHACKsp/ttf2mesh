#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "ttf2mesh.h"

ttf_t       *ttf = NULL;
ttf_glyph_t *glyph = NULL;
ttf_mesh_t  *mesh = NULL;
unsigned     mode = 0;

bool set_drawing_symbol(char symbol)
{
    int glyph_index = ttf_find_glyph(ttf, symbol);
    if (glyph_index == -1)
    {
        printf("unable to locate symbol '%c' in font\n", symbol);
        return false;
    }
    glyph = &ttf->glyphs[glyph_index];
    ttf_free_mesh(mesh);
    if (ttf_glyph2mesh(glyph, &mesh, TTF_QUALITY_NORMAL, 0) != 0)
    {
        mesh = NULL;
        printf("unable to create mesh\n");
        return false;
    }
    return true;
}

void draw_symbol(unsigned mode)
{
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-0.5, 1, -0.5, 1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    //glEnable(GL_MULTISAMPLE_ARB);

    if (mesh == NULL) return;

    /* Draw contours */
    if (mode == 0)
    {
        int i;
        for (i = 0; i < mesh->outline->ncontours; i++)
        {
            glColor3f(0.0, 0.0, 0.0);
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(2, GL_FLOAT, sizeof(ttf_point_t), &mesh->outline->cont[i].pt->x);
            glDrawArrays(GL_LINE_LOOP, 0, mesh->outline->cont[i].length);
            glDisableClientState(GL_VERTEX_ARRAY);
        }
    }

    /* Draw wireframe/solid */
    if (mode == 1 || mode == 2)
    {
        glColor3f(0.0, 0.0, 0.0);
        glPolygonMode(GL_FRONT_AND_BACK, mode == 1 ? GL_LINE : GL_FILL);
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, &mesh->vert->x);
        glDrawElements(GL_TRIANGLES, mesh->nfaces * 3, GL_UNSIGNED_INT, &mesh->faces->v1);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    glFlush();
}

LONG WINAPI WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static PAINTSTRUCT ps;
    switch (uMsg) 
    {
    case WM_PAINT:
        draw_symbol(mode % 3);
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return 0;

    case WM_SIZE:
        glViewport(0, 0, LOWORD(lParam), HIWORD(lParam));
        PostMessage(hWnd, WM_PAINT, 0, 0);
        return 0;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case 27: /* ESC key */
            PostMessageA(hWnd, WM_QUIT, 0, 0);
            break;
        case ' ': /* Space key */
            mode++;
            PostMessage(hWnd, WM_PAINT, 0, 0);
            break;
        default:
            set_drawing_symbol(wParam);
            PostMessage(hWnd, WM_PAINT, 0, 0);
            break;
        }
        return 0;

    case WM_CLOSE:
        PostMessageA(hWnd, WM_QUIT, 0, 0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

HWND CreateOpenGLWindow(LPCWSTR title, int x, int y, int width, int height, BYTE type, DWORD flags)
{
    int         pf;
    HDC         hDC;
    HWND        hWnd;
    PIXELFORMATDESCRIPTOR pfd;
    static HINSTANCE hInstance = 0;

    /* only register the window class once - use hInstance as a flag. */
    if (!hInstance) {
        WNDCLASSEXW wcex;

        wcex.cbSize = sizeof(WNDCLASSEX);

        wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wcex.lpfnWndProc = WindowProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = NULL;
        wcex.lpszClassName = L"OpenGL";
        wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

        if (!RegisterClassExW(&wcex)) 
        {
            MessageBoxA(NULL, "RegisterClass() failed: Cannot register window class.", "Error", MB_OK);
            return NULL;
        }
    }

    hWnd = CreateWindowW(L"OpenGL", title, WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        x, y, width, height, NULL, NULL, hInstance, NULL);

    if (hWnd == NULL) {
        MessageBoxA(NULL, "CreateWindow() failed:  Cannot create a window.", "Error", MB_OK);
        return NULL;
    }

    hDC = GetDC(hWnd);

    /* there is no guarantee that the contents of the stack that become
    the pfd are zeroed, therefore _make sure_ to clear these bits. */
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | flags;
    pfd.iPixelType = type;
    pfd.cColorBits = 32;

    pf = ChoosePixelFormat(hDC, &pfd);
    if (pf == 0) {
        MessageBoxA(NULL, "ChoosePixelFormat() failed: Cannot find a suitable pixel format.", "Error", MB_OK);
        return 0;
    }

    if (SetPixelFormat(hDC, pf, &pfd) == FALSE) {
        MessageBoxA(NULL, "SetPixelFormat() failed: Cannot set format specified.", "Error", MB_OK);
        return 0;
    }

    DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

    ReleaseDC(hWnd, hDC);

    return hWnd;
}

int APIENTRY WinMain(HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow)
{
    HDC      hDC;   /* device context */
    HGLRC    hRC;   /* opengl context */
    HWND     hWnd;  /* window */
    MSG      msg;   /* message */

    if (ttf_load_from_file("C:\\Windows\\Fonts\\times.ttf", &ttf, false) != 0)
    {
        printf("unable to load font times.ttf\n");
        return 1;
    }
    printf("Font name: %s (%s)\n", ttf->info.family, ttf->info.subfamily);

    if (!set_drawing_symbol('a'))
        return 1;

    hWnd = CreateOpenGLWindow(L"Control keys: Space, Escape, ASCII symbol", 0, 0, 512, 512, PFD_TYPE_RGBA, 0);
    if (hWnd == NULL)
        exit(1);

    hDC = GetDC(hWnd);
    hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);

    ShowWindow(hWnd, nCmdShow);

    while (GetMessage(&msg, hWnd, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    wglMakeCurrent(NULL, NULL);
    ReleaseDC(hWnd, hDC);
    wglDeleteContext(hRC);
    DestroyWindow(hWnd);

    return msg.wParam;
}
