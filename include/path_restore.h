#ifndef PATH_RESTORE_H
#define PATH_RESTORE_H
#include "map.h"
#include "order.h"

typedef struct{
    int *points;//动态数组保存道路顶点下标
    int count;//实际经过的顶点数
    double distance;//实际路线总距离
}DeliveryRoute;
//构建完整道路路线
int buildFullRoute(const Graph *g,const Order orders[],int orderCount,const int route[],DeliveryRoute *result);
//打印顶点坐标,并检验总距离
int printDeliveryRoute(const Graph *g,const DeliveryRoute *result,double tspDistance);
//打印完整路线
void printFullRoute(const Graph *g,const Order orders[],int count,const int route[]);

void freeDeliveryRoute(DeliveryRoute *result);

#endif