#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#else
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;

/**
 * Класс Board инкапсулирует:
 *  - состояние доски (матрица фигур, выделения, активная клетка, история),
 *  - ресурсы SDL (окно, рендерер, текстуры),
 *  - полный цикл перерисовки (rerender) при любом изменении состояния,
 *  - утилиты: подсветка клеток, перемещение фигур, откат хода, показ результата.
 */
class Board
{
public:
    Board() = default;
    Board(const unsigned int W, const unsigned int H) : W(W), H(H) {}

    /**
     * Стартовая инициализация окна/рендерера/текстур и отрисовка начальной позиции.
     * Возвращает 0 при успехе, 1 при ошибке. Ошибки логируются в файл.
     */
    int start_draw()
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }

        // Если размеры окна не заданы — подстраиваемся под размер экрана (почти квадрат)
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desctop display mode");
                return 1;
            }
            W = min(dm.w, dm.h);
            W -= W / 15; // небольшой отступ от краёв
            H = W;
        }

        // Окно и рендерер
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }

        // Загрузка текстур (доска, шашки, дамки, кнопки «назад» и «повтор»)
        board  = IMG_LoadTexture(ren, board_path.c_str());
        w_piece= IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece= IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen= IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen= IMG_LoadTexture(ren, queen_black_path.c_str());
        back   = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }

        // Синхронизируем W/H с реальным размером рендерера
        SDL_GetRendererOutputSize(ren, &W, &H);

        // Сформировать стартовую матрицу фигур и перерисовать
        make_start_mtx();
        rerender();
        return 0;
    }

    /**
     * Полный сброс состояния партии к началу и перерисовка.
     */
    void redraw()
    {
        game_results = -1;
        history_mtx.clear();
        history_beat_series.clear();
        make_start_mtx();
        clear_active();
        clear_highlight();
    }

    /**
     * Перемещение с учётом возможного снятия побитой фигуры (если xb/yb != -1).
     * beat_series — длина текущей серии взятий (для истории).
     */
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        if (turn.xb != -1)
        {
            mtx[turn.xb][turn.yb] = 0; // убрать побитую фигуру
        }
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    /**
     * Базовый перенос фигуры из (i,j) в (i2,j2).
     * Бросает исключение, если клетка «куда» занята или «откуда» пуста.
     * Автоматически превращает в дамку при достижении последней линии.
     * Добавляет снимок в историю.
     */
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        if (mtx[i2][j2])
        {
            throw runtime_error("final position is not empty, can't move");
        }
        if (!mtx[i][j])
        {
            throw runtime_error("begin position is empty, can't move");
        }

        // Автоповышение в дамки (1→3, 2→4)
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2;

        // Перенос фигуры
        mtx[i2][j2] = mtx[i][j];
        drop_piece(i, j);    // обнуляем исходную клетку и перерисовываем
        add_history(beat_series);
    }

    /**
     * Удаляет фигуру с клетки (i,j) и перерисовывает доску.
     */
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0;
        rerender();
    }

    /**
     * Превращает фигуру на клетке (i,j) в дамку; проверяет валидность.
     */
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
        {
            throw runtime_error("can't turn into queen in this position");
        }
        mtx[i][j] += 2;
        rerender();
    }

    /**
     * Возвращает текущую матрицу доски (копию).
     * 1 - white, 2 - black, 3 - white queen, 4 - black queen, 0 - пусто.
     */
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    /**
     * Подсветить набор клеток (x,y) и перерисовать.
     */
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (const auto& pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1;
        }
        rerender();
    }

    /**
     * Очистить всю подсветку и перерисовать.
     */
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0);
        }
        rerender();
    }

    /**
     * Сделать клетку активной (красная рамка) и перерисовать.
     */
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x;
        active_y = y;
        rerender();
    }

    /**
     * Снять активную клетку и перерисовать.
     */
    void clear_active()
    {
        active_x = -1;
        active_y = -1;
        rerender();
    }

    /**
     * Проверить, подсвечена ли клетка (x,y).
     */
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    /**
     * Откатить последний ход (или цепочку добиваний) по истории.
     * Восстанавливает состояние, снимает подсветку и активную клетку.
     */
    void rollback()
    {
        // Сколько снимков истории откатить: минимум 1, либо длина последней серии взятий
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back();
            history_beat_series.pop_back();
        }
        mtx = *(history_mtx.rbegin());
        clear_highlight();
        clear_active();
    }

    /**
     * Показать результат (ничья/победа белых/чёрных) поверх доски.
     */
    void show_final(const int res)
    {
        game_results = res;
        rerender();
    }

    /**
     * Если окно ресайзят — синхронизировать размеры и перерисовать.
     */
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H);
        rerender();
    }

    /**
     * Корректно освободить все ресурсы SDL.
     */
    void quit()
    {
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    ~Board()
    {
        if (win)
            quit();
    }

private:
    /**
     * Сохранить снимок текущей доски и длину серии взятий в историю.
     */
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);
        history_beat_series.push_back(beat_series);
    }

    /**
     * Заполнить стартовую расстановку шашек (классическая 8x8).
     * Белые (1) снизу, чёрные (2) сверху, только на тёмных клетках.
     */
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;
                if (i < 3 && (i + j) % 2 == 1)
                    mtx[i][j] = 2; // чёрные вверху
                if (i > 4 && (i + j) % 2 == 1)
                    mtx[i][j] = 1; // белые внизу
            }
        }
        add_history();
    }

    /**
     * Полная перерисовка кадра:
     *  - фон-доска
     *  - фигуры (с масштабированием под текущие W/H)
     *  - подсветка возможных ходов (зелёные рамки)
     *  - активная клетка (красная рамка)
     *  - кнопки «назад» и «повтор»
     *  - окончательный результат (если задан)
     *
     * В конце — Present, небольшой delay (особенно для macOS) и очистка очереди событий.
     */
    void rerender()
    {
        // фон: доска
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, board, NULL, NULL);

        // фигуры
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j]) { continue; }

                // позиция и размер спрайта по сетке 10x10 (рамки/отступы учитываются)
                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };

                // выбор текстуры по типу фигуры
                SDL_Texture* piece_texture;
                if (mtx[i][j] == 1) { piece_texture = w_piece; }
                else if (mtx[i][j] == 2) { piece_texture = b_piece; }
                else if (mtx[i][j] == 3) { piece_texture = w_queen; }
                else { piece_texture = b_queen; }

                SDL_RenderCopy(ren, piece_texture, NULL, &rect);
            }
        }

        // подсветка (зелёные рамки) — рисуем в увеличенном масштабе для толщины линий
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);
        const double scale = 2.5;
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j]) { continue; }
                SDL_Rect cell{
                    int(W * (j + 1) / 10 / scale),
                    int(H * (i + 1) / 10 / scale),
                    int(W / 10 / scale),
                    int(H / 10 / scale)
                };
                SDL_RenderDrawRect(ren, &cell);
            }
        }

        // активная клетка (красная рамка)
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
            SDL_Rect active_cell{
                int(W * (active_y + 1) / 10 / scale),
                int(H * (active_x + 1) / 10 / scale),
                int(W / 10 / scale),
                int(H / 10 / scale)
            };
            SDL_RenderDrawRect(ren, &active_cell);
        }
        SDL_RenderSetScale(ren, 1, 1); // вернуть масштаб по умолчанию

        // кнопки управления
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, back, NULL, &rect_left);
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        // картинка результата партии (при наличии)
        if (game_results != -1)
        {
            string result_path = draw_path;
            if (game_results == 1) { result_path = white_path; }
            else if (game_results == 2) { result_path = black_path; }

            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            SDL_DestroyTexture(result_texture);
        }

        // показать кадр
        SDL_RenderPresent(ren);

        // небольшой «дыхательный» зазор и обработка «хвоста» событий (особенно для macOS)
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    /**
     * Логирование ошибки в файл (добавление в конец), вместе с SDL_GetError().
     */
    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". "<< SDL_GetError() << endl;
        fout.close();
    }

  public:
    // Текущие размеры окна/рендерера (синхронизируются при старте/ресайзе)
    int W = 0;
    int H = 0;

    // История состояний доски по ходам (снимки mtx)
    vector<vector<vector<POS_T>>> history_mtx;

  private:
    // SDL-ресурсы
    SDL_Window *win = nullptr;
    SDL_Renderer *ren = nullptr;

    // Текстуры доски/фигур/кнопок
    SDL_Texture *board = nullptr;
    SDL_Texture *w_piece = nullptr;
    SDL_Texture *b_piece = nullptr;
    SDL_Texture *w_queen = nullptr;
    SDL_Texture *b_queen = nullptr;
    SDL_Texture *back = nullptr;
    SDL_Texture *replay = nullptr;

    // Пути к текстурам
    const string textures_path     = project_path + "Textures/";
    const string board_path        = textures_path + "board.png";
    const string piece_white_path  = textures_path + "piece_white.png";
    const string piece_black_path  = textures_path + "piece_black.png";
    const string queen_white_path  = textures_path + "queen_white.png";
    const string queen_black_path  = textures_path + "queen_black.png";
    const string white_path        = textures_path + "white_wins.png";
    const string black_path        = textures_path + "black_wins.png";
    const string draw_path         = textures_path + "draw.png";
    const string back_path         = textures_path + "back.png";
    const string replay_path       = textures_path + "replay.png";

    // Активная (выделенная красным) клетка
    int active_x = -1, active_y = -1;

    // Результат партии: -1 — нет, 1 — белые, 2 — чёрные, 0 — ничья
    int game_results = -1;

    // Матрица подсветки возможных ходов
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));

    // Матрица фигур: 0 — пусто; 1 — white; 2 — black; 3 — white queen; 4 — black queen
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));

    // История длин серий взятий (для корректного отката)
    vector<int> history_beat_series;
};
