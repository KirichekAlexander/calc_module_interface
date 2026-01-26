#ifndef RHYTHMIC_DELIVERY_H
#define RHYTHMIC_DELIVERY_H


#include "aux_module.h"

// класс результата для прямого метода
struct DeliveryResult {

    Vecr x;     // поставки
    Vecr V;     // склад/ресурс по тактам
    bool ok; // выполнимость 

    DeliveryResult()=default;
    DeliveryResult(Vecr const& x, Vecr const& V, bool ok);
};


// класс результата для итерационного метода
struct UniformityIterResult : DeliveryResult {
    double Mp;       // средняя поставка (для критерия равномерности)
    int maxIter;     // лимит итераций
    int iters;       // сколько реально сделали итераций

    UniformityIterResult()=default;
    UniformityIterResult(Vecr const& x, Vecr const& V, bool ok, double Mp, int maxIter, int iters);
};


// функции для метода проекции градиента

// функция проекции вектора на многомерный куб задаваемый векторами lb, ub
void clamp_vec(Vecr& vec, Vecr const& lb, Vecr const& ub);


// итерационный метод решения задачи о равномерных поставках, критерий: равномерности. PG - проекция градиента
UniformityIterResult solve_rhythmic_delivery_uniform_pg(Vecr const& p, double V0, double minV, double maxV);

//



// прямой метод решения задачи о равномерных поставках, критерий: содержание объёма ресурса в границах объёма склада
DeliveryResult solve_rhythmic_delivery_bounds_direct(Vecr const& p, double V0, double minV, double maxV);

#endif