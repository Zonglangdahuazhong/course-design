#include "tsp.h"
#include "dijkstra.h"
double nearestNeighbor(double matrix[MAX_ORDERS][MAX_ORDERS],int count, int route[]){
    int visited[MAX_ORDERS]={0};//标记数组
    int current=0;
    double totalDistance=0.0;//总距离
    route[0]=0;//初始点为0点
    visited[0]=1;
    for (int i=1;i<count;i++){
        double minDistance=INF;
        int next=-1;
        for (int j=0;j<count;j++){
            if(!visited[j]&&matrix[current][j]<minDistance){
                minDistance=matrix[current][j];
                next =j;
            }
        }
        if(next==-1)
        {
            return -1.0;
        }
        route[i]=next;
        visited[next]=1;
        totalDistance+=minDistance;
        current=next;
    }
    totalDistance+=matrix[current][0];
    route[count]=0;//声明count 点对应0
    return totalDistance;
}
static void reverseRoute(int route[],int left,int right){
    while(left <right){
        int temp=route[left];
        route[left]=route[right];
        route[right]=temp;
        left++;
        right--;
    }
}
double twoOpt(double matrix[MAX_ORDERS][MAX_ORDERS], int count ,int route[]){
    int improved=1;
    while(improved){
        improved=0;
        for(int i=1;i<count-1;i++){
            for(int j=i+1;j<count;j++){
                int a=route[i-1];
                int b=route[i];
                int c=route[j];
                int d=route[j+1];
                double oldDistance =matrix[a][b] +matrix[c][d];
                double newDistance =matrix[a][c] +matrix[b][d];
                if(newDistance<oldDistance){
                    reverseRoute(route,i,j);
                    improved=1;
                }

            }
        }
    }
    double totalDistance=0.0;
    for (int i=0;i<count;i++){
        totalDistance +=matrix[route[i]][route[i+1]];
    }
    return totalDistance;
}
