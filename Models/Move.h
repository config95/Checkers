#pragma once
#include <stdlib.h>

typedef int8_t POS_T; // тип координаты на доске (-128..127)

// Структура описывает один возможный ход фигуры
struct move_pos
{
    POS_T x, y;             // исходная клетка (откуда ходим)
    POS_T x2, y2;           // целевая клетка (куда ходим)
    POS_T xb = -1, yb = -1; // клетка побитой фигуры (если есть взятие), -1 = нет

    // Конструктор: обычный ход без побитой фигуры
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2)
        : x(x), y(y), x2(x2), y2(y2)
    {
    }

    // Конструктор: ход с указанием побитой фигуры
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2,
             const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }

    // Сравнение двух ходов: равны, если совпадает начальная и конечная клетка
    bool operator==(const move_pos &other) const
    {
        return (x == other.x && y == other.y &&
                x2 == other.x2 && y2 == other.y2);
    }

    // Неравенство реализовано через оператор ==
    bool operator!=(const move_pos &other) const
    {
        return !(*this == other);
    }
};
