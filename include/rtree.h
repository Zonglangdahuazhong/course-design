#ifndef RTREE_H
#define RTREE_H
#define MAX_ENTRIES 4
#include "graph.h"
#include "order.h"

typedef struct {
    double min_x;
    double min_y;
    double max_x;
    double max_y;
} MBR;
//接下来开始R树的数据结构：
/*
Node
 │
 │ 包含多个
 ↓
Entry
 │
 ├── MBR ──────→ 描述 child 的空间范围
 │
 └── child ────→ 实际对象/子节点
*/
typedef struct{
 MBR mbr;
    void *child;
}RTreeEntry;
//孩子节点对应的索引  并且要注意这里用的是void 因为它可能指向孩子节点（非叶子节点）  也可能是Order(叶子节点）两者并不相同 故用void
typedef struct RTreeNode {

    int is_leaf;

    int count;

    RTreeEntry entries[MAX_ENTRIES];

} RTreeNode;  
//节点的数据结构  一个节点可以有很多孩子  所以用一个数组装entry


typedef struct {

    RTreeNode *root;

} RTree;


void insert_order(RTreeNode *node, const Order *order, const Graph *graph);
MBR order_mbr(const Order *order, const Graph *graph);
MBR mbr_combine(MBR a, MBR b);
int is_over(MBR a, MBR b);
RTreeNode *create_node(int is_leaf);
double R_area(MBR mbr);
double mbr_waste(MBR a, MBR b);     
void pick_seeds(RTreeEntry *entries, int count, int *seed1, int *seed2);
MBR node_mbr(const RTreeNode *node);
double mbr_enlargement(MBR group_mbr, MBR entry_mbr);
void insert_entry(RTreeNode *node, RTreeEntry entry);
void quadratic_split(RTreeEntry *entries,int count,RTreeNode **group_a,RTreeNode **group_b);
#endif              


/*

+-------------------------------------------------------+
| 节点 (Node)                                           |
| +-------------------+ +-------------------+           |
| | entry 1           | | entry 2          | ...       |
| | - 矩形框 (MBR)    | | - 矩形框 (MBR)    |           |
| | - 指针 (Child*)   | | - 指针 (Child*)   |           |
| +-------------------+ +-------------------+           |
+-------------------------------------------------------+




*/