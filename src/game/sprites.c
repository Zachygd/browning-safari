/*
 * sprites.c - hit testing and scene map lookups for the animals (sprites.obj)
 */
#include "safari.h"

static int unscale(int pos, int scale)
{
    return (int)((double)pos / ((double)scale / 100.0));
}

/* Is (xpos,ypos) inside the animal's vital area?  The kill zone masks are
 * stored 200 graphics after the animal frames; 0xFF marks a vital spot. */
int CheckKillZone(int which, int xpos, int ypos, int flip, int scale)
{
    int gr = sprites[which].image + 200;
    if (gr < 0 || gr >= MAX_GRAPHICS || !graphic[gr].data) return 0;
    if (scale != 100) {
        xpos = unscale(xpos, scale);
        ypos = unscale(ypos, scale);
    }
    int width = graphic[gr].width;
    int offset = flip == 0 ? width * ypos + xpos : (width - xpos) + width * ypos;
    if (offset < 0 || graphic[gr].height * width <= offset) {
        Log("ERROR CheckKillZone %d, %d, %d, %d, %d", which, xpos, ypos, flip, scale);
        return 0;
    }
    return graphic[gr].data[offset] == 0xff;
}

/* Did the shot land on a non-transparent pixel of the animal? */
int CheckSpriteData(int which, int xpos, int ypos, int flip, int scale)
{
    int gr = sprites[which].image;
    if (gr < 0 || gr >= MAX_GRAPHICS || !graphic[gr].data) return 0;
    if (scale != 100) {
        xpos = unscale(xpos, scale);
        ypos = unscale(ypos, scale);
    }
    int width = graphic[gr].width;
    int offset = flip == 0 ? width * ypos + xpos : (width - xpos) + width * ypos;
    if (offset < 0 || graphic[gr].height * width <= offset) {
        Log("ERROR CheckSpriteData %d, %d, %d, %d, %d", which, xpos, ypos, flip, scale);
        return 0;
    }
    return graphic[gr].data[offset] != 0;
}

/* is the animal in front of the scenery at this screen position? */
int CheckLayer(int which, int xpos, int ypos)
{
    if (!layer_map) return 1;
    long off = (long)ypos * 0x1180 + xpos;
    if (off < 0 || off >= 0x1180L * 0x168) return 0;
    return layer_map[off] <= (uchar)sprites[which].z;
}

/* order the animals far to near (by z) for drawing */
void SortSprites(int num)
{
    for (int i = 0; i < 5; i++) sp_sort[i] = i;
    for (int i = 0; i < num - 1; i++) {
        for (int j = i + 1; j < num; j++) {
            if (sprites[j].active && sprites[j].z < sprites[i].z) {
                int temp = sp_sort[i];
                sp_sort[i] = sp_sort[j];
                sp_sort[j] = temp;
            }
        }
    }
}

int GetSpriteScale(int xpos, int ypos)
{
    (void)xpos;
    if (ypos < 0) ypos = 0;
    if (ypos > 0x167) ypos = 0x167;
    if (!scale_map) {
        Log("GetSpriteScale: NO SCALE MAP DATA");
        return 100;
    }
    return scale_map[ypos * 2];
}

/* path map: one byte per 4x4 cell of the panorama; 0 = blocked,
 * 7 = land only, 0xb = water only */
int CheckSpritePath(int xpos, int ypos, int unused)
{
    (void)unused;
    if (!path_map) return 7;
    if (xpos < 0) return 0;
    if (ypos < 0) return 7;
    if (xpos > 0x1180) return 7;
    if (ypos > 0x168) return 0;
    int offset = (ypos / 4) * 0x460 + xpos / 4;
    if (offset < 0 || offset > 0x189bf) {
        Log("ERROR: CheckSpritePath: offset = %d", offset);
        return 3;
    }
    return path_map[offset];
}
