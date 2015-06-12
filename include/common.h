#ifndef __COMMON_H_
#define __COMMON_H_

enum class TIMEORDERENUM { EULER, RK2, RK3 };
enum class BC2D { PERIODIC, NEUMANN, DIRICHLET, AXISYM, CUSTOM };
enum class BC3D { PERIODIC, NEUMANN, DIRICHLET, CUSTOM };
enum class PLTTYPE { ASCII, BINARY, BOTH, NONE };
enum class POISSONTYPE { MKL, CG, GS, BICGSTAB, ICPCG };
enum class GAXISENUM2D { X, Y, Z };
enum class GAXISENUM2DAXISYM { R, Z };
enum class GAXISENUM3D { X, Y, Z };

#endif __COMMON_H_