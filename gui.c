#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "map.h"
#include "order.h"
#include "rtree.h"
#include "distance.h"
#include "tsp.h"
#include "path_restore.h"

/* =========================
 * 全局数据
 * ========================= */
static Graph g_graph;
static Order *g_orders = NULL;
static int g_orderCount = 0;
static RTree g_rtree;

static int g_queryResults[MAX_ORDERS];
static int g_queryCount = 0;
static int g_queryActive = 0;
static Rect g_queryRect = {0, 0, 0, 0};

static DeliveryRoute g_fullRoute = {0};
static double g_routeDistance = 0.0;

/* GTK控件 */
static GtkWidget *g_window;
static GtkWidget *g_drawingArea;
static GtkWidget *g_statusLabel;
static GtkWidget *g_orderSpin;
static GtkWidget *g_xminEntry;
static GtkWidget *g_yminEntry;
static GtkWidget *g_xmaxEntry;
static GtkWidget *g_ymaxEntry;

/* =========================
 * 工具函数
 * ========================= */
static void clearCurrentRoute(void)
{
    freeDeliveryRoute(&g_fullRoute);
    g_routeDistance = 0.0;
}

static void setStatus(const char *text)
{
    gtk_label_set_text(GTK_LABEL(g_statusLabel), text);
}

static int orderIsSelected(int orderIndex)
{
    if (!g_queryActive)
        return 0;

    for (int i = 0; i < g_queryCount; i++)
    {
        if (g_queryResults[i] == orderIndex)
            return 1;
    }

    return 0;
}

static int parseEntryDouble(GtkWidget *entry, double *value)
{
    const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
    char *end = NULL;
    double v = strtod(text, &end);

    if (text == end || *end != '\0')
        return 0;

    *value = v;
    return 1;
}

/* =========================
 * 坐标变换
 * ========================= */
typedef struct
{
    double minX;
    double maxX;
    double minY;
    double maxY;
    double scale;
    double offsetX;
    double offsetY;
    double height;
} ViewTransform;

static ViewTransform makeTransform(int width, int height)
{
    ViewTransform t;
    const double margin = 35.0;

    if (g_graph.vertexCount <= 0)
    {
        t.minX = t.maxX = t.minY = t.maxY = 0.0;
        t.scale = 1.0;
        t.offsetX = margin;
        t.offsetY = margin;
        t.height = height;
        return t;
    }

    t.minX = t.maxX = g_graph.vertices[0].x;
    t.minY = t.maxY = g_graph.vertices[0].y;

    for (int i = 1; i < g_graph.vertexCount; i++)
    {
        double x = g_graph.vertices[i].x;
        double y = g_graph.vertices[i].y;

        if (x < t.minX) t.minX = x;
        if (x > t.maxX) t.maxX = x;
        if (y < t.minY) t.minY = y;
        if (y > t.maxY) t.maxY = y;
    }

    double dataW = t.maxX - t.minX;
    double dataH = t.maxY - t.minY;

    if (dataW < 1e-9) dataW = 1.0;
    if (dataH < 1e-9) dataH = 1.0;

    double sx = (width - 2.0 * margin) / dataW;
    double sy = (height - 2.0 * margin) / dataH;
    t.scale = sx < sy ? sx : sy;

    double usedW = dataW * t.scale;
    double usedH = dataH * t.scale;

    t.offsetX = (width - usedW) / 2.0 - t.minX * t.scale;
    t.offsetY = (height - usedH) / 2.0 - t.minY * t.scale;
    t.height = height;

    return t;
}

static double screenX(const ViewTransform *t, double x)
{
    return t->offsetX + x * t->scale;
}

static double screenY(const ViewTransform *t, double y)
{
    /* GTK屏幕Y轴向下，所以翻转 */
    double raw = t->offsetY + y * t->scale;
    return t->height - raw;
}

/* =========================
 * 绘图
 * ========================= */
static gboolean onDraw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    (void)data;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    int width = allocation.width;
    int height = allocation.height;
    ViewTransform t = makeTransform(width, height);

    /* 背景 */
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    /* 1. 道路网络 */
    cairo_set_line_width(cr, 1.2);
    cairo_set_source_rgb(cr, 0.78, 0.78, 0.78);

    for (int i = 0; i < g_graph.vertexCount; i++)
    {
        Edge *e = g_graph.vertices[i].first;

        while (e != NULL)
        {
            int j = e->index;

            /* 无向边存了两次，只画一次 */
            if (i < j && j >= 0 && j < g_graph.vertexCount)
            {
                double x1 = screenX(&t, g_graph.vertices[i].x);
                double y1 = screenY(&t, g_graph.vertices[i].y);
                double x2 = screenX(&t, g_graph.vertices[j].x);
                double y2 = screenY(&t, g_graph.vertices[j].y);

                cairo_move_to(cr, x1, y1);
                cairo_line_to(cr, x2, y2);
            }

            e = e->next;
        }
    }
    cairo_stroke(cr);

    /* 2. 图顶点 */
    cairo_set_source_rgb(cr, 0.45, 0.45, 0.45);
    for (int i = 0; i < g_graph.vertexCount; i++)
    {
        double x = screenX(&t, g_graph.vertices[i].x);
        double y = screenY(&t, g_graph.vertices[i].y);
        cairo_arc(cr, x, y, 2.2, 0, 2.0 * G_PI);
        cairo_fill(cr);
    }

    /* 3. 查询矩形 */
    if (g_queryActive)
    {
        double x1 = screenX(&t, g_queryRect.xmin);
        double x2 = screenX(&t, g_queryRect.xmax);
        double y1 = screenY(&t, g_queryRect.ymin);
        double y2 = screenY(&t, g_queryRect.ymax);

        double left = x1 < x2 ? x1 : x2;
        double top = y1 < y2 ? y1 : y2;
        double rectW = x1 < x2 ? x2 - x1 : x1 - x2;
        double rectH = y1 < y2 ? y2 - y1 : y1 - y2;

        cairo_set_source_rgba(cr, 0.10, 0.45, 0.95, 0.10);
        cairo_rectangle(cr, left, top, rectW, rectH);
        cairo_fill_preserve(cr);

        cairo_set_source_rgb(cr, 0.10, 0.45, 0.95);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);
    }

    /* 4. 最终道路路线 */
    if (g_fullRoute.points != NULL && g_fullRoute.count >= 2)
    {
        cairo_set_source_rgb(cr, 0.90, 0.18, 0.12);
        cairo_set_line_width(cr, 4.0);

        for (int i = 0; i < g_fullRoute.count; i++)
        {
            int point = g_fullRoute.points[i];
            if (point < 0 || point >= g_graph.vertexCount)
                continue;

            double x = screenX(&t, g_graph.vertices[point].x);
            double y = screenY(&t, g_graph.vertices[point].y);

            if (i == 0)
                cairo_move_to(cr, x, y);
            else
                cairo_line_to(cr, x, y);
        }

        cairo_stroke(cr);
    }

    /* 5. 所有订单 */
    for (int i = 0; i < g_orderCount; i++)
    {
        int point = g_orders[i].pointid;
        if (point < 0 || point >= g_graph.vertexCount)
            continue;

        double x = screenX(&t, g_graph.vertices[point].x);
        double y = screenY(&t, g_graph.vertices[point].y);

        if (orderIsSelected(i))
            cairo_set_source_rgb(cr, 0.05, 0.65, 0.25);
        else
            cairo_set_source_rgb(cr, 0.95, 0.55, 0.05);

        cairo_arc(cr, x, y, 6.5, 0, 2.0 * G_PI);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        /* 订单号 */
        char label[32];
        snprintf(label, sizeof(label), "%d", g_orders[i].id);
        cairo_set_font_size(cr, 11.0);
        cairo_move_to(cr, x + 7.0, y - 7.0);
        cairo_show_text(cr, label);
    }

    return FALSE;
}

/* =========================
 * 生成订单 + 构建R树
 * ========================= */
static void onGenerateOrders(GtkButton *button, gpointer data)
{
    (void)button;
    (void)data;

    int count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(g_orderSpin));

    if (count < 2 || count > MAX_ORDERS)
    {
        setStatus("订单数量必须在 2 到 MAX_ORDERS 之间。");
        return;
    }

    clearCurrentRoute();
    freeRTree(&g_rtree);
    initRTree(&g_rtree);

    if (g_orders != NULL)
    {
        free_orders(g_orders);
        g_orders = NULL;
    }

    g_queryActive = 0;
    g_queryCount = 0;

    g_orders = generate(&g_graph, count);
    if (g_orders == NULL)
    {
        setStatus("订单生成失败。");
        return;
    }

    g_orderCount = count;

    for (int i = 0; i < g_orderCount; i++)
    {
        if (!insertOrder(&g_rtree, &g_graph, g_orders, i))
        {
            setStatus("R树插入订单失败，请检查 rtree.c。");
            gtk_widget_queue_draw(g_drawingArea);
            return;
        }
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
             "已生成 %d 个订单，并全部建立 R 树空间索引。",
             g_orderCount);
    setStatus(msg);
    gtk_widget_queue_draw(g_drawingArea);
}

/* =========================
 * R树范围查询
 * ========================= */
static void onRangeQuery(GtkButton *button, gpointer data)
{
    (void)button;
    (void)data;

    if (g_orders == NULL || g_orderCount <= 0)
    {
        setStatus("请先生成订单。");
        return;
    }

    double xmin, ymin, xmax, ymax;

    if (!parseEntryDouble(g_xminEntry, &xmin) ||
        !parseEntryDouble(g_yminEntry, &ymin) ||
        !parseEntryDouble(g_xmaxEntry, &xmax) ||
        !parseEntryDouble(g_ymaxEntry, &ymax))
    {
        setStatus("查询区域坐标格式错误，请输入数字。");
        return;
    }

    if (xmin > xmax)
    {
        double tmp = xmin;
        xmin = xmax;
        xmax = tmp;
    }

    if (ymin > ymax)
    {
        double tmp = ymin;
        ymin = ymax;
        ymax = tmp;
    }

    g_queryRect.xmin = xmin;
    g_queryRect.ymin = ymin;
    g_queryRect.xmax = xmax;
    g_queryRect.ymax = ymax;

    g_queryCount = rangeQuery(&g_rtree,
                              g_queryRect,
                              g_queryResults,
                              MAX_ORDERS);
    g_queryActive = 1;
    clearCurrentRoute();

    char msg[256];
    snprintf(msg, sizeof(msg),
             "R树查询完成：区域 [%.1f, %.1f] - [%.1f, %.1f] 内找到 %d 个订单。",
             xmin, ymin, xmax, ymax, g_queryCount);
    setStatus(msg);
    gtk_widget_queue_draw(g_drawingArea);
}

/* =========================
 * 取消区域筛选
 * ========================= */
static void onUseAllOrders(GtkButton *button, gpointer data)
{
    (void)button;
    (void)data;

    g_queryActive = 0;
    g_queryCount = 0;
    clearCurrentRoute();
    setStatus("已取消区域筛选；路径规划将使用全部订单。");
    gtk_widget_queue_draw(g_drawingArea);
}

/* =========================
 * 规划最终配送路线
 * R树筛选 -> Dijkstra距离矩阵 -> 最近邻 -> 2-opt -> 路径还原
 * ========================= */
static void onPlanRoute(GtkButton *button, gpointer data)
{
    (void)button;
    (void)data;

    if (g_orders == NULL || g_orderCount < 2)
    {
        setStatus("请先生成至少 2 个订单。");
        return;
    }

    int activeCount = g_queryActive ? g_queryCount : g_orderCount;

    if (activeCount < 2)
    {
        setStatus("当前筛选区域内少于 2 个订单，无法进行配送路线规划。");
        return;
    }

    if (activeCount > MAX_ORDERS)
    {
        setStatus("订单数量超过 MAX_ORDERS，无法构造距离矩阵。");
        return;
    }

    Order *activeOrders = malloc(sizeof(Order) * (size_t)activeCount);
    if (activeOrders == NULL)
    {
        setStatus("内存申请失败。");
        return;
    }

    for (int i = 0; i < activeCount; i++)
    {
        int src = g_queryActive ? g_queryResults[i] : i;
        activeOrders[i] = g_orders[src];
    }

    static double matrix[MAX_ORDERS][MAX_ORDERS];
    int route[MAX_ORDERS + 1];

    if (!buildDistanceMatrix(&g_graph,
                             activeOrders,
                             activeCount,
                             matrix))
    {
        free(activeOrders);
        setStatus("订单距离矩阵构建失败。");
        return;
    }

    double nnDistance = nearestNeighbor(matrix, activeCount, route);
    double optimizedDistance = twoOpt(matrix, activeCount, route);

    clearCurrentRoute();

    if (!buildFullRoute(&g_graph,
                        activeOrders,
                        activeCount,
                        route,
                        &g_fullRoute))
    {
        free(activeOrders);
        setStatus("完整道路路线构建失败。");
        return;
    }

    g_routeDistance = optimizedDistance;

    char msg[512];
    snprintf(msg, sizeof(msg),
             "路线规划完成：%d 个订单；最近邻距离 %.2f；2-opt 后 %.2f；完整道路包含 %d 个顶点。",
             activeCount,
             nnDistance,
             optimizedDistance,
             g_fullRoute.count);
    setStatus(msg);

    free(activeOrders);
    gtk_widget_queue_draw(g_drawingArea);
}

/* =========================
 * 重置路线
 * ========================= */
static void onClearRoute(GtkButton *button, gpointer data)
{
    (void)button;
    (void)data;

    clearCurrentRoute();
    setStatus("已清除当前配送路线。");
    gtk_widget_queue_draw(g_drawingArea);
}

/* =========================
 * 退出时释放内存
 * ========================= */
static void onDestroy(GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;

    clearCurrentRoute();
    freeRTree(&g_rtree);

    if (g_orders != NULL)
    {
        free_orders(g_orders);
        g_orders = NULL;
    }

    gtk_main_quit();
}

/* =========================
 * 创建一行坐标输入
 * ========================= */
static GtkWidget *makeEntry(const char *defaultText)
{
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), defaultText);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 8);
    return entry;
}

/* =========================
 * main
 * ========================= */
int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);
    srand((unsigned int)time(NULL));

    if (!loadMap(&g_graph, "data/graph_100.txt"))
    {
        fprintf(stderr, "地图读取失败：data/graph_100.txt\n");
        return 1;
    }

    initRTree(&g_rtree);

    g_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_window), "校园智能配送系统 - R树 + 最短路径 + 路线优化");
    gtk_window_set_default_size(GTK_WINDOW(g_window), 1180, 760);
    gtk_container_set_border_width(GTK_CONTAINER(g_window), 8);
    g_signal_connect(g_window, "destroy", G_CALLBACK(onDestroy), NULL);

    GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(g_window), mainBox);

    /* 左侧控制面板 */
    GtkWidget *controlBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(controlBox, 285, -1);
    gtk_box_pack_start(GTK_BOX(mainBox), controlBox, FALSE, FALSE, 0);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>校园智能配送控制台</b>");
    gtk_box_pack_start(GTK_BOX(controlBox), title, FALSE, FALSE, 4);

    GtkWidget *separator1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controlBox), separator1, FALSE, FALSE, 0);

    /* 订单生成 */
    GtkWidget *orderLabel = gtk_label_new("订单数量：");
    gtk_widget_set_halign(orderLabel, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(controlBox), orderLabel, FALSE, FALSE, 0);

    g_orderSpin = gtk_spin_button_new_with_range(2, MAX_ORDERS < 50 ? MAX_ORDERS : 50, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(g_orderSpin), 20);
    gtk_box_pack_start(GTK_BOX(controlBox), g_orderSpin, FALSE, FALSE, 0);

    GtkWidget *generateButton = gtk_button_new_with_label("1. 生成订单并建立R树");
    g_signal_connect(generateButton, "clicked", G_CALLBACK(onGenerateOrders), NULL);
    gtk_box_pack_start(GTK_BOX(controlBox), generateButton, FALSE, FALSE, 0);

    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controlBox), separator2, FALSE, FALSE, 2);

    /* 范围查询 */
    GtkWidget *queryTitle = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(queryTitle), "<b>R树矩形范围查询</b>");
    gtk_widget_set_halign(queryTitle, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(controlBox), queryTitle, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_box_pack_start(GTK_BOX(controlBox), grid, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("xmin"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("ymin"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("xmax"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("ymax"), 0, 3, 1, 1);

    g_xminEntry = makeEntry("0");
    g_yminEntry = makeEntry("0");
    g_xmaxEntry = makeEntry("50");
    g_ymaxEntry = makeEntry("50");

    gtk_grid_attach(GTK_GRID(grid), g_xminEntry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), g_yminEntry, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), g_xmaxEntry, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), g_ymaxEntry, 1, 3, 1, 1);

    GtkWidget *queryButton = gtk_button_new_with_label("2. R树查询区域订单");
    g_signal_connect(queryButton, "clicked", G_CALLBACK(onRangeQuery), NULL);
    gtk_box_pack_start(GTK_BOX(controlBox), queryButton, FALSE, FALSE, 0);

    GtkWidget *allButton = gtk_button_new_with_label("取消筛选 / 使用全部订单");
    g_signal_connect(allButton, "clicked", G_CALLBACK(onUseAllOrders), NULL);
    gtk_box_pack_start(GTK_BOX(controlBox), allButton, FALSE, FALSE, 0);

    GtkWidget *separator3 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controlBox), separator3, FALSE, FALSE, 2);

    /* 路线规划 */
    GtkWidget *routeButton = gtk_button_new_with_label("3. 生成最终配送路线");
    g_signal_connect(routeButton, "clicked", G_CALLBACK(onPlanRoute), NULL);
    gtk_box_pack_start(GTK_BOX(controlBox), routeButton, FALSE, FALSE, 0);

    GtkWidget *clearButton = gtk_button_new_with_label("清除路线");
    g_signal_connect(clearButton, "clicked", G_CALLBACK(onClearRoute), NULL);
    gtk_box_pack_start(GTK_BOX(controlBox), clearButton, FALSE, FALSE, 0);

    GtkWidget *separator4 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(controlBox), separator4, FALSE, FALSE, 2);

    GtkWidget *legend = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(legend),
        "<b>图例</b>\n"
        "灰色：校园道路网络\n"
        "橙色：普通订单\n"
        "绿色：R树筛选订单\n"
        "蓝框：R树查询区域\n"
        "红线：最终配送道路路线");
    gtk_label_set_xalign(GTK_LABEL(legend), 0.0);
    gtk_box_pack_start(GTK_BOX(controlBox), legend, FALSE, FALSE, 0);

    g_statusLabel = gtk_label_new("系统已启动。准备生成订单。");
    gtk_label_set_line_wrap(GTK_LABEL(g_statusLabel), TRUE);
    gtk_label_set_xalign(GTK_LABEL(g_statusLabel), 0.0);
    gtk_box_pack_end(GTK_BOX(controlBox), g_statusLabel, FALSE, FALSE, 8);

    /* 右侧地图绘图区 */
    GtkWidget *frame = gtk_frame_new("校园道路地图 / 配送路线可视化");
    gtk_box_pack_start(GTK_BOX(mainBox), frame, TRUE, TRUE, 0);

    g_drawingArea = gtk_drawing_area_new();
    gtk_widget_set_size_request(g_drawingArea, 820, 700);
    g_signal_connect(g_drawingArea, "draw", G_CALLBACK(onDraw), NULL);
    gtk_container_add(GTK_CONTAINER(frame), g_drawingArea);

    gtk_widget_show_all(g_window);

    /* 启动后直接生成一次默认订单 */
    onGenerateOrders(NULL, NULL);

    gtk_main();
    return 0;
}
