#ifndef __COMMON_H_
#define __COMMON_H_

enum class BC { PERIODIC, NEUMANN, DIRICHLET, CUSTOM };
enum class PLTTYPE { ASCII, BINARY, BOTH, NONE };
enum class POISSONTYPE { MKL, CG, JACOBIPCG, ICPCG, RCICG, GS };

#endif