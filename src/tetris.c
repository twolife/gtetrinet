/*
 *  GTetrinet
 *  Copyright (C) 1999, 2000  Ka-shu Wong (kswong@zip.com.au)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <string.h>

#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "misc.h"


TETRISBLOCK b1[2] = {
    {
        {1,1,1,1},
        {0,0,0,0},
        {0,0,0,0},
        {0,0,0,0}
    }, {
        {0,0,1,0},
        {0,0,1,0},
        {0,0,1,0},
        {0,0,1,0}
    }
};

TETRISBLOCK b2[1] = {
    {
        {0,2,2,0},
        {0,2,2,0},
        {0,0,0,0},
        {0,0,0,0}
    }
};

TETRISBLOCK b3[4] = {
    {
        {0,0,3,0},
        {0,0,3,0},
        {0,3,3,0},
        {0,0,0,0}
    }, {
        {0,3,0,0},
        {0,3,3,3},
        {0,0,0,0},
        {0,0,0,0}
    }, {
        {0,3,3,0},
        {0,3,0,0},
        {0,3,0,0},
        {0,0,0,0}
    }, {
        {0,3,3,3},
        {0,0,0,3},
        {0,0,0,0},
        {0,0,0,0}
    }
};

TETRISBLOCK b4[4] = {
    {
        {0,4,0,0},
        {0,4,0,0},
        {0,4,4,0},
        {0,0,0,0}
    }, {
        {0,4,4,4},
        {0,4,0,0},
        {0,0,0,0},
        {0,0,0,0}
    }, {
        {0,4,4,0},
        {0,0,4,0},
        {0,0,4,0},
        {0,0,0,0}
    }, {
        {0,0,0,4},
        {0,4,4,4},
        {0,0,0,0},
        {0,0,0,0}
    }
};


TETRISBLOCK b5[2] = {
    {
        {0,0,5,0},
        {0,5,5,0},
        {0,5,0,0},
        {0,0,0,0}
    }, {
        {0,5,5,0},
        {0,0,5,5},
        {0,0,0,0},
        {0,0,0,0}
    }
};

TETRISBLOCK b6[2] = {
    {
        {0,1,0,0},
        {0,1,1,0},
        {0,0,1,0},
        {0,0,0,0}
    }, {
        {0,0,1,1},
        {0,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    }
};
TETRISBLOCK b7[4] = {
    {
        {0,0,2,0},
        {0,2,2,0},
        {0,0,2,0},
        {0,0,0,0}
    }, {
        {0,0,2,0},
        {0,2,2,2},
        {0,0,0,0},
        {0,0,0,0}
    }, {
        {0,2,0,0},
        {0,2,2,0},
        {0,2,0,0},
        {0,0,0,0}
    }, {
        {0,2,2,2},
        {0,0,2,0},
        {0,0,0,0},
        {0,0,0,0}
    }
};
static TETRISBLOCK *blocks[7] = { b1, b2, b3, b4, b5, b6, b7 };
static int blockcount[7] = { 2, 1, 4, 4, 2, 2, 4 };

static int blocknum = -1, blockorient; /* which block */
static int blockx, blocky; /* current location of block */

static int blockobstructed (FIELD field, int block, int orient, int bx, int by);
static int obstructed (FIELD field, int x, int y);
static void placeblock (FIELD field, int block, int orient, int bx, int by);

void tetris_drawcurrentblock (void)
{
    FIELD field;
    copyfield (field, fields[playernum]);
    if (blocknum >= 0)
        placeblock (field, blocknum, blockorient, blockx, blocky);
    fields_drawfield (playerfield(playernum), field);
}

int tetris_makeblock (int block, int orient)
{
    blocknum = block;
    blockorient = orient;
    blockx = FIELDWIDTH/2-2;
    blocky = 0;

    if (block >= 0 &&
        blockobstructed (fields[playernum], blocknum,
                         blockorient, blockx, blocky))
    {
        /* player is dead */
        tetrinet_playerlost ();
        blocknum = -1;
        return 1;
    }
    else return 0;
}

int tetris_randomorient (int block)
{
    return randomnum (blockcount[block]);
}

P_TETRISBLOCK tetris_getblock (int block, int orient)
{
    return blocks[block][orient];
}

/* returns -1 if block solidifies, 0 otherwise */
int tetris_blockdown (void)
{
    if (blocknum < 0) return 0;
    /* move the block down one */
    if (blockobstructed (fields[playernum], blocknum,
                         blockorient, blockx, blocky+1))
    {
        /* cant move down */
#ifdef DEBUG
        printf ("blockobstructed: %d %d\n", blockx, blocky);
#endif
        return -1;
    }
    else {
        blocky ++;
        return 0;
    }
}

void tetris_blockmove (int dir)
{
    if (blocknum < 0) return;
    if (blockobstructed (fields[playernum], blocknum,
                         blockorient, blockx+dir, blocky))
    /* do nothing */;
    else blockx += dir;
}

void tetris_blockrotate (int dir)
{
    int neworient = blockorient + dir;
    if (blocknum < 0) return;
    if (neworient >= blockcount[blocknum]) neworient = 0;
    if (neworient < 0) neworient = blockcount[blocknum] - 1;
    switch (blockobstructed (fields[playernum], blocknum,
                             neworient, blockx, blocky))
    {
    case 1: return; /* cant rotate if obstructed by blocks */
    case 2: /* obstructed by sides - move block away if possible */
        {
            int shifts[4] = {1, -1, 2, -2};
            int i;
            for (i = 0; i < 4; i ++) {
                if (!blockobstructed (fields[playernum], blocknum,
                                      neworient, blockx+shifts[i],
                                      blocky))
                {
                    blockx += shifts[i];
                    goto end;
                }
            }
            return; /* unsuccessful */
        }
    }
end:
    blockorient = neworient;
}

void tetris_blockdrop (void)
{
    if (blocknum < 0) return;
    while (tetris_blockdown () == 0);
}

void tetris_addlines (int count, int type)
{
    int x, y, n, i;
    FIELD field;
    copyfield (field, fields[playernum]);
    for (i = 0; i < count; i ++) {
        /* check top row */
        for (x = 0; x < FIELDWIDTH; x ++) {
            if (field[0][x]) {
                /* player is dead */
                tetrinet_playerlost ();
                return;
            }
        }
        /* move everything up one */
        for (y = 0; y < FIELDHEIGHT-1; y ++) {
            for (x = 0; x < FIELDWIDTH; x ++)
                field[y][x] = field[y+1][x];
        }
        /* generate a random line with spaces in it */
        switch (type) {
        case 1: /* addline lines */
            /* ### This is how the original tetrinet seems to do an add line */
            for (x = 0; x < FIELDWIDTH; x ++)
                field[FIELDHEIGHT-1][x] = randomnum(6);
            field[FIELDHEIGHT-1][randomnum(FIELDWIDTH)] = 0;
            /* ### Corrected by Pihvi */
            break;
        case 2: /* classicmode lines */
            /* fill up the line */
            for (x = 0; x < FIELDWIDTH; x ++)
                field[FIELDHEIGHT-1][x] = randomnum(5) + 1;
            /* add a single space */
            field[FIELDHEIGHT-1][randomnum(FIELDWIDTH)] = 0;
            break;
        }
    }
    tetrinet_updatefield (field);
}

/* this function removes full lines */
int tetris_removelines (char *specials)
{
    int x, y, o, c = 0, i;
    FIELD field;
    if (!playing) return 0;
    copyfield (field, fields[playernum]);
    /* remove full lines */
    for (y = 0; y < FIELDHEIGHT; y ++) {
        o = 0;
        /* count holes */
        for (x = 0; x < FIELDWIDTH; x ++)
            if (field[y][x] == 0) o ++;
        if (o) continue; /* if holes */
        /* no holes */
        /* increment line count */
        c ++;
        /* grab specials */
        if (specials)
            for (x = 0; x < FIELDWIDTH; x ++)
                if (field[y][x] > 5)
                    *specials++ = field[y][x];
        /* move field down */
        for (i = y-1; i >= 0; i --)
            for (x = 0; x < FIELDWIDTH; x ++)
                field[i+1][x] = field[i][x];
        /* clear top line */
        for (x = 0; x < FIELDWIDTH; x ++)
            field[0][x] = 0;
    }
    if (specials) *specials = 0; /* null terminate */
    if (c) tetrinet_updatefield (field);
    return c;
}

void tetris_solidify (void)
{
    FIELD field;
    copyfield (field, fields[playernum]);
    if (blocknum < 0) return;
    if (blockobstructed (field, blocknum, blockorient, blockx, blocky)) {
        /* move block up until we get a free spot */
        for (blocky --; blocky >= 0; blocky --)
            if (!blockobstructed (field, blocknum, blockorient, blockx, blocky))
            {
                placeblock (field, blocknum, blockorient, blockx, blocky);
                break;
            }
        if (blocky < 0) {
            /* no space - player has lost */
            tetrinet_playerlost ();
            blocknum = -1;
            return;
        }
    }
    else {
        placeblock (field, blocknum, blockorient, blockx, blocky);
    }
    tetrinet_updatefield (field);
    blocknum = -1;
}

static int blockobstructed (FIELD field, int block, int orient, int bx, int by)
{
    int x, y, side = 0;
    for (y = 0; y < 4; y ++)
        for (x = 0; x < 4; x ++)
            if (blocks[block][orient][y][x]) {
                switch (obstructed (field, bx+x, by+y)) {
                case 0: continue;
                case 1: return 1;
                case 2: side = 2;
                }
            }
    return side;
}

static int obstructed (FIELD field, int x, int y)
{
    if (x < 0) return 2;
    if (x >= FIELDWIDTH) return 2;
    if (y < 0) return 1;
    if (y >= FIELDHEIGHT) return 1;
    if (field[y][x]) return 1;
    return 0;
}

static void placeblock (FIELD field, int block, int orient, int bx, int by)
{
    int x, y;
    for (y = 0; y < 4; y ++)
        for (x = 0; x < 4; x ++) {
            if (blocks[block][orient][y][x])
                field[y+by][x+bx] = blocks[block][orient][y][x];
        }
}

void copyfield (FIELD dest, FIELD src)
{
    memcpy ((void *)dest, (void *)src, FIELDHEIGHT*FIELDWIDTH);
}
