#ifndef DATA_H
#define DATA_H

typedef struct {
    int id;
    double x;
    double y;
    int base;   //标记是否为配送中心
} Point;

typedef struct{
int sum; //订单总数
Point *points; //指向Point数组的指针   存放所有的点
double **distance; //指向距离矩阵的指针  存放两点间的距离  行数据是每一个点  列数据是该点到其余各个点的距离
}Graph;

#endif 




//ifndegf 意思是如果没定义  #define 标记一下已经被定义  这样就可以避免重复定义，出现两段这个结构体的定义。 DATA_H这个名字可以随意改  只要一致就行


