#ifndef CMAP_H
#define CMAP_H//进行头文件保护
#define MAX 10000//定义一个MAX
typedef struct Edge{
    int index;// 指向哪个点
    int weight;//边长
    struct Edge *next;//下一条边
}Edge;
typedef struct Vertex{
    int id;
    double x;
    double y;
    char data;//该点的名称
    Edge *first;//连接的第一条边

}Vertex;

typedef struct Graph{
    Vertex vertices[MAX];//顶点数组
    int vertexCount;//顶点数量
    int edgeCount;//边数量
}Graph;
//初始化
void initGraph(Graph *c);
//添加节点
void addVertex(Graph *c,double x,double y,char data);
//添加边
void addEdge(Graph *c,int start ,int end,int weight);
//打印图
void printGraph(Graph *c);
#endif
