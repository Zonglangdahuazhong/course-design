#include"stdio.h"
#include"rtree.h"


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