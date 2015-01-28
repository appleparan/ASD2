#ifndef __MAC2D_H_
#define __MAC2D_H_

#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>
#include <cfloat>
#include <cstdlib>
#include <iostream>
#include <istream>
#include <fstream>
#include <string>

#include <algorithm>
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

class MACSolver2D {
public:
	const int kNx, kNy, kNumBCGrid;
	const double kBaseX, kBaseY, kLenX, kLenY, kDx, kDy;

	const double kRe, kWe, kFr;
	const double kLScale, kUScale, kSigma, kG, kMuScale, kRhoScale;
	// *I : inside fluid, *O : outside fluid
	const double kRhoI, kRhoO, kRhoRatio;
	const double kMuI, kMuO, kMuRatio;

	const int kMaxIter, kNIterSkip;
	const double kCFL, kMaxTime;
	const bool kWriteVTK;
	POISSONTYPE m_PoissonSolverType;
	PLTTYPE m_PLTType;

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

	MACSolver2D();
	MACSolver2D(double Re, double We, double Fr,
		double L, double U, double sigma, double densityRatio, double viscosityRatio, double rhoI, double mu1,
		int nx, int ny, double baseX, double baseY, double lenX, double lenY, double cfl,
		int maxtime, int maxiter, int niterskip, int num_bc_grid, bool writeVTK);
	MACSolver2D(double rhoI, double rhoO, double muI, double muO, double gConstant,
		double L, double U, double sigma,
		int nx, int ny, double baseX, double baseY, double lenX, double lenY, double cfl,
		int maxtime, int maxiter, int niterskip, int num_bc_grid, bool writeVTK);
	~MACSolver2D();

	int AllocateVariables();

	// Related to Level Set Related
	std::vector<double> UpdateKappa(const std::vector<double>& ls);
	int UpdateJumpCond(const std::vector<double>& u, const std::vector<double>& v, 
		const std::vector<double>& ls);
	std::vector<double> UpdateFU(const std::shared_ptr<LevelSetSolver2D>& LSolver,
		std::vector<double> ls, std::vector<double>u, std::vector<double> v);
	std::vector<double> UpdateFV(const std::shared_ptr<LevelSetSolver2D>& LSolver,
		std::vector<double> ls, std::vector<double>u, std::vector<double> v);

	// Convection Term
	std::vector<double> AddConvectionFU(const std::vector<double>& u, const std::vector<double>& v, const std::vector<double>& ls);
	std::vector<double> AddConvectionFV(const std::vector<double>& u, const std::vector<double>& v, const std::vector<double>& ls);
	int UnitHJWENO5(
		const std::vector<double> &F, std::vector<double> &FP, std::vector<double> &FM, const double d, const int n);

	// Viscosity Term
	std::vector<double> AddViscosityFU(
		const std::vector<double>& u, const std::vector<double>& v, const std::vector<double>& ls);
	std::vector<double> AddViscosityFV(
		const std::vector<double>& u, const std::vector<double>& v, const std::vector<double>& ls);
	
	// Gravity Term
	std::vector<double> AddGravityFU();
	std::vector<double> AddGravityFV();
	std::vector<double> GetUhat(const std::vector<double>& u, const std::vector<double>& rhsu);
	std::vector<double> GetVhat(const std::vector<double>& v, const std::vector<double>& rhsv);
	// Poisson 
	int SetPoissonSolver(POISSONTYPE type);
	int SolvePoisson(std::vector<double>& ps, const std::vector<double>& div, const std::vector<double>& ls,
		const std::vector<double>& u, const std::vector<double>& v);

	// update velocity using projection
	std::vector<double> GetDivergence(const std::vector<double>& u, const std::vector<double>& v);
	int UpdateVel(std::vector<double>& u, std::vector<double>& v,
		const std::vector<double>& us, const std::vector<double>& vs, const std::vector<double>& ps, const std::vector<double>& ls);
	double UpdateDt(const std::vector<double>& u, const std::vector<double>& v);

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

	void TDMA(double* a, double* b, double* c, double* d, int n);
	int SetPLTType(PLTTYPE type);
	int OutRes(int iter, double curTime, const std::string fname_vel_base, const std::string fname_div_base,
		const std::vector<double>& u, const std::vector<double>& v,
		const std::vector<double>& ps, const std::vector<double>& ls);
	int OutResClose();

	int idx(int i, int j);
};

#endif __MAC2D_H_
