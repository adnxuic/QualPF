#ifndef QUALPF_LEGACY_CRYSTAL_BRIDGE_H
#define QUALPF_LEGACY_CRYSTAL_BRIDGE_H

#include <QString>

// Data extracted from a CIF file via Fortran get_crystal_info_from_cif
namespace qualpf::testing {
struct CifCrystalInfo {
    int   nat        = 0;
    int   zval       = 0;
    int   nrefl      = 0;
    int   nrefl_print = 0;
    float cellpar[6] = {};
    int   icell[6]   = {};
    float vol        = 0.f;
    float dens       = 0.f;
    float mu         = 0.f;
    float rir        = 0.f;
    float wavelen    = 0.f;
    char  sform[256] = {};
    char  subfile[32] = {};
    char  spg_sym[64] = {};
    char  crysys[64]  = {};
    int   refl_h[500]    = {};
    int   refl_k[500]    = {};
    int   refl_l[500]    = {};
    int   refl_mult[500] = {};
    float refl_tth[500]  = {};
    float refl_d[500]    = {};
    float refl_lp[500]   = {};
    float refl_fc2[500]  = {};
    float refl_inte[500] = {};
    float refl_ipct[500] = {};
    int   nelem          = 0;
    char  specie_label[100][3] = {};  // element symbols, null-terminated (2 chars + '\0')
    char  chem_name[256]    = {};
    char  mineral_name[256] = {};
};

}
#endif
