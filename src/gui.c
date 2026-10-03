#include <gtk/gtk.h>
#include <cairo.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "../include/gui.h"
#include "../include/graph.h"
#include "../include/order.h"


typedef struct {

    Graph *graph;

    // 地图坐标范围
    double min_x;
    double max_x;
    double min_y;
    double max_y;

    // 当前订单
    Order *orders;
    int order_count;

    // 地图绘图区
    GtkWidget *drawing_area;

    // 订单数量输入框
    GtkWidget *entry;

    // 状态文字
    GtkWidget *status_label;

} MapView;


/*
 * ==========================
 * 计算地图坐标范围
 * ==========================
 */
static void calculate_bounds(MapView *view)
{
    Graph *graph = view->graph;

    if (graph == NULL || graph->sum <= 0) {
        return;
    }

    view->min_x = graph->points[0].x;
    view->max_x = graph->points[0].x;

    view->min_y = graph->points[0].y;
    view->max_y = graph->points[0].y;

    for (int i = 1; i < graph->sum; i++) {

        if (graph->points[i].x < view->min_x)
            view->min_x = graph->points[i].x;

        if (graph->points[i].x > view->max_x)
            view->max_x = graph->points[i].x;

        if (graph->points[i].y < view->min_y)
            view->min_y = graph->points[i].y;

        if (graph->points[i].y > view->max_y)
            view->max_y = graph->points[i].y;
    }
}


/*
 * ==========================
 * 地图 X → 屏幕 X
 * ==========================
 */
static double to_screen_x(MapView *view,
                          double x,
                          double width)
{
    double range_x = view->max_x - view->min_x;

    if (range_x == 0)
        return width / 2.0;

    return (x - view->min_x) / range_x * width;
}


/*
 * ==========================
 * 地图 Y → 屏幕 Y
 * ==========================
 */
static double to_screen_y(MapView *view,
                          double y,
                          double height)
{
    double range_y = view->max_y - view->min_y;

    if (range_y == 0)
        return height / 2.0;

    /*
     * Cairo 的 Y 轴向下，
     * 所以这里把 Y 轴翻转。
     */
    return height -
           (y - view->min_y) / range_y * height;
}


/*
 * ==========================
 * 屏幕 X → 地图 X
 * ==========================
 */
static double to_map_x(MapView *view,
                        double screen_x,
                        double width)
{
    double range_x = view->max_x - view->min_x;

    if (width == 0)
        return view->min_x;

    return view->min_x +
           screen_x / width * range_x;
}


/*
 * ==========================
 * 屏幕 Y → 地图 Y
 * ==========================
 */
static double to_map_y(MapView *view,
                        double screen_y,
                        double height)
{
    double range_y = view->max_y - view->min_y;

    if (height == 0)
        return view->min_y;

    /*
     * 这里和 to_screen_y() 相反，
     * 把屏幕坐标还原成地图坐标。
     */
    return view->min_y +
           (height - screen_y) / height * range_y;
}


/*
 * ==========================
 * 绘制地图
 * ==========================
 */
static gboolean draw_map(GtkWidget *widget,
                         cairo_t *cr,
                         gpointer data)
{
    MapView *view = (MapView *)data;

    Graph *graph = view->graph;

    double width =
        gtk_widget_get_allocated_width(widget);

    double height =
        gtk_widget_get_allocated_height(widget);


    /*
     * ==========================
     * 1. 绘制道路
     * ==========================
     */

    for (int i = 0; i < graph->sum; i++) {

        Point *from = &graph->points[i];

        double x1 =
            to_screen_x(view, from->x, width);

        double y1 =
            to_screen_y(view, from->y, height);

        Edge *edge = graph->adj[i];

        while (edge != NULL) {

            /*
             * edge->to 是 0-based
             */
            Point *to =
                &graph->points[edge->to];

            double x2 =
                to_screen_x(view, to->x, width);

            double y2 =
                to_screen_y(view, to->y, height);

            cairo_move_to(cr, x1, y1);
            cairo_line_to(cr, x2, y2);
            cairo_stroke(cr);

            edge = edge->next;
        }
    }


    /*
     * ==========================
     * 2. 绘制订单
     * ==========================
     */

    for (int i = 0; i < view->order_count; i++) {

        Order *order = &view->orders[i];

        /*
         * Order.pointid 是 1-based
         *
         * Graph.points[] 是 0-based
         */
        int point_index =
            order->pointid - 1;

        /*
         * 防止数组越界
         */
        if (point_index < 0 ||
            point_index >= graph->sum) {

            continue;
        }

        Point *point =
            &graph->points[point_index];

        double x =
            to_screen_x(view, point->x, width);

        double y =
            to_screen_y(view, point->y, height);


        /*
         * 画订单圆点
         */
        cairo_arc(
            cr,
            x,
            y,
            5.0,
            0,
            2 * M_PI
        );

        cairo_fill(cr);
    }


    return FALSE;
}


/*
 * ==========================
 * 找距离鼠标最近的节点
 * ==========================
 */
static int find_nearest_node(MapView *view,
                             double mouse_x,
                             double mouse_y)
{
    Graph *graph = view->graph;

    double width =
        gtk_widget_get_allocated_width(
            view->drawing_area
        );

    double height =
        gtk_widget_get_allocated_height(
            view->drawing_area
        );


    /*
     * 鼠标的屏幕坐标
     * ↓
     * 转换成地图坐标
     */
    double map_x =
        to_map_x(view, mouse_x, width);

    double map_y =
        to_map_y(view, mouse_y, height);


    int nearest = -1;

    double min_distance = 1e100;


    /*
     * 遍历所有地图节点
     */
    for (int i = 0; i < graph->sum; i++) {

        double dx =
            graph->points[i].x - map_x;

        double dy =
            graph->points[i].y - map_y;

        double distance =
            dx * dx + dy * dy;


        if (distance < min_distance) {

            min_distance = distance;
            nearest = i;
        }
    }


    return nearest;
}


/*
 * ==========================
 * 鼠标点击地图
 * ==========================
 */
static gboolean map_button_press_callback(
    GtkWidget *widget,
    GdkEventButton *event,
    gpointer data)
{
    printf("鼠标点击：x=%f y=%f\n", event->x, event->y);
    MapView *view = (MapView *)data;


    /*
     * 只处理鼠标左键
     */
    if (event->button != 1) {
        return FALSE;
    }


    /*
     * 找最近的 Graph 节点
     */
    int node_index =
        find_nearest_node(
            view,
            event->x,
            event->y
        );


    if (node_index < 0) {
        return FALSE;
    }


    /*
     * ==========================
     * 增加一个 Order
     * ==========================
     */

    Order *new_orders =
        realloc(
            view->orders,
            (view->order_count + 1)
            * sizeof(Order)
        );


    if (new_orders == NULL) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "创建订单失败：内存不足"
        );

        return FALSE;
    }


    view->orders = new_orders;


    /*
     * 新订单编号
     */
    int new_id =
        view->order_count + 1;


    /*
     * Order.pointid 是 1-based
     *
     * node_index 是 0-based
     *
     * 所以 +1
     */
    view->orders[view->order_count].id =
        new_id;

    view->orders[view->order_count].pointid =
        node_index + 1;


    /*
     * 订单数量 +1
     */
    view->order_count++;


    /*
     * 更新状态
     */
    char status[100];

    snprintf(
        status,
        sizeof(status),
        "当前订单：%d",
        view->order_count
    );

    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        status
    );


    /*
     * 重新绘制地图
     */
    gtk_widget_queue_draw(
        view->drawing_area
    );


    return TRUE;
}


/*
 * ==========================
 * 点击「生成订单」
 * ==========================
 */
static void generate_orders_callback(
    GtkWidget *button,
    gpointer data)
{
    MapView *view = (MapView *)data;


    /*
     * 从输入框获取字符串
     */
    const char *text =
        gtk_entry_get_text(
            GTK_ENTRY(view->entry)
        );


    /*
     * 转换成整数
     */
    int count = atoi(text);


    /*
     * 检查订单数量
     */
    if (count <= 0) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "请输入大于 0 的订单数量"
        );

        return;
    }


    /*
     * 释放上一批订单
     */
    free_orders(view->orders);

    view->orders = NULL;
    view->order_count = 0;


    /*
     * 调用你原来的订单生成函数
     */
    view->orders =
        generate(view->graph, count);


    /*
     * 判断生成是否成功
     */
    if (view->orders == NULL) {

        gtk_label_set_text(
            GTK_LABEL(view->status_label),
            "订单生成失败"
        );

        return;
    }


    /*
     * 保存订单数量
     */
    view->order_count = count;


    /*
     * 更新状态文字
     */
    char status[100];

    snprintf(
        status,
        sizeof(status),
        "当前订单：%d",
        count
    );

    gtk_label_set_text(
        GTK_LABEL(view->status_label),
        status
    );


    /*
     * 重新绘制地图
     */
    gtk_widget_queue_draw(
        view->drawing_area
    );
}


/*
 * ==========================
 * 窗口关闭
 * ==========================
 */
static void destroy_callback(
    GtkWidget *widget,
    gpointer data)
{
    MapView *view = (MapView *)data;


    /*
     * 释放订单
     */
    free_orders(view->orders);


    /*
     * 退出 GTK
     */
    gtk_main_quit();
}


/*
 * ==========================
 * 启动 GUI
 * ==========================
 */
void gui_start(Graph *graph)
{
    /*
     * GUI 状态
     */
    MapView view;

    view.graph = graph;

    view.orders = NULL;
    view.order_count = 0;

    view.drawing_area = NULL;
    view.entry = NULL;
    view.status_label = NULL;


    /*
     * 计算地图范围
     */
    calculate_bounds(&view);


    /*
     * 初始化 GTK
     */
    gtk_init(NULL, NULL);


    /*
     * ==========================
     * 创建窗口
     * ==========================
     */

    GtkWidget *window =
        gtk_window_new(
            GTK_WINDOW_TOPLEVEL
        );

    gtk_window_set_title(
        GTK_WINDOW(window),
        "校园送 · 配送调度系统"
    );

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        1200,
        800
    );


    /*
     * ==========================
     * 整体布局
     * ==========================
     */

    GtkWidget *main_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            5
        );

    gtk_container_add(
        GTK_CONTAINER(window),
        main_box
    );


    /*
     * ==========================
     * 顶部控制栏
     * ==========================
     */

    GtkWidget *control_box =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            5
        );

    gtk_box_pack_start(
        GTK_BOX(main_box),
        control_box,
        FALSE,
        FALSE,
        5
    );


    /*
     * 订单数量文字
     */
    GtkWidget *count_label =
        gtk_label_new(
            "订单数量："
        );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        count_label,
        FALSE,
        FALSE,
        5
    );


    /*
     * 输入框
     */
    view.entry =
        gtk_entry_new();

    gtk_entry_set_text(
        GTK_ENTRY(view.entry),
        "10"
    );

    gtk_entry_set_width_chars(
        GTK_ENTRY(view.entry),
        8
    );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        view.entry,
        FALSE,
        FALSE,
        5
    );


    /*
     * 生成订单按钮
     */
    GtkWidget *generate_button =
        gtk_button_new_with_label(
            "生成订单"
        );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        generate_button,
        FALSE,
        FALSE,
        5
    );


    /*
     * 状态文字
     */
    view.status_label =
        gtk_label_new(
            "当前订单：0"
        );

    gtk_box_pack_start(
        GTK_BOX(control_box),
        view.status_label,
        FALSE,
        FALSE,
        20
    );


    /*
     * ==========================
     * 地图绘图区
     * ==========================
     */

    view.drawing_area =
        gtk_drawing_area_new();

    gtk_box_pack_start(
        GTK_BOX(main_box),
        view.drawing_area,
        TRUE,
        TRUE,
        5
    );


    /*
     * ==========================
     * 开启鼠标事件
     * ==========================
     */

    gtk_widget_add_events(
        view.drawing_area,
        GDK_BUTTON_PRESS_MASK
    );


    /*
     * ==========================
     * 连接信号
     * ==========================
     */

    g_signal_connect(
        view.drawing_area,
        "draw",
        G_CALLBACK(draw_map),
        &view
    );


    g_signal_connect(
        view.drawing_area,
        "button-press-event",
        G_CALLBACK(map_button_press_callback),
        &view
    );


    g_signal_connect(
        generate_button,
        "clicked",
        G_CALLBACK(generate_orders_callback),
        &view
    );


    g_signal_connect(
        window,
        "destroy",
        G_CALLBACK(destroy_callback),
        &view
    );


    /*
     * 显示窗口
     */
    gtk_widget_show_all(window);


    /*
     * GTK 主循环
     */
    gtk_main();
}