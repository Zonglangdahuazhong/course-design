#include <stdio.h>
#include <stdkib.h>
#include <string.h>
#define MAX 100//定义一个MAX
typedef struct Vertex{
    
    char data;//该点的名称
    Edge *first;//连接的第一条边

}Vertex;
typedef struct Edge{
    int index;// 指向哪个点
    int weight;//边长
    struct Edge *next;//下一条边
}Edge;
typedef struct Graph{
    Vertex vertices[MAX];//顶点数组
    int count;//顶点数量
}Graph;