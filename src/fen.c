#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "search.c"

char testFens[][90] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r6r/1b2k1bq/8/8/7B/8/8/R3K2R b KQ - 3 2",
    "8/8/8/2k5/2pP4/8/B7/4K3 b - d3 0 3",
    "r1bqkbnr/pppppppp/n7/8/8/P7/1PPPPPPP/RNBQKBNR w KQkq - 2 2",
    "r3k2r/p1pp1pb1/bn2Qnp1/2qPN3/1p2P3/2N5/PPPBBPPP/R3K2R b KQkq - 3 2",
    "2kr3r/p1ppqpb1/bn2Qnp1/3PN3/1p2P3/2N5/PPPBBPPP/R3K2R b KQ - 3 2",
    "rnb2k1r/pp1Pbppp/2p5/q7/2B5/8/PPPQNnPP/RNB1K2R w KQ - 3 9",
    "2r5/3pk3/8/2P5/8/2K5/8/8 w - - 5 4",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1",
    "8/3p4/8/K1P4q/8/8/8/7k b - - 0 1",
    "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1",
    "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1",
    "5k2/8/8/8/8/8/8/4K2R w K - 0 1",
    "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1",
    "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1",
    "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1",
    "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1",
    "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1",
    "4k3/1P6/8/8/8/8/K7/8 w - - 0 1",
    "8/P1k5/K7/8/8/8/8/8 w - - 0 1",
    "K1k5/8/P7/8/8/8/8/8 w - - 0 1",
    "8/k1P5/8/1K6/8/8/8/8 w - - 0 1",
    "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1",
};

const int testDepths[] = {1, 2, 3, 4, 5, 6, 1, 1, 1, 1, 1, 1, 1, 3, 3, 6, 6, 6, 6, 6, 6, 4, 4, 6, 5, 6, 6, 6, 7, 4};

const int expectedResults[] = {20, 400, 8902, 197281, 4865609, 119060324, 8, 8, 19, 5, 44, 39, 9, 62379, 89890, 1134888, 2382656, 1015133, 1440467, 661072, 803711, 1274206, 1720476, 3821001, 1004658, 217342, 92683, 2217, 567584, 23527};

const int totalTests = 30;

int getNum(char *str)
{
    int num = 0;
    if (str != NULL)
    {
        int idx = 0;
        while (str[idx] != '\0')
        {
            if (str[idx] >= '0' && str[idx] <= '9')
            {
                num *= 10;
                num += str[idx] - '0';
            }
            else if (str[idx] != ' ' && str[idx] != '\t')
            {
                return num;
            }
            idx++;
        }
    }
    return num;
}

bool cmpStart(int startIdx, char *str, int len, char *cmpStr)
{
    if (str == NULL || startIdx > len)
    {
        return false;
    }
    for (int i = 0; i < strlen(cmpStr); i++)
    {
        if (str[i + startIdx] != cmpStr[i])
        {
            return false;
        }
    }
    return true;
}

char *parseMoves(int *board, struct Data *data, char *str, int idx)
{
    char character = str[idx];
    int len = strlen(str);
    // Poor man Regex
    while (character != '\0')
    {
        if (len - idx >= 4 && character >= 'a' && character <= 'h') // is 1st character a-h and string has 4 or more characters remaining
        {
            if (str[idx + 1] >= '1' && str[idx + 1] <= '8') // is 2nd character 1-8
            {
                if (str[idx + 2] >= 'a' && str[idx + 2] <= 'h') // is 3rd character a-h
                {
                    if (str[idx + 3] >= '1' && str[idx + 3] <= '8') // is 4th character 1-8
                    {
                        // We got a move!
                        struct Move move;
                        move.startSquare = (str[idx + 1] - '1') * 8 + character - 'a';
                        move.endSquare = (str[idx + 3] - '1') * 8 + str[idx + 2] - 'a';
                        move.capturedPiece = board[move.endSquare];
                        enum Pieces piece = board[move.startSquare] & ~1;
                        if (piece == Pawn)
                        {
                            if (move.endSquare == data->enPassant)
                            {
                                move.flag = EnPassantCapture;
                            }
                            else if (abs(move.startSquare - move.endSquare) == offsets[0] * 2)
                            {
                                move.flag = DoublePawnMove;
                            }
                            else if (len - idx > 4) // Length is one extra than the final index, so this is actually checking if there are 5 characters left in the string
                            {
                                switch (str[idx + 4])
                                {
                                case 'n':
                                case 'N':
                                    move.flag = PromoteToKnight;
                                    idx++;
                                    break;
                                case 'b':
                                case 'B':
                                    move.flag = PromoteToBishop;
                                    idx++;
                                    break;
                                case 'r':
                                case 'R':
                                    move.flag = PromoteToRook;
                                    idx++;
                                    break;
                                case 'q':
                                case 'Q':
                                    move.flag = PromoteToQueen;
                                    idx++;
                                    break;
                                default:
                                    move.flag = NoFlag;
                                    break;
                                }
                            }
                        }
                        else if (piece == King)
                        {
                            if (abs(move.startSquare - move.endSquare) == offsets[3] * 2)
                            {
                                move.flag = Castle;
                            }
                        }
                        // Actually play the move, because that was the whole point of this
                        doMove(board, data, move, true);
                        idx += 4;
                    }
                }
            }
        }
        idx++;
        character = str[idx];
    }
    return NULL;
}

char *toFEN(int *board, struct Data *data)
{
    char *fen = malloc(sizeof(char) * 100);
    int idx = 0;
    // Set the piece arangment part of the FEN string
    for (int rank = 0; rank < 8; rank++)
    {
        int emptyCount = 0;
        for (int file = 7; file >= 0; file--)
        {
            if ((board[(rank * 8 + file - 63) * -1] & ~1) != Empty)
            {
                if (emptyCount > 0)
                {
                    fen[idx] = emptyCount + '0';
                    idx++;
                }
                fen[idx] = pieceLookup[board[(rank * 8 + file - 63) * -1]];
                idx++;
                emptyCount = 0;
            }
            else
            {
                emptyCount++;
            }
        }
        if (emptyCount > 0)
        {
            fen[idx] = emptyCount + '0';
            idx++;
        }
        if (rank < 7)
        {
            fen[idx] = '/';
            idx++;
        }
    }
    // Set the turn of the player to move
    fen[idx] = ' ';
    if (data->turn == White)
    {
        fen[idx + 1] = 'w';
    }
    else
    {
        fen[idx + 1] = 'b';
    }
    fen[idx + 2] = ' ';
    idx += 3;
    // Set the castling rights
    if (data->kingSideCastling | data->queenSideCastling)
    {
        if (data->kingSideCastling & WhiteCastling)
        {
            fen[idx] = 'K';
            idx++;
        }
        if (data->queenSideCastling & WhiteCastling)
        {
            fen[idx] = 'Q';
            idx++;
        }
        if (data->kingSideCastling & BlackCastling)
        {
            fen[idx] = 'k';
            idx++;
        }
        if (data->queenSideCastling & BlackCastling)
        {
            fen[idx] = 'q';
            idx++;
        }
    }
    else
    {
        fen[idx] = '-';
        idx++;
    }
    fen[idx] = ' ';
    idx++;
    fen[idx] = '\0';
    // Add the En Passant square
    if (data->enPassant > 0)
    {
        char *enpassant = intSquareToString(data->enPassant);
        sprintf(fen + idx, "%s", enpassant);
        idx += 2;
        free(enpassant);
    }
    else
    {
        fen[idx] = '-';
        idx++;
        fen[idx] = '\0';
    }
    // Finally add the Fifty move rule, and move counter values to the end of the string
    sprintf(fen + idx, " %i %i", data->fiftyMoveRule, data->fullMoveCount);
    return fen;
}

char *parseFEN(int *board, struct Data *data, char *str)
{
    // 7r/pp1PkNpp/8/2p2p2/5B2/P7/1P1RrnPP/R5K1 b - - 0 52 random test string

    // Clear all of the board's data
    memset(board, 0, sizeof(int) * 64);
    unsigned long long *ptr = data->repetitionHistory;
    *data = (struct Data){0};
    data->repetitionHistory = ptr;
    memset(data->repetitionHistory, 0, sizeof(unsigned long long) * 512);
    data->fullMoveCount = 0;
    data->repetitionHistory[0] = zobristHash(board, data);
    data->repetitionEnd = 1;
    data->repeated = false;

    if (str == NULL)
    {
        char *err = strdup("'' is not a valid FEN string");
        return err;
    }

    int idx = -1;
    int boardIdx;
    int file = 0;
    int rank = 7;
    int enPassantRank = 0;
    int enPassantFile = 0;
    int section = 0;
    bool lookForNextSection = false;
    char character = str[0];
    while (character != '\0')
    {
        idx++;
        character = str[idx];
        if (lookForNextSection)
        {
            if (character == ' ')
            {
                lookForNextSection = false;
            }
        }
        else
        {
            switch (section)
            {
            case 0:
                // First part of the FEN string, all the pieces on the board going from 8th rank to 1st rank, ranks being seperated by "/"
                boardIdx = rank * 8 + file;
                switch (character)
                {
                case '/':
                    file = 0, rank--;
                    break;
                case 'p':
                case 'n':
                case 'b':
                case 'r':
                case 'q':
                case 'k':
                    board[boardIdx] = charLookup[character - 'b'] + Black;
                    file++;
                    break;
                case 'P':
                case 'N':
                case 'B':
                case 'R':
                case 'Q':
                case 'K':
                    board[boardIdx] = charLookup[character - 'B'] + White;
                    file++;
                    break;
                case ' ':
                    section = 1;
                    break;
                default:
                    if (character <= '9' && character >= '0')
                    {
                        file += character - '0';
                    }
                }
                if (file >= 8)
                {
                    file = 0;
                }
                if (rank >= 8)
                {
                    lookForNextSection = true;
                    section = 1;
                }
                break;
            case 1:
                // Second part of a FEN string, the player to move. w for white, b for black
                if (character == 'b' || character == 'B')
                {
                    data->turn = Black;
                }
                else
                {
                    data->turn = White;
                }
                lookForNextSection = true;
                section = 2;
                break;
            case 2:
                // Third part of a FEN string, castling rights. k - black kingside, q - black queenside, K - white kingside, Q black queenside
                switch (character)
                {
                case 'k':
                    data->kingSideCastling |= BlackCastling;
                    break;
                case 'q':
                    data->queenSideCastling |= BlackCastling;
                    break;
                case 'K':
                    data->kingSideCastling |= WhiteCastling;
                    break;
                case 'Q':
                    data->queenSideCastling |= WhiteCastling;
                    break;
                case ' ':
                    section = 3;
                    break;
                }
                break;
            case 3:
                // Forth part of a FEN string, the En Passant square
                if (character == '-')
                {
                    lookForNextSection = true;
                    section = 4;
                }
                else if (character >= 'A' && character <= 'H')
                {
                    enPassantFile = character - 'A';
                }
                else if (character >= 'a' && character <= 'h')
                {
                    enPassantFile = character - 'a';
                }
                else if (character >= '1' && character <= '8')
                {
                    enPassantRank = character - '1';
                }
                else if (character == ' ')
                {
                    // En Passant can only happen on the 3rd and 6th rank, so using 0 is fine to represent "No Enpassant"
                    data->enPassant = enPassantRank * 8 + enPassantFile;
                    section = 4;
                }
                break;
            case 4:
                // Fith part of a FEN string, the 50 move rule (half move) counter
                if (character >= '0' && character <= '9')
                {
                    data->fiftyMoveRule *= 10;
                    data->fiftyMoveRule += character - '0';
                }
                else if (character == ' ')
                {
                    section = 5;
                }
                break;
            case 5:
                // Sixth and final part of a FEN string, the full move counter
                if (character >= '0' && character <= '9')
                {
                    data->fullMoveCount *= 10;
                    data->fullMoveCount += character - '0';
                }
                else if (character == ' ')
                {
                    if (strlen(str) > idx + 6 && cmpStart(idx + 1, str, strlen(str), "moves"))
                    {
                        return parseMoves(board, data, str, idx + 6);
                    }
                }
                break;
            }
        }
    }
    return NULL;
}

char *parseGame(int *board, struct Data *data, char *str)
{
    parseFEN(board, data, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    if (str == NULL)
    {
        return NULL;
    }
    if (strlen(str) > 6 && cmpStart(0, str, strlen(str), "moves"))
    {
        parseMoves(board, data, str, 6);
    }
    return NULL;
}

void testPositions(int *board, struct MoveGenData *moveGenData, struct Data *data)
{
    int passed =  0;
    clock_t start, end;
    start = clock();
    for (int testN = 0; testN < totalTests; testN++)
    {
        int expectedPositionCount = expectedResults[testN];
        int depth = testDepths[testN];
        char *fen = testFens[testN];
        parseFEN(board, data, fen);
        // Multi-threaded version of countPositions
        unsigned long long positions = countPositionsMain(board, moveGenData, data, depth, false);
        printf("Test %i:\n\tFEN: %s\n\tDepth: %i\n\tPositions: %llu\n\tExpected: %i\n\t", testN + 1, fen, depth, positions, expectedPositionCount);
        if (expectedPositionCount == positions)
        {
            printf("[PASS]");
            passed++;
        }
        else
        {
            printf("[FAIL]");
        }
        printf(" [%i / %i]\n\n", passed, testN + 1);
        fflush(stdout);
    }
    end = clock();
    double duration = (double)(end - start) / CLOCKS_PER_SEC;
    printf("--------------------\n  Test Complete\n  Time: %.03fs\n  Passed: %i / %i\n  Failed: %i / %i\n--------------------\n", duration, passed, totalTests, totalTests - passed, totalTests);
    fflush(stdout);
}

void printBoard(int *board, struct MoveGenData *moveGenData, struct Data *data)
{
    char boardStr[64];
    for (int i = 0; i < 64; i++)
    {
        boardStr[i] = pieceLookup[board[i]];
    }
    char *filled = fillBoard(boardStr);
    char *fen = toFEN(board, data);
    printf("%s\n FEN: %s\n Hash: %llx\n Eval: %.2f\n\n", filled, fen, zobristHash(board, data), (double)eval(board, moveGenData, White) / 100);
    free(filled);
    free(fen);
}