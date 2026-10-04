#ifndef DISTANCE_H
#define DISTANCE_H
#include "map.h"
#include "order.h"
#define MAX_ORDERS 100//订单最大数量为100
int buildDistanceMatrix(const Graph *g,const Order orders[],int count,double matrix[MAX_ORDERS][MAX_ORDERS]);
//创建一个二维数组，即构建出订单矩阵。 
void printDistanceMatrix(double matrix[MAX_ORDERS][MAX_ORDERS],int count );
#endif