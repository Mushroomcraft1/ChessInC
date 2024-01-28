#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include "moves.c"

struct MoveScore
{
    struct Move move;
    int score;
};

struct Stats
{
    unsigned long long nodes;
};

struct Package
{
    int *board;
    struct MoveGenData moveGenData;
    struct Data data;
    int threadCount;
    int depth;
    bool log;
    bool useTime;
};

#define MIN_DEPTH 3
#define SEL_SEARCH 4

pthread_mutex_t idMut, workIDXMut, packageMut, positionsMut;
clock_t start, end;
double timeLimit = 0;
int threadCount = 4;
unsigned long long totalPositions;
int workIDX = 0;
int threadID = 0;
struct Stats stats;
struct Move workQueue[218];
int results[218];
bool completed[218];
struct MoveScore *lines[218];

int popCount(unsigned long long x)
{
    int count = 0;
    while (x)
    {
        ++count;
        x &= x - 1;
    }
    return count;
}

int kingSafety(int colour, int kingLocation, unsigned long long attackingBitboard)
{
    int safety = 0;
    safety -= popCount(attackingBitboard & possibleMoveBitboards[256 + kingLocation]) * 20;
    return safety;
}

int eval(int *board, struct MoveGenData *moveGenData, int colour)
{
    int eval = 0;
    eval -= kingSafety(colour, moveGenData->kingLocation, moveGenData->attackingBitBoard);
    for (int square = 0; square < 64; ++square)
    {
        int piece = board[square] & ~1;
        if (piece != Empty)
        {
            int pieceColour = board[square] & 1;
            /*
                (piece != Empty) * eval
                Branchless for if (piece != Empty)
                (Not faster)
            */
            eval += (pieceValues[piece + pieceColour] + positionalScores[((piece - pieceColour) << 6) + (moveGenData->endgame << 10) - square]);
        }
    }
    // If you are winning and its the endgame, get your king closer to the other king to assist with checkmate
    if (moveGenData->endgame && ((eval > 4 && colour == 1) || (eval <= 4 && colour == 0)))
    {
        eval -= distanceBetweenSquares(moveGenData->kingLocation, moveGenData->enemyKingLocation) * 50;
    }
    else if (moveGenData->endgame)
    {
        eval += distanceBetweenSquares(moveGenData->kingLocation, moveGenData->enemyKingLocation) * 50;
    }
    return eval * (colour * 2 - 1);
}

// Yea, we get some BIG numbers
unsigned long long countPositions(int *board, struct MoveGenData *moveGenData, struct Data *data, int depth, bool log)
{
    if (depth < 1)
    {
        return 1;
    }
    unsigned long long positions = 0;
    struct Move *moves = updateLegalMoves(board, moveGenData, data);
    int oldMoveCount = moveGenData->legalMoveCount;
    for (int i = 0; i < oldMoveCount; ++i)
    {
        struct Move move = moves[i];
        unsigned long long movePositions = 1;
        struct Data prevData = *data;
        doMove(board, data, move, false);
        movePositions = countPositions(board, moveGenData, data, depth - 1, false);
        undoMove(board, data, move, prevData);
        if (log)
        {
            char *str = moveToString(move);
            printf("%s: %llu\n", str, movePositions);
            fflush(stdout);
            free(str);
        }
        positions += movePositions;
    }
    free(moves);
    return positions;
}

void *countPositionsWorker(void *arg)
{
    pthread_mutex_lock(&idMut);
    int id = threadID;
    threadID++;
    pthread_mutex_unlock(&idMut);

    pthread_mutex_lock(&packageMut);
    struct Package *package = (struct Package *)arg;
    struct Data *data = malloc(sizeof(struct Data));
    struct MoveGenData *moveGenData = malloc(sizeof(struct MoveGenData));
    moveGenData->pinned = malloc(sizeof(int) * 64);
    int *board = malloc(sizeof(int) * 64);
    int depth = package->depth;
    bool log = package->log;
    memcpy(board, package->board, sizeof(int) * 64);
    memcpy(data, &package->data, sizeof(struct Data));
    pthread_mutex_unlock(&packageMut);

    int totalWork = package->moveGenData.legalMoveCount;
    while (true)
    {
        pthread_mutex_lock(&workIDXMut);
        if (workIDX >= totalWork)
        {
            pthread_mutex_unlock(&workIDXMut);
            break;
        }
        int workID = workIDX;
        workIDX++;
        pthread_mutex_unlock(&workIDXMut);
        struct Move move = workQueue[workID];
        struct Data prevData = *data;
        doMove(board, data, move, false);
        unsigned long long movePositions = countPositions(board, moveGenData, data, depth - 1, false);
        undoMove(board, data, move, prevData);
        if (log)
        {
            char *str = moveToString(move);
            printf("%s: %llu\n", str, movePositions);
            fflush(stdout);
            free(str);
        }
        pthread_mutex_lock(&positionsMut);
        totalPositions += movePositions;
        pthread_mutex_unlock(&positionsMut);
    }

    free(board);
    free(data);
    free(moveGenData->pinned);
    free(moveGenData);
    pthread_exit(NULL);
}

unsigned long long countPositionsMain(int *board, struct MoveGenData *moveGenData, struct Data *data, int depth, bool log)
{
    if (depth == 0)
    {
        return 1;
    }
    struct Move *legalMoves = updateLegalMoves(board, moveGenData, data);
    memcpy(workQueue, legalMoves, sizeof(struct Move) * moveGenData->legalMoveCount);
    free(legalMoves);
    int threadCountLock = threadCount;
    if (depth < 3)
    {
        threadCountLock = 1;
    }

    struct Package *package = malloc(sizeof(struct Package));
    package->board = board;
    package->moveGenData = *moveGenData;
    package->data = *data;
    package->depth = depth;
    package->log = log;
    package->threadCount = threadCountLock;
    threadID = 0;
    workIDX = 0;
    totalPositions = 0;

    pthread_t *threads = malloc(sizeof(pthread_t) * threadCountLock);
    for (int i = 0; i < threadCountLock; ++i)
    {
        pthread_create(&threads[i], NULL, countPositionsWorker, package);
    }

    for (int i = 0; i < threadCountLock; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(package);
    return totalPositions;
}

int negaMax(int *board, struct MoveGenData *moveGenData, struct Data *data, struct Stats *stats, struct MoveScore *line, int depth, int alpha, int beta, int colour, double timeLimit)
{
    if (clock() >= timeLimit)
    {
        return 0;
    }
    struct Move *moves = updateLegalMoves(board, moveGenData, data);
    int oldMoveCount = moveGenData->legalMoveCount;
    if (data->gameState != Ongoing)
    {
        free(moves);
        ++stats->nodes;
        return data->gameState * (-1000000 - depth); // Biiig negative number (good for opponent, bad for player) and add the depth so that it chooses the quickest mating line
    }
    if (depth < 1)
    {
        free(moves);
        ++stats->nodes;
        return -eval(board, moveGenData, colour);
    }
    struct MoveScore bestMove;
    for (int i = 0; i < oldMoveCount; ++i)
    {
        struct Move move = moves[i];
        struct Data prevData = *data;
        doMove(board, data, move, true);
        int score = -negaMax(board, moveGenData, data, stats, line, depth - 1, -beta, -alpha, !colour, timeLimit);
        undoMove(board, data, move, prevData);
        if (score >= beta)
        {
            free(moves);
            return beta;
        }
        // If this is the best score, save it
        if (score > alpha)
        {
            alpha = score;
            bestMove.score = score;
            bestMove.move = move;
            // line[depth] = bestMove;
        }
    }
    free(moves);
    return alpha;
}

int compare(const void *a, const void *b)
{
    struct MoveScore *moveA = (struct MoveScore *)a;
    struct MoveScore *moveB = (struct MoveScore *)b;
    return (moveB->score - moveA->score);
}

void *searchWorker(void *arg)
{
    pthread_mutex_lock(&idMut);
    int id = threadID;
    threadID++;
    pthread_mutex_unlock(&idMut);

    // Initialize all data for the thread
    struct Data *data = malloc(sizeof(struct Data));
    struct MoveGenData *moveGenData = malloc(sizeof(struct MoveGenData));
    moveGenData->pinned = malloc(sizeof(int) * 64);
    int *board = malloc(sizeof(int) * 64);

    // Copy over all data from the main thread's board
    pthread_mutex_lock(&packageMut);
    struct Package *package = (struct Package *)arg;
    int depth = package->depth;
    int totalWork = package->moveGenData.legalMoveCount;
    bool useTime = package->useTime;

    memcpy(board, package->board, sizeof(int) * 64);
    memcpy(data, &package->data, sizeof(struct Data));
    data->repetitionHistory = malloc(sizeof(unsigned long long) * 512);
    memcpy(data->repetitionHistory, package->data.repetitionHistory, sizeof(unsigned long long) * (data->repetitionEnd));
    pthread_mutex_unlock(&packageMut);
    int alpha = -INT_MAX;
    int beta = INT_MAX;

    while (true)
    {
        pthread_mutex_lock(&workIDXMut);
        if (workIDX >= totalWork)
        {
            pthread_mutex_unlock(&workIDXMut);
            break;
        }
        int workID = workIDX;
        ++workIDX;
        pthread_mutex_unlock(&workIDXMut);
        struct Move move = workQueue[workID];
        struct Data prevData = *data;
        int score;

        struct Stats *moveStats = malloc(sizeof(struct Stats));
        // struct MoveScore *line = malloc(sizeof(struct MoveScore) * 50);
        // memset(line, 0, sizeof(struct MoveScore) * 50);
        memset(moveStats, 0, sizeof(struct Stats));

        doMove(board, data, move, false);
        score = -negaMax(board, moveGenData, data, moveStats, NULL, depth - 1, alpha, beta, !data->turn, useTime ? timeLimit : DBL_MAX);
        undoMove(board, data, move, prevData);

        pthread_mutex_lock(&positionsMut);
        stats.nodes += moveStats->nodes;
        pthread_mutex_unlock(&positionsMut);

        free(moveStats);

        if (clock() < timeLimit || !useTime)
        {
            // lines[workID] = line;
            results[workID] = score;
            completed[workID] = true;
        }
        else
        {
            // free(line);
            break;
        }
    }

    free(board);
    free(data->repetitionHistory);
    free(data);
    free(moveGenData->pinned);
    free(moveGenData);
    pthread_exit(NULL);
}

void freeLines(int moves)
{
    return;
    for (int i = 0; i < moves; ++i)
    {
        if (completed[i])
        {
            free(lines[i]);
        }
    }
}

struct MoveScore searchMain(int *board, struct MoveGenData *moveGenData, struct Data *data, int depth, long timeLimit, bool log, bool canQuit, bool recursion, struct MoveScore *ordered)
{
    struct Move *moves = updateLegalMoves(board, moveGenData, data);
    int oldMoveCount = moveGenData->legalMoveCount;

    struct MoveScore bestMove = (struct MoveScore){0};
    struct Move prevBest = (struct Move){0};

    // If there is only one legal move, no point evaluating the score lol
    if (moveGenData->legalMoveCount == 1)
    {
        bestMove.move = moves[0];
    }
    else if (1 < moveGenData->legalMoveCount)
    {
        int selDepth = depth;
        int selSearchCount = min(SEL_SEARCH, oldMoveCount);
        // Reset results
        memset(results, 0, sizeof(struct MoveScore) * oldMoveCount);
        memset(completed, false, sizeof(bool) * oldMoveCount);

        if (ordered != NULL)
        {
            prevBest = ordered[0].move;
            // for (int i = 0; i < selSearchCount; i++)
            // {
            //     struct Data prevData = *data;
            //     doMove(board, data, moves[i], true);
            //     struct MoveScore search = searchMain(board, moveGenData, data, selDepth, timeLimit, false, true, false, NULL);
            //     undoMove(board, data, moves[i], prevData);
            //     if (search.move.startSquare != search.move.endSquare)
            //     {
            //         completed[i] = true;
            //         results[i] = search.score;
            //     }
            // }
        }
        else
        {
            memcpy(workQueue, moves, sizeof(struct Move) * oldMoveCount);
        }

        // Store the scores of all the moves for them to be ordered to speed up the next search because more branches will be cut
        struct MoveScore *moveScores = malloc(sizeof(struct MoveScore) * oldMoveCount);

        int threadCountLock = threadCount;
        if (depth < 3)
        {
            threadCountLock = 1;
        }

        struct Package *package = malloc(sizeof(struct Package));
        package->board = board;
        package->moveGenData = *moveGenData;
        package->data = *data;
        package->depth = depth;
        package->useTime = canQuit;
        package->threadCount = threadCountLock;
        threadID = 0;
        workIDX = 0;
        stats = (struct Stats){0};

        pthread_t *threads = malloc(sizeof(pthread_t) * threadCountLock);
        for (int i = 0; i < threadCountLock; ++i)
        {
            pthread_create(&threads[i], NULL, searchWorker, package);
        }

        for (int i = 0; i < threadCountLock; ++i)
        {
            pthread_join(threads[i], NULL);
        }
        free(threads);
        free(package);
        memcpy(moveScores, results, sizeof(struct MoveScore) * oldMoveCount);

        int idx = 0;
        bool searchedPrevBest = false;
        // Check if the search of the previous best move successfully completed, if not, discard the results because they are useless.
        for (int i = 0; i < oldMoveCount; ++i)
        {
            struct Move move = moves[i];
            if (completed[i])
            {
                if (compareMove(move, prevBest))
                {
                    searchedPrevBest = true;
                }
                moveScores[idx].score = results[i];
                moveScores[idx].move = moves[i];
                ++idx;
            }
            char *str = moveToString(move);
            free(str);
        }
        // If the search was canceled and the results are useless because the previous best move wasn't searched, return early
        if (ordered != NULL && (idx == 0 || searchedPrevBest == false || (clock() >= timeLimit && canQuit)))
        {
            // Better not forget to free the memory......
            freeLines(oldMoveCount);
            free(moves);
            free(moveScores);
            return bestMove;
        }

        qsort(moveScores, idx, sizeof(struct MoveScore), compare);

        bestMove = moveScores[0];
        int pvLength = 1;
        int mate = 0;

        // Feed back some info to the GUI
        if (log && bestMove.move.startSquare != bestMove.move.endSquare)
        {
            end = clock();
            double duration = (double)(end - start) / CLOCKS_PER_SEC;
            double positionsPerSec = stats.nodes / duration;
            char *score;
            // Checkmate is on the board
            if (abs(bestMove.score) >= 1000000)
            {
                mate = depth - abs(bestMove.score) + 1000000;
                score = malloc(sizeof(char) * ((int)log10(mate | 1) + 7)); // OR log10(mate) with 1, because log10(0) is -Infinity
                if (bestMove.score > 0)
                {
                    snprintf(score, sizeof(score), "mate %d", mate);
                }
                else
                {
                    snprintf(score, sizeof(score), "mate -%d", abs(mate));
                }
            }
            else
            {
                score = malloc(sizeof(char) * ((int)log10(abs(bestMove.score) | 1) + 6)); // OR log10(score) with 1, because log10(0) is -Infinity
                if (bestMove.score >= 0)
                {
                    snprintf(score, sizeof(score), "cp %i", bestMove.score);
                }
                else
                {
                    snprintf(score, sizeof(score), "cp -%i", abs(bestMove.score));
                }
            }
            printf("info depth %i seldepth %i time %.0f nodes %llu nps %.0f score %s pv ", depth, selDepth, duration * 1000, stats.nodes, positionsPerSec, score);
            free(score);
            for (int i = 0; i < oldMoveCount; ++i)
            {
                if (compareMove(moves[i], bestMove.move))
                {
                    char *firstMove = moveToString(bestMove.move);
                    printf("%s", firstMove);
                    free(firstMove);
                    for (int j = depth - 1; j > 0; --j)
                    {
                        // if (lines[i][j].move.startSquare == lines[i][j].move.endSquare)
                        // {
                        //     break;
                        // }
                        // char *str = moveToString(lines[i][j].move);
                        // printf(" %s", str);
                        // free(str);
                        pvLength++;
                    }
                    printf("\n");
                }
            }
        }
        fflush(stdout);

        // Free the lines
        freeLines(oldMoveCount);
        // Go for a deeper search if we have time
        if (timeLimit > clock() && recursion && mate == 0 && (!log || pvLength == depth))
        {
            ++depth;
            struct MoveScore deeperSearch = searchMain(board, moveGenData, data, depth, timeLimit, log, true, true, moveScores);
            if (deeperSearch.move.startSquare != deeperSearch.move.endSquare)
            {
                bestMove = deeperSearch;
            }
        }

        free(moveScores);
    }

    if (log && ordered == NULL)
    {
        char *str = moveToString(bestMove.move);
        printf("bestmove %s\n", str);
        fflush(stdout);
        free(str);
    }

    free(moves);
    return bestMove;
}