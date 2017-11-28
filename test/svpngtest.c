#include <stdlib.h>
#include "../base/svpng.h"

#define W 256
#define H 256

void test_rgb(void)
{
    unsigned char rgb[W * H * 3], *p = rgb;
    unsigned x, y;
    FILE *fp = fopen("rgb.png", "wb");
    for (x = 0; x < W; x++)
    {
        for (y = 0; y < H; y++)
        {
            *p++ = (unsigned char)x; // R
            *p++ = (unsigned char)y; // G
            *p++ = 128;              // B
        }
    }
    svpng(fp, W, H, rgb, 0);
    fclose(fp);
}

void test_rgba(void)
{
    unsigned char rgba[W * H * 4], *p = rgba;
    unsigned x, y;
    FILE *fp = fopen("rgba.png", "wb");
    for (x = 0; x < W; x++)
    {
        for (y = 0; y < H; y++)
        {
            *p++ = (unsigned char)x;             // R
            *p++ = (unsigned char)y;             // G
            *p++ = 128;                          // B
            *p++ = (unsigned char)((x + y) / 2); // A
        }
    }
    svpng(fp, W, H, rgba, 1);
    fclose(fp);
}

int main(void)
{
    test_rgb();
    test_rgba();
    return 0;
}