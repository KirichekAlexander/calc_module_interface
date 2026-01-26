#ifndef PCPLP_H
#define PCPLP_H 


#include "aux_module.h"


//структура начальных данных
struct Instance {

    int N=0;           // количество работ
    int M=0;           // количество видов ресурсов
    Veci dur;          // вектор продолжительности работ
    Veci rel;          // вектор минимального времени начал работ
    Veci cap;          // вектор объёмов ресурса во время одного временного такта
    VecVecPairii demands; // потребность в ресурсах для каждой работы
    VecVeci preds;     // предшественные работы
    VecVeci succs;     // последующие работы

};



struct Schedule {
    Schedule()=default;
    Veci start;
    Veci finish;
    int cmax = 0;
};

// класс особи 
struct Individ {

    Veci perm;  // порядок выполнения работ
    int cmax=0; // время

};



Schedule solve_PCPLP(int N,           
                 int M,         
                 Veci dur,          
                 Veci rel,          
                 Veci cap,          
                 VecVecPairii demands,
                 VecVeci preds);


// построение последующих работ
void build_succs(Instance& inst);


// построение рандомной перестановки
Veci make_random_perm(int N, std::mt19937& rng);


// построение рандомной перестановки с учётом предшественников
Veci make_random_topo_perm(Instance const& inst, std::mt19937& rng);


using Individs = std::vector<Individ>;



struct DecoderWS {
    int H = 0;
    VecVeci usage;         // M x H
    Schedule S;            // start/finish/cmax
    Veci remPred;          // N
    std::vector<char> done;// N
};

// генерация популяции
Individs init_population(Instance const& inst, int POP, std::mt19937& rng, double topo_share, DecoderWS& ws);



int compute_H(const Instance& inst);


// инициализация данных для декодера
void init_ws(const Instance& inst, DecoderWS& ws);


void reset_ws(const Instance& inst, DecoderWS& ws);


// то самое evaluate_cmax
int evaluate_cmax(const Instance& inst, const Veci& perm, DecoderWS& ws);


// usage[m][t] = сколько занято ресурса m в момент t
bool can_place(const Instance& inst, int job, int t,
                      const VecVeci& usage);


void place_job(const Instance& inst, int job, int t,
                      VecVeci& usage);


Schedule serial_decode_SGS(const Instance& inst, const Veci& perm, DecoderWS& ws);


// сортировка: меньше cmax — лучше
bool better(const Individ& a, const Individ& b);

// турнирный отбор: выбрать лучшего из k случайных
int tournament_select(const Individs& pop, int k, std::mt19937& rng);

// OX (Order Crossover) — классический кроссовер для перестановок
Veci crossover_OX(const Veci& p1, const Veci& p2, std::mt19937& rng);
// мутация swap: поменять местами два случайных гена
void mutate_swap(Veci& perm, std::mt19937& rng);

// построение следующего поколения
Individs next_generation(
    const Instance& inst,
    const Individs& pop,
    int ELITE,
    int TOURN_K,
    double PCROSS,
    double PMUT,
    std::mt19937& rng,
    DecoderWS& ws
);


#endif