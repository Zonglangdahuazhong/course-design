#ifndef RTREE_H
#define RTREE_H
#include "map.h"
#include "order.h"
#define RTREE_MAX_ENTRIES 4
#define RTREE_MIN_ENTRIES 2

typedef struct { double xmin,ymin,xmax,ymax; } Rect;
typedef struct RTreeNode RTreeNode;
typedef struct {
    Rect rect;
    int orderIndex;
    RTreeNode *child;
} RTreeEntry;
struct RTreeNode {
    int isLeaf;
    int count;
    RTreeEntry entries[RTREE_MAX_ENTRIES];
};
typedef struct { RTreeNode *root; } RTree;

void initRTree(RTree *tree);
int insertOrder(RTree *tree,const Graph *g,const Order orders[],int orderIndex);
int rangeQuery(const RTree *tree,Rect query,int result[],int capacity);
void freeRTree(RTree *tree);
Rect makePointRect(double x,double y);
int rectIntersect(Rect a,Rect b);
#endif
