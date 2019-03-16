#define INT_TO_FP(n) n*(1<<14)
#define FP_TO_INT_ROUND_ZERO(x) x/(1<<14)
#define FP_TO_INT_ROUND_NEAR(x) ((x>>31)&((x - (1<<14)/ 2) / (1<<14))) + (~(x>>31)&((x + (1<<14)/ 2) / (1<<14)))
#define ADD_FPS(x,y) x+y
#define SUB_FPS(x,y) x-y
#define MUL_FPS(x,y) ((int64_t) x) * y / (1<<14)
#define DIV_FPS(x,y) ((int64_t) x) * (1<<14) / y
#define ADD_FP_INT(x,n) x+n*(1<<14)
#define SUB_FP_INT(x,n) x-n*(1<<14)
#define MUL_FP_INT(x,n) x*n
#define DIV_FP_INT(x,n) x/n

