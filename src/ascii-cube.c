// ASCII CUBES - spinning ASCII cubes with a banner title.
//
// NOTE: save this file as UTF-8. With MSVC compile with /utf-8.
// gcc/clang (MinGW included) need no extra flags.

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

// Minimal usleep replacement for Windows (high-resolution waitable timer)
void usleep(__int64 usec) {
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -10 * usec;
    timer = CreateWaitableTimer(NULL, TRUE, NULL);

    if (timer) {
        SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
        WaitForSingleObject(timer, INFINITE);
        CloseHandle(timer);
    }
}
#else
#include <unistd.h>
#endif

#define WIDTH 160
#define HEIGHT 44
#define BUFFER_SIZE (WIDTH * HEIGHT)

// Rotation angles around the three axes
float A = 0.0f;
float B = 0.0f;
float C = 0.0f;

float zBuffer[BUFFER_SIZE];
char buffer[BUFFER_SIZE];

const int backgroundASCIICode = '.';
const int distanceFromCam = 100;
const float K1 = 40.0f;
const float incrementSpeed = 0.6f;

float horizontalOffset;
float verticalOffset = 7.0f;

float x, y, z;
float ooz;
int xp, yp;
int idx;

// ASCII title ("ANSI Shadow" style).
// These are multibyte UTF-8 characters, so the title cannot live in the
// char-per-cell buffer: it is printed directly by render() instead.
const char *title[] = {
    " █████╗ ███████╗ ██████╗██╗██╗    ██████╗██╗   ██╗██████╗ ███████╗███████╗",
    "██╔══██╗██╔════╝██╔════╝██║██║   ██╔════╝██║   ██║██╔══██╗██╔════╝██╔════╝",
    "███████║███████╗██║     ██║██║   ██║     ██║   ██║██████╔╝█████╗  ███████╗",
    "██╔══██║╚════██║██║     ██║██║   ██║     ██║   ██║██╔══██╗██╔══╝  ╚════██║",
    "██║  ██║███████║╚██████╗██║██║   ╚██████╗╚██████╔╝██████╔╝███████╗███████║",
    "╚═╝  ╚═╝╚══════╝ ╚═════╝╚═╝╚═╝    ╚═════╝ ╚═════╝ ╚═════╝ ╚══════╝╚══════╝"
};

#define TITLE_LINES ((int)(sizeof(title) / sizeof(title[0])))

// Width in terminal columns: counts UTF-8 code points, not bytes
static int utf8Width(const char *s) {
    int w = 0;

    for (; *s; s++) {
        if (((unsigned char)*s & 0xC0) != 0x80) {
            w++;
        }
    }

    return w;
}

// Print one title row centered, padded with the background character
static void renderTitleRow(int row) {
    int len = utf8Width(title[row]);
    int left = (WIDTH - len) / 2;
    int right = WIDTH - len - left;

    if (left < 0) {
        left = 0;
    }
    if (right < 0) {
        right = 0;
    }

    for (int i = 0; i < left; i++) {
        putchar(backgroundASCIICode);
    }

    fputs(title[row], stdout);

    for (int i = 0; i < right; i++) {
        putchar(backgroundASCIICode);
    }
}

// Rotated coordinates of a point (i, j, k)
float calculateX(float i, float j, float k) {
    return j * sinf(A) * sinf(B) * cosf(C)
         - k * cosf(A) * sinf(B) * cosf(C)
         + j * cosf(A) * sinf(C)
         + k * sinf(A) * sinf(C)
         + i * cosf(B) * cosf(C);
}

float calculateY(float i, float j, float k) {
    return j * cosf(A) * cosf(C)
         + k * sinf(A) * cosf(C)
         - j * sinf(A) * sinf(B) * sinf(C)
         + k * cosf(A) * sinf(B) * sinf(C)
         - i * cosf(B) * sinf(C);
}

float calculateZ(float i, float j, float k) {
    return k * cosf(A) * cosf(B)
         - j * sinf(A) * cosf(B)
         + i * sinf(B);
}

// Project one surface point onto the screen, using the z-buffer
// to keep only the point closest to the camera
void calculateForSurface(float cubeX, float cubeY, float cubeZ, int ch) {
    x = calculateX(cubeX, cubeY, cubeZ);
    y = calculateY(cubeX, cubeY, cubeZ);
    z = calculateZ(cubeX, cubeY, cubeZ) + distanceFromCam;

    if (z <= 0.0f) {
        return;
    }

    ooz = 1.0f / z;

    // The x factor of 2 compensates for terminal cells being taller than wide
    xp = (int)(WIDTH / 2 + horizontalOffset + K1 * ooz * x * 2);
    yp = (int)(HEIGHT / 2 + verticalOffset + K1 * ooz * y);

    if (xp < 0 || xp >= WIDTH || yp < 0 || yp >= HEIGHT) {
        return;
    }

    idx = xp + yp * WIDTH;

    if (ooz > zBuffer[idx]) {
        zBuffer[idx] = ooz;
        buffer[idx] = (char)ch;
    }
}

// Draw a cube of the given size, shifted horizontally by offset.
// Each face uses a different character.
void drawCube(float size, float offset) {
    horizontalOffset = offset;

    for (float cubeX = -size; cubeX < size;
         cubeX += incrementSpeed) {
        for (float cubeY = -size; cubeY < size;
             cubeY += incrementSpeed) {

            calculateForSurface(cubeX, cubeY, -size, '@');
            calculateForSurface(size, cubeY, cubeX, '$');
            calculateForSurface(-size, cubeY, -cubeX, '~');
            calculateForSurface(-cubeX, cubeY, size, '#');
            calculateForSurface(cubeX, -size, -cubeY, ';');
            calculateForSurface(cubeX, size, cubeY, '+');
        }
    }
}

void render(void) {
    // Move the cursor to the top-left corner (no full clear, avoids flicker)
    printf("\x1b[H");

    for (int row = 0; row < HEIGHT; row++) {
        if (row < TITLE_LINES) {
            // The title rows are printed directly (UTF-8, multibyte)
            renderTitleRow(row);
        } else {
            for (int col = 0; col < WIDTH; col++) {
                putchar(buffer[row * WIDTH + col]);
            }
        }
        putchar('\n');
    }

    fflush(stdout);
}

int main(void) {
#ifdef _WIN32
    // Make the Windows console interpret the output as UTF-8
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Clear the screen and hide the cursor
    printf("\x1b[2J\x1b[?25l");

    while (1) {
        memset(buffer, backgroundASCIICode, sizeof(buffer));
        memset(zBuffer, 0, sizeof(zBuffer));

        drawCube(20.0f, -40.0f);
        drawCube(10.0f, 10.0f);
        drawCube(5.0f, 40.0f);

        render();

        A += 0.05f;
        B += 0.05f;
        C += 0.01f;

        usleep(16000);
    }

    return 0;
}
    