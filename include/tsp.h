#ifndef TSP_H
#define TSP_H
#include "distance.h"
double nearestNeighbor(double matrix[MAX_ORDERS][MAX_ORDERS],int count, int route[]);
double twoOpt(double matrix[MAX_ORDERS][MAX_ORDERS], int count ,int route[]);
#endif