#include "aux_module.h"



Vecr operator-(Vecr const& vec1, Vecr const& vec2) {

    int vec_sz = vec1.size();
    Vecr res(vec_sz, 0.0);
    for(int i = 0; i < vec_sz; ++i) {
        res[i] = vec1[i] - vec2[i];
    }
    return res;

}


Vecr operator*(Vecr const& vec, double num) {

    int vec_sz = vec.size();
    Vecr res(vec_sz, 0.0);
    for(int i = 0; i < vec_sz; ++i) {
        res[i] = vec[i] * num;
    }
    return res;

}


Vecr operator*(double num, Vecr const& vec) {
    return vec * num;
}


double lc_norm(Vecr const& Vecr) {

    double norm = 0.0;
    int Vecr_sz = Vecr.size();
    for(int i = 0; i < Vecr_sz; ++i) {
        norm = std::max(norm, std::abs(Vecr[i]));
    }
    return norm;

}