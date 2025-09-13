#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    vector<move_pos> find_best_turns(bool color) {
    // TODO: реализовать по заданию №22
    return {};
    }


private:
    /**
     * Выполняет ход на копии матрицы доски и возвращает новое состояние.
     * Удаляет побитую фигуру (если xb/yb != -1),
     * перемещает шашку или дамку на новую позицию,
     * при необходимости превращает шашку в дамку.
     *
     * @param mtx  — исходное состояние доски
     * @param turn — структура с координатами хода
     * @return новое состояние доски после применения хода
     */
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    /**
     * Вычисляет «оценку позиции» для бота.
     * Используется как функция оценки (heuristic) в алгоритме поиска.
     *
     * Алгоритм:
     *  - считаем количество белых/чёрных шашек и дамок;
     *  - при режиме "NumberAndPotential" добавляем небольшой бонус
     *    за продвижение шашек вперёд (потенциал превращения в дамку);
     *  - если у соперника фигур не осталось — возвращаем INF (выигрыш);
     *  - если у бота фигур не осталось — возвращаем 0 (проигрыш);
     *  - в противном случае возвращаем отношение силы соперника к силе бота
     *    (меньшее — лучше для бота).
     *
     * @param mtx             — текущее состояние доски
     * @param first_bot_color — цвет, которым играет бот
     * @return числовая оценка позиции (чем меньше, тем лучше для бота)
     */
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        if (w + wq == 0)
            return INF;
        if (b + bq == 0)
            return 0;
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
                                double alpha = -1)
    {
        // TODO: реализовать по заданию №22
        return 0.0;
    }

    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
                               double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // TODO: реализовать по заданию №22
        return 0.0;
    }

public:
    /**
     * Находит все возможные ходы для игрока заданного цвета.
     * Использует текущее состояние доски из объекта Board.
     *
     * @param color — цвет игрока (0 = белые, 1 = чёрные)
     */
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }
    /**
     * Находит все возможные ходы для фигуры в клетке (x, y).
     * Использует текущее состояние доски из объекта Board.
     *
     * @param x — координата по вертикали (строка)
     * @param y — координата по горизонтали (столбец)
     */
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    /**
     * Находит все возможные ходы для игрока заданного цвета на основе переданной матрицы.
     * Сначала проверяет наличие ударов (beats). Если удары есть, то остальные ходы не рассматриваются.
     * Результат перемешивается (shuffle) для случайности.
     *
     * @param color — цвет игрока (0 = белые, 1 = чёрные)
     * @param mtx   — текущее состояние доски
     */
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }
    /**
     * Находит все возможные ходы для фигуры в клетке (x, y) на основе переданной матрицы.
     * Сначала ищет удары (beats):
     *   - для шашек: проверяет клетки через одну диагональ (x±2, y±2);
     *   - для дамок: ищет по диагоналям до первой встреченной фигуры, проверяя возможность взятия.
     * Если ударов нет — ищет простые ходы:
     *   - для шашек: на одну клетку вперёд по диагонали;
     *   - для дамок: на любое количество клеток по диагоналям до преграды.
     *
     * Результаты сохраняются в поле turns, а флаг have_beats показывает, есть ли удары.
     *
     * @param x   — координата по вертикали
     * @param y   — координата по горизонтали
     * @param mtx — текущее состояние доски
     */
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    // список возможных ходов, найденных последним вызовом find_turns()
    vector<move_pos> turns;

    // true, если среди возможных ходов есть взятие (beat);
    // в шашках взятие обязательно, поэтому этот флаг определяет,
    // какие ходы реально допустимы
    bool have_beats;

    // максимальная глубина рекурсии при поиске лучшего хода (ограничение для minimax/alpha-beta)
    int Max_depth;

private:
    // генератор случайных чисел (используется для перемешивания ходов,
    // чтобы бот не делал всегда один и тот же ход)
    default_random_engine rand_eng;

    // выбранный режим оценки позиции:
    //   "Number"              — считать только количество фигур;
    //   "NumberAndPotential"  — учитывать продвижение и потенциал превращения в дамки
    string scoring_mode;

    // выбранный режим оптимизации поиска
    string optimization;

    // для каждого состояния хранится выбранный следующий ход (используется при восстановлении лучшей последовательности)
    vector<move_pos> next_move;

    // связь между состояниями: для каждого состояния храним индекс следующего
    // (используется для восстановления цепочки ходов)
    vector<int> next_best_state;

    // указатель на объект доски, чтобы вызывать board->get_board()
    Board *board;

    // указатель на объект конфигурации, чтобы читать параметры (задержки, режимы и т.п.)
    Config *config;

};
