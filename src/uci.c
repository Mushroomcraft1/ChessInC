#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include "fen.c"

#define help 2090324718
#define uci 193507782
#define ucinewgame 157606794
#define isready 2867576470
#define setoption 1752732138
#define name 2090536006
#define optionValue 277698370
#define threadsOption 3758332912
#define pos 193502743
#define position 1290762938
#define go 5863419
#define stop 1053619551
#define d 177673
#define display 315443099
#define debug 256484652
#define clear 255552908
#define q 177686
#define quit 2090665480
#define fen 193491518
#define startpos 1765689381
#define perft 270732646
#define depth 256499866
#define movetime 2397115307
#define infinite 1808929275
#define wtime 279563563
#define btime 254659222
#define winc 2090868182
#define binc 2090113505
#define scores 459181940
#define bitboards 1053619551
#define flush 259128679
#define update 552456360
#define mem 193499140
#define test 2090756197

const unsigned long hash(const char *str)
{
    if (str == NULL)
    {
        return 0;
    }
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

int main()
{
    int *board = malloc(sizeof(enum Pieces) * 64);
    struct Data *data = malloc(sizeof(struct Data));
    struct MoveGenData *moveGenData = malloc(sizeof(struct MoveGenData));
    data->repetitionHistory = malloc(sizeof(unsigned long long) * 512);
    moveGenData->pinned = malloc(sizeof(int) * 64);
    init();
    printf("%lu\n", hash("mem"));
    printf("-----------\n   | \\_        _____ _                       _            _____ \n   /  .\\_     / ____| |                     (_)          / ____|\n  |   ___)   | |    | |__   ___  ___ ___     _ _ __     | |     \n  |    \\     | |    | '_ \\ / _ \\/ __/ __|   | | '_ \\    | |     \n  |  =  |    | |____| | | |  __/\\__ \\__ \\   | | | | |   | |____ \n  /_____\\     \\_____|_| |_|\\___||___/___/   |_|_| |_|    \\_____|\n [_______]   ===================================================\n-----------\n\n(c) Xavier Marcal 2023\n\n");
    fflush(stdout);
    parseGame(board, data, NULL);
    free(updateLegalMoves(board, moveGenData, data));
    char *str = malloc(sizeof(char) * 4096);
    while (true)
    {
        fgets(str, 4096, stdin);
        int length = strlen(str);
        str[length - 1] = '\0';
        char *cmd = strtok(str, " ");
        char *option = strtok(NULL, " ");
        switch (hash(cmd))
        {
        case help:
            printf("----------\n Commands:\n----------\n uci\n ucinewgame\n isready\n setoption name [threads] value ...\n position [fen | startpos] moves ...\n go [perft | depth | movetime | wtime | btime | winc | binc]\n display\n debug [bitboards | scores | update | test | flush]\n clear\n quit\n");
            fflush(stdout);
            break;
        case uci:
            printf("id name Chess in C - V7 - Selective Searching\nid author Xavier Marcal\n\noption name Threads type spin default 4 min 1 max 1024\nuciok\n");
            fflush(stdout);
            break;
        case ucinewgame:
            // Clear hash table
            // Soon™
            break;
        case isready:
            printf("readyok\n");
            fflush(stdout);
            break;
        case setoption:
            if (hash(option) == name)
            {
                char *optionName  = malloc(sizeof(char) * length);
                char *word = strtok(NULL, " ");
                int idx = 0;
                while ((hash(word) != optionValue) && (word != NULL))
                {
                    for (int i = 0; i < strlen(word); ++i)
                    {
                        optionName[idx] = tolower(word[i]);
                        idx++;
                    }
                    word = strtok(NULL, " ");
                }
                optionName[idx] = '\0';
                if (hash(word) == optionValue)
                {
                    switch (hash(optionName))
                    {
                    case threadsOption:
                    {
                        int num = getNum(strtok(NULL, " "));
                        threadCount = max(1, min(1024, num));
                        break;
                    }
                    default:
                        printf("[Error] '%s' is not a valid option. Type 'help' for move info.\n", optionName);
                        fflush(stdout);
                        break;
                    }
                }
                free(optionName);
            }
            break;
        case pos:
        case position:
        {
            if (option == NULL)
            {
                printf("[Error] No option provided. Type 'help' for move info.\n");
                fflush(stdout);
                break;
            }
            char *value = strtok(NULL, "");
            char *err;
            switch (hash(option))
            {
            case fen:
                err = parseFEN(board, data, value);
                if (err != NULL)
                {
                    printf("[Error] %s\n", err);
                    fflush(stdout);
                    free(err);
                }
                break;
            case startpos:
                err = parseGame(board, data, value);
                if (err != NULL)
                {
                    printf("[Error] %s\n", err);
                    fflush(stdout);
                    free(err);
                }
                break;
            default:
                printf("[Error] '%s' is not a valid option. Type 'help' for move info.\n", option);
                fflush(stdout);
                break;
            }
            free(updateLegalMoves(board, moveGenData, data));
            break;
        }
        case go:
        {
            int searchDepth = MIN_DEPTH;
            timeLimit = 0;
            bool infiniteSearch = false;
            int num = 0;
            bool doSearch = true;
            while (option != NULL)
            {
                switch (hash(option))
                {
                case perft:
                {
                    doSearch = false;
                    num = getNum(strtok(NULL, " "));
                    start = clock();
                    unsigned long long positions = countPositionsMain(board, moveGenData, data, num, true);
                    end = clock();
                    double duration = (double)(end - start) / CLOCKS_PER_SEC;
                    double positionsPerSec = positions / duration;
                    printf("\nNodes searched: %llu\nTime: %.03fs (%.0f positions / sec)\n", positions, duration, positionsPerSec);
                    fflush(stdout);
                    break;
                }
                case depth:
                {
                    searchDepth = getNum(strtok(NULL, " "));
                    break;
                }
                case movetime:
                {
                    timeLimit = getNum(strtok(NULL, " "));
                    break;
                }
                case wtime:
                {
                    if (data->turn == White)
                    {
                        timeLimit += getNum(strtok(NULL, " ")) / 40;
                    }
                    break;
                }
                case btime:
                {
                    if (data->turn == Black)
                    {
                        timeLimit += getNum(strtok(NULL, " ")) / 40;
                    }
                    break;
                }
                case winc:
                {
                    if (data->turn == White)
                    {
                        int inc = getNum(strtok(NULL, " "));
                        timeLimit = min(inc, timeLimit);
                    }
                    break;
                }
                case binc:
                {
                    if (data->turn == Black)
                    {
                        int inc = getNum(strtok(NULL, " "));
                        timeLimit = min(inc, timeLimit);
                    }
                    break;
                }
                case infinite:
                {
                    infiniteSearch = true;
                    break;
                }
                }
                option = strtok(NULL, " ");
            }
            if (doSearch)
            {
                start = clock();
                if (timeLimit > 0)
                {
                    timeLimit /= 1000;
                    timeLimit *= CLOCKS_PER_SEC;
                    timeLimit += clock();
                }
                if (infiniteSearch)
                {
                    timeLimit = LONG_MAX;
                }
                // Feed inputs into the search function
                searchMain(board, moveGenData, data, searchDepth, timeLimit, true, false, true, NULL);
                break;
            }
            break;
        }
        case stop:
            timeLimit = 0;
            break;
        case d:
        case display:
            printBoard(board, moveGenData, data);
            fflush(stdout);
            break;
        case debug:
        {
            free(updateLegalMoves(board, moveGenData, data));
            if (option == NULL)
            {
                printf("[Error] no option provided. Type 'help' for move info.\n");
                fflush(stdout);
                break;
            }
            switch (hash(option))
            {
            case scores:
            {
                char boardStr[64];
                char *positionals[24];
                for (int p = 0; p < 24; p++)
                {
                    for (int i = 0; i < 64; i++)
                    {
                        int score = positionalScores[p * 64 - i + 64 + (p < 12 ? 0 : 256)];
                        boardStr[i] = score == 0 ? ' ' : (score > 0 ? '+' : '-');
                    }
                    positionals[p] = fillBoard(boardStr);
                }
                printf("White Pawns:\n%s\nBlack Pawns:\n%s\nWhite Knights:\n%s\nBlack Knights:\n%s\nWhite Bishops:\n%s\nBlack Bishops:\n%s\nWhite Rooks:\n%s\nBlack Rooks:\n%s\nWhite Queens:\n%s\nBlack Queens:\n%s\nWhite King:\n%s\nBlack King:\n%s\nWhite Pawns Endgame:\n%s\nBlack Pawns Endgame:\n%s\nWhite Knights Endgame:\n%s\nBlack Knights Endgame:\n%s\nWhite Bishops Endgame:\n%s\nBlack Bishops Endgame:\n%s\nWhite Rooks Endgame:\n%s\nBlack Rooks Endgame:\n%s\nWhite Queens Endgame:\n%s\nBlack Queens Endgame:\n%s\nWhite King Endgame:\n%s\nBlack King Endgame:\n%s\n", positionals[0], positionals[1], positionals[2], positionals[3], positionals[4], positionals[5], positionals[6], positionals[7], positionals[8], positionals[9], positionals[10], positionals[11], positionals[12], positionals[13], positionals[14], positionals[15], positionals[16], positionals[17], positionals[18], positionals[19], positionals[20], positionals[21], positionals[22], positionals[23]);
                fflush(stdout);
                for (int p = 0; p < 24; p++)
                {
                    free(positionals[p]);
                }
                break;
            }
            case bitboards:
            {
                char boardStr[64];
                for (int i = 0; i < 64; i++)
                {
                    boardStr[i] = moveGenData->attackingBitBoard & 1ULL << i ? '*' : ' ';
                }
                char *attackingBitboard = fillBoard(boardStr);
                for (int i = 0; i < 64; i++)
                {
                    boardStr[i] = moveGenData->blockCheckBitBoard & 1ULL << i ? '*' : ' ';
                }
                char *blockCheckBitboard = fillBoard(boardStr);
                for (int i = 0; i < 64; i++)
                {
                    boardStr[i] = moveGenData->pinned[i] > 0 ? (moveGenData->pinned[i] + '0') : ' ';
                }
                char *pinsArray = fillBoard(boardStr);
                printf("Attacking Bitboard:\n%s\nBlock Check Bitboard:\n%s\nPinned Array:\n%s\n", attackingBitboard, blockCheckBitboard, pinsArray);
                fflush(stdout);
                free(attackingBitboard);
                free(blockCheckBitboard);
                free(pinsArray);
                break;
            }
            case update:
                printf("Updated, now %i legal moves.\n", moveGenData->legalMoveCount);
                fflush(stdout);
                break;
            case mem:
                break;
            case test:
                testPositions(board, moveGenData, data);
                break;
            case flush:
                printf("Flushed\n");
                fflush(stdout);
                break;
            default:
                printf("[Error] '%s' is not a valid option. Type 'help' for move info.\n", option);
                fflush(stdout);
                break;
            }
            break;
        }
        case clear:
            system("cls");
            break;
        case q:
        case quit:
            return 0;
        default:
            printf("[Error] '%s' is not a valid command. Type 'help' for move info.\n", cmd);
            fflush(stdout);
            break;
        }
    }
}