#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "board.c"

enum MoveFlag
{
    NoFlag = 0,
    EnPassantCapture = 1,
    DoublePawnMove = 2,
    Castle = 3,
    PromoteToKnight = 4,
    PromoteToBishop = 6,
    PromoteToRook = 8,
    PromoteToQueen = 10
};

struct Move
{
    unsigned short startSquare : 6;
    unsigned short endSquare : 6;
    /*
        0  - None
        2  - Pawn
        4  - Knight
        6  - Bishop
        8  - Rook
        10 - Queen
        12 - King
    */
    unsigned short capturedPiece : 4;
    /*
        0  - No flag
        1  - En Passant capture
        2  - Double pawn move
        3  - Castling
        4  - Promote to Knight
        6  - Promote to Bishop
        8  - Promote to Rook
        10 - Promote to Queen
    */
    unsigned short flag : 4;
};

struct MoveGenData
{
    unsigned short check : 1;
    unsigned short doubleCheck : 1;
    unsigned short attackers : 4;
    unsigned short kingLocation : 6;
    unsigned short enemyKingLocation : 6;
    unsigned short legalMoveCount : 8;
    unsigned long long attackingBitBoard;
    unsigned long long blockCheckBitBoard;
    bool endgame;
    int *pinned;
};

const int offsets[] = {
    8,  //    ⬆     0
    -8, //    ⬇     1
    -1, //    ⬅    2
    1,  //    ➡    3
    7,  //    ↖     4
    -9, //    ↙     5
    9,  //    ↗     6
    -7, //    ↘     7
};

unsigned long long rand64()
{
    unsigned long long r = 0;
    for (int i = 0; i < 64; i++)
    {
        r = r * 2 + rand() % 2;
    }
    return r;
}

//  8 * 64 = 512 (1D array implementation)
int movesToEdge[512];

// 64 * 5
unsigned long long possibleMoveBitboards[320];

void init()
{
    // Pre-compute moves to edge
    for (int i = 0; i < 64; ++i)
    {
        int rank = i / 8;
        int file = i - (rank << 3);
        movesToEdge[i << 3] = 7 - rank;
        movesToEdge[(i << 3) + 1] = rank;
        movesToEdge[(i << 3) + 2] = file;
        movesToEdge[(i << 3) + 3] = 7 - file;
        movesToEdge[(i << 3) + 4] = min(file, 7 - rank);
        movesToEdge[(i << 3) + 5] = min(file, rank);
        movesToEdge[(i << 3) + 6] = min(7 - file, 7 - rank);
        movesToEdge[(i << 3) + 7] = min(7 - file, rank);
    };

    // Pre-compute possible moves
    for (int i = 0; i < 5; ++i)
    {
        for (int square = 0; square < 64; ++square)
        {
            unsigned long long bitboard = 0;
            switch (i + 2)
            {
            case King:
                for (int j = 0; j < 8; j++)
                {
                    if (movesToEdge[(square << 3) + j] > 0)
                    {
                        bitboard |= 1ULL << (square + offsets[j]);
                    }
                }
                break;
            default:
                break;
            }
            possibleMoveBitboards[i * 64 + square] = bitboard;
        }
    }

    srand(572);
    // Create Zobrist Hash table
    for (int i = 0; i < 64; ++i)
    {
        for (int piece = 0; piece < 16; ++piece)
        {
            hashTable[(i << 4) + piece] = piece >> 1 == 0 ? 0 : rand64();
        }
    }
}

char *moveToString(struct Move move)
{
    char *str;
    if (move.flag > Castle)
    {
        str = malloc(sizeof(char) * 6);
        str[4] = pieceLookup[move.flag];
        str[5] = '\0';
    }
    else
    {
        str = malloc(sizeof(char) * 5);
        str[4] = '\0';
    }
    char *square1 = intSquareToString(move.startSquare);
    char *square2 = intSquareToString(move.endSquare);
    strncpy(str, square1, 2);
    strncpy(str + 2, square2, 2);
    free(square1);
    free(square2);
    return str;
}

bool compareMove(struct Move moveA, struct Move moveB)
{
    return (moveA.startSquare == moveB.startSquare) && (moveA.endSquare == moveB.endSquare) && (moveA.flag == moveB.flag);
}

void updateAttackedBitboard(int *board, struct MoveGenData *moveGenData, struct Data *data)
{
    for (int square = 0; square < 64; ++square)
    {
        enum Pieces piece = board[square] & ~1;
        enum Colours colour = board[square] & 1;
        if (piece != Empty && colour != data->turn)
        {
            enum Colours otherColour = !colour;
            switch (piece)
            {
            case Pawn:
                // Set a piece diagonal to a pawn as attacked if the pawn is not on the edge of the board
                // If the pawn isn't on the A file
                if ((square) % 8 != 0)
                {
                    int offset = square + offsets[5 - colour];
                    moveGenData->attackingBitBoard |= 1ULL << offset;
                    if (moveGenData->kingLocation == offset)
                    {
                        moveGenData->blockCheckBitBoard |= 1ULL << square;
                        ++moveGenData->attackers;
                    }
                }
                // If the pawn isn't on the H file
                if ((square + 1) % 8 != 0)
                {
                    int offset = square + offsets[7 - colour];
                    moveGenData->attackingBitBoard |= 1ULL << offset;
                    if (moveGenData->kingLocation == offset)
                    {
                        moveGenData->blockCheckBitBoard |= 1ULL << square;
                        ++moveGenData->attackers;
                    }
                }
                break;
            case Knight:
                // Loop through all directions then loop through the offsets which are perpendicular to the original direction
                for (int i = 0; i < 4; ++i)
                {
                    if (movesToEdge[(square << 3) + i] >= 2)
                    {
                        int perpendicularOffset = i > 1 ? 0 : 2;
                        for (int j = perpendicularOffset; j < perpendicularOffset + 2; j++)
                        {

                            if (movesToEdge[(square << 3) + j] > 0)
                            {
                                int offset = square + offsets[i] * 2 + offsets[j];
                                moveGenData->attackingBitBoard |= 1ULL << offset;
                                if (moveGenData->kingLocation == offset)
                                {
                                    moveGenData->blockCheckBitBoard |= 1ULL << square;
                                    ++moveGenData->attackers;
                                }
                            }
                        }
                    }
                }
                break;
            case Bishop:
            case Rook:
            case Queen:
            case King:
                // Sliders
                for (int i = (piece == Bishop ? 4 : 0); i < (piece == Rook ? 4 : 8); i++)
                {
                    // Loop through all directions they can go in
                    for (int j = 1; j <= movesToEdge[(square << 3) + i]; ++j)
                    {
                        int offset = square + offsets[i] * j;
                        moveGenData->attackingBitBoard |= 1ULL << offset;
                        // We hit a piece
                        if ((board[offset] & ~1) != Empty)
                        {
                            // If we hit a friendly piece, stop
                            if ((board[offset] & 1) == colour)
                            {
                                break;
                            }
                            if (moveGenData->kingLocation == offset)
                            {
                                moveGenData->blockCheckBitBoard |= 1ULL << square;
                                ++moveGenData->attackers;
                            }
                            else
                            {
                                // Break if the piece is not the enemy king, because we want to X-Ray through the enemy king
                                break;
                            }
                        }
                        // King can only go 1 square
                        if (piece == King)
                        {
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
}

void updatePinnedPieces(int *board, struct MoveGenData *moveGenData, struct Data *data)
{
    enum Colours colour = data->turn;
    enum Colours otherColour = !colour;
    int kingLocation = moveGenData->kingLocation;
    memset(moveGenData->pinned, 0, sizeof(int) * 64);
    // Loop through all 8 directions from the king's location until an attacker is found or the edge
    for (int i = 0; i < 8; ++i)
    {
        bool attacking = false;
        int attackerLocation;
        int blockingCount = 0;
        for (int j = 1; j <= movesToEdge[(kingLocation << 3) + i]; ++j)
        {
            int idx = kingLocation + offsets[i] * j;
            enum Pieces piece = board[idx] & ~1;
            if (piece != Empty)
            {
                if ((board[idx] & 1) == otherColour)
                {
                    // If it matches any of these, the piece is giving a check
                    if (piece == Queen || (i > 3 && piece == Bishop) || (i < 4 && piece == Rook))
                    {
                        attacking = true;
                        attackerLocation = j;
                        break;
                    }
                }
                // The piece isn't giving check
                ++blockingCount;
            }
        }
        if (attacking)
        {
            // Mark this direction as a pin
            if (blockingCount == 1)
            {
                for (int j = attackerLocation; j > 0; j--)
                {
                    moveGenData->pinned[kingLocation + offsets[i] * j] = abs(offsets[i]);
                }
            }
            // If there is no piece blocking (check) and there isn't double check, mark all the squares in the ray for blocking check
            else if (blockingCount == 0 && !moveGenData->doubleCheck)
            {
                unsigned long long ray = 0;
                for (int j = attackerLocation; j > 0; j--)
                {
                    ray |= 1ULL << (kingLocation + offsets[i] * j);
                }
                moveGenData->blockCheckBitBoard |= ray;
            }
        }
    }
}

void findKings(int *board, struct MoveGenData *moveGenData, struct Data *data)
{
    int pieceCount = 0;
    int enemyPieceCount = 0;
    for (int idx = 0; idx < 64; ++idx)
    {
        enum Pieces piece = board[idx] & ~1;
        enum Colours colour = board[idx] & 1;
        if (piece != Empty)
        {
            if (piece == King)
            {
                if (colour == data->turn)
                {
                    moveGenData->kingLocation = idx;
                }
                else
                {
                    moveGenData->enemyKingLocation = idx;
                }
            }
            else if (colour == data->turn)
            {
                pieceCount++;
            }
            else
            {
                enemyPieceCount++;
            }
        }
    }
    if ((enemyPieceCount + pieceCount) / 2 < 9)
    {
        moveGenData->endgame = true;
    }
}

void getSlidingMoves(int *board, struct MoveGenData *moveGenData, struct Move *legalMoves, enum Pieces piece, enum Colours colour, int square)
{
    // Loop through all 8 directions for sliding pieces, but only horizontally and vertically for rooks and diagonally for bishops
    for (int i = (piece == Bishop ? 4 : 0); i < (piece == Rook ? 4 : 8); i++)
    {
        // If the piece is pinned, limit its movement
        if (moveGenData->pinned[square] == 0 || moveGenData->pinned[square] == abs(offsets[i]))
        {
            // Loop until we hit a piece or get to the edge
            for (int j = 1; j <= movesToEdge[(square << 3) + i]; ++j)
            {
                int idx = square + offsets[i] * j;
                if ((board[idx] & ~1) != Empty)
                {
                    // Break if the piece is the same colour as the piece we are generating moves for otherwise, add the move then break
                    if ((board[idx] & 1) == colour)
                    {
                        break;
                    }
                    // Don't allow the king to take a piece which is defended
                    if (piece == King && moveGenData->attackingBitBoard & 1ULL << idx)
                    {
                        break;
                    }
                    // If the king is in check, only allow moves that block the check or take the checking piece
                    if (!moveGenData->check || moveGenData->blockCheckBitBoard & 1ULL << idx || piece == King)
                    {
                        legalMoves[moveGenData->legalMoveCount] = (struct Move){square, idx, board[idx]};
                        ++moveGenData->legalMoveCount;
                        break;
                    }
                    break;
                }
                // Only allow the king to move one square and only if the square is safe
                if (piece == King)
                {
                    if (!(moveGenData->attackingBitBoard & 1ULL << idx))
                    {
                        legalMoves[moveGenData->legalMoveCount] = (struct Move){square, idx, board[idx]};
                        ++moveGenData->legalMoveCount;
                    }
                    break;
                }
                else if (!moveGenData->check || moveGenData->blockCheckBitBoard & 1ULL << idx)
                {
                    legalMoves[moveGenData->legalMoveCount] = (struct Move){square, idx};
                    ++moveGenData->legalMoveCount;
                }
            }
        }
    }
}

void promotion(int *board, struct MoveGenData *moveGenData, struct Move *legalMoves, int square1, int square2, int colour)
{
    // If the king is in check and promoting doesn't stop check, don't allow it
    if (!moveGenData->check || moveGenData->blockCheckBitBoard & 1ULL << square2)
    {
        // If the pawn's move is to the 1st or 8th rank allow it to promote
        if (square2 > 55 || square2 < 8)
        {
            for (int i = PromoteToKnight; i <= PromoteToQueen; i += 2)
            {
                legalMoves[moveGenData->legalMoveCount] = (struct Move){square1, square2, board[square2], i};
                ++moveGenData->legalMoveCount;
            }
        }
        else
        {
            legalMoves[moveGenData->legalMoveCount] = (struct Move){square1, square2, board[square2]};
            ++moveGenData->legalMoveCount;
        }
    }
}

bool recalcEnPassantPins(int *board, struct MoveGenData *moveGenData, int square1, int square2, int colour)
{
    enum Colours otherColour = !colour;
    int enPassantSquare = square2 + offsets[colour];
    int kingLocation = moveGenData->kingLocation;
    // Reject the move if it doesn't block check
    if (moveGenData->check && !(moveGenData->blockCheckBitBoard & 1ULL << square2))
    {
        // Except if it captures the checking piece
        if (!(moveGenData->blockCheckBitBoard & 1ULL << enPassantSquare))
        {
            return false;
        }
    }
    int pinned = false;
    // Stripped down version of the pin calculating function
#pragma GCC unroll 8
    for (int i = 0; i < 8; ++i)
    {
        bool attacking = false;
        int blockingCount = 0;
        for (int j = 1; j <= movesToEdge[(kingLocation << 3) + i]; ++j)
        {
            int idx = kingLocation + offsets[i] * j;
            int piece = board[idx] & ~1;
            if (idx == enPassantSquare || idx == square1)
            {
                pinned = abs(offsets[i]);
            }
            else if (piece != Empty || idx == square2)
            {
                if ((board[idx] & 1) == otherColour)
                {
                    // If it matches any of these, the piece is giving a check
                    if (piece == Queen || (i > 3 && piece == Bishop) || (i < 4 && piece == Rook))
                    {
                        attacking = true;
                        break;
                    }
                }
                // The piece isn't giving check
                ++blockingCount;
            }
        }
        if (!attacking || blockingCount > 0)
        {
            pinned = false;
        }
        if (pinned != false && pinned != abs(square2 - square1))
        {
            return false;
        }
    }
    return true;
}

void getLegalMoves(int *board, struct MoveGenData *moveGenData, struct Data *data, struct Move *legalMoves, int square)
{
    enum Pieces piece = board[square] & ~1;
    if (piece == Empty)
    {
        return;
    }
    enum Colours colour = board[square] & 1;
    // Don't want to generate moves for the opponent
    if (colour != data->turn)
    {
        return;
    }
    switch (piece)
    {
    case Pawn:
        // fun
        // If the square ahead is empty, consider allowing it to advance
        if ((board[square + offsets[!colour]] & ~1) == Empty)
        {
            // Check if the pawn is not pinned or pinned in a way that advancing it would be legal
            if (moveGenData->pinned[square] == 0 || moveGenData->pinned[square] == offsets[0])
            {
                // Allow the pawn to move 1 square forwards or promote if it can
                promotion(board, moveGenData, legalMoves, square, square + offsets[!colour], colour);
                // If the pawn is on the 2nd and white or is on 7th and is black and the square 2 spaces infront is empty, allow it to move twice
                if ((board[square + offsets[!colour] * 2] & ~1) == Empty && ((colour == White && square < 16) || (colour == Black && square > 47)))
                {
                    if (!moveGenData->check || moveGenData->blockCheckBitBoard & 1ULL << (square + offsets[!colour] * 2))
                    {
                        legalMoves[moveGenData->legalMoveCount] = (struct Move){square, square + offsets[!colour] * 2, Empty, DoublePawnMove};
                        ++moveGenData->legalMoveCount;
                    }
                }
            }
        }
        // Check for captures on both diagonals
        for (int i = 0; i < 2; ++i)
        {
            int square2 = square + offsets[5 + i * 2 - colour];
            // Check for captures and check if the piece isn't on the edge of the board
            if (((board[square2] & ~1) != Empty || (data->enPassant == square2 && data->enPassant != 0)) && (square + i) % 8 != 0) // "null" position for the En Passant square is A1, so if a pawn promotes there, it is treated as an En Passant capture leading to undefined behaviour. Found this out the hard way.
            {
                // Check if the capture square is occupied by an enemy piece
                if ((board[square2] & 1) != colour || (data->enPassant == square2 && data->enPassant != 0 && (board[square2 + offsets[colour]] & 1) != colour)) // Imagine how confused I was when I found this bug
                {
                    // Checking if the pawn is not pinned in a way that moving it would put the king in check
                    if (moveGenData->pinned[square] == 0 || moveGenData->pinned[square] == abs(offsets[5 + i * 2 - colour]))
                    {
                        if (data->enPassant == square2 && data->enPassant != 0) // It's always En Passant
                        {
                            // Recalculate pins for En Passant edge cases, and allow it if it's valid
                            if (recalcEnPassantPins(board, moveGenData, square, square2, colour))
                            {
                                legalMoves[moveGenData->legalMoveCount] = (struct Move){square, square2, Empty, EnPassantCapture};
                                ++moveGenData->legalMoveCount;
                            }
                        }
                        // Capture the piece, and promote if this was done on the promotion rank
                        else
                        {
                            promotion(board, moveGenData, legalMoves, square, square2, colour);
                        }
                    }
                }
            }
        }
        break;
    case Knight:
        // If the knight is pinned... tough luck
        if (moveGenData->pinned[square] > 0)
        {
            return;
        }
        // Loop through all directions then loop through the offsets which are perpendicular to the original direction
#pragma GCC unroll 4
        for (int i = 0; i < 4; ++i)
        {
            if (movesToEdge[(square << 3) + i] >= 2)
            {
                int perpendicularOffset = i > 1 ? 0 : 2;
#pragma GCC unroll 2
                for (int j = perpendicularOffset; j < perpendicularOffset + 2; ++j)
                {

                    if (movesToEdge[(square << 3) + j] > 0)
                    {
                        int offset = square + offsets[i] * 2 + offsets[j];
                        // If the king is not in check of the move blocks the check, allow the move
                        if (!moveGenData->check || moveGenData->blockCheckBitBoard & 1ULL << offset)
                        {
                            // If the piece is empty or there is an enemy piece on it, allow the move
                            if ((board[offset] & ~1) == Empty || (board[offset] & 1) != colour)
                            {
                                legalMoves[moveGenData->legalMoveCount] = (struct Move){square, offset, board[offset]};
                                ++moveGenData->legalMoveCount;
                            }
                        }
                    }
                }
            }
        }
        break;
    case Bishop:
    case Rook:
    case Queen:
        getSlidingMoves(board, moveGenData, legalMoves, piece, colour, square);
        break;
    case King:
        getSlidingMoves(board, moveGenData, legalMoves, piece, colour, square);
        // Making sure the king is not in check, as you can't castle out of check (saddly)
        if (!moveGenData->check)
        {
            // Checking if we can castle king-side
            if (data->kingSideCastling & colour + 1)
            {
                // Checking if the rook exists and is ours
                if ((board[square + offsets[3] * 3] & ~1) == Rook && (board[square + offsets[3] * 3] & 1) == colour)
                {
                    // Making sure there are no pieces blocking and that the king isn't castling into or through check
                    if ((board[square + offsets[3]] & ~1) == Empty && (board[square + offsets[3] * 2] & ~1) == Empty)
                    {
                        if (!(moveGenData->attackingBitBoard & 1ULL << (square + offsets[3])) && !(moveGenData->attackingBitBoard & 1ULL << (square + offsets[3] * 2)))
                        {
                            legalMoves[moveGenData->legalMoveCount] = (struct Move){square, square + offsets[3] * 2, Empty, Castle};
                            ++moveGenData->legalMoveCount;
                        }
                    }
                }
                else
                {
                    // uh... why are we here
                    data->kingSideCastling &= ~(colour + 1);
                }
            }
            if (data->queenSideCastling & colour + 1)
            {
                // Checking if the rook exists and is ours
                if ((board[square + offsets[2] * 4] & ~1) == Rook && (board[square + offsets[2] * 4] & 1) == colour)
                {
                    // Making sure there are no pieces blocking and that the king isn't castling into or through check
                    if ((board[square + offsets[2]] & ~1) == Empty && (board[square + offsets[2] * 2] & ~1) == Empty && (board[square + offsets[2] * 3] & ~1) == Empty)
                    {
                        if (!(moveGenData->attackingBitBoard & 1ULL << (square + offsets[2])) && !(moveGenData->attackingBitBoard & 1ULL << (square + offsets[2] * 2)))
                        {
                            legalMoves[moveGenData->legalMoveCount] = (struct Move){square, square + offsets[2] * 2, Empty, Castle};
                            ++moveGenData->legalMoveCount;
                        }
                    }
                }
                else
                {
                    // uh... why are we here
                    data->kingSideCastling &= ~(colour + 1);
                }
            }
        }
        break;
    }
}

struct Move *updateLegalMoves(int *board, struct MoveGenData *moveGenData, struct Data *data)
{
    struct Move *legalMoves = malloc(sizeof(struct Move) * 218);
    moveGenData->legalMoveCount = 0;
    int *ptr = moveGenData->pinned;
    *moveGenData = (struct MoveGenData){0};
    moveGenData->pinned = ptr;
    findKings(board, moveGenData, data);
    updateAttackedBitboard(board, moveGenData, data);
    if (moveGenData->attackers > 0)
    {
        moveGenData->check = true;
        if (moveGenData->attackers > 1)
        {
            moveGenData->doubleCheck = true;
        }
    }
    updatePinnedPieces(board, moveGenData, data);
    // If it's double check, only the King will have legal moves, so no point calculating any other piece
    if (moveGenData->doubleCheck)
    {
        getLegalMoves(board, moveGenData, data, legalMoves, moveGenData->kingLocation);
    }
    else
    {
#pragma GCC unroll 64 // Who doesn't love free performance?
        for (int square = 0; square < 64; ++square)
        {
            getLegalMoves(board, moveGenData, data, legalMoves, square);
        }
    }
    // Update the state of the game
    if (moveGenData->legalMoveCount < 1)
    {
        if (moveGenData->check)
        {
            data->gameState = WhiteWin;
        }
        else if (moveGenData->check)
        {
            data->gameState = BlackWin;
        }
        else
        {
            data->gameState = Draw;
        }
    }
    else if (data->fiftyMoveRule >= 100)
    {
        data->gameState = Draw;
    }
    else if (data->repeated)
    {
        data->gameState = Draw;
    }
    else
    {
        data->gameState = Ongoing;
    }
    return legalMoves;
}

void doMove(int *board, struct Data *data, struct Move move, bool doDraws)
{
    enum Pieces piece = board[move.startSquare] & ~1;
    enum Colours colour = board[move.startSquare] & 1;
    board[move.startSquare] = Empty;
    data->enPassant = 0;
    ++data->fiftyMoveRule;
    if (move.capturedPiece != Empty)
    {
        data->fiftyMoveRule = 0;
        data->repetitionIDX = data->repetitionEnd;
    }
    if (move.capturedPiece == Rook)
    {
        // Can't castle with no ROOK
        switch (move.endSquare)
        {
        case 0:
            data->queenSideCastling &= ~WhiteCastling;
            break;
        case 7:
            data->kingSideCastling &= ~WhiteCastling;
            break;
        case 56:
            data->queenSideCastling &= ~BlackCastling;
            break;
        case 63:
            data->kingSideCastling &= ~BlackCastling;
            break;
        }
    }
    if (piece == Pawn)
    {
        data->repetitionIDX = data->repetitionEnd;
        data->fiftyMoveRule = 0;
        if (move.flag == DoublePawnMove)
        {
            data->enPassant = move.endSquare + offsets[colour];
        }
        // If the move is a promotion, change the piece to the promotion piece of choice
        else if (move.flag > Castle && (move.endSquare > 55 || move.endSquare < 8))
        {
            piece = move.flag;
        }
        else if (move.flag == EnPassantCapture)
        {
            board[move.endSquare + offsets[colour]] = Empty;
        }
    }
    else if (piece == Rook)
    {
        // Can't castle if THE ROOK moved
        switch (move.startSquare)
        {
        case 0:
            data->queenSideCastling &= ~WhiteCastling;
            break;
        case 7:
            data->kingSideCastling &= ~WhiteCastling;
            break;
        case 56:
            data->queenSideCastling &= ~BlackCastling;
            break;
        case 63:
            data->kingSideCastling &= ~BlackCastling;
            break;
        }
    }
    else if (piece == King)
    {
        // If the king moves, disable castling perms
        if (colour == White)
        {
            data->queenSideCastling &= ~WhiteCastling;
            data->kingSideCastling &= ~WhiteCastling;
        }
        else
        {
            data->queenSideCastling &= ~BlackCastling;
            data->kingSideCastling &= ~BlackCastling;
        }
        // Move THE ROOOOK
        if (move.flag == Castle && abs(move.startSquare - move.endSquare) == offsets[3] * 2)
        {
            // King side castling
            if (move.startSquare < move.endSquare)
            {
                board[move.startSquare + offsets[3]] = Rook + colour;
                board[move.startSquare + offsets[3] * 3] = Empty;
            }
            else
            {
                board[move.startSquare + offsets[2]] = Rook + colour;
                board[move.startSquare + offsets[2] * 4] = Empty;
            }
        }
    }
    board[move.endSquare] = piece + colour;
    data->turn = !data->turn;
    if (doDraws)
    {
        unsigned long long hash = zobristHash(board, data);
        int occurances = 1;
        for (int i = data->repetitionIDX; i < data->repetitionEnd; ++i)
        {
            occurances += data->repetitionHistory[i] == hash;
        }
        data->repetitionHistory[data->repetitionEnd] = hash;
        ++data->repetitionEnd;
        data->repeated = occurances >= 3;
    }
    if (data->turn == White)
    {
        ++data->fullMoveCount;
    }
}

void undoMove(int *board, struct Data *data, struct Move move, struct Data prevData)
{
    *data = prevData;
    enum Pieces piece = board[move.endSquare] & ~1;
    enum Colours colour = board[move.endSquare] & 1;
    // If the move was a promotion, change the piece back to a pawn :(
    if (move.flag > Castle)
    {
        piece = Pawn;
    }
    if (piece == Pawn && move.flag == EnPassantCapture)
    {
        // Un-kill the poor pawn :')
        board[move.endSquare + offsets[colour]] = Pawn + !colour;
    }
    else if (piece == King)
    {
        // Un-castle :O
        if (move.flag == Castle)
        {
            // King side castling
            if (move.startSquare < move.endSquare)
            {
                board[move.startSquare + offsets[3]] = Empty;
                board[move.startSquare + offsets[3] * 3] = Rook + colour;
            }
            else
            {
                board[move.startSquare + offsets[2]] = Empty;
                board[move.startSquare + offsets[2] * 4] = Rook + colour;
            }
        }
    }
    // Revive any captured piece
    board[move.endSquare] = move.capturedPiece;
    // Make the piece go back to where it came from >:)
    board[move.startSquare] = piece + colour;
}