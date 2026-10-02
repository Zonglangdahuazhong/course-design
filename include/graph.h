#ifndef GRAPH_H
#define GRAPH_H




typedef struct {
    int id;
    double x;       // 经度
    double y;       // 纬度
} Point; 
typedef struct Edge {
    int to;  // 终点
    double distance; //距离
    struct Edge *next;  //指向下一个边的指针
} Edge;

typedef struct {
    int sum;
    Point *points; // 存储所有点的数组
    Edge **adj; // 存储邻接表的数组，每个元素是一个指向Edge的指针，表示该点的所有边
} Graph;

//初步定义时未考虑移植性，全部用的int等，后续再进行优化吧.

/*


Graph
│
├── sum = 5
│
├── points
│     │
│     ├── Point 0
│     ├── Point 1
│     ├── Point 2
│     ├── Point 3
│     └── Point 4
│
└── adj
      │
      ↓
    ┌──────┬──────┬──────┬──────┬──────┐
    │  *   │  *   │  *   │  *   │  *   │
    └──┬───┴──┬───┴──┬───┴──┬───┴──┬───┘
       ↓      ↓      ↓      ↓      ↓
      0的表   1的表   2的表   3的表   4的表
       ↓      ↓      ↓      ↓      ↓
     [1,10] [0,10] [1,15] [2,12] [0,20]
       ↓      ↓      ↓
     [4,20] [2,15] [4,8]
                       ↓
                     [3,12]

*/




/*最开始存储所有点用的是一个指向存放所有点的数组的指针，一个指向所有边长的的数组的二级指针。但是这样一定会浪费很多空间，因为很多点之间是没有边的，所以就改成了一个指向存放所有点的数组的指针，
 一个指向所有邻接表的二级指针。这样就节省了很多空间.*/

Graph *mapload(const char *filename);
void printGraph(Graph *graph);
#endif 





//ifndegf 意思是如果没定义  #define 标记一下已经被定义  这样就可以避免重复定义，出现两段这个结构体的定义。 GRAPH_H这个名字可以随意改  只要一致就行


