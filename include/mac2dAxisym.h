#ifndef __MAC2DAXISYM_H_
#define __MAC2DAXISYM_H_

#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>
#include <cfloat>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <istream>
#include <fstream>
#include <string>

#include <algorithm>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

#include "common.h"
#include "bc2d.h"
#include "levelset2d.h"
#include "poisson2d.h"

// Tecplot  IO
#include "TECIO.h"
#include "TECXXX.h"

class MACSolver2DAxisym {
public:
    const int kNr, kNz, kNumBCGrid;
    const int64_t kArrSize;
    const double kBaseR, kBaseZ, kLenR, kLenZ, kDr, kDz;

    const double kRe, kWe, kFr;
    const double kLScale, kUScale, kSigma, kG, kMuScale, kRhoScale;
    // *H : Heavy fluid(such as liquid), *L : light fluid (such as gas)
    const double kRhoH, kRhoL, kRhoRatio;
    const double kMuH, kMuL, kMuRatio;
    
    const int kMaxIter, kNIterSkip;
    const double kCFL, kMaxTime;
    const bool kWriteVTK;
    POISSONTYPE m_PoissonSolverType;
    PLTTYPE m_PLTType;
    GAXISENUM2DAXISYM kGAxis;

    const double kEps_div = 1.0e-6;

    std::shared_ptr<BoundaryCondition2D> m_BC;
    std::shared_ptr<PoissonSolver2D> m_Poisson;
    int m_iter;
    double m_dt, m_curTime;
    std::string m_outFname;

    // velocity
    std::vector<double> m_u, m_v;
    // density, curvature, viscosity
    std::vector<double> m_rho, m_kappa, m_mu;

    std::vector<double> m_ps, m_p;

    // Jump condition
    std::vector<double> m_J11, m_J12, m_J21, m_J22;
    
    // normal vector & tangent vector
    std::vector<double> m_nr, m_nz;
    std::vector<double> m_t1r, m_t1z, m_t1t;
    std::vector<double> m_t2r, m_t2z, m_t2t;

    MACSolver2DAxisym();
    MACSolver2DAxisym(double Re, double We, double Fr, GAXISENUM2DAXISYM GAxis,
        double L, double U, double sigma, double densityRatio, double viscosityRatio, double rhoI, double mu1,
        int nr, int nz, double baseR, double baseZ, double lenR, double lenZ, 
        double cfl, double maxtime, int maxiter, int niterskip,
        int num_bc_grid, bool writeVTK);
    MACSolver2DAxisym(double rhoI, double rhoO, double muI, double muO, double gConstant, GAXISENUM2DAXISYM GAxis,
        double L, double U, double sigma,
        int nr, int nz, double baseR, double baseZ, double lenR, double lenZ,
        double cfl, double maxtime, int maxiter, int niterskip,
        int num_bc_grid, bool writeVTK);
    ~MACSolver2DAxisym();

    int AllocateVariables();

    // Related to Level Set Related
    std::vector<double> UpdateSmoothHeavisideFunc(const std::vector<double>& ls);
    std::vector<double> UpdateHeavisideFunc(const std::vector<double>& ls);
    int UpdateNTK(const std::shared_ptr<LevelSetSolver2D>& LSolver, const std::vector<double>& ls,
        std::tuple<std::vector<double>&, std::vector<double>&> normalVec,
        std::tuple<std::vector<double>&, std::vector<double>&, std::vector<double>&> t1Vec,
        std::tuple<std::vector<double>&, std::vector<double>&, std::vector<double>&> t2Vec,
        std::vector<double>& kappa);
    int UpdateKappa(const std::vector<double>& ls);
    std::vector<double> GetRHSU(const std::vector<double>& ls, const std::vector<double>& u, const std::vector<double>& v, const std::vector<double>& H);
    std::vector<double> GetRHSV(const std::vector<double>& ls, const std::vector<double>& u, const std::vector<double>& uhat, const std::vector<double>& v, const std::vector<double>& H);

    // Convection Term
    std::vector<double> AddConvectionFU(const std::vector<double>& u, const std::vector<double>& v);
    std::vector<double> AddConvectionFV(const std::vector<double>& u, const std::vector<double>& v);
    int UnitHJWENO5(
        const std::vector<double> &F, std::vector<double> &FP, std::vector<double> &FM, const double d, const int n);

    // Viscosity Term
    std::vector<double> AddExternalViscosityFU(
        const std::vector<double>& u, const std::vector<double>& v, const std::vector<double>& ls, const std::vector<double>& H);
    std::vector<double> AddExternalViscosityFV(
        const std::vector<double>& u, const std::vector<double>& uhat, const std::vector<double>& v, const std::vector<double>& ls, const std::vector<double>& H);
    
    // Gravity Term
    std::vector<double> AddGravityFU();
    std::vector<double> AddGravityFV();
    
    // Intermediate Velocity
    std::vector<double> GetUHat(const std::vector<double>& ls, const std::vector<double>& u,
        const std::vector<double>& rhs, const std::vector<double>& H);
    std::vector<double> GetVHat(const std::vector<double>& ls, const std::vector<double>& v,
        const std::vector<double>& rhs, const std::vector<double>& H);
    // Poisson 
    int SetPoissonSolver(POISSONTYPE type);
    int SolvePoisson(std::vector<double>& ps, const std::vector<double>& div,
        const std::vector<double>& ls, const std::vector<double>& lsB,
        const std::vector<double>& u, const std::vector<double>& v, const std::vector<double>& H, const int maxiter);
    std::vector<double> GetDivergence4Poisson(const std::vector<double>& u, const std::vector<double>& v);

    // update velocity using projection
    std::vector<double> GetDivergence(const std::vector<double>& u, const std::vector<double>& v);
    int UpdateVel(std::vector<double>& u, std::vector<double>& v,
        const std::vector<double>& us, const std::vector<double>& vs, const std::vector<double>& ps,
        const std::vector<double>& ls, const std::vector<double>& lsB,
        const std::vector<double>& H);
    double UpdateDt(const std::vector<double>& u, const std::vector<double>& v);
    
    int CheckCompatibility(const std::vector<double>& u, const std::vector<double>& v,
        const std::vector<double>& uhat, const std::vector<double>& vhat);
    // BC
    int SetBC_U_2D(std::string BC_W, std::string BC_E, std::string BC_S, std::string BC_N);
    int SetBC_V_2D(std::string BC_W, std::string BC_E, std::string BC_S, std::string BC_N);
    int SetBC_P_2D(std::string BC_W, std::string BC_E, std::string BC_S, std::string BC_N);
    int ApplyBC_U_2D(std::vector<double>& arr);
    int ApplyBC_V_2D(std::vector<double>& arr);
    int ApplyBC_P_2D(std::vector<double>& arr);
    void SetBCConstantUW(double BC_ConstantW);
    void SetBCConstantUE(double BC_ConstantE);
    void SetBCConstantUS(double BC_ConstantS);
    void SetBCConstantUN(double BC_ConstantN);
    void SetBCConstantVW(double BC_ConstantW);
    void SetBCConstantVE(double BC_ConstantE);
    void SetBCConstantVS(double BC_ConstantS);
    void SetBCConstantVN(double BC_ConstantN);
    void SetBCConstantPW(double BC_ConstantW);
    void SetBCConstantPE(double BC_ConstantE);
    void SetBCConstantPS(double BC_ConstantS);
    void SetBCConstantPN(double BC_ConstantN);
    void UpdateAmbientPressure(double ambientPressure);

    int SolveTDMA(std::vector<double>& a, std::vector<double>& b, std::vector<double>& c, std::vector<double>& d, int n);
    int sign(const double& val);
    int SetPLTType(PLTTYPE type);
    int OutRes(const int iter, const double curTime, const std::string fname_vel_base, const std::string fname_div_base,
        const std::vector<double>& u, const std::vector<double>& v,
        const std::vector<double>& ps, const std::vector<double>& ls);
    int OutResClose();

    int idx(int i, int j);
};

#endif __MAC2DAXISYM_H_
