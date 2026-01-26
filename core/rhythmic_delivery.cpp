#include "rhythmic_delivery.h"

// реализация конструкторов для результатов

DeliveryResult::DeliveryResult(Vecr const& x, Vecr const& V, bool ok) 
    : x(x)
    , V(V)
    , ok(ok)
{
}


UniformityIterResult::UniformityIterResult(Vecr const& x, Vecr const& V, bool ok, double Mp, int maxIter, int iters) 
    : DeliveryResult(x, V, ok)
    , Mp(Mp)
    , maxIter(maxIter)
    , iters(iters)
{
}

//


// реализация итерационного метода 

void clamp_vec(Vecr& vec, Vecr const& lb, Vecr const& ub) {

    size_t vec_sz = vec.size();
    for (size_t i = 0; i < vec_sz; ++i) {

        if (vec[i] < lb[i]) vec[i] = lb[i];
        if (vec[i] > ub[i]) vec[i] = ub[i];

    }

}


UniformityIterResult solve_rhythmic_delivery_uniform_pg(Vecr const& p, double V0, double minV, double maxV) {

    const size_t n = p.size(); // количество тактов поставок

    Vecr lb(n, 0.0); // вектор нижних границ
    Vecr ub(n, 0.0); // вектор верхних границ

    double s = 0.0; // сумма поставок из РС потребителям
    

    // вычисление суммы поставок, нижних и верхних границ поставок в каждый такт
    for(size_t t = 0; t < n; ++t) {

        s += p[t];             // сумма поставок
        lb[t] = minV - V0 + s; // нижние границы
        ub[t] = maxV - V0 + s; // верхние границы

    }
    double Mp = s / n; // средняя величина поставок
    //


    // метод проекции градиента
    bool ok = false;   // флаг того, что метод сошёлся и выполнено принадлежность границам
    // выбор eps
    double scale = 0.0;
    for (size_t i = 0; i < n ; ++i) {
        scale = std::max(scale, ub[i] - lb[i]);
    }
    const double eps = 1e-10 * std::max(1.0, scale);
    //

    // оценка порядка O(n^2 * log(scale/eps)); коэффициент с запасом
    const int maxIter =static_cast<int>(std::ceil( 2.0 * (16.0 * n * n) / (pi * pi) * std::log(1e10)));

    double alpha = 0.125;  // шаг метода 


    // код метода
    // сделаем начальное приближение
    Vecr y(n, 0.0);
    for (size_t t = 0; t < n; ++t) {

        // y[t] = (t + 1.0) * Mp; // для задачи из статьи идеальное начальное приближение
        y[t] = 0.5*(lb[t]+ub[t]); // приближение средними границ

    }
    clamp_vec(y, lb, ub); // проецирование на множество lb[t]<=y[t]<=ub[t]

    Vecr r(n, 0.0);  // r[t] = x[t] - Mp
    Vecr g(n, 0.0);  // градиент F(y) grad(F)[t] = g[t] = 2r[t] - 2r[t+1] или 2r[n]
    Vecr new_y(n, 0.0); // новое приближение


    // градиентный спуск
    int it = 0;
    for (; it < maxIter; ++it) {

        // вычисление r[t]
        r[0] = y[0] - Mp;
        for (int t = 1; t < n; ++t) {
            r[t] = (y[t] - y[t - 1]) - Mp;
        }
        //

        // вычисления градиента
        for (int i = 0; i < n - 1; ++i) {
            g[i] = 2.0 * r[i] - 2.0 * r[i + 1];
        }
        g[n - 1] = 2.0 * r[n - 1];
        //

        // шаг метода
        new_y = y - alpha * g;
        clamp_vec(new_y, lb, ub); // проецирование нового приближения на допустимое множество
        double maxDiff = lc_norm(new_y - y); // норма разности
        std::swap(y, new_y); // меням местами память

        // критерий остановки
        if (maxDiff < eps) {

            ok = true;
            break;

        }

    }
    //

    // восстановление x
    Vecr x(n, 0.0);
    x[0] = y[0];
    for (int t = 1; t < n; ++t) {
        x[t] = y[t] - y[t - 1];
    }
    //

    Vecr vecV(n, 0.0); // объёмы склада


    // проверка границ
    for (int t = 0; t < n; ++t) {

        vecV[t] = x[t] - p[t] + (t == 0 ? V0 : vecV[t - 1]);
        if (vecV[t] < minV || vecV[t] > maxV) {
            ok = false;
        }

    }
    //


    return UniformityIterResult{x,
                          vecV,
                          ok,
                          Mp,
                          maxIter,
                          it};

}
//


// реализация прямого метода(средние поставки)
DeliveryResult solve_rhythmic_delivery_bounds_direct(Vecr const& p, double V0, double minV, double maxV) {

    const size_t n = p.size(); // количество тактов
    Vecr x(n, 0.0);             // вектор поставок из РЦ в РС
    Vecr vecV(n, 0.0);          // вектор объёма ресурса на складе

    bool ok = (V0 >= minV && V0 <= maxV); // флаг того, что критерий изначально выполнялся
    double meanV = 0.5 * (minV + maxV);   // средний объём
    double curV = V0;                     // текущий объём

    for (size_t t = 0; t < n; ++t) {

        // вычисление средние поставки, чтобы придерживаться среднего уровня
        const double need = meanV - (curV - p[t]);
        x[t] = std::max(0.0, need);               
        //

        vecV[t] = curV = curV + x[t] - p[t]; // вычисление текущего объёма
        
    }

    return DeliveryResult{x,
                          vecV,
                          ok};

}
//