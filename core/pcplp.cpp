#include "pcplp.h"


// реализация решения задачи календарного планирования с ограниченными ресурсами - генетический алгоритм
Schedule solve_PCPLP(int N,           
                 int M,         
                 Veci dur,          
                 Veci rel,          
                 Veci cap,          
                 VecVecPairii demands,
                 VecVeci preds)
{
    Instance inst;
    inst.N = N;
    inst.M = M;
    inst.dur = dur;
    inst.rel = rel;
    inst.cap = cap;
    inst.demands = demands;
    inst.preds = preds;
    build_succs(inst); // построим последователей(у работ также есть предшественники) - последующие работы

    std::random_device rd;  // рандомный сид
    std::mt19937 rng(rd()); // генератор случайных чисел

    const int POP = std::clamp(2 * N, 60, 140);    // количество особей 
    const int GEN = std::clamp(4 * N, 150, 500);   // количество поколений
    const int stall_limit = std::max(30, GEN / 3); // количество итераций для остановки если результат не улучшается
    const int ELITE = 3;       // количество элитных особей
    const int TOURN_K = 3;     // количество особей, участвующих в турнирном отборе
    const double PCROSS = 0.9; // вероятность скрещивания
    const double PMUT = 0.2;   // вероятность мутации
    DecoderWS ws;
    init_ws(inst, ws);

    Individs pop = init_population(inst, POP, rng, 0.7, ws); // сгенерируем начальную популяцию

    auto best_it = std::min_element(pop.begin(), pop.end(), better);
    Individ best = *best_it; // лучший индивид

    int stall = 0; // для ранней остановки - количество неулучшаемых поколений

    for (int g = 1; g <= GEN; ++g) {

        pop = next_generation(inst, pop, ELITE, TOURN_K, PCROSS, PMUT, rng, ws); // сгенерируем новое поколение

        Individ curBest = *std::min_element(pop.begin(), pop.end(), better); // лучший индивид в новом поколении
        if (curBest.cmax < best.cmax) {
            best = curBest;
            stall = 0;
        } else {
            ++stall;
        }

        if (stall >= 50) break; // быстрое завершение, если нет улучшений
    }


    return serial_decode_SGS(inst, best.perm, ws);

}
//


// Реализация построения последователей
void build_succs(Instance& inst)
{

    inst.succs.assign(inst.N, {});

    for (int j = 0; j < inst.N; ++j) {
        for (int p : inst.preds[j]) {
            inst.succs[p].push_back(j);
        }
    }

}
//


//  
Veci make_random_perm(
                      int N,            // количество работ
                      std::mt19937& rng // генератор чисел
                     )
{

    Veci p(N); 
    std::iota(p.begin(), p.end(), 0);      // заполняем массив числами от 0 до N-1
    std::shuffle(p.begin(), p.end(), rng); // перемешиваем
    return p;

}
//


//
Veci make_random_topo_perm(
                           Instance const& inst, // начальные данные
                           std::mt19937& rng     // генератор чисел
                          )
{

    const int N = inst.N; 
    Veci indeg(N, 0); // indeg[j] = количество предшественников для работы j
    for (int j = 0; j < N; ++j) indeg[j] = (int)inst.preds[j].size();

    Veci eligible; // вектор работ, у которых нет предшественников
    eligible.reserve(N);
    for (int j = 0; j < N; ++j) if (indeg[j] == 0) eligible.push_back(j);

    Veci order; // наша перестановка
    order.reserve(N);

    while (!eligible.empty()) {

        std::uniform_int_distribution<int> dist(0, (int)eligible.size() - 1); // сначала рандомно добавляем работы, у которых нет предшественников
        int k = dist(rng);
        int u = eligible[k];
        eligible[k] = eligible.back();
        eligible.pop_back();

        order.push_back(u);

        // потом смотрим можем ли мы добавить работы, все предшественники которой уже добавлены
        for (int v : inst.succs[u]) {
            if (--indeg[v] == 0) eligible.push_back(v);
        }

    }

    return order;
}
//


// реализация генерации популяции
Individs init_population(
                         Instance const& inst, // начальные данные
                         int POP,              // количество особей в популяции
                         std::mt19937& rng,    // генератор случайных чисел
                         double topo_share,     // доля перестановок работ, удовлетворяющих порядку выполнения(вещественное число в интервале(0, 1))
                         DecoderWS& ws
                        )
{

    Individs pop;     // популяция
    pop.reserve(POP); // выделение памяти под заданное количество особей

    int topo_cnt = (int)(POP * topo_share); // количество особей, удовлетворяющих порядку выполнения

    // генерация топологически верных перестановок работ
    for (int i = 0; i < topo_cnt; ++i) {
        Individ ind;      
        ind.perm = make_random_topo_perm(inst, rng);
        ind.cmax = evaluate_cmax(inst, ind.perm, ws); 
        pop.push_back(std::move(ind));
    }
    //


    // генерация абсолютно рандомных перестановок работ
    for (int i = topo_cnt; i < POP; ++i) {
        Individ ind;
        ind.perm = make_random_perm(inst.N, rng);
        ind.cmax = evaluate_cmax(inst, ind.perm, ws);
        pop.push_back(std::move(ind));
    }
    //

    return pop;

}


// Вычисление верхней оценки плана работ
int compute_H(const Instance& inst) {
    int sumDur = std::accumulate(inst.dur.begin(), inst.dur.end(), 0);
    int maxRel = *std::max_element(inst.rel.begin(), inst.rel.end());
    return maxRel + sumDur + 5;
}
//


// инициализация данных для декодера
void init_ws(const Instance& inst, DecoderWS& ws) {
    ws.H = compute_H(inst);
    ws.usage.assign(inst.M, Veci(ws.H, 0));
    ws.S.start.assign(inst.N, -1);
    ws.S.finish.assign(inst.N, -1);
    ws.remPred.assign(inst.N, 0);
    ws.done.assign(inst.N, 0);
}
//


// перезаполнение данных для декодера
void reset_ws(const Instance& inst, DecoderWS& ws) {
    for (int m = 0; m < inst.M; ++m)
        std::fill(ws.usage[m].begin(), ws.usage[m].end(), 0);

    std::fill(ws.S.start.begin(), ws.S.start.end(), -1);
    std::fill(ws.S.finish.begin(), ws.S.finish.end(), -1);
    ws.S.cmax = 0;

    for (int j = 0; j < inst.N; ++j)
        ws.remPred[j] = (int)inst.preds[j].size();

    std::fill(ws.done.begin(), ws.done.end(), 0);
}
//


// вычисление времени конца данной рабочей перестановки
int evaluate_cmax(
                  const Instance& inst, // начальные данные
                  const Veci& perm,      // перестановка
                  DecoderWS& ws
                 )
{
    return serial_decode_SGS(inst, perm, ws).cmax; // декодер(считаем время, за которое может выполниться данная перестановка)
}
//


// usage[m][t] = сколько занято ресурса m в момент t
bool can_place(const Instance& inst, // начальные данные
               int job,              // номер работы
               int t,                // текущее время постанвоки работы
               const VecVeci& usage  //
              )
{

    int d = inst.dur[job]; // длительность работы
    // сделаем проходку по всем потребяностям данной работы
    for (auto [m, qty] : inst.demands[job]) { 
        // хватит ли ресурсов для этой работы ?
        for (int tt = t; tt < t + d; ++tt) {
            if (usage[m][tt] + qty > inst.cap[m]) return false;
        }
        //
    }
    //
    return true;

}

void place_job(const Instance& inst, int job, int t,
                      VecVeci& usage)
{
    int d = inst.dur[job];
    for (auto [m, qty] : inst.demands[job]) {
        for (int tt = t; tt < t + d; ++tt) {
            usage[m][tt] += qty;
        }
    }
}


// реализация декодера вовзвращает структуру - график работ
Schedule serial_decode_SGS(const Instance& inst, const Veci& perm, DecoderWS& ws)
{
    const int N = inst.N; // количество работ
    const int M = inst.M; // количество ресурсов

    reset_ws(inst, ws);
    int doneCnt = 0;              // количество выполненных работ

    // пока все работы не выполнены
    while (doneCnt < N) {

        // выберем первую доступную работы в перестановке
        int job = -1;
        for (int k = 0; k < N; ++k) {
            int cand = perm[k];
            if (!ws.done[cand] && ws.remPred[cand] == 0) { job = cand; break; } // то есть не сделана и все предшественники выполнены
        }
        //

        // возможное время начала работы
        int ES = inst.rel[job]; // минимальное время старта
        for (int p : inst.preds[job]) ES = std::max(ES, ws.S.finish[p]); // и сравнение с концом выполненности предшественников
        //

        // учитывание доступности ресурсов
        int t = ES;
        while (true) {
            if (can_place(inst, job, t, ws.usage)) break;
            ++t; // сдвиг вправо
        }
        //

        
        // фиксируем в графике расписание данной работы
        ws.S.start[job]  = t;
        ws.S.finish[job] = t + inst.dur[job];
        place_job(inst, job, t, ws.usage);
        //

        ws.done[job] = 1; // работа выполнена
        ++doneCnt; 


        for (int s : inst.succs[job]) --ws.remPred[s]; // для каждого последователя говорим, что данная работа была выполнена
    }

    
    for (int j = 0; j < N; ++j) ws.S.cmax = std::max(ws.S.cmax, ws.S.finish[j]); // время затраченное на весь план
    return ws.S; // возврат графика работ

}



// сортировка: меньше cmax — лучше
bool better(const Individ& a, const Individ& b) {
    return a.cmax < b.cmax;
}

// турнирный отбор: выбрать лучшего из k случайных
int tournament_select(
                      const Individs& pop, // поколение
                      int k,               // количество особей в турнире
                      std::mt19937& rng    // генератор случайных чисел
                     )
{

    std::uniform_int_distribution<int> dist(0, (int)pop.size() - 1);
    int bestIdx = dist(rng);
    for (int i = 1; i < k; ++i) {
        int cand = dist(rng);
        if (pop[cand].cmax < pop[bestIdx].cmax) bestIdx = cand;
    }
    return bestIdx;

}


// реализация скрещивания
Veci crossover_OX(
                  const Veci& p1,   // родитель один
                  const Veci& p2,   // родитель два
                  std::mt19937& rng // генератор чисел
                 )
{

    // создаём отрезок который будет скопирован из первого родителя
    int N = (int)p1.size();
    std::uniform_int_distribution<int> dist(0, N - 1);
    int a = dist(rng), b = dist(rng);
    if (a > b) std::swap(a, b);
    //

    Veci child(N, -1);
    std::vector<char> used(N, 0); // вектор используемых генов

    // 1) копируем отрезок из p1
    for (int i = a; i <= b; ++i) {
        child[i] = p1[i];
        used[p1[i]] = 1;
    }

    // 2) заполняем остальные позиции, сохраняя порядок p2
    int cur = (b + 1) % N;
    for (int i = 0; i < N; ++i) {
        int gene = p2[(b + 1 + i) % N];
        if (used[gene]) continue;
        while (child[cur] != -1) cur = (cur + 1) % N;
        child[cur] = gene;
        used[gene] = 1;
    }
    return child;

}
//


// мутация swap: поменять местами два случайных гена
void mutate_swap(Veci& perm, std::mt19937& rng)
{
    int N = (int)perm.size();
    if (N < 2) return;
    std::uniform_int_distribution<int> dist(0, N - 1);
    int i = dist(rng), j = dist(rng);
    while (j == i) j = dist(rng);
    std::swap(perm[i], perm[j]);
}

// построение следующего поколения
Individs next_generation(
                         const Instance& inst, // начальные данные
                         const Individs& pop,  // предыдущая поколение
                         int ELITE,            // количество лучших скопированных особей
                         int TOURN_K,          // количество особе, участвующих в трунирном отборе
                         double PCROSS,        // вероятность скрещивания
                         double PMUT,          // вероятность мутации
                         std::mt19937& rng,     // генератор случайных чисел
                         DecoderWS& ws
                        )
{

    std::uniform_real_distribution<double> ur(0.0, 1.0); // создадим равномерное распределение

    // отсортируем предыдущее поколение по времени
    Individs sorted = pop;                 
    std::sort(sorted.begin(), sorted.end(), better);

    Individs next; // новое поколение
    next.reserve(sorted.size());

    // элитизм: копируем лучших без изменений
    ELITE = std::min(ELITE, (int)sorted.size());
    for (int i = 0; i < ELITE; ++i) next.push_back(sorted[i]);

    // остальных рождаем
    while ((int)next.size() < (int)sorted.size()) {

        // сделаем турнирный отбор
        int i1 = tournament_select(sorted, TOURN_K, rng); // индекс первого родителя
        int i2 = tournament_select(sorted, TOURN_K, rng); // индекс второго родителя
        //

        Veci child = sorted[i1].perm; // вектор ребёнка, скрещивания может и не произойти

        // выполняем скрещивание и мутацию
        if (ur(rng) < PCROSS) {
            child = crossover_OX(sorted[i1].perm, sorted[i2].perm, rng);
        }
        if (ur(rng) < PMUT) {
            mutate_swap(child, rng);
        }
        //

        Individ ind;
        ind.perm = std::move(child);
        ind.cmax = evaluate_cmax(inst, ind.perm, ws);
        next.push_back(std::move(ind)); 

    }

    return next;
}
//