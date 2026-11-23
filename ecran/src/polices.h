// Polices Barlow Condensed (SIL Open Font License 1.1) converties en polices lissées.
#pragma once

#include <LovyanGFX.hpp>

enum Police {
    B12, S14, B15, S16, S17, B18, S21, B24, B28, NB_POLICES
};

void initPolices();
const lgfx::IFont* police(Police p);
