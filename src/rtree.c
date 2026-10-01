#include"stdio.h"
#include"rtree.h"
#include"stdlib.h"


MBR order_mbr(const Order *order, const Graph *graph){


int i = order->pointid - 1;//数组下标与我随机生成的订单的节点标号差1 因为一个1开始  一个0开始和最开始生成数据结构时的from-1同理


    double x = graph->points[i].x;
    double y = graph->points[i].y;
    MBR mbr;
    mbr.min_x = x;
    mbr.max_x = x;
    mbr.min_y = y;
    mbr.max_y = y;

    return mbr;
}

//接下来实现mbr的合并  最小外包矩形


MBR mbr_combine(MBR a, MBR b)
{
MBR result;

    result.min_x = (a.min_x < b.min_x) ? a.min_x : b.min_x;
    result.min_y = (a.min_y < b.min_y) ? a.min_y : b.min_y;
    result.max_x = (a.max_x > b.max_x) ? a.max_x : b.max_x;
    result.max_y = (a.max_y > b.max_y) ? a.max_y : b.max_y;

    return result;



}//小的是两者之中最小的，大的是两者之中最大的


//接下来判断矩形是否相交  无非四种情况  A在B右 A在B左 A在B上 A在B下 分别对应下边if里的条件

int is_over(MBR a, MBR b)
  { 
 
    if(a.min_x >b.max_x || a.max_x < b.min_x || a.min_y > b.max_y || a.max_y < b.min_y)
    {
        return 0; // 不相交
    }
    else
    {
        return 1; // 相交
    }
}

RTreeNode *create_node(int is_leaf)
{
    RTreeNode *node = malloc(sizeof(RTreeNode));

    if (node == NULL) {
        return NULL;
    }

    node->is_leaf = is_leaf;
    node->count = 0;

    return node;
}//生成节点


//把订单插入叶子节点
void insert_order(RTreeNode *node, const Order *order, const Graph *graph)
{
    if (node->count >= MAX_ENTRIES) {
        return;
    }

    RTreeEntry *entry = &node->entries[node->count];//新建node时设置的count为0

    entry->mbr = order_mbr(order, graph);
    entry->child = (void *)order;//先录入叶子节点，

    node->count++;
}

//接下来实现分裂算法   我选择二次分裂 后边有时间优化就尝试一下R*
/*

两两组合计算“浪费面积”，把组合后浪费面积最大的一对做“种子”，剩余条目逐个分配。

*/
//所以先计算面积
double R_area(MBR mbr)
{
    return (mbr.max_x - mbr.min_x) * (mbr.max_y - mbr.min_y);
}
//计算浪费面积
double mbr_waste(MBR a, MBR b)
{
    return R_area(mbr_combine(a, b)) - R_area(a) - R_area(b);
}
//接下来选择seed

void pick_seeds(RTreeEntry *entries, int count, int *seed1, int *seed2){
    double max_waste = -1.0;
    int i, j;

    for (i = 0; i < count; i++) {
        for (j = i + 1; j < count; j++) {
            double waste = mbr_waste(entries[i].mbr, entries[j].mbr);
            if (waste > max_waste) {
                max_waste = waste;
                *seed1 = i;
                *seed2 = j;
            }
        }
    }




}