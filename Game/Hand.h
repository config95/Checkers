#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс Hand обрабатывает действия "руки игрока" (ввода с мыши и окна)
class Hand
{
  public:
    // Конструктор: сохраняем указатель на игровое поле (Board)
    Hand(Board *board) : board(board)
    {
    }

    /**
     * Ожидает нажатие мыши или выход из игры.
     *
     * Возвращает кортеж:
     *   - Response (QUIT, BACK, REPLAY, CELL),
     *   - координата X (ячейка на доске, -1 если не выбрана),
     *   - координата Y (ячейка на доске, -1 если не выбрана).
     */
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;
        int x = -1, y = -1;     // реальные пиксельные координаты мыши
        int xc = -1, yc = -1;   // координаты клетки на доске

        while (true)
        {
            if (SDL_PollEvent(&windowEvent)) // проверяем очередь событий SDL
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT; // закрытие окна
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    // определяем клетку по координатам мыши
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    // разные реакции в зависимости от зоны клика
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK; // кнопка "назад"
                    }
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY; // кнопка "повтор"
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL; // корректная клетка
                    }
                    else
                    {
                        // щёлкнули вне доски и кнопок
                        xc = -1;
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT:
                    // обработка изменения размеров окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();
                        break;
                    }
                }
                // выходим из цикла ожидания, если получен не Response::OK
                if (resp != Response::OK)
                    break;
            }
        }
        return {resp, xc, yc};
    }

    /**
     * Ожидает событие REPLAY или QUIT.
     * Используется, например, в меню или в конце партии.
     */
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;

        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT; // закрытие окна
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    board->reset_window_size();
                    break;

                case SDL_MOUSEBUTTONDOWN: {
                    // определяем клетку, куда кликнули
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);

                    // щёлчок по кнопке "повтор"
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                break;
                }
                if (resp != Response::OK)
                    break;
            }
        }
        return resp;
    }

  private:
    Board *board; // ссылка на игровое поле для пересчёта размеров и координат
};
