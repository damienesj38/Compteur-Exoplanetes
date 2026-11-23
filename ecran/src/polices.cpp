#include "polices.h"

#include "fonts/f_b12.h"
#include "fonts/f_b15.h"
#include "fonts/f_b18.h"
#include "fonts/f_b24.h"
#include "fonts/f_b28.h"
#include "fonts/f_s14.h"
#include "fonts/f_s16.h"
#include "fonts/f_s17.h"
#include "fonts/f_s21.h"

namespace {

const uint8_t* const DONNEES[NB_POLICES] = {f_b12, f_s14, f_b15, f_s16, f_s17, f_b18, f_s21, f_b24, f_b28};
const uint32_t TAILLES[NB_POLICES] = {sizeof f_b12, sizeof f_s14, sizeof f_b15, sizeof f_s16, sizeof f_s17,
                                      sizeof f_b18, sizeof f_s21, sizeof f_b24, sizeof f_b28};

lgfx::PointerWrapper g_sources[NB_POLICES];
lgfx::VLWfont g_polices[NB_POLICES];

}  // namespace

void initPolices() {
    for (int i = 0; i < NB_POLICES; ++i) {
        g_sources[i].set(DONNEES[i], TAILLES[i]);
        g_polices[i].loadFont(&g_sources[i]);
    }
}

const lgfx::IFont* police(Police p) { return &g_polices[p]; }
