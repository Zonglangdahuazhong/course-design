#include"stdio.h"
#include"../include/order.h"
#include <stdlib.h>

Order *generate(const Graph *graph, int count) {
 if(graph==NULL || count<=0){
 
     return NULL;
 }



Order *orders = (Order *)malloc(count * sizeof(Order));


/*Order（结构体类型）
                  │
       ┌──────────┴──────────┐
       ↓                     ↓
   Order order          Order *orders
   一个结构体             一个指针
                             │
                             ↓
                     ┌───────────────┐
                     │ Order[0]      │
                     │ Order[1]      │
                     │ Order[2]      │
                     │ ...           │
                     └───────────────┘*/
 if(orders==NULL)
 {
     perror(" malloc failed");
     return NULL;

 }
 
 for (int i = 0; i < count; i++) {
int randomnum = rand()% graph->vertexCount;
orders[i].id = i+1;
orders[i].pointid= randomnum;


 }

    return orders;

}
 
    void free_orders(Order *orders){
        if(orders != NULL){
            free(orders);
        }   

    }