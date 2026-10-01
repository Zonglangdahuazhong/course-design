#ifndef RTREE_H
#define RTREE_H
#include "graph.h"
#include "order.h"

typedef struct {
    double min_x;
    double min_y;
    double max_x;
    double max_y;
} MBR;

MBR order_mbr(const Order *order, const Graph *graph);
MBR mbr_combine(MBR a, MBR b);
int is_over(MBR a, MBR b);
#endif