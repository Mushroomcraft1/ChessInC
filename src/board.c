#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Debugging macros */

// #include <pthread.h>
// static int memoryAllocationCounter = 0;
// pthread_mutex_t mem;

// #define malloc(size) ({        \
//     void *ptr = malloc(size);       \
//     if (ptr)                        \
//     {                               \
//         pthread_mutex_lock(&mem); \
//         memoryAllocationCounter++;  \
//         pthread_mutex_unlock(&mem); \
//     }                               \
//     ptr;                            \
// })

// #define free(ptr)              \
//     {                               \
//         free(ptr);                  \
//         pthread_mutex_lock(&mem); \
//         memoryAllocationCounter--;  \
//         pthread_mutex_unlock(&mem); \
//     }

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

enum Pieces
{
    Empty = 0,
    Pawn = 2,
    Knight = 4,
    Bishop = 6,
    Rook = 8,
    Queen = 10,
    King = 12
};

enum Colours
{
    White = 1,
    Black = 0
};

enum CastlingRights
{
    BlackCastling = 1,
    WhiteCastling = 2
};

enum GameState
{
    WhiteWin = 1,
    BlackWin = -1,
    Draw = 0,
    Ongoing = -2
};

struct Square
{
    unsigned short rank : 6;
    unsigned short file : 6;
};

struct Data
{
    // 4-byte bitfield of all of the extra data for the game
    unsigned short fiftyMoveRule : 7;
    short fullMoveCount;
    unsigned short enPassant : 6;
    unsigned short repetitionIDX : 14;
    unsigned short repetitionEnd : 14;
    bool repeated : 1;
    unsigned long long *repetitionHistory;
    /*
        0 - Both can't
        1 - Black can
        2 - White can
        3 - Both can
    */
    unsigned short kingSideCastling : 2;
    /*
        0 - Both can't
        1 - Black can
        2 - White can
        3 - Both can
    */
    unsigned short queenSideCastling : 2;
    unsigned short turn : 1;
    /*
       -2 - Ongoing
        1 - WhiteWin
        0 - Draw
       -1 - BlackWIn
    */
    short gameState : 2;
};

const char boardTemplate[] = "   ---------------------------------\n 8 |   |   |   |   |   |   |   |   |\n   ---------------------------------\n 7 |   |   |   |   |   |   |   |   |\n   ---------------------------------\n 6 |   |   |   |   |   |   |   |   |\n   ---------------------------------\n 5 |   |   |   |   |   |   |   |   |\n   ---------------------------------\n 4 |   |   |   |   |   |   |   |   |\n   ---------------------------------\n 3 |   |   |   |   |   |   |   |   |\n   ---------------------------------\n 2 |   |   |   |   |   |   |   |   |\n   ---------------------------------\n 1 |   |   |   |   |   |   |   |   |\n   ---------------------------------\n     a   b   c   d   e   f   g   h\n";
const char pieceLookup[] = "  pPnNbBrRqQkK";
const unsigned short charLookup[] = {Bishop, 0, 0, 0, 0, 0, 0, 0, 0, King, 0, 0, Knight, 0, Pawn, Queen, Rook};

// Values in centipawns
const int pieceValues[] = {
    // Nothing
    0,
    0,
    // Pawn
    -100,
    100,
    // Knight
    -300,
    300,
    // Bishop
    -310,
    310,
    // Rook
    -500,
    500,
    // Queen
    -900,
    900,
    // King
    -20000,
    20000,
};

const int positionalScores[] = {
    // Offset
    0,
    // All black's values are pre-negated for performance (when you are doing something 10,000,000 times per second, the littelest things make the biggest differences)
    // White Pawns
    0, 0, 0, 0, 0, 0, 0, 0, 50, 50, 50, 50, 50, 50, 50, 50, 10, 10, 20, 30, 30, 20, 10, 10, 5, 5, 10, 25, 25, 10, 5, 5, -10, 0, 0, 20, 20, 0, 0, -10, -5, 5, -10, 0, 0, -10, 5, -5, 5, 10, 10, -20, -20, 10, 10, 5, 0, 0, 0, 0, 0, 0, 0, 0,
    // Black Pawns
    0, 0, 0, 0, 0, 0, 0, 0, -5, -10, -10, 20, 20, -10, -10, -5, 5, -5, 10, 0, 0, 10, -5, 5, 10, 0, 0, -20, -20, 0, 0, 10, -5, -5, -10, -25, -25, -10, -5, -5, -10, -10, -20, -30, -30, -20, -10, -10, -50, -50, -50, -50, -50, -50, -50, -50, 0, 0, 0, 0, 0, 0, 0, 0,
    // White Knights
    -50, -60, -40, -30, -30, -40, -60, -50, -40, -20, 0, 0, 0, 0, -20, -40, -30, 0, 10, 15, 15, 10, 0, -30, -30, 5, 15, 20, 20, 15, 5, -30, -30, 0, 15, 20, 20, 15, 0, -30, -30, 5, 10, 15, 15, 10, 5, -30, -40, -20, 0, 5, 5, 0, -20, -40, -50, -60, -40, -30, -30, -40, -60, -50,
    // Black Knights
    50, 60, 40, 30, 30, 40, 60, 50, 40, 20, 0, -5, -5, 0, 20, 40, 30, -5, -10, -15, -15, -10, -5, 30, 30, 0, -15, -20, -20, -15, 0, 30, 30, -5, -15, -20, -20, -15, -5, 30, 30, 0, -10, -15, -15, -10, 0, 30, 40, 20, 0, 0, 0, 0, 20, 40, 50, 60, 40, 30, 30, 40, 60, 50,
    // White Bishops
    -20, -10, -10, -10, -10, -10, -10, -20, -10, 0, 0, 0, 0, 0, 0, -10, -10, 0, 5, 10, 10, 5, 0, -10, -10, 5, 5, 10, 10, 5, 5, -10, -10, 0, 10, 10, 10, 10, 0, -10, -10, 10, 10, 10, 10, 10, 10, -10, -10, 5, 0, 0, 0, 0, 5, -10, -20, -10, -10, -10, -10, -10, -10, -20,
    // Black Bishops
    -20, -10, -10, -10, -10, -10, -10, -20, -10, 5, 0, 0, 0, 0, 5, -10, -10, 10, 10, 10, 10, 10, 10, -10, -10, 0, 10, 10, 10, 10, 0, -10, -10, 5, 5, 10, 10, 5, 5, -10, -10, 0, 5, 10, 10, 5, 0, -10, -10, 0, 0, 0, 0, 0, 0, -10, -20, -10, -10, -10, -10, -10, -10, -20,
    // White Rooks
    0, 0, 0, 0, 0, 0, 0, 0, 5, 10, 10, 10, 10, 10, 10, 5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -25, 0, 0, 0, 0, 0, 0, -25, 0, -50, 15, 5, 15, 0, -50, 0,
    // Black Rooks
    20, 10, 10, 10, 10, 10, 10, 20, 10, -5, 0, 0, 0, 0, -5, 10, 10, -10, -10, -10, -10, -10, -10, 10, 10, 0, -10, -10, -10, -10, 0, 10, 10, -5, -5, -10, -10, -5, -5, 10, 10, 0, -5, -10, -10, -5, 0, 10, 10, 0, 0, 0, 0, 0, 0, 10, 20, 10, 10, 10, 10, 10, 10, 20,
    // White Queens
    -20, -10, -10, -5, -5, -10, -10, -20, -10, 0, 0, 0, 0, 0, 0, -10, -10, 0, 5, 5, 5, 5, 0, -10, -5, 0, 5, 5, 5, 5, 0, -5, 0, 0, 5, 5, 5, 5, 0, -5, -10, 5, 5, 5, 5, 5, 0, -10, -10, 0, 5, 0, 0, 0, 0, -10, -20, -10, -10, -5, -5, -10, -10, -20,
    // Black Queens
    20, 10, 10, 5, 5, 10, 10, 20, 10, 0, 0, 0, 0, -5, 0, 10, 10, 0, -5, -5, -5, -5, -5, 10, 5, 0, -5, -5, -5, -5, 0, 0, 5, 0, -5, -5, -5, -5, 0, 5, 10, 0, -5, -5, -5, -5, 0, 10, 10, 0, 0, 0, 0, 0, 0, 10, 20, 10, 10, 5, 5, 10, 10, 20,
    // White King
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -30, -30, -20, -20, -10, 20, 20, -10, -30, -30, -10, 20, 20, 20, 40, 5, 0, 0, 10, 40, 20,
    // Black King
    -20, -40, -5, 0, 0, -10, -40, -20, -20, -20, 10, 30, 30, 10, -20, -20, 10, 20, 20, 20, 30, 30, 20, 10, 20, 30, 30, 40, 40, 30, 30, 20, 30, 40, 40, 50, 50, 40, 40, 30, 30, 40, 40, 50, 50, 40, 40, 30, 30, 40, 40, 50, 50, 40, 40, 30, 30, 40, 40, 50, 50, 40, 40, 30,
    // Filler for better performance (so that the indexes can be powers of 2)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // White Pawns Endgame
    0, 0, 0, 0, 0, 0, 0, 0, 60, 60, 60, 60, 60, 60, 60, 60, 30, 30, 30, 30, 30, 30, 30, 30, 10, 10, 10, 10, 10, 10, 10, 10, 5, 5, 5, 5, 5, 5, 5, 5, -5, -5, -5, -5, -5, -5, -5, -5, -10, -10, -10, -10, -10, -10, -10, -10, 0, 0, 0, 0, 0, 0, 0, 0,
    // Black Pawns Endgame
    0, 0, 0, 0, 0, 0, 0, 0, 10, 10, 10, 10, 10, 10, 10, 10, 5, 5, 5, 5, 5, 5, 5, 5, -5, -5, -5, -5, -5, -5, -5, -5, -10, -10, -10, -10, -10, -10, -10, -10, -30, -30, -30, -30, -30, -30, -30, -30, -60, -60, -60, -60, -60, -60, -60, -60, 0, 0, 0, 0, 0, 0, 0, 0,
    // White Knights Endgame
    -25, -10, -10, -10, -10, -10, -10, -25, -10, -5, -5, 0, 0, -5, -5, -10, -10, -5, 0, 0, 0, 0, -5, -10, -10, 0, 0, 0, 0, 0, 0, -10, -10, 0, 0, 0, 0, 0, 0, -10, -10, -5, 0, 0, 0, 0, -5, -10, -10, -5, -5, 0, 0, -5, -5, -10, -25, -10, -10, -10, -10, -10, -10, -25,
    // Black Knights Endgame
    25, 10, 10, 10, 10, 10, 10, 25, 10, 5, 5, 0, 0, 5, 5, 10, 10, 5, 0, 0, 0, 0, 5, 10, 10, 0, 0, 0, 0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0, 10, 10, 5, 0, 0, 0, 0, 5, 10, 10, 5, 5, 0, 0, 5, 5, 10, 25, 10, 10, 10, 10, 10, 10, 25,
    // White Bishops Endgame
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Black Bishops Endgame
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // White Rooks Endgame
    0, 0, 0, 20, 20, 0, 0, 0, 0, 20, 20, 20, 20, 20, 20, 0, 0, 20, 0, 0, 0, 0, 20, 0, 20, 20, 0, 0, 0, 0, 20, 20, 20, 20, 0, 0, 0, 0, 20, 20, 0, 20, 0, 0, 0, 0, 20, 0, 0, 20, 20, 20, 20, 20, 20, 0, 0, 0, 0, 20, 20, 0, 0, 0,
    // Black Rooks Endgame
    0, 0, 0, -20, -20, 0, 0, 0, 0, -20, -20, -20, -20, -20, -20, 0, 0, -20, 0, 0, 0, 0, -20, 0, -20, -20, 0, 0, 0, 0, -20, -20, -20, -20, 0, 0, 0, 0, -20, -20, 0, -20, 0, 0, 0, 0, -20, 0, 0, -20, -20, -20, -20, -20, -20, 0, 0, 0, 0, -20, -20, 0, 0, 0,
    // White Queens Endgame
    0, 0, 0, 20, 20, 0, 0, 0, 0, 20, 20, 20, 20, 20, 20, 0, 0, 20, 0, 0, 0, 0, 20, 0, 20, 20, 0, 0, 0, 0, 20, 20, 20, 20, 0, 0, 0, 0, 20, 20, 0, 20, 0, 0, 0, 0, 20, 0, 0, 20, 20, 20, 20, 20, 20, 0, 0, 0, 0, 20, 20, 0, 0, 0,
    // Black Queens Endgame
    0, 0, 0, -20, -20, 0, 0, 0, 0, -20, -20, -20, -20, -20, -20, 0, 0, -20, 0, 0, 0, 0, -20, 0, -20, -20, 0, 0, 0, 0, -20, -20, -20, -20, 0, 0, 0, 0, -20, -20, 0, -20, 0, 0, 0, 0, -20, 0, 0, -20, -20, -20, -20, -20, -20, 0, 0, 0, 0, -20, -20, 0, 0, 0,
    // White King Endgame
    -100, -70, -70, -50, -50, -70, -70, -100, -70, -30, -30, -15, -15, -30, -30, -70, -70, -30, -15, 0, 0, -15, -30, -70, -50, -15, 0, 0, 0, 0, -15, -50, -50, -15, 0, 0, 0, 0, -15, -50, -70, -30, -15, 0, 0, -15, -30, -70, -70, -30, -30, -15, -15, -30, -30, -70, -100, -70, -70, -50, -50, -70, -70, -100,
    // Black King Endgame
    100, 70, 70, 50, 50, 70, 70, 100, 70, 30, 30, 15, 15, 30, 30, 70, 70, 30, 15, 0, 0, 15, 30, 70, 50, 15, 0, 0, 0, 0, 15, 50, 50, 15, 0, 0, 0, 0, 15, 50, 70, 30, 15, 0, 0, 15, 30, 70, 70, 30, 30, 15, 15, 30, 30, 70, 100, 70, 70, 50, 50, 70, 70, 100

};

// 64 * 16
unsigned long long hashTable[1024];

char *intSquareToString(int square)
{
    char *str = malloc(sizeof(char) * 3);
    int rank = square / 8;
    int file = square - rank * 8;
    str[0] = 'a' + file;
    str[1] = '1' + rank;
    str[2] = '\0';
    return str;
}

char *fillBoard(char *str)
{
    int file = 0;
    int rank = 7;
    char *boardStr = malloc(sizeof(char) * strlen(boardTemplate) + 1);
    strcpy(boardStr, boardTemplate);
    for (int i = 0; i < 64; i++)
    {
        boardStr[42 + rank * 74 + file * 4] = str[i];
        file++;
        if (file > 7)
        {
            file = 0;
            rank--;
        }
    }
    return boardStr;
}

struct Square getRankAndFile(int square)
{
    struct Square calculated;
    calculated.rank = square / 8;
    calculated.file = square - calculated.rank * 8;
    return calculated;
}

int distanceBetweenSquares(int square1, int square2)
{
    struct Square a = getRankAndFile(square1);
    struct Square b = getRankAndFile(square2);
    return abs(a.rank - b.rank + a.file - b.file);
}

unsigned long long zobristHash(int *board, struct Data *data)
{
    unsigned long long hash = 0xC32736679CA - data->kingSideCastling - (data->queenSideCastling << 4);
#pragma GCC unroll 64 // Free performance babyyyy
    for (int i = 0; i < 64; ++i)
    {
        hash ^= hashTable[(i << 4) + board[i]];
    }
    return hash;
}