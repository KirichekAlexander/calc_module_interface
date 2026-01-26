#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "rhythmic_delivery.h"
#include "pcplp.h"

namespace py = pybind11;

PYBIND11_MODULE(calc_module, m) {
    m.doc() =  "Calculation module (C++/pybind11)";

    py::class_<DeliveryResult>(m, "DeliveryResult")
        .def(py::init<>())
        .def(py::init<const Vecr&, const Vecr&, bool>(),
             py::arg("x"), py::arg("V"), py::arg("ok"))
        .def_readonly("x", &DeliveryResult::x)
        .def_readonly("V", &DeliveryResult::V)
        .def_readonly("ok", &DeliveryResult::ok);

    py::class_<UniformityIterResult, DeliveryResult>(m, "UniformityIterResult")
        .def(py::init<>()) 
        .def(py::init<const Vecr&, const Vecr&, bool, double, int, int>(),
             py::arg("x"), py::arg("V"), py::arg("ok"),
             py::arg("Mp"), py::arg("maxIter"), py::arg("iters"))
        .def_readonly("Mp", &UniformityIterResult::Mp)
        .def_readonly("maxIter", &UniformityIterResult::maxIter)
        .def_readonly("iters", &UniformityIterResult::iters);


    m.def("solve_uniform_pg", &solve_rhythmic_delivery_uniform_pg,
            py::arg("p"), py::arg("V0"), py::arg("minV"), py::arg("maxV"));
    
    m.def("solve_direct", &solve_rhythmic_delivery_bounds_direct,
        py::arg("p"), py::arg("V0"), py::arg("minV"), py::arg("maxV"));


    py::class_<Schedule>(m, "Schedule")
        .def(py::init<>())
        .def_readonly("start", &Schedule::start)
        .def_readonly("finish", &Schedule::finish)
        .def_readonly("cmax", &Schedule::cmax);

     m.def("solve_pcplp", &solve_PCPLP,
          py::arg("N"), py::arg("M"),
          py::arg("dur"), py::arg("rel"), py::arg("cap"), py::arg("demands"), py::arg("preds"));

}
