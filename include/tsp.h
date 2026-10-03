#ifndef TSP_H
#define TSP_H

typedef struct {
    int *route;          // 访问顺序
    int count;           // 节点数量
    double distance;     // 总路线长度
} Route;

Route *solve_tsp(double **matrix,int n);

/* 释放 Route */
void free_route(Route *route);

/* 打印路线 */
void print_route(Route *route);

#endif