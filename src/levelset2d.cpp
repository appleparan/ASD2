﻿#include "levelset2d.h"

LevelSetSolver2D::LevelSetSolver2D(int nx, int ny, int num_bc_grid, double baseX, double baseY, double dx, double dy) :
    kEps(std::min(dx, dy)),
    kThickness(1.5 * kEps),
    kNx(nx), kNy(ny), kNumBCGrid(num_bc_grid),
    kArrSize(static_cast<int64_t>(kNx + 2 * kNumBCGrid) *
    static_cast<int64_t>(kNy + 2 * kNumBCGrid)),
    kBaseX(baseX), kBaseY(baseY),
    kDx(dx), kDy(dy),
    kAdt(kEps * 0.5), m_atime(0.0), kMaxATime(1.5 * kThickness),
    kENOSpatialOrder(2)  {
}

LevelSetSolver2D::LevelSetSolver2D(int nx, int ny, int num_bc_grid, double baseX, double baseY, double dx, double dy, double maxTime) :
    kEps(std::min(dx, dy)),
    kThickness(1.5 * kEps),
    kNx(nx), kNy(ny), kNumBCGrid(num_bc_grid),
    kArrSize(static_cast<int64_t>(kNx + 2 * kNumBCGrid) *
    static_cast<int64_t>(kNy + 2 * kNumBCGrid)),
    kBaseX(baseX), kBaseY(baseY),
    kDx(dx), kDy(dy),
    kAdt(kEps * 0.5), m_atime(0.0), kMaxATime(maxTime),
    kENOSpatialOrder(2)  {
}

std::vector<double> LevelSetSolver2D::UpdateHeavisideFuncDeriv(const std::vector<double>& ls) {
    std::vector<double> HeavisideDeriv(kArrSize, 0.0);
    
    for (int i = 0; i < kNx + 2 * kNumBCGrid; i++)
    for (int j = 0; j < kNy + 2 * kNumBCGrid; j++) {
        if (std::fabs(ls[idx(i, j)]) > kEps)
            HeavisideDeriv[idx(i, j)] = 0.0;
        else
            HeavisideDeriv[idx(i, j)]
            = 0.5 * (1.0 / kEps
            + cos(M_PI * ls[idx(i, j)] / kEps));
    }

    return HeavisideDeriv;
}

int LevelSetSolver2D::Solve_LevelSet_2D(
    std::vector<double>& ls, const std::vector<double>& u, const std::vector<double>& v, double dt) {

    // Level set is a P grid, interpolate u and v to P grid
    std::vector<double> tmpU(kArrSize, 0.0);
    std::vector<double> tmpV(kArrSize, 0.0);

    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        tmpU[idx(i, j)] = (u[idx(i, j)] + u[idx(i + 1, j)]) * 0.5;
        tmpV[idx(i, j)] = (v[idx(i, j)] + v[idx(i, j + 1)]) * 0.5;
    }
    
    ApplyBC_P_2D(tmpU);
    ApplyBC_P_2D(tmpV);

    // TVD RK3
    std::vector<double> DLS1 = HJWENO5_LS_2D(ls, tmpU, tmpV);
    
    std::vector<double> LS1(kArrSize, 0.0);
    std::vector<double> LS2(kArrSize, 0.0);
    
    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        LS1[idx(i, j)] = ls[idx(i, j)] - dt * DLS1[idx(i, j)];
    }
    ApplyBC_P_2D(LS1);
    std::vector<double> DLS2 = HJWENO5_LS_2D(LS1, tmpU, tmpV);
    
    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        LS2[idx(i, j)] = 0.75 * ls[idx(i, j)] + 0.25 * LS1[idx(i, j)] - 0.25 * dt * DLS2[idx(i, j)];
    }
    ApplyBC_P_2D(LS2);
    std::vector<double> DLS3 = HJWENO5_LS_2D(LS2, tmpU, tmpV);

    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        ls[idx(i, j)] = 1.0 / 3.0 * ls[idx(i, j)] + 2.0 / 3.0 * LS2[idx(i, j)] - 2.0 / 3.0 * dt * DLS3[idx(i, j)];
    }
    ApplyBC_P_2D(ls);
    
    return 0;
}

std::vector<double> LevelSetSolver2D::HJWENO5_LS_2D(std::vector<double>& ls,
    const std::vector<double>& u, const std::vector<double>& v) {

    std::vector<double> L(kArrSize, 0.0);
    std::vector<double> LXP(kArrSize, 0.0);
    std::vector<double> LXM(kArrSize, 0.0);
    std::vector<double> LYP(kArrSize, 0.0);
    std::vector<double> LYM(kArrSize, 0.0);

    std::vector<double> vecF_LSX(kNx + 2 * kNumBCGrid, 0.0), vecF_LSY(kNy + 2 * kNumBCGrid, 0.0);

    ApplyBC_P_2D(ls);

    // U : X direction
    std::vector<double> FXP(kNx + 2 * kNumBCGrid, 0.0), FXM(kNx + 2 * kNumBCGrid, 0.0);
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        for (int i = 0; i < kNx + 2 * kNumBCGrid; i++) {
            vecF_LSX[i] = ls[idx(i, j)] * u[idx(i, j)];
        }

        UnitHJWENO5(vecF_LSX, FXP, FXM, kDx, kNx);

        for (int i = 0; i < kNx + 2 * kNumBCGrid; i++) {
            LXP[idx(i, j)] = FXP[i];
            LXM[idx(i, j)] = FXM[i];
        }

        // set all vector elements to zero keeping its size
        std::fill(FXP.begin(), FXP.end(), 0.0);
        std::fill(FXM.begin(), FXM.end(), 0.0);
        std::fill(vecF_LSX.begin(), vecF_LSX.end(), 0.0);
    }

    // U : Y direction  
    std::vector<double> FYP(kNy + 2 * kNumBCGrid, 0.0), FYM(kNy + 2 * kNumBCGrid, 0.0);
    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++) {
        for (int j = 0; j < kNy + 2 * kNumBCGrid; j++) {
            vecF_LSY[j] = ls[idx(i, j)] * v[idx(i, j)];
        }

        UnitHJWENO5(vecF_LSY, FYP, FYM, kDy, kNy);

        for (int j = 0; j < kNy + 2 * kNumBCGrid; j++) {
            LYP[idx(i, j)] = FYP[j];
            LYM[idx(i, j)] = FYM[j];
        }

        // set all vector elements to zero keeping its size
        std::fill(FYP.begin(), FYP.end(), 0.0);
        std::fill(FYM.begin(), FYM.end(), 0.0);
        std::fill(vecF_LSY.begin(), vecF_LSY.end(), 0.0);
    }

    // combine together with Local Lax-Friedrichs Scheme
    double alphaX = 0.0, alphaY = 0.0;

    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        alphaX = u[idx(i, j)];
        alphaY = v[idx(i, j)];

        L[idx(i, j)]
            = 0.5 * (LXP[idx(i, j)] + LXM[idx(i, j)])
            + 0.5 * (LYP[idx(i, j)] + LYM[idx(i, j)])
            - alphaX * (0.5 * (LXP[idx(i, j)] - LXM[idx(i, j)]))
            - alphaY * (0.5 * (LYP[idx(i, j)] - LYM[idx(i, j)]));
    }

    return L;
}

int LevelSetSolver2D::UnitHJWENO5(
    const std::vector<double>& F, std::vector<double>& FP, std::vector<double>& FM, const double d, const int n) {
    int stat = 0;
    const double kWeightEps = 1e-6;

    double alphaPlus[3], alphaMinus[3];
    double weightPlus[3], weightMinus[3];

    std::vector<double> IS0(n + 2 * kNumBCGrid, 0.0);
    std::vector<double> IS1(n + 2 * kNumBCGrid, 0.0);
    std::vector<double> IS2(n + 2 * kNumBCGrid, 0.0);

    // \dfrac{\Delta^+ F}{\Delta x}
    std::vector<double> DFPlus = std::vector<double>(n + 2 * kNumBCGrid, 0.0);

    // Compute Smoothness for phi^{-}
    for (int i = 0; i < n + 2 * kNumBCGrid - 1; i++)
        DFPlus[i] = F[i + 1] - F[i];

    for (int i = kNumBCGrid; i < n + kNumBCGrid; i++) {
        IS0[i]
            = 13.0 / 12.0
            * std::pow((DFPlus[i - 3] - 2.0 * DFPlus[i - 2] + DFPlus[i - 1]) / d, 2.0)
            + 1.0 / 4.0
            * std::pow((DFPlus[i - 3] - 4.0 * DFPlus[i - 2] + 3.0 * DFPlus[i - 1]) / d, 2.0);
        IS1[i]
            = 13.0 / 12.0
            * std::pow((DFPlus[i - 2] - 2.0 * DFPlus[i - 1] + DFPlus[i]) / d, 2.0)
            + 1.0 / 4.0 * std::pow((DFPlus[i - 2] - DFPlus[i]) / d, 2.0);
        IS2[i]
            = 13.0 / 12.0
            * std::pow((DFPlus[i - 1] - 2.0 * DFPlus[i] + DFPlus[i + 1]) / d, 2.0)
            + 1.0 / 4.0
            * std::pow((3.0 * DFPlus[i - 1] - 4.0 * DFPlus[i] + DFPlus[i + 1]) / d, 2.0);
    }

    // phi^{-}
    for (int i = kNumBCGrid; i < n + kNumBCGrid; i++) {
        alphaMinus[0] = 0.1 / std::pow((kWeightEps + IS0[i]), 2.0);
        alphaMinus[1] = 0.6 / std::pow((kWeightEps + IS1[i]), 2.0);
        alphaMinus[2] = 0.3 / std::pow((kWeightEps + IS2[i]), 2.0);

        weightMinus[0] = alphaMinus[0]
            / (alphaMinus[0] + alphaMinus[1] + alphaMinus[2]);
        weightMinus[1] = alphaMinus[1]
            / (alphaMinus[0] + alphaMinus[1] + alphaMinus[2]);
        weightMinus[2] = alphaMinus[2]
            / (alphaMinus[0] + alphaMinus[1] + alphaMinus[2]);

        FM[i] =
            weightMinus[0]
            * (1.0 / 3.0 * DFPlus[i - 3] - 7.0 / 6.0 * DFPlus[i - 2] + 11.0 / 6.0 * DFPlus[i - 1]) / d +
            weightMinus[1]
            * (-1.0 / 6.0 * DFPlus[i - 2] + 5.0 / 6.0 * DFPlus[i - 1] + 1.0 / 3.0 * DFPlus[i]) / d +
            weightMinus[2]
            * (1.0 / 3.0 * DFPlus[i - 1] + 5.0 / 6.0 * DFPlus[i] - 1.0 / 6.0 * DFPlus[i + 1]) / d;

        assert(FM[i] == FM[i]);
    }

    // Compute Smoothness for phi^{+}
    for (int i = kNumBCGrid; i < n + kNumBCGrid; i++) {
        IS0[i]
            = 13.0 / 12.0
            * std::pow((DFPlus[i + 2] - 2.0 * DFPlus[i + 1] + DFPlus[i]) / d, 2.0)
            + 1.0 / 4.0
            * std::pow((DFPlus[i + 2] - 4.0 * DFPlus[i + 1] + 3.0 * DFPlus[i]) / d, 2.0);
        IS1[i]
            = 13.0 / 12.0
            * std::pow((DFPlus[i + 1] - 2.0 * DFPlus[i] + DFPlus[i - 1]) / d, 2.0)
            + 1.0 / 4.0 * std::pow((DFPlus[i + 1] - DFPlus[i - 1]) / d, 2.0);
        IS2[i]
            = 13.0 / 12.0
            * std::pow((DFPlus[i] - 2.0 * DFPlus[i - 1] + DFPlus[i - 2]) / d, 2.0)
            + 1.0 / 4.0
            * std::pow((3.0 * DFPlus[i] - 4.0 * DFPlus[i - 1] + DFPlus[i - 2]) / d, 2.0);
    }

    // phi^{+}
    for (int i = kNumBCGrid; i < n + kNumBCGrid; i++) {
        alphaPlus[0] = 0.1 / std::pow((kWeightEps + IS0[i]), 2.0);
        alphaPlus[1] = 0.6 / std::pow((kWeightEps + IS1[i]), 2.0);
        alphaPlus[2] = 0.3 / std::pow((kWeightEps + IS2[i]), 2.0);

        weightPlus[0] = alphaPlus[0]
            / (alphaPlus[0] + alphaPlus[1] + alphaPlus[2]);
        weightPlus[1] = alphaPlus[1]
            / (alphaPlus[0] + alphaPlus[1] + alphaPlus[2]);
        weightPlus[2] = alphaPlus[2]
            / (alphaPlus[0] + alphaPlus[1] + alphaPlus[2]);

        FP[i] =
            weightPlus[0]
            * (1.0 / 3.0 * DFPlus[i + 2] - 7.0 / 6.0 * DFPlus[i + 1] + 11.0 / 6.0 * DFPlus[i]) / d +
            weightPlus[1]
            * (-1.0 / 6.0 * DFPlus[i + 1] + 5.0 / 6.0 * DFPlus[i] + 1.0 / 3.0 * DFPlus[i - 1]) / d +
            weightPlus[2]
            * (1.0 / 3.0 * DFPlus[i] + 5.0 / 6.0 * DFPlus[i - 1] - 1.0 / 6.0 * DFPlus[i - 2]) / d;

        assert(FP[i] == FP[i]);
    }

    return 0;
}

int LevelSetSolver2D::Reinit_Original_2D(std::vector<double>& ls) {
    m_atime = 0.0;

    std::vector<double> lsInit(kArrSize, 0.0), tmpLS(kArrSize, 0.0);
    lsInit = ls;

    std::vector<double> absdLS1(kArrSize, 0.0), absdLS2(kArrSize, 0.0);
    std::vector<double> LS1(kArrSize, 0.0), LS2(kArrSize, 0.0);
    std::vector<double> heavisideDeriv(kArrSize, 0.0);

    while (m_atime < kMaxATime) {
        m_atime += kAdt;
        
        // Apply Boundary Condition First
        ApplyBC_P_2D(ls);

        // absdLS1 = HJWENO5_DerivAbsLS_2D(ls, lsInit);
        absdLS1 = ENO_DerivAbsLS_2D(ls, lsInit);

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            LS1[idx(i, j)] = ls[idx(i, j)]
                - kAdt * sign(lsInit[idx(i, j)]) * (absdLS1[idx(i, j)] - 1.0);
        }
        ApplyBC_P_2D(LS1);

        // absdLS2 = HJWENO5_DerivAbsLS_2D(LS1, lsInit);
        absdLS2 = ENO_DerivAbsLS_2D(LS1, lsInit);

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            LS2[idx(i, j)] = LS1[idx(i, j)]
                - kAdt * sign(lsInit[idx(i, j)]) * (absdLS2[idx(i, j)] - 1.0);
        }
        ApplyBC_P_2D(LS2);

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            tmpLS[idx(i, j)] = 0.5 * (ls[idx(i, j)] + LS2[idx(i, j)]);
        }
        ApplyBC_P_2D(tmpLS);

        heavisideDeriv = UpdateHeavisideFuncDeriv(tmpLS);
        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            // if (heavisideDeriv[idx(i, j)] != 0.0)
                ls[idx(i, j)] = tmpLS[idx(i, j)];
        }
    }

    ApplyBC_P_2D(ls);
    return 0;
}

int LevelSetSolver2D::Reinit_Sussman_2D(std::vector<double>& ls) {
    // using 2nd order HJ ENO, based on Sussman's work
    // Sussman, Mark, et al. "An improved level set method for incompressible two-phase flows."
    // Computers & Fluids 27.5 (1998): 663-680.

    // HJENO 2nd order + RK2
    m_atime = 0.0;

    std::vector<double> lsInit(kArrSize, 0.0);
    lsInit = ls;

    std::vector<double> absdLS1(kArrSize, 0.0), absdLS2(kArrSize, 0.0);
    std::vector<double> LS1(kArrSize, 0.0), LS2(kArrSize, 0.0);
    std::vector<double> tmpLS(kArrSize, 0.0);
    std::vector<double> sussmanConstraint(kArrSize, 0.0);
    std::vector<double> heavisideDeriv(kArrSize, 0.0);

    while (m_atime < kMaxATime) {
        m_atime += kAdt;
        
        // Apply Boundary Condition First
        ApplyBC_P_2D(ls);

        // absdLS1 = HJWENO5_DerivAbsLS_2D(ls, lsInit);
        absdLS1 = ENO_DerivAbsLS_2D(ls, lsInit);

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            LS1[idx(i, j)] = ls[idx(i, j)]
                - kAdt * sign(lsInit[idx(i, j)]) * (absdLS1[idx(i, j)] - 1.0);
        }

        // absdLS2 = HJWENO5_DerivAbsLS_2D(LS1, lsInit);
        absdLS2 = ENO_DerivAbsLS_2D(LS1, lsInit);

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            LS2[idx(i, j)] = LS1[idx(i, j)]
                - kAdt * sign(lsInit[idx(i, j)]) * (absdLS2[idx(i, j)] - 1.0);
        }

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            tmpLS[idx(i, j)] = 0.5 * (ls[idx(i, j)] + LS2[idx(i, j)]);
        }
        ApplyBC_P_2D(tmpLS);

        // Sussman's reinitialization
        heavisideDeriv = UpdateHeavisideFuncDeriv(tmpLS);
        sussmanConstraint = GetSussmanReinitConstraint(tmpLS, lsInit, heavisideDeriv);
        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            ls[idx(i, j)] += sussmanConstraint[idx(i, j)];
        }
    }
    ApplyBC_P_2D(ls);
    return 0;
}

int LevelSetSolver2D::FirstTimeOnlyReinit_Sussman_2D(std::vector<double>& ls) {
    // using 2nd order HJ ENO, based on Sussman's work
    // Sussman, Mark, et al. "An improved level set method for incompressible two-phase flows."
    // Computers & Fluids 27.5 (1998): 663-680.

    // HJENO 2nd order + RK2
    m_atime = 0.0;
    std::vector<double> lsInit(kArrSize, 0.0);
    lsInit = ls;

    std::vector<double> absdLS1(kArrSize, 0.0), absdLS2(kArrSize, 0.0);
    std::vector<double> LS1(kArrSize, 0.0), LS2(kArrSize, 0.0);

    absdLS1 = ENO_DerivAbsLS_2D(lsInit, lsInit);

    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        if (absdLS1[idx(i, j)] > 0.0)
            lsInit[idx(i, j)] = lsInit[idx(i, j)] / absdLS1[idx(i, j)];
    }
    ApplyBC_P_2D(lsInit);

    while (m_atime < std::max(kDx * kNx, kDy * kNy)) {
        m_atime += kAdt;
        
        // Apply Boundary Condition First
        ApplyBC_P_2D(ls);

        absdLS1 = ENO_DerivAbsLS_2D(ls, lsInit);
        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            LS1[idx(i, j)] = ls[idx(i, j)]
                - kAdt * sign(lsInit[idx(i, j)]) * (absdLS1[idx(i, j)] - 1.0);
        }
        ApplyBC_P_2D(LS1);

        absdLS2 = ENO_DerivAbsLS_2D(LS1, lsInit);

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            LS2[idx(i, j)] = ls[idx(i, j)]
                - kAdt * sign(lsInit[idx(i, j)]) * (absdLS2[idx(i, j)] - 1.0);
        }
        ApplyBC_P_2D(LS2);

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            ls[idx(i, j)] = 0.5 * (ls[idx(i, j)] + LS2[idx(i, j)]);
        }
    }

    ApplyBC_P_2D(ls);

    return 0;
}

int LevelSetSolver2D::Reinit_MinRK2_2D(std::vector<double>& ls) {
    m_atime = 0.0;
    ApplyBC_P_2D(ls);

    std::vector<double> lsInit(kArrSize, 0.0);
    lsInit = ls;
    
    std::vector<double> absdLS1(kArrSize, 0.0), absdLS2(kArrSize, 0.0);
    std::vector<double> LS1(kArrSize, 0.0), LS2(kArrSize, 0.0);
    std::vector<double> tmpLS(kArrSize, 0.0);
    std::vector<double> heavisideDeriv(kArrSize, 0.0), smoothedSignFunc(kArrSize, 0.0);
    
    smoothedSignFunc = GetSmoothedSignFunc(lsInit);
    
    while (m_atime < kMaxATime) {
        m_atime += kAdt;
        
        // Apply Boundary Condition First
        ApplyBC_P_2D(ls);

        absdLS1 = SubcellENO_Min_DerivAbsLS_2D(ls, lsInit);

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            LS1[idx(i, j)] = ls[idx(i, j)]
                 - kAdt * smoothedSignFunc[idx(i, j)] * (absdLS1[idx(i, j)] - 1.0);
        }
        ApplyBC_P_2D(LS1);

        absdLS2 = SubcellENO_Min_DerivAbsLS_2D(LS1, lsInit);

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            LS2[idx(i, j)] = LS1[idx(i, j)]
                - kAdt * smoothedSignFunc[idx(i, j)] * (absdLS2[idx(i, j)] - 1.0);
        }
        ApplyBC_P_2D(LS2);

        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            tmpLS[idx(i, j)] = 0.5 * (ls[idx(i, j)] + LS2[idx(i, j)]);
        }
        ApplyBC_P_2D(tmpLS);

        // sussmanConstraint = GetSussmanReinitConstraint(tmpLS, lsInit, heavisideDeriv);
        for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
        for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
            ls[idx(i, j)] = tmpLS[idx(i, j)];
        }
    }

    ApplyBC_P_2D(ls);
    return 0;
}

std::vector<double> LevelSetSolver2D::HJWENO5_DerivAbsLS_2D(const std::vector<double>& ls, const std::vector<double>& lsInit) {

    std::vector<double> absdLS(kArrSize, 0.0);
    std::vector<double> dPX(kArrSize, 0.0);
    std::vector<double> dMX(kArrSize, 0.0);
    std::vector<double> dPY(kArrSize, 0.0);
    std::vector<double> dMY(kArrSize, 0.0);

    std::vector<double> vecF_dX(kNx + 2 * kNumBCGrid, 0.0), vecF_dY(kNy + 2 * kNumBCGrid, 0.0);

    // U : X direction
    std::vector<double> FXP(kNx + 2 * kNumBCGrid, 0.0), FXM(kNx + 2 * kNumBCGrid, 0.0);
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        for (int i = 0; i < kNx + 2 * kNumBCGrid; i++) {
            vecF_dX[i] = ls[idx(i, j)];
        }

        UnitHJWENO5(vecF_dX, FXP, FXM, kDx, kNx);

        for (int i = 0; i < kNx + 2 * kNumBCGrid; i++) {
            dPX[idx(i, j)] = FXP[i];
            dMX[idx(i, j)] = FXM[i];
        }

        // set all vector elements to zero keeping its size
        std::fill(FXP.begin(), FXP.end(), 0.0);
        std::fill(FXM.begin(), FXM.end(), 0.0);
        std::fill(vecF_dX.begin(), vecF_dX.end(), 0.0);
    }

    // U : Y direction  
    std::vector<double> FYP(kNy + 2 * kNumBCGrid, 0.0), FYM(kNy + 2 * kNumBCGrid, 0.0);
    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++) {
        for (int j = 0; j < kNy + 2 * kNumBCGrid; j++) {
            vecF_dY[j] = ls[idx(i, j)];
        }

        UnitHJWENO5(vecF_dY, FYP, FYM, kDy, kNy);

        for (int j = 0; j < kNy + 2 * kNumBCGrid; j++) {
            dPY[idx(i, j)] = FYP[j];
            dMY[idx(i, j)] = FYM[j];
        }

        // set all vector elements to zero keeping its size
        std::fill(FYP.begin(), FYP.end(), 0.0);
        std::fill(FYM.begin(), FYM.end(), 0.0);
        std::fill(vecF_dY.begin(), vecF_dY.end(), 0.0);
    }

    // Use godunov scheme
    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        if (lsInit[idx(i, j)] >= 0)
            absdLS[idx(i, j)] = std::sqrt(
            std::max(std::pow(std::min(dPX[idx(i, j)], 0.0), 2.0),
            std::pow(std::max(dMX[idx(i, j)], 0.0), 2.0))
            + std::max(std::pow(std::min(dPY[idx(i, j)], 0.0), 2.0),
            std::pow(std::max(dMY[idx(i, j)], 0.0), 2.0)));
        else
            absdLS[idx(i, j)] = std::sqrt(
            std::max(std::pow(std::max(dPX[idx(i, j)], 0.0), 2.0),
            std::pow(std::min(dMX[idx(i, j)], 0.0), 2.0))
            + std::max(std::pow(std::max(dPY[idx(i, j)], 0.0), 2.0),
            std::pow(std::min(dMY[idx(i, j)], 0.0), 2.0)));
    
    }

    return absdLS;
}

std::vector<double>
LevelSetSolver2D::ENO_DerivAbsLS_2D(const std::vector<double>& ls, const std::vector<double>& lsInit) {
    std::vector<double>
        dPX(kArrSize, 0.0), dMX(kArrSize, 0.0),
        dPY(kArrSize, 0.0), dMY(kArrSize, 0.0);

    std::vector<double>
        dLSdX(kArrSize, 0.0), dLSdY(kArrSize, 0.0);

    std::vector<double> absdLS(kArrSize, 0.0);

    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        // 2nd order HJENO Method with minmod limiter
        dPX[idx(i, j)] = (ls[idx(i + 1, j)] - ls[idx(i, j)]) / kDx
            - kDx * 0.5 * MinMod(
            (ls[idx(i - 1, j)] - 2.0 * ls[idx(i, j)] + ls[idx(i + 1, j)]) / (kDx * kDx),
            (ls[idx(i, j)] - 2.0 * ls[idx(i + 1, j)] + ls[idx(i + 2, j)]) / (kDx * kDx));
        dMX[idx(i, j)] = (ls[idx(i, j)] - ls[idx(i - 1, j)]) / kDx
            + kDx * 0.5 * MinMod(
            (ls[idx(i - 1, j)] - 2.0 * ls[idx(i, j)] + ls[idx(i + 1, j)]) / (kDx * kDx),
            (ls[idx(i - 2, j)] - 2.0 * ls[idx(i - 1, j)] + ls[idx(i, j)]) / (kDx * kDx));
    }

    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        // 2nd order HJENO Method with minmod limiter
        dPY[idx(i, j)] = (ls[idx(i, j + 1)] - ls[idx(i, j)]) / kDy
            - kDy * 0.5 * MinMod(
            (ls[idx(i, j - 1)] - 2.0 * ls[idx(i, j)] + ls[idx(i, j + 1)]) / (kDy * kDy),
            (ls[idx(i, j)] - 2.0 * ls[idx(i, j + 1)] + ls[idx(i, j + 2)]) / (kDy * kDy));
        dMY[idx(i, j)] = (ls[idx(i, j)] - ls[idx(i, j - 1)]) / kDy
            + kDy * 0.5 * MinMod(
            (ls[idx(i, j - 1)] - 2.0 * ls[idx(i, j)] + ls[idx(i, j + 1)]) / (kDy * kDy),
            (ls[idx(i, j - 2)] - 2.0 * ls[idx(i, j - 1)] + ls[idx(i, j)]) / (kDy * kDy));
    }

    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        if (lsInit[idx(i, j)] >= 0)
            absdLS[idx(i, j)] = std::sqrt(
            std::max(std::pow(std::min(dPX[idx(i, j)], 0.0), 2.0),
                     std::pow(std::max(dMX[idx(i, j)], 0.0), 2.0))
            + std::max(std::pow(std::min(dPY[idx(i, j)], 0.0), 2.0),
                       std::pow(std::max(dMY[idx(i, j)], 0.0), 2.0)));
        else
            absdLS[idx(i, j)] = std::sqrt(
            std::max(std::pow(std::max(dPX[idx(i, j)], 0.0), 2.0),
                     std::pow(std::min(dMX[idx(i, j)], 0.0), 2.0))
            + std::max(std::pow(std::max(dPY[idx(i, j)], 0.0), 2.0),
                       std::pow(std::min(dMY[idx(i, j)], 0.0), 2.0)));
    }

    return absdLS;
}

std::vector<double>
LevelSetSolver2D::SubcellENO_Min_DerivAbsLS_2D(const std::vector<double>& ls, const std::vector<double>& lsInit) {
    // Min, Chohong, and Frédéric Gibou.
    // "A second order accurate level set method on non-graded adaptive cartesian grids."
    // Journal of Computational Physics 225.1 (2007) : 300 - 321.
    std::vector<double>
        dPX(kArrSize, 0.0), dMX(kArrSize, 0.0),
        dPY(kArrSize, 0.0), dMY(kArrSize, 0.0);

    std::vector<double> dLSdX(kArrSize, 0.0), dLSdY(kArrSize, 0.0);
    std::vector<double> absdLS(kArrSize, 0.0);

    // new dx or dy using subcell resolution
    double newDxP = 0.0, newDxM = 0.0, newDyP = 0.0, newDyM = 0.0;
    // minmod of divided differences of lsInitial
    double DDX = 0.0, DDY = 0.0;
    // coefficient of quadaratic poynomial
    double c0 = 0.0, c1 = 0.0, c2 = 0.0;
    const double kSmallValue = 1.0e-10;

    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        // 2nd order ENO Method
        dPX[idx(i, j)] = (ls[idx(i + 1, j)] - ls[idx(i, j)]) / kDx
            - kDx * 0.5 * MinMod(
            (ls[idx(i - 1, j)] - 2.0 * ls[idx(i, j)] + ls[idx(i + 1, j)]) / (kDx * kDx),
            (ls[idx(i, j)] - 2.0 * ls[idx(i + 1, j)] + ls[idx(i + 2, j)]) / (kDx * kDx));
        dMX[idx(i, j)] = (ls[idx(i, j)] - ls[idx(i - 1, j)]) / kDx
            + kDx * 0.5 * MinMod(
            (ls[idx(i - 1, j)] - 2.0 * ls[idx(i, j)] + ls[idx(i + 1, j)]) / (kDx * kDx),
            (ls[idx(i - 2, j)] - 2.0 * ls[idx(i - 1, j)] + ls[idx(i, j)]) / (kDx * kDx));
        // ENO polynomial from Fig. 4 in "A second order accurate level set method on non-graded adaptive cartesian grids."
        // U(x) = U[x_i] + (x - x_i) * U[x_i, x_i+1] + (x - x_i) * (x - x_i+1) * U[x_i-1, x_i, x_i+1]
        // phi(x) = phi(-s2/2) + (x - s2/2) * (phi(x_i+1) - phi(x_i)) / s2 + (x - s2/2) * (x + s2/2) * MinMod(phi[x_i-1, x_i, x_i+1], phi[x_i, x_i+1, x_i+2])/2
        // phi(x) = phi(-s2/2)-(phi(x_i+1) - phi(x_i))/2 -(s2 * s2) / 4 * MinMod(phi[x_i-1, x_i, x_i+1], phi[x_i, x_i+1, x_i+2])/2 + x * (phi(x_i+1) - phi(x_i)) / s2 + x * x * MinMod(phi[x_i-1, x_i, x_i+1], phi[x_i, x_i+1, x_i+2])/2
        // phi(x) = c0 + c1 * x + c2 * x * x
        // x = (-c1 +- sqrt(c1 * c1  - 4.0 * c0 * c2)) / (2.0 * c2)
        // x = (-c1 - sqrt(c1 * c1  - 4.0 * c0 * c2)) / (2.0 * c2) if phi(x_i) > 0 (draw parabola)
        // x = (-c1 + sqrt(c1 * c1  - 4.0 * c0 * c2)) / (2.0 * c2) if phi(x_i) < 0 

        if (lsInit[idx(i, j)] * lsInit[idx(i + 1, j)] < 0) {
            DDX = MinMod(
                (lsInit[idx(i - 1, j)] - 2.0 * lsInit[idx(i, j)] + lsInit[idx(i + 1, j)]) / (kDx * kDx),
                (lsInit[idx(i, j)] - 2.0 * lsInit[idx(i + 1, j)] + lsInit[idx(i + 2, j)]) / (kDx * kDx));

            c2 = 0.5 * DDX;
            c1 = (lsInit[idx(i + 1, j)] - lsInit[idx(i, j)]) / kDx;
            c0 = (lsInit[idx(i + 1, j)] + lsInit[idx(i, j)]) * 0.5 - c2 * kDx * kDx * 0.25;
            // std::fabs(DDX) > kSmallValue : quadaratic interpolation
            // std::fabs(DDX) <= kSmallValue : linear interpolation
            // no need to use smooth signed func, just want remove else if statement
            if (std::fabs(c2) > kSmallValue) {
                newDxP = 0.5 * kDx +
                    (-c1 - sign(lsInit[idx(i, j)]) * std::sqrt(c1 * c1 - 4.0 * c2 * c0)) / (2.0 * c2);
            }
            else
                newDxP = 0.5 * kDx - c0 / c1;
            
            dPX[idx(i, j)] = -ls[idx(i, j)] / newDxP - 0.5 * newDxP * DDX;

            if (std::fabs(newDxP) < kDx * 0.01)
                dPX[idx(i, j)] = -0.5 * newDxP * DDX;
        }
        // ENO polynomial from Fig. 4 in "A second order accurate level set method on non-graded adaptive cartesian grids."
        // U(x) = U[x_i] + (x - x_i) * U[x_i-1, x_i] + (x - x_i-1) * (x - x_i) * U[x_i-1, x_i, x_i+1]
        // phi(x) = phi(x_i) + (x - s1/2) * (phi(x_i) - phi(x_i-1)) / s1 + (x - s1/2) * (x + s1/2) * MinMod(phi[x_i-2, x_i-1, x_i], phi[x_i-1, x_i, x_i+1])/2
        // phi(x) = phi(x_i)-(phi(x_i) - phi(x_i-1))/2 -(s1 * s1) / 4 * MinMod(phi[x_i-2, x_i-1, x_i], phi[x_i-1, x_i, x_i+1])/2 + x * (phi(x_i) - phi(x_i-1)) / s1 + x * x * MinMod(phi[x_i-2, x_i-1, x_i], phi[x_i-1, x_i, x_i+1])/2
        // phi(x) = c0 + c1 * x + c2 * x * x
        // x = (-c1 +- sqrt(c1 * c1  - 4.0 * c0 * c2)) / (2.0 * c2)
        // x = (-c1 - sqrt(c1 * c1  - 4.0 * c0 * c2)) / (2.0 * c2) if phi(x_i) > 0 (draw parabola)
        // x = (-c1 - sqrt(c1 * c1  - 4.0 * c0 * c2)) / (2.0 * c2) if phi(x_i) < 0
        if (lsInit[idx(i, j)] * lsInit[idx(i - 1, j)] < 0) {
            DDX = MinMod(
                (lsInit[idx(i - 1, j)] - 2.0 * lsInit[idx(i, j)] + lsInit[idx(i + 1, j)]) / (kDx * kDx),
                (lsInit[idx(i - 2, j)] - 2.0 * lsInit[idx(i - 1, j)] + lsInit[idx(i, j)]) / (kDx * kDx));
            
            c2 = 0.5 * DDX;
            c1 = (lsInit[idx(i, j)] - lsInit[idx(i - 1, j)]) / kDx;
            c0 = (lsInit[idx(i, j)] + lsInit[idx(i - 1, j)]) * 0.5 - c2 * kDx * kDx * 0.25;
            // std::fabs(DDX) > kSmallValue : quadaratic interpolation
            // std::fabs(DDX) <= kSmallValue : linear interpolation
            // no need to use smooth signed func, just want remove else if statement
            if (std::fabs(c2) > kSmallValue) {
                newDxM = 0.5 * kDx -
                    (-c1 + sign(lsInit[idx(i, j)]) * std::sqrt(c1 * c1 - 4.0 * c2 * c0)) / (2.0 * c2);
            }
            else
                newDxM = 0.5 * kDx + c0 / c1;
            
            dMX[idx(i, j)] = ls[idx(i, j)] / newDxM + 0.5 * newDxM * DDX;

            if (std::fabs(newDxM) < kDx * 0.01)
                dMX[idx(i, j)] = 0.5 * newDxM * DDX;
        }
    }

    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        // 2nd order ENO Method
        dPY[idx(i, j)] = (ls[idx(i, j + 1)] - ls[idx(i, j)]) / kDy
            - kDy * 0.5 * MinMod(
            (ls[idx(i, j - 1)] - 2.0 * ls[idx(i, j)] + ls[idx(i, j + 1)]) / (kDy * kDy),
            (ls[idx(i, j)] - 2.0 * ls[idx(i, j + 1)] + ls[idx(i, j + 2)]) / (kDy * kDy));
        dMY[idx(i, j)] = (ls[idx(i, j)] - ls[idx(i, j - 1)]) / kDy
            + kDy * 0.5 * MinMod(
            (ls[idx(i, j - 1)] - 2.0 * ls[idx(i, j)] + ls[idx(i, j + 1)]) / (kDy * kDy),
            (ls[idx(i, j - 2)] - 2.0 * ls[idx(i, j - 1)] + ls[idx(i, j)]) / (kDy * kDy));
        
        if (lsInit[idx(i, j)] * lsInit[idx(i, j + 1)] < 0) {
            DDY = MinMod(
                (lsInit[idx(i, j - 1)] - 2.0 * lsInit[idx(i, j)] + lsInit[idx(i, j + 1)]) / (kDy * kDy),
                (lsInit[idx(i, j)] - 2.0 * lsInit[idx(i, j + 1)] + lsInit[idx(i, j + 2)]) / (kDy * kDy));
            c2 = 0.5 * DDY;
            c1 = (lsInit[idx(i, j + 1)] - lsInit[idx(i, j)]) / kDy;
            c0 = (lsInit[idx(i, j + 1)] + lsInit[idx(i, j)]) * 0.5 - c2 * kDy * kDy * 0.25;
            // std::fabs(DDY) > kSmallValue : quadaratic interpolation
            // std::fabs(DDY) <= kSmallValue : linear interpolation
            if (std::fabs(c2) > kSmallValue) {
                newDyP = 0.5 * kDy +
                    (-c1 - sign(lsInit[idx(i, j)]) * std::sqrt(c1 * c1 - 4.0 * c2 * c0)) / (2.0 * c2);
            }
            else
                newDyP = 0.5 * kDy - c0 / c1;
            
            dPY[idx(i, j)] = -ls[idx(i, j)] / newDyP - 0.5 * newDyP * DDY;

            if (std::fabs(newDyP) < kDy * 0.01)
                dPY[idx(i, j)] = -0.5 * newDyP * DDY;
        }
        
        if (lsInit[idx(i, j)] * lsInit[idx(i, j - 1)] < 0) {
            DDY = MinMod(
                (lsInit[idx(i, j - 1)] - 2.0 * lsInit[idx(i, j)] + lsInit[idx(i, j + 1)]) / (kDy * kDy),
                (lsInit[idx(i, j - 2)] - 2.0 * lsInit[idx(i, j - 1)] + lsInit[idx(i, j)]) / (kDy * kDy));
            c2 = 0.5 * DDY;
            c1 = (lsInit[idx(i, j)] - lsInit[idx(i, j - 1)]) / kDy;
            c0 = (lsInit[idx(i, j)] + lsInit[idx(i, j - 1)]) * 0.5 - c2 * kDy * kDy * 0.25;
            // std::fabs(DDY) > kSmallValue : quadaratic interpolation
            // std::fabs(DDY) <= kSmallValue : linear interpolation
            if (std::fabs(c2) > kSmallValue) {
                newDyM = 0.5 * kDy -
                    (-c1 + sign(lsInit[idx(i, j)]) * std::sqrt(c1 * c1 - 4.0 * c2 * c0)) / (2.0 * c2);
            }
            else
                newDyM = 0.5 * kDy + c0 / c1;
            
            dMY[idx(i, j)] = ls[idx(i, j)] / newDyM + 0.5 * newDyM * DDY;

            if (std::fabs(newDyM) < kDy * 0.01)
                dMY[idx(i, j)] = 0.5 * newDyM * DDY;
        }
    }

    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        if (lsInit[idx(i, j)] >= 0)
            absdLS[idx(i, j)] = std::sqrt(
            std::max(std::pow(std::min(dPX[idx(i, j)], 0.0), 2.0),
                     std::pow(std::max(dMX[idx(i, j)], 0.0), 2.0))
            + std::max(std::pow(std::min(dPY[idx(i, j)], 0.0), 2.0),
                       std::pow(std::max(dMY[idx(i, j)], 0.0), 2.0)));
        else
            absdLS[idx(i, j)] = std::sqrt(
            std::max(std::pow(std::max(dPX[idx(i, j)], 0.0), 2.0),
                     std::pow(std::min(dMX[idx(i, j)], 0.0), 2.0))
            + std::max(std::pow(std::max(dPY[idx(i, j)], 0.0), 2.0),
                       std::pow(std::min(dMY[idx(i, j)], 0.0), 2.0)));
    }
    
    return absdLS;
}

std::vector<double> LevelSetSolver2D::GetSussmanReinitConstraint(const std::vector<double>& ls,
    const std::vector<double>& lsInit, const std::vector<double>& heavisideDeriv) {
    double lambda = 0.0;
    double lsFraction[3][3], lsInitFraction[3][3], Hp[3][3], L[3][3], dLSAbs[3][3], f[3][3];
    std::vector<double> sussmanConstraint(kArrSize, 0.0);

    // Sussman's reinitialization
    for (int i = kNumBCGrid; i < kNx + kNumBCGrid; i++)
    for (int j = kNumBCGrid; j < kNy + kNumBCGrid; j++) {
        if (heavisideDeriv[idx(i, j)] == 0.0)
            continue;

        // ls
        lsFraction[0][0] = 0.25 * (ls[idx(i, j)] + ls[idx(i - 1, j)]
            + ls[idx(i - 1, j - 1)] + ls[idx(i, j - 1)]);
        lsFraction[0][1] = 0.5 * (ls[idx(i, j - 1)] + ls[idx(i, j)]);
        lsFraction[0][2] = 0.25 * (ls[idx(i, j)] + ls[idx(i - 1, j)]
            + ls[idx(i - 1, j + 1)] + ls[idx(i, j + 1)]);
        lsFraction[1][0] = 0.5 * (ls[idx(i, j - 1)] + ls[idx(i, j)]);
        lsFraction[1][1] = ls[idx(i, j)];
        lsFraction[1][2] = 0.5 * (ls[idx(i, j)] + ls[idx(i, j + 1)]);
        lsFraction[2][0] = 0.25 * (ls[idx(i, j)] + ls[idx(i + 1, j)]
            + ls[idx(i + 1, j - 1)] + ls[idx(i, j - 1)]);
        lsFraction[2][1] = 0.5 * (ls[idx(i, j)] + ls[idx(i + 1, j)]);
        lsFraction[2][2] = 0.25 * (ls[idx(i, j)] + ls[idx(i + 1, j)]
            + ls[idx(i + 1, j + 1)] + ls[idx(i, j + 1)]);

        // lsInit
        lsInitFraction[0][0] = 0.25 * (lsInit[idx(i, j)] + lsInit[idx(i - 1, j)]
            + lsInit[idx(i - 1, j - 1)] + lsInit[idx(i, j - 1)]);
        lsInitFraction[0][1] = 0.5 * (lsInit[idx(i, j - 1)] + lsInit[idx(i, j)]);
        lsInitFraction[0][2] = 0.25 * (lsInit[idx(i, j)] + lsInit[idx(i - 1, j)]
            + lsInit[idx(i - 1, j + 1)] + lsInit[idx(i, j + 1)]);
        lsInitFraction[1][0] = 0.5 * (lsInit[idx(i, j - 1)] + lsInit[idx(i, j)]);
        lsInitFraction[1][1] = lsInit[idx(i, j)];
        lsInitFraction[1][2] = 0.5 * (lsInit[idx(i, j)] + lsInit[idx(i, j + 1)]);
        lsInitFraction[2][0] = 0.25 * (lsInit[idx(i, j)] + lsInit[idx(i + 1, j)]
            + lsInit[idx(i + 1, j - 1)] + lsInit[idx(i, j - 1)]);
        lsInitFraction[2][1] = 0.5 * (lsInit[idx(i, j)] + lsInit[idx(i + 1, j)]);
        lsInitFraction[2][2] = 0.25 * (lsInit[idx(i, j)] + lsInit[idx(i + 1, j)]
            + lsInit[idx(i + 1, j + 1)] + lsInit[idx(i, j + 1)]);

        // |\nabla \phi |
        dLSAbs[0][0]
            = std::sqrt(std::pow(((lsInit[idx(i, j)] + lsInit[idx(i, j - 1)]) * 0.5
            - 0.5 * (lsInit[idx(i - 1, j)] + lsInit[idx(i - 1, j - 1)])) / kDx, 2.0)
            + std::pow((lsInitFraction[0][1]
            - 0.5 * (lsInit[idx(i, j - 1)] + lsInit[idx(i - 1, j - 1)])) / kDy, 2.0));
        dLSAbs[0][1]
            = std::sqrt(std::pow((lsInitFraction[1][1]
            - lsInit[idx(i - 1, j)]) / kDx, 2.0)
            + std::pow((lsInitFraction[2][0] - lsInitFraction[0][0]) / kDy, 2.0));
        dLSAbs[0][2]
            = std::sqrt(std::pow((lsInitFraction[1][2] -
            -0.5 * (lsInit[idx(i - 1, j)] + lsInit[idx(i - 1, j + 1)])) / kDx, 2.0)
            + std::pow((0.5 * (lsInit[idx(i - 1, j + 1)] + lsInit[idx(i, j + 1)])
            - lsInitFraction[0][1]) / kDy, 2.0));
        dLSAbs[1][0]
            = std::sqrt(std::pow((lsInitFraction[2][0] - lsInitFraction[0][0]) / kDx, 2.0)
            + std::pow((lsInitFraction[1][1]
            - 0.5 * (lsInit[idx(i, j - 1)] + lsInit[idx(i, j)])) / kDy, 2.0));
        dLSAbs[1][1]
            = std::sqrt(std::pow((lsInitFraction[2][1] - lsInitFraction[0][1]) / kDx, 2.0)
            + std::pow((lsInitFraction[1][2] - lsInitFraction[1][0]) / kDy, 2.0));
        dLSAbs[1][2]
            = std::sqrt(std::pow((lsInitFraction[2][2] - lsInitFraction[0][2]) / kDx, 2.0)
            + std::pow((lsInit[idx(i, j + 1)] - lsInitFraction[1][1]) / kDy, 2.0));
        dLSAbs[2][0]
            = std::sqrt(std::pow((0.5 * (lsInit[idx(i + 1, j)] + lsInit[idx(i + 1, j - 1)])
            - lsInitFraction[1][0]) / kDx, 2.0)
            + std::pow((lsInitFraction[2][1]
            - 0.5 * (lsInit[idx(i, j - 1)] + lsInit[idx(i + 1, j - 1)])) / kDy, 2.0));
        dLSAbs[2][1]
            = std::sqrt(std::pow((lsInit[idx(i + 1, j)] - lsInitFraction[1][1]) / (2.0 * kDx), 2.0)
            + std::pow((lsInitFraction[2][2] - lsInitFraction[2][0]) / kDy, 2.0));
        dLSAbs[2][2]
            = std::sqrt(std::pow((0.5 * (lsInit[idx(i + 1, j)] + lsInit[idx(i + 1, j + 1)])
            - lsInitFraction[1][1]) / kDx, 2.0)
            + std::pow((0.5 * (lsInit[idx(i, j + 1)] + lsInit[idx(i + 1, j + 1)])
            - lsInitFraction[2][1]) / kDy, 2.0));

        // Hp, L
        for (int li = 0; li < 3; li++)
        for (int lj = 0; lj < 3; lj++)  {
            if (std::fabs(lsFraction[li][lj]) > kThickness)
                Hp[li][lj] = 0.0;
            else
                Hp[li][lj] = 0.5 / kEps + 0.5 / kEps * cos(M_PI * lsFraction[li][lj] / kEps);
            L[li][lj] = (lsFraction[li][lj] - lsInitFraction[li][lj]) / kAdt;

            f[li][lj] = Hp[li][lj] * dLSAbs[li][lj];
        }
        // Simpson's Rule with 9 point stencil
        double sum1 = 0.0, sum2 = 0.0;
        for (int li = 0; li < 3; li++)
        for (int lj = 0; lj < 3; lj++) {
            sum1 += Hp[li][lj] * L[li][lj];
            sum2 += Hp[li][lj] * f[li][lj];
        }
        sum1 += 15.0 * Hp[1][1] * L[1][1];
        sum2 += 15.0 * Hp[1][1] * f[1][1];

        sussmanConstraint[idx(i, j)] -= kAdt * f[1][1] * sum1 / sum2;
    }

    return sussmanConstraint;
}

std::vector<int> LevelSetSolver2D::GetSignedLSNormalized(const std::vector<double> &v) {
    std::vector<int> r(v.size());
    // std::transform(v.begin(), v.end(), r.begin(), (static_cast<int>((const int&)sign)));
    
    for (int i = 0; i < kNx + 2 * kNumBCGrid; i++)
    for (int j = 0; j < kNy + 2 * kNumBCGrid; j++)  {
        if (std::fabs(v[idx(i, j)]) <= kEps)
            r[idx(i, j)] 
                = static_cast<int>(
                2.0 * ((0.5 + cos(M_PI * v[idx(i, j)] / kEps)) / kEps - 0.5));
        else if (std::fabs(v[idx(i, j)]) > kEps && v[idx(i, j)] > 0)
            r[idx(i, j)] = 1;
        else if (std::fabs(v[idx(i, j)]) > kEps && v[idx(i, j)] < 0)
            r[idx(i, j)] = -1;
    }

    return r;
}

std::vector<double> LevelSetSolver2D::GetSmoothedSignFunc(const std::vector<double> &ls) {
    std::vector<double> signF(ls.size());
    
    for (int i = 0; i < kNx + 2 * kNumBCGrid; i++)
    for (int j = 0; j < kNy + 2 * kNumBCGrid; j++)  {
        signF[idx(i, j)] = ls[idx(i, j)] / std::sqrt(std::pow(ls[idx(i, j)], 2.0) + kEps * kEps);
    }

    return signF;
}

int LevelSetSolver2D::SetBC_U_2D(std::string BC_W, std::string BC_E, std::string BC_S, std::string BC_N) {
    if (!m_BC) {
        m_BC = std::make_shared<BoundaryCondition2D>(kNx, kNy, kDx, kDy, kNumBCGrid);
    }

    m_BC->SetBC_U_2D(BC_W, BC_E, BC_S, BC_N);

    return 0;
}

int LevelSetSolver2D::SetBC_V_2D(std::string BC_W, std::string BC_E, std::string BC_S, std::string BC_N) {
    if (!m_BC) {
        m_BC = std::make_shared<BoundaryCondition2D>(kNx, kNy, kDx, kDy, kNumBCGrid);
    }

    m_BC->SetBC_V_2D(BC_W, BC_E, BC_S, BC_N);

    return 0;
}

int LevelSetSolver2D::SetBC_P_2D(std::string BC_W, std::string BC_E, std::string BC_S, std::string BC_N) {
    if (!m_BC) {
        m_BC = std::make_shared<BoundaryCondition2D>(kNx, kNy, kDx, kDy, kNumBCGrid);
    }

    m_BC->SetBC_P_2D(BC_W, BC_E, BC_S, BC_N);

    return 0;
}

int LevelSetSolver2D::ApplyBC_U_2D(std::vector<double>& arr) {
    m_BC->ApplyBC_U_2D(arr);
    
    return 0;
}

int LevelSetSolver2D::ApplyBC_V_2D(std::vector<double>& arr) {
    m_BC->ApplyBC_V_2D(arr);

    return 0;
}

int LevelSetSolver2D::ApplyBC_P_2D(std::vector<double>& arr) {
    m_BC->ApplyBC_P_2D(arr);
    
    return 0;
}

void LevelSetSolver2D::SetBCConstantUW(double BC_ConstantW) {
    return m_BC->SetBCConstantUW(BC_ConstantW);
}

void LevelSetSolver2D::SetBCConstantUE(double BC_ConstantE) {
    return m_BC->SetBCConstantUE(BC_ConstantE);
}

void LevelSetSolver2D::SetBCConstantUS(double BC_ConstantS) {
    return m_BC->SetBCConstantUS(BC_ConstantS);
}

void LevelSetSolver2D::SetBCConstantUN(double BC_ConstantN) {
    return m_BC->SetBCConstantUN(BC_ConstantN);
}

void LevelSetSolver2D::SetBCConstantVW(double BC_ConstantW) {
    return m_BC->SetBCConstantVW(BC_ConstantW);
}

void LevelSetSolver2D::SetBCConstantVE(double BC_ConstantE) {
    return m_BC->SetBCConstantVE(BC_ConstantE);
}

void LevelSetSolver2D::SetBCConstantVS(double BC_ConstantS) {
    return m_BC->SetBCConstantVS(BC_ConstantS);
}

void LevelSetSolver2D::SetBCConstantVN(double BC_ConstantN) {
    return m_BC->SetBCConstantVN(BC_ConstantN);
}

void LevelSetSolver2D::SetBCConstantPW(double BC_ConstantW) {
    return m_BC->SetBCConstantPW(BC_ConstantW);
}

void LevelSetSolver2D::SetBCConstantPE(double BC_ConstantE) {
    return m_BC->SetBCConstantPE(BC_ConstantE);
}

void LevelSetSolver2D::SetBCConstantPS(double BC_ConstantS) {
    return m_BC->SetBCConstantPS(BC_ConstantS);
}

void LevelSetSolver2D::SetBCConstantPN(double BC_ConstantN) {
    return m_BC->SetBCConstantPN(BC_ConstantN);
}

// http://stackoverflow.com/questions/11990030/c-sign-function-from-matlab
inline int LevelSetSolver2D::sign(const double& val) {
    return (val > 0) - (val < 0);
}

double LevelSetSolver2D::MinAbs(const double& val1, const double& val2) {
    if (std::fabs(val1) < std::fabs(val2))
        return val1;
    else
        return val2;

    // impossible
    return std::numeric_limits<double>::quiet_NaN();
}

double LevelSetSolver2D::MinMod(const double& val1, const double& val2) {
    if ((std::fabs(val1) <= std::fabs(val2)) && (val1 * val2 > 0)) 
        return val1;
    else if ((std::fabs(val1) > std::fabs(val2)) && (val1 * val2 > 0))
        return val2;
    else if (val1 * val2 <= 0)
        return 0.0;

    // impossible
    return std::numeric_limits<double>::quiet_NaN();
}

inline int LevelSetSolver2D::idx(int i, int j) {
    return (j + (kNy + 2 * kNumBCGrid) * (i));
}