#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    /*
    * Основной игровой цикл.
    * Главный игровой цикл: запускает/перезапускает игру, чередует ходы игрока и бота,
    * учитывает лимит ходов, измеряет длительность партии и показывает итог.
    */
    int play()
    {
        auto start = chrono::steady_clock::now();
        if (is_replay)
        {
            logic = Logic(&board, &config);
            config.reload();
            board.redraw();
        }
        else
        {
            board.start_draw();
        }
        is_replay = false;

        int turn_num = -1;
        bool is_quit = false;
        const int Max_turns = config("Game", "MaxNumTurns");
        while (++turn_num < Max_turns)
        {
            beat_series = 0;
            logic.find_turns(turn_num % 2);
            if (logic.turns.empty())
                break;
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                auto resp = player_turn(turn_num % 2);
                if (resp == Response::QUIT)
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)
                {
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                bot_turn(turn_num % 2);
        }
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        if (is_replay)
            return play();
        if (is_quit)
            return 0;
        int res = 2;
        if (turn_num == Max_turns)
        {
            res = 0;
        }
        else if (turn_num % 2)
        {
            res = 1;
        }
        board.show_final(res);
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }
        return res;
    }

  private:
    /**
     * Выполняет ход бота заданного цвета.
     *
     * Логика:
     *  1) Фиксируем время начала — для телеметрии.
     *  2) Читаем задержку хода из конфигурации ("BotDelayMS").
     *  3) Стартуем вспомогательный поток с SDL_Delay, чтобы гарантировать
     *     минимальную «паузу обдумывания» перед тем, как появится первый ход,
     *     независимо от времени вычисления best-ходов.
     *  4) Вычисляем оптимальную последовательность ходов бота (в т.ч. серию взятий)
     *     через logic.find_best_turns(color).
     *  5) Дожидаемся завершения «выравнивающей» задержки и применяем ходы по очереди:
     *       - перед первым ходом задержка уже была (в отдельном потоке),
     *         перед каждым последующим — делаем ту же паузу вручную,
     *       - beat_series увеличиваем на каждый ход со взятием (turn.xb != -1),
     *         чтобы корректно вести историю/визуализацию длин серий,
     *       - board.move_piece(turn, beat_series) обновляет состояние доски и историю.
     *  6) Логируем затраченное время в log.txt (в миллисекундах).
     *
     * Параметры:
     *   @param color — цвет бота (true/false), используется в поиске хода.
     *
     * Побочные эффекты:
     *   - Изменяет состояние доски ( Board::move_piece ).
     *   - Обновляет глобальный счетчик beat_series.
     *   - Пишет строку с таймингом в файл логов.
     */
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now();

        auto delay_ms = config("Bot", "BotDelayMS");
        // new thread for equal delay for each turn
        thread th(SDL_Delay, delay_ms);
        auto turns = logic.find_best_turns(color);
        th.join();
        bool is_first = true;
        // making moves
        for (auto turn : turns)
        {
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            beat_series += (turn.xb != -1);
            board.move_piece(turn, beat_series);
        }

        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    /**
     * Выполняет ход игрока.
     *
     * Алгоритм работы:
     * 1. Подсвечивает все возможные ходы (взятые из logic.turns).
     * 2. Ждёт, пока игрок выберет клетку:
     *    - если выбрана некорректная клетка, подсветка сбрасывается и цикл выбора продолжается;
     *    - если выбран корректный ход, фигура перемещается.
     * 3. Если в результате хода возможны взятия (beat), продолжается серия добиваний:
     *    - после каждого удачного удара снова ищутся возможные добивания;
     *    - игрок выбирает клетку для следующего удара;
     *    - серия продолжается, пока есть возможность бить.
     * 4. В любой момент, если пользовательский ввод возвращает не Response::CELL 
     *    (например, игрок нажал «выход»), функция завершает работу и возвращает это значение.
     *
     * Параметры:
     *   @param color — цвет игрока (true/false), пока не используется в логике.
     *
     * Возвращает:
     *   Response::OK   — если ход завершён успешно;
     *   Другое значение Response — если игрок прервал ход (например, выбрал выход).
     */
    Response player_turn(const bool color)
    {
        // return 1 if quit
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells);
        move_pos pos = {-1, -1, -1, -1};
        POS_T x = -1, y = -1;
        // trying to make first move
        while (true)
        {
            auto resp = hand.get_cell();
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

            bool is_correct = false;
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn;
                    break;
                }
            }
            if (pos.x != -1)
                break;
            if (!is_correct)
            {
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue;
            }
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y);
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);
        }
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1);
        if (pos.xb == -1)
            return Response::OK;
        // continue beating while can
        beat_series = 1;
        while (true)
        {
            logic.find_turns(pos.x2, pos.y2);
            if (!logic.have_beats)
                break;

            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);
            // trying to make move
            while (true)
            {
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

                bool is_correct = false;
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }
                if (!is_correct)
                    continue;

                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series);
                break;
            }
        }

        return Response::OK;
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
